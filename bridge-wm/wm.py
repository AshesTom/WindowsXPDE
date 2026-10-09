#!/usr/bin/env python3
"""Small single-screen X11 WM and file IPC endpoint for the Win32 shell bridge."""
import argparse
import json
import os
from pathlib import Path
import select
import signal
import struct
import time
from Xlib import X, XK, Xatom, Xutil, display, error, protocol
from icons import payload as icon_payload


class Manager:
    def __init__(self, ipc):
        self.ipc = ipc.resolve()
        self.ipc.mkdir(mode=0o700, parents=True, exist_ok=True)
        stat = self.ipc.stat()
        if stat.st_uid != os.getuid() or stat.st_mode & 0o077:
            raise SystemExit('IPC directory must be owned by this user, mode 0700')
        self.d = display.Display()
        self.root = self.d.screen().root
        self.atoms = {}
        self.clients = {}
        self.active = 0
        self.drag = None
        self.running = True
        self.previous_snapshot = None
        self.previous_geometry = None
        self.previous_icons = {}
        self.frames = {}
        self.cascade_slot = 0
        self.bad_access = False
        def onerror(err, request):
            if isinstance(err, error.BadAccess):
                self.bad_access = True
        self.d.set_error_handler(onerror)
        self.root.change_attributes(event_mask=X.SubstructureRedirectMask | X.SubstructureNotifyMask |
                                    X.PropertyChangeMask | X.StructureNotifyMask)
        self.d.sync()
        if self.bad_access:
            raise SystemExit('Another WM already owns this X display')
        self.support = self.root.create_window(-1, -1, 1, 1, 0, X.CopyFromParent)
        self.setprop(self.root, '_NET_SUPPORTING_WM_CHECK', Xatom.WINDOW, [self.support.id])
        self.setprop(self.support, '_NET_SUPPORTING_WM_CHECK', Xatom.WINDOW, [self.support.id])
        self.support.change_property(self.atom('_NET_WM_NAME'), self.atom('UTF8_STRING'), 8, b'win4wine')
        supported = ['_NET_SUPPORTED', '_NET_SUPPORTING_WM_CHECK', '_NET_CLIENT_LIST',
                     '_NET_CLIENT_LIST_STACKING', '_NET_ACTIVE_WINDOW', '_NET_WORKAREA',
                     '_NET_NUMBER_OF_DESKTOPS', '_NET_CURRENT_DESKTOP', '_NET_WM_STATE',
                     '_NET_WM_STATE_HIDDEN', '_NET_WM_STATE_MAXIMIZED_VERT', '_NET_WM_STATE_MAXIMIZED_HORZ',
                     '_NET_WM_STATE_ABOVE',
                     '_NET_WM_WINDOW_TYPE', '_NET_WM_WINDOW_TYPE_DESKTOP', '_NET_WM_WINDOW_TYPE_DOCK',
                     '_NET_WM_ICON', '_NET_FRAME_EXTENTS', '_NET_CLOSE_WINDOW', '_NET_WM_STRUT', '_NET_WM_STRUT_PARTIAL']
        self.setprop(self.root, '_NET_SUPPORTED', Xatom.ATOM, [self.atom(a) for a in supported])
        self.setprop(self.root, '_NET_NUMBER_OF_DESKTOPS', Xatom.CARDINAL, [1])
        self.setprop(self.root, '_NET_CURRENT_DESKTOP', Xatom.CARDINAL, [0])
        self.own_xsettings()
        self.tab = self.d.keysym_to_keycode(XK.string_to_keysym('Tab'))
        for modifiers in (0, X.LockMask, X.Mod2Mask, X.LockMask | X.Mod2Mask):
            self.root.grab_key(self.tab, X.Mod1Mask | modifiers, True, X.GrabModeAsync, X.GrabModeAsync)
            self.root.grab_key(self.tab, X.Mod1Mask | X.ShiftMask | modifiers, True, X.GrabModeAsync, X.GrabModeAsync)
            for button in (1, 3):
                self.root.grab_button(button, X.Mod1Mask | modifiers, False,
                                      X.ButtonPressMask | X.ButtonReleaseMask | X.PointerMotionMask,
                                      X.GrabModeAsync, X.GrabModeAsync, X.NONE, X.NONE)
            self.root.grab_key(self.d.keysym_to_keycode(XK.string_to_keysym('F4')), X.Mod1Mask | modifiers,
                               True, X.GrabModeAsync, X.GrabModeAsync)
        for w in self.root.query_tree().children:
            if w.get_attributes().map_state == X.IsViewable:
                self.manage(w)
        self.workarea()

    def own_xsettings(self):
        """Hide GTK header-bar window buttons; the Win32 frame already has them.

        Only claims an unowned XSETTINGS selection, so an existing settings
        daemon keeps control. GTK falls back to its defaults for other keys.
        """
        selection = self.atom('_XSETTINGS_S%d' % self.d.get_default_screen())
        if self.d.get_selection_owner(selection) != X.NONE:
            return
        def pad(data):
            return data + b'\0' * (-len(data) % 4)
        name, value = b'Gtk/DecorationLayout', b':'
        entry = struct.pack('<BBH', 1, 0, len(name)) + pad(name) + struct.pack('<II', 0, len(value)) + pad(value)
        settings = struct.pack('<B3xII', 0, 0, 1) + entry
        kind = self.atom('_XSETTINGS_SETTINGS')
        self.support.change_property(kind, kind, 8, settings)
        self.support.set_selection_owner(selection, X.CurrentTime)
        if self.d.get_selection_owner(selection) != self.support:
            return
        self.root.send_event(protocol.event.ClientMessage(
            window=self.root, client_type=self.atom('MANAGER'),
            data=(32, [X.CurrentTime, selection, self.support.id, 0, 0])), event_mask=X.StructureNotifyMask)
        self.d.flush()

    def atom(self, name):
        if name not in self.atoms:
            self.atoms[name] = self.d.intern_atom(name)
        return self.atoms[name]

    def prop(self, w, name):
        p = w.get_full_property(self.atom(name), X.AnyPropertyType)
        return p.value if p else []

    def setprop(self, w, name, kind, data):
        w.change_property(self.atom(name), kind, 32, data)

    def kind(self, w):
        cls = bytes(self.prop(w, '_WIN4WINE_CLASS')).decode('ascii', 'replace').strip('\0').lower()
        types = self.prop(w, '_NET_WM_WINDOW_TYPE')
        if cls == 'dv2controlhost':
            return 'popup'
        if cls == 'win4wineproxy':
            return 'proxy'
        if cls in ('progman', 'workerw') or self.atom('_NET_WM_WINDOW_TYPE_DESKTOP') in types:
            return 'desktop'
        if cls == 'shell_traywnd' or self.atom('_NET_WM_WINDOW_TYPE_DOCK') in types:
            return 'dock'
        return 'normal'

    def manage(self, w):
        attrs = w.get_attributes()
        if attrs.override_redirect or w.id == self.support.id:
            w.map()
            return
        if w.id not in self.clients:
            self.clients[w.id] = {'w': w, 'hidden': False, 'kind': self.kind(w), 'restore': None, 'geometry': None, 'framed': False, 'icon_dirty': True}
            w.change_attributes(event_mask=X.PropertyChangeMask | X.FocusChangeMask)
            w.grab_button(1, 0, True, X.ButtonPressMask, X.GrabModeSync, X.GrabModeAsync, X.NONE, X.NONE)
        client = self.clients[w.id]
        client['kind'] = self.kind(w)
        if client['kind'] == 'proxy':
            if w.id not in self.frames:
                w.unmap()
            return
        client['hidden'] = False
        self.setprop(w, 'WM_STATE', self.atom('WM_STATE'), [1, 0])
        w.map()
        if client['kind'] in ('normal', 'popup'):
            self.focus(w)
        self.restack()
        self.workarea()

    def redraw_native(self, w):
        # Wine keeps its own window surfaces. Native clients (notably xterm on
        # the VM's VGA path) can retain stale pixels after resize/activation.
        # Ask exposed descendants to repaint without clearing their contents or
        # injecting input. A terminal's drawing window is a child of its client.
        if len(self.prop(w, '_WIN4WINE_HWND')):
            return
        pending = [w]
        for _ in range(2048):
            if not pending:
                break
            child = pending.pop()
            if child.get_attributes().map_state != X.IsViewable:
                continue
            r = child.get_geometry()
            child.send_event(protocol.event.Expose(window=child, x=0, y=0,
                             width=r.width, height=r.height, count=0), event_mask=X.ExposureMask)
            pending.extend(child.query_tree().children)

    def configure_notify(self, w):
        # ICCCM: report root coordinates even when a move/stack request did not
        # generate a size change for the client. Native clients stay root children.
        r = w.get_geometry()
        w.send_event(protocol.event.ConfigureNotify(event=w, window=w,
                     above_sibling=X.NONE, x=r.x, y=r.y, width=r.width,
                     height=r.height, border_width=r.border_width,
                     override=False), event_mask=X.StructureNotifyMask)

    def focus(self, w):
        # Wine activation targets the proxy's XID; the native client owns X
        # keyboard focus and the single EWMH task identity.
        if w.id in self.frames:
            native = self.clients.get(self.frames[w.id])
            if not native:
                return
            w = native['w']
        c = self.clients.get(w.id)
        if c and c['kind'] == 'popup':
            # An XP Start popup needs real X keyboard focus for Wine activation,
            # without becoming the active application task or an Alt+Tab entry.
            w.map()
            w.configure(stack_mode=X.Above)
            if self.atom('WM_TAKE_FOCUS') in self.prop(w, 'WM_PROTOCOLS'):
                self.message(w, 'WM_PROTOCOLS', [self.atom('WM_TAKE_FOCUS'), X.CurrentTime, 0, 0, 0])
            w.set_input_focus(X.RevertToPointerRoot, X.CurrentTime)
            return
        if not c or c['kind'] != 'normal':
            return
        c['hidden'] = False
        w.map()
        self.setprop(w, 'WM_STATE', self.atom('WM_STATE'), [1, 0])
        states = list(self.prop(w, '_NET_WM_STATE'))
        self.setprop(w, '_NET_WM_STATE', Xatom.ATOM, [s for s in states if s != self.atom('_NET_WM_STATE_HIDDEN')])
        w.configure(stack_mode=X.Above)
        protocols = self.prop(w, 'WM_PROTOCOLS')
        if self.atom('WM_TAKE_FOCUS') in protocols:
            self.message(w, 'WM_PROTOCOLS', [self.atom('WM_TAKE_FOCUS'), X.CurrentTime, 0, 0, 0])
        w.set_input_focus(X.RevertToPointerRoot, X.CurrentTime)
        self.active = w.id
        self.setprop(self.root, '_NET_ACTIVE_WINDOW', Xatom.WINDOW, [w.id])
        self.restack()
        self.redraw_native(w)

    def message(self, w, name, values):
        w.send_event(protocol.event.ClientMessage(window=w, client_type=self.atom(name), data=(32, values)))

    def minimize(self, w):
        c = self.clients[w.id]
        c['hidden'] = True
        w.unmap()
        for frame_id, native_id in self.frames.items():
            if native_id == w.id:
                self.clients[frame_id]['w'].unmap()
        self.setprop(w, 'WM_STATE', self.atom('WM_STATE'), [3, 0])
        states = list(self.prop(w, '_NET_WM_STATE'))
        hidden = self.atom('_NET_WM_STATE_HIDDEN')
        self.setprop(w, '_NET_WM_STATE', Xatom.ATOM, list(set(states + [hidden])))
        if self.active == w.id:
            self.active = 0
            visible = [c for c in self.clients.values() if c['kind'] == 'normal' and not c['hidden']]
            if visible:
                self.focus(visible[-1]['w'])
            else:
                self.root.set_input_focus(X.RevertToPointerRoot, X.CurrentTime)
                self.setprop(self.root, '_NET_ACTIVE_WINDOW', Xatom.WINDOW, [0])

    def close(self, w):
        if self.atom('WM_DELETE_WINDOW') in self.prop(w, 'WM_PROTOCOLS'):
            self.message(w, 'WM_PROTOCOLS', [self.atom('WM_DELETE_WINDOW'), X.CurrentTime, 0, 0, 0])
        else:
            w.kill_client()

    def unmanage(self, window_id):
        self.clients.pop(window_id, None)
        self.previous_icons.pop(window_id, None)
        (self.ipc / f'icon-{window_id:x}.argb').unlink(missing_ok=True)
        # Withdrawals can arrive before the next 200ms proxy snapshot. Hide the
        # orphan frame immediately and reject any queued activation for it.
        for frame_id, native_id in list(self.frames.items()):
            if frame_id == window_id or native_id == window_id:
                frame = self.clients.get(frame_id)
                if frame:
                    frame['w'].unmap()
                del self.frames[frame_id]
        if self.active == window_id:
            self.active = 0
            self.cycle()
        self.restack()
        self.workarea()

    def restack(self):
        # Native clients remain root children. The genuine Wine nonclient frame
        # sits immediately below its native client, so Wine draws the XP caption
        # and resize borders while the native pixels cover its unused client area.
        for frame_id, native_id in self.frames.items():
            if frame_id in self.clients and native_id in self.clients:
                self.clients[frame_id]['w'].configure(sibling=self.clients[native_id]['w'], stack_mode=X.Below)
        for c in self.clients.values():
            if c['kind'] == 'desktop':
                c['w'].configure(stack_mode=X.Below)
        for c in self.clients.values():
            if c['kind'] == 'dock' and not c['hidden']:
                c['w'].configure(stack_mode=X.Above)
        # Win32 topmost windows (EWMH above), such as XP's dimmed shut down
        # screen, cover the topmost taskbar; keep their transients above them.
        above = self.atom('_NET_WM_STATE_ABOVE')
        raised = set()
        for c in list(self.clients.values()):
            if c['kind'] == 'normal' and not c['hidden'] and above in self.prop(c['w'], '_NET_WM_STATE'):
                c['w'].configure(stack_mode=X.Above)
                raised.add(c['w'].id)
        for i, c in list(self.clients.items()):
            if c['kind'] == 'normal' and not c['hidden'] and i not in raised:
                try:
                    owner = c['w'].get_wm_transient_for()
                except error.XError:
                    owner = None
                if owner and owner.id in raised:
                    c['w'].configure(stack_mode=X.Above)
        for c in self.clients.values():
            if c['kind'] == 'popup' and not c['hidden']:
                c['w'].configure(stack_mode=X.Above)
        ids = [i for i, c in self.clients.items() if c['kind'] != 'proxy']
        self.setprop(self.root, '_NET_CLIENT_LIST', Xatom.WINDOW, ids)
        stack = [self.frames.get(w.id, w.id) for w in self.root.query_tree().children if w.id in ids or w.id in self.frames]
        stack = list(dict.fromkeys(stack))
        self.setprop(self.root, '_NET_CLIENT_LIST_STACKING', Xatom.WINDOW, stack)

    def workarea(self):
        g = self.root.get_geometry()
        left = top = right = bottom = 0
        for c in self.clients.values():
            if c['kind'] != 'dock' or c['hidden']:
                continue
            w = c['w']
            strut = list(self.prop(w, '_NET_WM_STRUT_PARTIAL')) or list(self.prop(w, '_NET_WM_STRUT'))
            if len(strut) >= 4:
                left, right, top, bottom = [max(a, b) for a, b in zip((left, right, top, bottom), strut[:4])]
            else:
                r = w.get_geometry()
                if r.width >= g.width // 2:
                    if r.y >= g.height // 2:
                        bottom = max(bottom, g.height - r.y)
                    else:
                        top = max(top, r.y + r.height)
                elif r.x >= g.width // 2:
                    right = max(right, g.width - r.x)
                else:
                    left = max(left, r.x + r.width)
        self.area = (left, top, max(1, g.width-left-right), max(1, g.height-top-bottom))
        if list(self.prop(self.root, '_NET_WORKAREA')) != list(self.area):
            self.setprop(self.root, '_NET_WORKAREA', Xatom.CARDINAL, list(self.area))

    def cycle(self, reverse=False):
        ids = [i for i, c in self.clients.items() if c['kind'] == 'normal']
        if not ids:
            self.active = 0
            self.setprop(self.root, '_NET_ACTIVE_WINDOW', Xatom.WINDOW, [0])
            return
        index = ids.index(self.active) if self.active in ids else -1
        self.focus(self.clients[ids[(index + (-1 if reverse else 1)) % len(ids)]]['w'])

    def maximize(self, w, restore=False):
        c = self.clients[w.id]
        if len(self.prop(w, '_WIN4WINE_HWND')):
            # Wine's owner thread supplies its computed outer rectangle. Only
            # acknowledge EWMH state here; resizing it races restore placement.
            atoms = {self.atom('_NET_WM_STATE_MAXIMIZED_VERT'), self.atom('_NET_WM_STATE_MAXIMIZED_HORZ')}
            states = set(self.prop(w, '_NET_WM_STATE')) - atoms
            if not restore:
                states.update(atoms)
            self.setprop(w, '_NET_WM_STATE', Xatom.ATOM, list(states))
            self.focus(w)
            return
        if restore and c['restore']:
            x, y, width, height = c['restore']
            c['restore'] = None
        elif not restore:
            if c['restore'] is None:
                r = w.get_geometry()
                c['restore'] = (r.x, r.y, r.width, r.height)
            x, y, width, height = self.area
            # Fit the complete Wine border inside the work area, using its
            # measured extents rather than assuming classic or Luna metrics.
            frame = next((self.clients[f]['w'] for f, n in self.frames.items() if n == w.id), None)
            if frame:
                r, outer = w.get_geometry(), frame.get_geometry()
                left, top = r.x-outer.x, r.y-outer.y
                width -= outer.width-r.width
                height -= outer.height-r.height
                x += left; y += top
        else:
            self.focus(w)
            return
        w.configure(x=x, y=y, width=max(1, width), height=max(1, height))
        atoms = [self.atom('_NET_WM_STATE_MAXIMIZED_VERT'), self.atom('_NET_WM_STATE_MAXIMIZED_HORZ')]
        states = set(self.prop(w, '_NET_WM_STATE')) - set(atoms)
        if not restore:
            states.update(atoms)
        self.setprop(w, '_NET_WM_STATE', Xatom.ATOM, list(states))
        self.configure_notify(w)
        self.focus(w)

    def positioned(self, w):
        try:
            hints = w.get_wm_normal_hints()
        except error.XError:
            return False
        if not hints or not hints.flags & (Xutil.USPosition | Xutil.PPosition):
            return False
        r = w.get_geometry()
        return bool(hints.flags & Xutil.USPosition) or (r.x, r.y) != (0, 0)

    def cascade(self, left, top):
        """Next XP default-window slot, as native client coordinates.

        XP steps 22px right (96 DPI, scaled here by the frame width) and
        SM_CYSIZE + SM_CYFRAME down, which is the caption extent minus one,
        and restarts after a quarter of the work area.
        """
        x, y, width, height = self.area
        step_x, step_y = max(1, round(22 * max(left, 1) / 4)), max(1, top - 1)
        slot = self.cascade_slot + 1
        if slot * step_x > width // 4 or slot * step_y > height // 4:
            slot = 1
        self.cascade_slot = slot
        return x + slot * step_x + left, y + slot * step_y + top

    def sync_frames(self):
        try:
            pairs = [line.split() for line in (self.ipc / 'frames.tsv').read_text().splitlines()]
            handles = {int(fields[0], 16): int(fields[1], 16) for fields in pairs}
            extents = {int(fields[1], 16): tuple(map(int, fields[2:7])) for fields in pairs if len(fields) >= 7}
            ready = {int(fields[1], 16): not int(fields[7]) for fields in pairs if len(fields) >= 8}
            clips = {int(fields[1], 16): tuple(map(int, fields[8:10])) for fields in pairs if len(fields) == 10}
        except (OSError, ValueError, IndexError):
            handles = {}
            extents = {}
            ready = {}
            clips = {}
        frames = {}
        for i, c in self.clients.items():
            if c['kind'] != 'proxy':
                continue
            hwnd = self.prop(c['w'], '_WIN4WINE_HWND')
            native_id = handles.get(int(hwnd[0])) if len(hwnd) else None
            native = self.clients.get(native_id)
            if native and native['kind'] == 'normal' and not len(self.prop(native['w'], '_WIN4WINE_HWND')):
                frames[i] = native_id
                metrics = extents.get(native_id)
                if metrics:
                    left, right, top, bottom, maximized = metrics
                    if not native['framed']:
                        r = native['w'].get_geometry()
                        if self.positioned(native['w']):
                            # Keep an explicit position, but keep the first
                            # caption visible above content placed at 0,0.
                            x = max(r.x, self.area[0]+left)
                            y = max(r.y, self.area[1]+top)
                        else:
                            x, y = self.cascade(left, top)
                        if (x, y) != (r.x, r.y):
                            native['w'].configure(x=x, y=y)
                            self.configure_notify(native['w'])
                        native['framed'] = True
                    self.setprop(native['w'], '_NET_FRAME_EXTENTS', Xatom.CARDINAL, [left, right, top, bottom])
                    if maximized and native['restore'] is not None:
                        x, y, width, height = self.area
                        bx, by = clips.get(native_id, (0, 0))
                        wanted = (x+left-bx, y+top-by, max(1, width-left-right+2*bx), max(1, height-top-bottom+2*by))
                        r = native['w'].get_geometry()
                        if (r.x, r.y, r.width, r.height) != wanted:
                            native['w'].configure(x=wanted[0], y=wanted[1], width=wanted[2], height=wanted[3])
                            self.configure_notify(native['w'])
                if native['w'].get_geometry().border_width:
                    native['w'].configure(border_width=0)
                if native['hidden'] or not ready.get(native_id, True):
                    c['w'].unmap()
                else:
                    c['w'].map()
            else:
                c['w'].unmap()
        if frames != self.frames:
            self.frames = frames
            self.restack()
        geometries = []
        for i, c in self.clients.items():
            if c['kind'] == 'normal' and not len(self.prop(c['w'], '_WIN4WINE_HWND')):
                r = c['w'].get_geometry()
                geometries.append(f'{i:x} {r.x} {r.y} {r.width} {r.height} {int(c["restore"] is not None)} {self.area[0]} {self.area[1]} {self.area[2]} {self.area[3]}\n')
        text = ''.join(geometries)
        if text != self.previous_geometry:
            temporary = self.ipc / 'geometry.tmp'
            temporary.write_text(text)
            temporary.replace(self.ipc / 'geometry.tsv')
            self.previous_geometry = text

    def snapshot(self):
        lines = []
        focused = self.d.get_input_focus().focus
        # An XP menu can take focus without changing the EWMH active client.
        # Do not keep stealing Win32 foreground from that menu back to a proxy.
        try:
            while (hasattr(focused, 'id') and focused.id not in (X.NONE, X.PointerRoot, self.root.id)
                   and focused.id not in self.clients):
                focused = focused.query_tree().parent
            focused_id = focused.id if hasattr(focused, 'id') else 0
        except error.XError:
            focused_id = 0
        for i, c in self.clients.items():
            w = c['w']
            if c['kind'] != 'normal' or len(self.prop(w, '_WIN4WINE_HWND')):
                continue
            if w.get_wm_transient_for():
                continue
            if c['icon_dirty']:
                data = icon_payload(self.d, w, self.atom('_NET_WM_ICON'))
                target = self.ipc / f'icon-{i:x}.argb'
                if i not in self.previous_icons or data != self.previous_icons[i]:
                    if data:
                        temporary = target.with_suffix('.tmp')
                        temporary.write_bytes(data)
                        temporary.replace(target)
                    else:
                        target.unlink(missing_ok=True)
                    self.previous_icons[i] = data
                c['icon_dirty'] = False
            title = self.prop(w, '_NET_WM_NAME')
            if len(title):
                name = bytes(title).decode('utf-8', 'replace')
            else:
                name = w.get_wm_name() or ''
            name = name.replace('\t', ' ').replace('\n', ' ').replace('\r', ' ')[:500]
            lines.append(f'{i:x}\t{int(c["hidden"])}\t{int(i == self.active and i == focused_id)}\t{name}\n')
        text = ''.join(lines)
        if text != self.previous_snapshot:
            temp = self.ipc / 'windows.tmp'
            temp.write_text(text, encoding='utf-8')
            temp.replace(self.ipc / 'windows.tsv')
            self.previous_snapshot = text
        for command in self.ipc.glob('*.cmd'):
            try:
                op, value, *args = command.read_text().split()
                i = int(value, 16)
                c = self.clients.get(i)
                if c and c['kind'] == 'normal' and not len(self.prop(c['w'], '_WIN4WINE_HWND')):
                    if op == 'geometry' and len(args) == 4:
                        x, y, width, height = map(int, args)
                        if width > 0 and height > 0:
                            c['w'].configure(x=x, y=y, width=width, height=height)
                            self.configure_notify(c['w'])
                    elif op == 'restore':
                        self.maximize(c['w'], restore=True)
                    else:
                        {'activate': self.focus, 'minimize': self.minimize, 'close': self.close, 'maximize': self.maximize}[op](c['w'])
            except (ValueError, KeyError, OSError):
                pass
            finally:
                command.unlink(missing_ok=True)

    def event(self, e):
        if e.type == X.MapRequest:
            self.manage(e.window)
        elif e.type == X.MapNotify:
            c = self.clients.get(e.window.id)
            if c and (c['kind'] == 'popup' or c['kind'] == 'normal' and self.active == e.window.id):
                # Wine defers WM_TAKE_FOCUS while waiting for the initial map acknowledgement.
                if self.atom('WM_TAKE_FOCUS') in self.prop(e.window, 'WM_PROTOCOLS'):
                    self.message(e.window, 'WM_PROTOCOLS', [self.atom('WM_TAKE_FOCUS'), X.CurrentTime, 0, 0, 0])
        elif e.type == X.ConfigureRequest:
            values = {name: getattr(e, name) for bit, name in
                      ((X.CWX, 'x'), (X.CWY, 'y'), (X.CWWidth, 'width'), (X.CWHeight, 'height'),
                       (X.CWBorderWidth, 'border_width'), (X.CWSibling, 'sibling'), (X.CWStackMode, 'stack_mode'))
                      if e.value_mask & bit}
            e.window.configure(**values)
            self.restack()
            self.workarea()
            if e.window.id in self.clients:
                self.configure_notify(e.window)
        elif e.type == X.DestroyNotify:
            self.unmanage(e.window.id)
        elif e.type == X.UnmapNotify:
            c = self.clients.get(e.window.id)
            if c and not c['hidden'] and c['kind'] != 'proxy':
                # ICCCM withdrawal acknowledgement. Wine waits for PropertyDelete
                # (WithdrawnState) before issuing a subsequent XMapWindow. Keep
                # iconic clients and deliberately hidden proxy frames registered.
                e.window.delete_property(self.atom('WM_STATE'))
                self.unmanage(e.window.id)
        elif e.type == X.PropertyNotify:
            c = self.clients.get(e.window.id)
            if c:
                if e.atom in (self.atom('_NET_WM_ICON'), self.atom('WM_HINTS')):
                    c['icon_dirty'] = True
                c['kind'] = self.kind(e.window)
                if c['kind'] == 'proxy' and e.window.id not in self.frames:
                    e.window.unmap()
                self.workarea()
        elif e.type == X.ButtonPress:
            if e.state & X.Mod1Mask and e.window.id == self.root.id and e.child:
                child = self.clients.get(e.child.id)
                if child and child['kind'] == 'normal':
                    self.focus(e.child)
                    r = e.child.get_geometry()
                    self.drag = (e.child, e.detail, e.root_x, e.root_y, r.x, r.y, r.width, r.height)
            else:
                self.focus(e.window)
                self.d.allow_events(X.ReplayPointer, X.CurrentTime)
        elif e.type == X.MotionNotify and self.drag:
            w, button, px, py, x, y, width, height = self.drag
            dx, dy = e.root_x-px, e.root_y-py
            if button == 1:
                w.configure(x=x+dx, y=y+dy)
            else:
                w.configure(width=max(64, width+dx), height=max(32, height+dy))
        elif e.type == X.ButtonRelease:
            self.drag = None
        elif e.type == X.KeyPress and e.detail == self.d.keysym_to_keycode(XK.string_to_keysym('F4')):
            if self.active in self.clients:
                self.close(self.clients[self.active]['w'])
        elif e.type == X.KeyPress and e.detail == self.tab:
            self.cycle(bool(e.state & X.ShiftMask))
        elif e.type == X.FocusIn and e.mode == X.NotifyNormal and e.detail not in (X.NotifyPointer, X.NotifyPointerRoot, X.NotifyDetailNone) and e.window.id in self.clients:
            if self.clients[e.window.id]['kind'] == 'normal':
                self.active = e.window.id
                self.setprop(self.root, '_NET_ACTIVE_WINDOW', Xatom.WINDOW, [self.active])
        elif e.type == X.ConfigureNotify:
            if e.window.id == self.root.id:
                self.workarea()
            elif not e.send_event and e.window.id in self.clients:
                c = self.clients[e.window.id]
                size = (e.width, e.height)
                if c['geometry'] != size:
                    c['geometry'] = size
                    if c['kind'] == 'normal' and not c['hidden']:
                        self.redraw_native(e.window)
        elif e.type == X.ClientMessage:
            values = e.data[1]
            if e.client_type == self.atom('_NET_ACTIVE_WINDOW'):
                self.focus(e.window)
            elif e.client_type == self.atom('_NET_CLOSE_WINDOW'):
                self.close(e.window)
            elif e.client_type == self.atom('WM_CHANGE_STATE') and values[0] == 3:
                if e.window.id in self.clients:
                    self.minimize(e.window)
            elif e.client_type == self.atom('_NET_WM_STATE') and e.window.id in self.clients:
                states = set(self.prop(e.window, '_NET_WM_STATE'))
                for atom in values[1:3]:
                    if not atom:
                        continue
                    if values[0] == 1 or values[0] == 2 and atom not in states:
                        states.add(atom)
                    else:
                        states.discard(atom)
                self.setprop(e.window, '_NET_WM_STATE', Xatom.ATOM, list(states))
                if self.atom('_NET_WM_STATE_ABOVE') in values[1:3]:
                    self.restack()
                maximize_atoms = {self.atom('_NET_WM_STATE_MAXIMIZED_VERT'), self.atom('_NET_WM_STATE_MAXIMIZED_HORZ')}
                if maximize_atoms & set(values[1:3]):
                    maximized = self.atom('_NET_WM_STATE_MAXIMIZED_VERT') in states or self.atom('_NET_WM_STATE_MAXIMIZED_HORZ') in states
                    if self.clients[e.window.id]['kind'] == 'proxy':
                        native = self.clients.get(self.frames.get(e.window.id))
                        if native and (native['restore'] is not None) != maximized:
                            self.maximize(native['w'], restore=not maximized)
                    else:
                        self.maximize(e.window, restore=not maximized)

    def run(self):
        while self.running:
            while self.d.pending_events():
                try:
                    self.event(self.d.next_event())
                except error.XError:
                    pass  # A client may disappear between any two requests.
            try:
                self.snapshot()
                self.sync_frames()
            except error.XError:
                pass
            self.d.flush()
            select.select([self.d.fileno()], [], [], .1)
        self.d.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ipc', required=True, type=Path)
    args = parser.parse_args()
    wm = Manager(args.ipc)
    for sig in (signal.SIGTERM, signal.SIGINT):
        signal.signal(sig, lambda *_: setattr(wm, 'running', False))
    wm.run()
