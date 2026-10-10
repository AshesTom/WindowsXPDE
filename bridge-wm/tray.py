#!/usr/bin/env python3
"""StatusNotifierItem host that shows Linux tray icons in the XP notification area.

Owns org.kde.StatusNotifierWatcher on the session bus and registers itself
as the host. Each item's icon, tooltip and menu go to the bridge IPC
directory; the Win32 proxy adds them with Shell_NotifyIcon and writes the
user's clicks back as *.tray files. Wakes only for D-Bus and directory events.

Files (all atomically renamed in the IPC directory):
- tray.tsv: "ID TAB status TAB tooltip" per shown item, status A (active)
  or N (needs attention); passive items are left out.
- tray-ID.argb: W4I1 icon at the size the proxy asks for in tray-size.
- menu-ID.tsv: "SEQ" then "depth TAB item TAB flags TAB label" per entry;
  flags are any of s (separator), d (disabled), c (checked), r (radio).
- from the proxy, *.tray: "activate ID X Y", "secondary ID X Y",
  "context ID X Y" or "event ID SEQ ITEM".
"""
import argparse
import os
from pathlib import Path
import signal
import struct
import sys
import time

import gi
gi.require_version('Gio', '2.0')
gi.require_version('GdkPixbuf', '2.0')
from gi.repository import Gio, GLib, GdkPixbuf

WATCHER = 'org.kde.StatusNotifierWatcher'
ITEM = 'org.kde.StatusNotifierItem'
MENU = 'com.canonical.dbusmenu'
WATCHER_XML = '''<node><interface name="org.kde.StatusNotifierWatcher">
<method name="RegisterStatusNotifierItem"><arg name="service" type="s" direction="in"/></method>
<method name="RegisterStatusNotifierHost"><arg name="service" type="s" direction="in"/></method>
<property name="RegisteredStatusNotifierItems" type="as" access="read"/>
<property name="IsStatusNotifierHostRegistered" type="b" access="read"/>
<property name="ProtocolVersion" type="i" access="read"/>
<signal name="StatusNotifierItemRegistered"><arg type="s"/></signal>
<signal name="StatusNotifierItemUnregistered"><arg type="s"/></signal>
<signal name="StatusNotifierHostRegistered"/>
<signal name="StatusNotifierHostUnregistered"/>
</interface></node>'''
MAX_ITEMS = 64
MAX_MENU = 256


def write_atomic(path, data):
    tmp = path.with_suffix('.tmp-tray')
    tmp.write_bytes(data)
    tmp.replace(path)


def mnemonic(label):
    """dbusmenu marks access keys with _ (and __ for a literal _); Win32 uses &."""
    out, i = [], 0
    while i < len(label):
        ch = label[i]
        if ch == '_' and i + 1 < len(label):
            out.append('_' if label[i + 1] == '_' else '&' + label[i + 1])
            i += 2
            continue
        out.append('&&' if ch == '&' else ch)
        i += 1
    return ''.join(out).replace('\t', ' ').replace('\n', ' ')


