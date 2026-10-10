#!/usr/bin/env python3
"""Check bridge-wm/tray.py against a self-made StatusNotifierItem.

Runs on a private session bus (dbus-run-session) and a temporary IPC
directory: the item's pixmap and theme icon reach tray-ID.argb at the size
in tray-size, tooltip and status reach tray.tsv, the proxy's *.tray commands
reach Activate, SecondaryActivate and the dbusmenu, and the entry goes when
the item turns passive or leaves the bus. A second host defers to the first.
"""
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import time
import zlib

ROOT = Path(__file__).resolve().parents[1]
if not os.environ.get('TRAY_HOST_TEST_BUS'):
    os.environ['TRAY_HOST_TEST_BUS'] = '1'
    os.execvp('dbus-run-session', ['dbus-run-session', '--', sys.executable, *sys.argv])

import gi
gi.require_version('Gio', '2.0')
from gi.repository import Gio, GLib

ITEM_XML = '''<node>
<interface name="org.kde.StatusNotifierItem">
<method name="Activate"><arg type="i" direction="in"/><arg type="i" direction="in"/></method>
<method name="SecondaryActivate"><arg type="i" direction="in"/><arg type="i" direction="in"/></method>
<method name="ContextMenu"><arg type="i" direction="in"/><arg type="i" direction="in"/></method>
<property name="Id" type="s" access="read"/><property name="Title" type="s" access="read"/>
<property name="Status" type="s" access="read"/><property name="IconName" type="s" access="read"/>
<property name="IconThemePath" type="s" access="read"/>
<property name="IconPixmap" type="a(iiay)" access="read"/>
<property name="ToolTip" type="(sa(iiay)ss)" access="read"/>
<property name="ItemIsMenu" type="b" access="read"/><property name="Menu" type="o" access="read"/>
<signal name="NewIcon"/><signal name="NewStatus"><arg type="s"/></signal>
</interface>
<interface name="com.canonical.dbusmenu">
<method name="GetLayout"><arg type="i" direction="in"/><arg type="i" direction="in"/><arg type="as" direction="in"/>
<arg type="u" direction="out"/><arg type="(ia{sv}av)" direction="out"/></method>
<method name="AboutToShow"><arg type="i" direction="in"/><arg type="b" direction="out"/></method>
<method name="Event"><arg type="i" direction="in"/><arg type="s" direction="in"/><arg type="v" direction="in"/>
<arg type="u" direction="in"/></method>
</interface></node>'''

failures = []


def check(condition, message):
    print(('PASS: ' if condition else 'FAIL: ') + message)
    if not condition:
        failures.append(message)


def png(path, size, rgba):
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    rows = b''.join(b'\0' + bytes(rgba) * size for _ in range(size))
    path.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', size, size, 8, 6, 0, 0, 0))
                     + chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b''))