class Item:
    def __init__(self, host, key, name, path):
        self.host, self.key, self.name, self.path = host, key, name, path
        self.id = host.next_id()
        self.props = {}
        self.icon = None  # last written icon bytes
        self.menu_seq = 0
        self.watch = Gio.bus_watch_name_on_connection(host.bus, name, Gio.BusNameWatcherFlags.NONE,
                                                      None, lambda *_: host.remove(key))
        self.subscription = host.bus.signal_subscribe(name, ITEM, None, path, None, Gio.DBusSignalFlags.NONE,
                                                      lambda *_: self.refresh())
        self.refresh()

    def close(self):
        Gio.bus_unwatch_name(self.watch)
        self.host.bus.signal_unsubscribe(self.subscription)
        for name in (f'tray-{self.id}.argb', f'menu-{self.id}.tsv'):
            try:
                (self.host.ipc / name).unlink()
            except FileNotFoundError:
                pass

    def refresh(self):
        self.host.bus.call(self.name, self.path, 'org.freedesktop.DBus.Properties', 'GetAll',
                           GLib.Variant('(s)', (ITEM,)), GLib.VariantType('(a{sv})'),
                           Gio.DBusCallFlags.NONE, 2000, None, self.refreshed)

    def refreshed(self, bus, result):
        try:
            self.props = bus.call_finish(result).unpack()[0]
        except GLib.Error as e:
            print('properties of', self.name, self.path, e.message, file=sys.stderr)
            return
        self.host.publish()

    def status(self):
        status = self.props.get('Status', 'Active')
        return {'Passive': '', 'NeedsAttention': 'N'}.get(status, 'A')

    def tooltip(self):
        tip = self.props.get('ToolTip')
        text = tip[2] if tip and len(tip) > 2 and tip[2] else self.props.get('Title', '') or self.props.get('Id', '')
        return ' '.join(text.split())[:127]

    def pixbuf(self, size):
        attention = self.status() == 'N'
        names = ('AttentionIcon', 'Icon') if attention else ('Icon',)
        for prefix in names:
            pixmaps = self.props.get(prefix + 'Pixmap') or []
            pixmaps = [p for p in pixmaps if p[0] > 0 and p[1] > 0 and len(p[2]) == p[0] * p[1] * 4]
            if pixmaps:
                # The smallest image at least as large as wanted scales best.
                w, h, data = min(pixmaps, key=lambda p: (p[0] < size, abs(p[0] - size)))
                rgba = bytearray(len(data))
                rgba[0::4], rgba[1::4], rgba[2::4], rgba[3::4] = data[1::4], data[2::4], data[3::4], data[0::4]
                return GdkPixbuf.Pixbuf.new_from_bytes(GLib.Bytes.new(bytes(rgba)), GdkPixbuf.Colorspace.RGB,
                                                       True, 8, w, h, w * 4)
            name = self.props.get(prefix + 'Name')
            if name:
                pixbuf = self.host.theme_icon(name, size, self.props.get('IconThemePath', ''))
                if pixbuf:
                    return pixbuf
        return None

    def icon_bytes(self, size):
        pixbuf = self.pixbuf(size)
        if not pixbuf:
            return None
        if pixbuf.get_width() != size or pixbuf.get_height() != size:
            w, h = pixbuf.get_width(), pixbuf.get_height()
            scale = size / max(w, h)
            sw, sh = max(1, round(w * scale)), max(1, round(h * scale))
            scaled = pixbuf.scale_simple(sw, sh, GdkPixbuf.InterpType.HYPER)
            pixbuf = GdkPixbuf.Pixbuf.new(GdkPixbuf.Colorspace.RGB, True, 8, size, size)
            pixbuf.fill(0)
            scaled.copy_area(0, 0, sw, sh, pixbuf, (size - sw) // 2, (size - sh) // 2)
        if not pixbuf.get_has_alpha():
            pixbuf = pixbuf.add_alpha(False, 0, 0, 0)
        stride, data = pixbuf.get_rowstride(), pixbuf.get_pixels()
        out = bytearray(b'W4I1' + struct.pack('<II', size, size))
        for y in range(size):
            row = data[y * stride:y * stride + size * 4]
            line = bytearray(size * 4)
            # W4I1 pixels are little-endian ARGB words: B, G, R, A in memory.
            line[0::4], line[1::4], line[2::4], line[3::4] = row[2::4], row[1::4], row[0::4], row[3::4]
            out += line
        return bytes(out)

    def call(self, method, args, done=None):
        self.host.bus.call(self.name, self.path, ITEM, method, args, None, Gio.DBusCallFlags.NONE, 2000, None,
                           done or self.host.ignore)

    def activate(self, x, y):
        if self.props.get('ItemIsMenu'):
            return self.context(x, y)
        self.call('Activate', GLib.Variant('(ii)', (x, y)))

    def secondary(self, x, y):
        self.call('SecondaryActivate', GLib.Variant('(ii)', (x, y)))

    def context(self, x, y):
        menu = self.props.get('Menu')
        if not menu or menu == '/':
            self.call('ContextMenu', GLib.Variant('(ii)', (x, y)))
            return
        self.menu_seq += 1
        seq = self.menu_seq
        # AboutToShow lets the application update the entries first.
        self.host.bus.call(self.name, menu, MENU, 'AboutToShow', GLib.Variant('(i)', (0,)), None,
                           Gio.DBusCallFlags.NONE, 1000, None,
                           lambda bus, result: self.layout(menu, seq, bus, result))

    def layout(self, menu, seq, bus, result):
        try:
            bus.call_finish(result)
        except GLib.Error:
            pass  # Optional for the application.
        bus.call(self.name, menu, MENU, 'GetLayout', GLib.Variant('(iias)', (0, -1, [])),
                 GLib.VariantType('(u(ia{sv}av))'), Gio.DBusCallFlags.NONE, 2000, None,
                 lambda bus, result: self.write_menu(menu, seq, bus, result))

    def write_menu(self, menu, seq, bus, result):
        try:
            _, root = bus.call_finish(result).unpack()
        except GLib.Error as e:
            print('menu of', self.name, e.message, file=sys.stderr)
            return
        lines = [str(seq)]

        def walk(node, depth):
            for child in node[2]:
                if len(lines) > MAX_MENU:
                    return
                item_id, props, _ = child
                if not props.get('visible', True):
                    continue
                flags = ''
                if props.get('type') == 'separator':
                    flags += 's'
                if not props.get('enabled', True):
                    flags += 'd'
                if props.get('toggle-state', 0) == 1:
                    flags += 'r' if props.get('toggle-type') == 'radio' else 'c'
                lines.append(f'{depth}\t{item_id}\t{flags or "-"}\t{mnemonic(props.get("label", ""))}')
                if depth < 8:
                    walk(child, depth + 1)
        walk(root, 0)
        self.menu = menu
        write_atomic(self.host.ipc / f'menu-{self.id}.tsv', ('\n'.join(lines) + '\n').encode())

    def event(self, seq, item):
        if seq != self.menu_seq or not getattr(self, 'menu', None):
            return
        self.host.bus.call(self.name, self.menu, MENU, 'Event',
                           GLib.Variant('(isvu)', (item, 'clicked', GLib.Variant('i', 0), int(time.time()))),
                           None, Gio.DBusCallFlags.NONE, 2000, None, self.host.ignore)


class Host:
    def __init__(self, ipc):
        self.ipc = ipc
        self.items = {}
        self.ids = 0
        self.tsv = None
        self.bus = Gio.bus_get_sync(Gio.BusType.SESSION)
        self.loop = GLib.MainLoop()
        self.monitor = Gio.File.new_for_path(str(ipc)).monitor_directory(Gio.FileMonitorFlags.WATCH_MOVES, None)
        self.monitor.connect('changed', lambda *_: self.commands())
        node = Gio.DBusNodeInfo.new_for_xml(WATCHER_XML)
        self.bus.register_object('/StatusNotifierWatcher', node.interfaces[0], self.method, self.property, None)
        Gio.bus_own_name_on_connection(self.bus, WATCHER, Gio.BusNameOwnerFlags.DO_NOT_QUEUE,
                                       self.acquired, self.lost)
        Gio.bus_own_name_on_connection(self.bus, f'org.kde.StatusNotifierHost-{os.getpid()}',
                                       Gio.BusNameOwnerFlags.NONE, None, None)
        # Every theme below the icon directories is searched; the names of
        # tray icons rarely collide between themes.
        self.theme_dirs = [p for d in [GLib.get_user_data_dir()] + GLib.get_system_data_dirs()
                           for p in (Path(d) / 'icons', Path(d) / 'pixmaps') if p.is_dir()]
        self.index = None

    def acquired(self, *_):
        self.emit('StatusNotifierHostRegistered', None)
        self.publish()

    def lost(self, *_):
        # Another watcher (a desktop's own tray) already serves this login.
        print('another StatusNotifierWatcher owns the name; exiting', file=sys.stderr)
        self.loop.quit()

    def emit(self, signal, params):
        self.bus.emit_signal(None, '/StatusNotifierWatcher', WATCHER, signal, params)

    def next_id(self):
        used = {item.id for item in self.items.values()}
        while True:
            self.ids = self.ids % 0xffff + 1
            if self.ids not in used:
                return self.ids

    def method(self, bus, sender, path, interface, method, params, invocation):
        if method == 'RegisterStatusNotifierItem':
            service = params.unpack()[0]
            # Some items pass their object path and are named by the sender.
            name, path = (sender, service) if service.startswith('/') else (service, '/StatusNotifierItem')
            key = f'{name}{path}'
            if key not in self.items and len(self.items) < MAX_ITEMS:
                self.items[key] = Item(self, key, name, path)
                self.emit('StatusNotifierItemRegistered', GLib.Variant('(s)', (key,)))
        invocation.return_value(None)

    def property(self, bus, sender, path, interface, name):
        if name == 'RegisteredStatusNotifierItems':
            return GLib.Variant('as', list(self.items))
        if name == 'IsStatusNotifierHostRegistered':
            return GLib.Variant('b', True)
        return GLib.Variant('i', 0)

    def remove(self, key):
        item = self.items.pop(key, None)
        if item:
            item.close()
            self.emit('StatusNotifierItemUnregistered', GLib.Variant('(s)', (key,)))
            self.publish()

    def ignore(self, bus, result):
        try:
            bus.call_finish(result)
        except GLib.Error as e:
            print('call failed:', e.message, file=sys.stderr)

    @staticmethod
    def scan(root, index):
        """Index PNG and SVG icons below root by name: (size or 0 for scalable, path)."""
        for dirpath, _, files in os.walk(root):
            size = 0
            for part in Path(dirpath).relative_to(root).parts:
                if 'x' in part and part.split('@')[0].split('x')[0].isdigit():
                    size = int(part.split('x')[0])
            for file in files:
                stem, ext = os.path.splitext(file)
                if ext in ('.png', '.svg'):
                    index.setdefault(stem, []).append((0 if ext == '.svg' else size, os.path.join(dirpath, file)))

    def theme_icon(self, name, size, extra):
        if name.startswith('/'):
            choices = [(0, name)]
        else:
            if self.index is None:
                self.index = {}
                for root in self.theme_dirs:
                    self.scan(root, self.index)
            extra_index = {}
            if extra and os.path.isdir(extra):
                self.scan(extra, extra_index)
            choices = extra_index.get(name) or self.index.get(name)
            if not choices and name.endswith('-symbolic'):
                choices = self.index.get(name[:-len('-symbolic')])
            if not choices:
                return None
            # An image of the wanted size, then a scalable one, then the
            # nearest larger and finally the largest smaller image.
            choices = sorted(choices, key=lambda c: (c[0] != size, c[0] != 0, c[0] < size,
                                                     abs(c[0] - size)))
        for _, path in choices:
            try:
                return GdkPixbuf.Pixbuf.new_from_file_at_size(path, size, size)
            except GLib.Error:
                continue
        return None

    def icon_size(self):
        try:
            size = int((self.ipc / 'tray-size').read_text().split()[0])
        except (OSError, ValueError, IndexError):
            return 16
        return size if 8 <= size <= 64 else 16

    def publish(self):
        size = self.icon_size()
        lines = []
        for item in sorted(self.items.values(), key=lambda i: i.id):
            status = item.status()
            if not status:
                continue
            icon = item.icon_bytes(size)
            if icon and icon != item.icon:
                write_atomic(self.ipc / f'tray-{item.id}.argb', icon)
            item.icon = icon
            lines.append(f'{item.id}\t{status}\t{item.tooltip()}')
        text = ('\n'.join(lines) + '\n' if lines else '').encode()
        if text != self.tsv:
            write_atomic(self.ipc / 'tray.tsv', text)
            self.tsv = text

    def commands(self):
        if (self.ipc / 'tray-size').exists() and getattr(self, 'size', None) != self.icon_size():
            self.size = self.icon_size()
            for item in self.items.values():
                item.icon = None
            self.publish()
        for path in sorted(self.ipc.glob('*.tray')):
            try:
                words = path.read_text().split()
                path.unlink()
            except OSError:
                continue
            try:
                op, ident, args = words[0], int(words[1]), [int(w) for w in words[2:]]
            except (IndexError, ValueError):
                continue
            item = next((i for i in self.items.values() if i.id == ident), None)
            if not item:
                continue
            if op in ('activate', 'secondary', 'context') and len(args) == 2:
                getattr(item, op)(*args)
            elif op == 'event' and len(args) == 2:
                item.event(*args)

    def run(self):
        for sig in (signal.SIGTERM, signal.SIGINT, signal.SIGHUP):
            GLib.unix_signal_add(GLib.PRIORITY_DEFAULT, sig, lambda: self.loop.quit() or GLib.SOURCE_REMOVE)
        self.commands()
        self.loop.run()
        try:
            (self.ipc / 'tray.tsv').unlink()
        except FileNotFoundError:
            pass


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--ipc', type=Path, required=True)
    Host(parser.parse_args().ipc).run()