class Item:
    def __init__(self, bus, theme):
        self.bus = bus
        self.calls = []
        # 16x16 ARGB (network order): a red square with a transparent border.
        pixels = bytearray(16 * 16 * 4)
        for y in range(2, 14):
            for x in range(2, 14):
                pixels[(y * 16 + x) * 4:(y * 16 + x) * 4 + 4] = b'\xff\xff\x00\x00'
        self.props = {
            'Id': GLib.Variant('s', 'tray-host-test'), 'Title': GLib.Variant('s', 'Title text'),
            'Status': GLib.Variant('s', 'Active'), 'IconName': GLib.Variant('s', ''),
            'IconThemePath': GLib.Variant('s', str(theme)),
            'IconPixmap': GLib.Variant('a(iiay)', [(16, 16, bytes(pixels))]),
            'ToolTip': GLib.Variant('(sa(iiay)ss)', ('', [], 'Tip\ttext', '')),
            'ItemIsMenu': GLib.Variant('b', False), 'Menu': GLib.Variant('o', '/Menu')}
        node = Gio.DBusNodeInfo.new_for_xml(ITEM_XML)
        bus.register_object('/StatusNotifierItem', node.interfaces[0], self.method, self.prop, None)
        bus.register_object('/Menu', node.interfaces[1], self.method, None, None)
        self.owner = Gio.bus_own_name_on_connection(bus, 'org.example.TrayHostTest', 0, None, None)

    def method(self, bus, sender, path, iface, name, params, invocation):
        self.calls.append((name, params.unpack()))
        if name == 'GetLayout':
            entries = [(1, {'label': GLib.Variant('s', '_Open')}, []),
                       (2, {'type': GLib.Variant('s', 'separator')}, []),
                       (3, {'label': GLib.Variant('s', 'Sub'), 'children-display': GLib.Variant('s', 'submenu')},
                        [GLib.Variant('(ia{sv}av)', (4, {'label': GLib.Variant('s', 'Inner && more'),
                                                        'toggle-type': GLib.Variant('s', 'checkmark'),
                                                        'toggle-state': GLib.Variant('i', 1)}, []))]),
                       (5, {'label': GLib.Variant('s', 'Hidden'), 'visible': GLib.Variant('b', False)}, []),
                       (6, {'label': GLib.Variant('s', 'Off'), 'enabled': GLib.Variant('b', False)}, [])]
            children = [GLib.Variant('(ia{sv}av)', e) for e in entries]
            invocation.return_value(GLib.Variant('(u(ia{sv}av))', (1, (0, {}, children))))
        elif name == 'AboutToShow':
            invocation.return_value(GLib.Variant('(b)', (False,)))
        else:
            invocation.return_value(None)

    def prop(self, bus, sender, path, iface, name):
        return self.props[name]

    def signal(self, name, params=None):
        self.bus.emit_signal(None, '/StatusNotifierItem', 'org.kde.StatusNotifierItem', name, params)


def wait(predicate, timeout=5.0):
    end = time.monotonic() + timeout
    context = GLib.MainContext.default()
    while time.monotonic() < end:
        while context.pending():
            context.iteration(False)
        if predicate():
            return True
        time.sleep(0.02)
    return False


def argb(path):
    data = path.read_bytes()
    width, height = struct.unpack('<II', data[4:12])
    return data[:4], width, height, [struct.unpack('<I', data[12 + i * 4:16 + i * 4])[0] for i in range(width * height)]


def command(ipc, text, number=[0]):
    number[0] += 1
    tmp = ipc / f'test-{number[0]}.tmp'
    tmp.write_text(text + '\n')
    tmp.replace(ipc / f'test-{number[0]}.tray')


def read(path):
    try:
        return path.read_text()
    except OSError:
        return None


def main():
    work = Path(tempfile.mkdtemp(prefix='tray-host-'))
    ipc, theme = work / 'ipc', work / 'icons'
    ipc.mkdir()
    (theme / 'hicolor/24x24/apps').mkdir(parents=True)
    png(theme / 'hicolor/24x24/apps/tray-host-test-icon.png', 24, (0, 0, 255, 255))
    (ipc / 'tray-size').write_text('16\n')
    log = (work / 'tray.log').open('w')
    host = subprocess.Popen([sys.executable, str(ROOT / 'bridge-wm/tray.py'), '--ipc', str(ipc)],
                            stdout=log, stderr=subprocess.STDOUT)
    bus = Gio.bus_get_sync(Gio.BusType.SESSION)
    item = Item(bus, theme)
    try:
        check(wait(lambda: bus.call_sync('org.freedesktop.DBus', '/org/freedesktop/DBus', 'org.freedesktop.DBus',
                                         'NameHasOwner', GLib.Variant('(s)', ('org.kde.StatusNotifierWatcher',)),
                                         None, 0, -1, None).unpack()[0]), 'host owns the watcher name')
        bus.call_sync('org.kde.StatusNotifierWatcher', '/StatusNotifierWatcher', 'org.kde.StatusNotifierWatcher',
                      'RegisterStatusNotifierItem', GLib.Variant('(s)', ('org.example.TrayHostTest',)),
                      None, 0, -1, None)
        tsv = ipc / 'tray.tsv'
        check(wait(lambda: (read(tsv) or '').startswith('1\tA\t')), 'item listed as active')
        check(read(tsv) == '1\tA\tTip text\n', 'tooltip from ToolTip, tab folded: %r' % read(tsv))
        icon = ipc / 'tray-1.argb'
        check(wait(icon.exists), 'icon written')
        magic, width, height, pixels = argb(icon)
        check((magic, width, height) == (b'W4I1', 16, 16), 'W4I1 16x16')
        check(pixels[8 * 16 + 8] == 0xffff0000 and pixels[0] >> 24 == 0, 'pixmap pixels as little-endian ARGB')

        (ipc / 'tray-size').write_text('24\n')
        command(ipc, 'noop 1')  # a directory change makes the host read the size
        check(wait(lambda: argb(icon)[1] == 24), 'icon re-rendered at the size in tray-size')

        item.props['IconPixmap'] = GLib.Variant('a(iiay)', [])
        item.props['IconName'] = GLib.Variant('s', 'tray-host-test-icon')
        item.signal('NewIcon')
        check(wait(lambda: argb(icon)[3][12 * 24 + 12] == 0xff0000ff), 'theme icon from IconThemePath')

        command(ipc, 'activate 1 10 20')
        check(wait(lambda: ('Activate', (10, 20)) in item.calls), 'left click reaches Activate')
        command(ipc, 'secondary 1 3 4')
        check(wait(lambda: ('SecondaryActivate', (3, 4)) in item.calls), 'middle click reaches SecondaryActivate')

        command(ipc, 'context 1 5 6')
        menu = ipc / 'menu-1.tsv'
        check(wait(menu.exists), 'menu written')
        check(read(menu) == '1\n0\t1\t-\t&Open\n0\t2\ts\t\n0\t3\t-\tSub\n1\t4\tc\tInner &&&& more\n0\t6\td\tOff\n',
              'menu entries, access keys, separator, submenu, check, hidden and disabled: %r' % read(menu))
        check(any(c[0] == 'AboutToShow' for c in item.calls), 'AboutToShow before the layout')
        command(ipc, 'event 1 1 4')
        check(wait(lambda: any(c[0] == 'Event' and c[1][:2] == (4, 'clicked') for c in item.calls)),
              'chosen entry reaches Event')
        command(ipc, 'event 1 0 4')
        time.sleep(0.3)
        check(sum(c[0] == 'Event' for c in item.calls) == 1, 'stale menu choice ignored')

        other = subprocess.run([sys.executable, str(ROOT / 'bridge-wm/tray.py'), '--ipc', str(work)],
                               capture_output=True, text=True, timeout=10)
        check(other.returncode == 0 and 'another StatusNotifierWatcher' in other.stderr,
              'second host defers to the first')

        item.props['Status'] = GLib.Variant('s', 'Passive')
        item.signal('NewStatus', GLib.Variant('(s)', ('Passive',)))
        check(wait(lambda: read(tsv) == ''), 'passive item hidden')
        item.props['Status'] = GLib.Variant('s', 'Active')
        item.signal('NewStatus', GLib.Variant('(s)', ('Active',)))
        check(wait(lambda: read(tsv) == '1\tA\tTip text\n'), 'active again')

        Gio.bus_unown_name(item.owner)
        check(wait(lambda: read(tsv) == '' and not icon.exists() and not menu.exists()),
              'entry, icon and menu removed when the item leaves')
    finally:
        host.terminate()
        host.wait(5)
        log.close()
    check(not tsv.exists(), 'tray.tsv removed when the host stops')
    if failures:
        print((work / 'tray.log').read_text())
        sys.exit(1)
    print('PASS: tray host')


main()
