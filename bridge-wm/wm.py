#!/usr/bin/env python3
"""Small single-screen X11 WM and file IPC endpoint for the Win32 shell bridge."""
import argparse
import json
import os
from pathlib import Path
import select
import signal
import time
from Xlib import X, XK, Xatom, display, error, protocol


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
                     '_NET_WM_WINDOW_TYPE', '_NET_WM_WINDOW_TYPE_DESKTOP', '_NET_WM_WINDOW_TYPE_DOCK',
                     '_NET_CLOSE_WINDOW', '_NET_WM_STRUT', '_NET_WM_STRUT_PARTIAL']
        self.setprop(self.root, '_NET_SUPPORTED', Xatom.ATOM, [self.atom(a) for a in supported])
        self.setprop(self.root, '_NET_NUMBER_OF_DESKTOPS', Xatom.CARDINAL, [1])
        self.setprop(self.root, '_NET_CURRENT_DESKTOP', Xatom.CARDINAL, [0])
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
        cls = bytes(self.prop(w, '_WIN4WINE_CLASS')).decode('ascii', 'replace').strip('\0')
        types = self.prop(w, '_NET_WM_WINDOW_TYPE')
        if cls == 'Win4WineProxy':
            return 'proxy'
        if cls in ('Progman', 'WorkerW') or self.atom('_NET_WM_WINDOW_TYPE_DESKTOP') in types:
            return 'desktop'
        if cls == 'Shell_TrayWnd' or self.atom('_NET_WM_WINDOW_TYPE_DOCK') in types:
            return 'dock'
        return 'normal'

    def manage(self, w):
        attrs = w.get_attributes()
        if attrs.override_redirect or w.id == self.support.id:
            w.map()
            return
        if w.id not in self.clients:
            self.clients[w.id] = {'w': w, 'hidden': False, 'kind': self.kind(w), 'restore': None}
            w.change_attributes(event_mask=X.PropertyChangeMask | X.FocusChangeMask)
            w.grab_button(1, 0, True, X.ButtonPressMask, X.GrabModeSync, X.GrabModeAsync, X.NONE, X.NONE)
        client = self.clients[w.id]
        client['kind'] = self.kind(w)
        if client['kind'] == 'proxy':
            w.unmap()
            return
        client['hidden'] = False
        self.setprop(w, 'WM_STATE', self.atom('WM_STATE'), [1, 0])
        w.map()
        if client['kind'] == 'normal':
            self.focus(w)
        self.restack()
        self.workarea()

    def focus(self, w):
        c = self.clients.get(w.id)
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

    def message(self, w, name, values):
        w.send_event(protocol.event.ClientMessage(window=w, client_type=self.atom(name), data=(32, values)))

    def minimize(self, w):
        c = self.clients[w.id]
        c['hidden'] = True
        w.unmap()
        self.setprop(w, 'WM_STATE', self.atom('WM_STATE'), [3, 0])
        states = list(self.prop(w, '_NET_WM_STATE'))
        hidden = self.atom('_NET_WM_STATE_HIDDEN')
        self.setprop(w, '_NET_WM_STATE', Xatom.ATOM, list(set(states + [hidden])))
        if self.active == w.id:
            self.active = 0
            self.cycle()

    def close(self, w):
        if self.atom('WM_DELETE_WINDOW') in self.prop(w, 'WM_PROTOCOLS'):
            self.message(w, 'WM_PROTOCOLS', [self.atom('WM_DELETE_WINDOW'), X.CurrentTime, 0, 0, 0])
        else:
            w.kill_client()

    def restack(self):
        for c in self.clients.values():
            if c['kind'] == 'desktop':
                c['w'].configure(stack_mode=X.Below)
        for c in self.clients.values():
            if c['kind'] == 'dock' and not c['hidden']:
                c['w'].configure(stack_mode=X.Above)
        ids = [i for i, c in self.clients.items() if c['kind'] != 'proxy']
        self.setprop(self.root, '_NET_CLIENT_LIST', Xatom.WINDOW, ids)
        stack = [w.id for w in self.root.query_tree().children if w.id in ids]
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
        self.setprop(self.root, '_NET_WORKAREA', Xatom.CARDINAL, list(self.area))

    def cycle(self, reverse=False):
        ids = [i for i, c in self.clients.items() if c['kind'] == 'normal']
        if not ids:
            self.active = 0
            self.setprop(self.root, '_NET_ACTIVE_WINDOW', Xatom.WINDOW, [0])
            return
        index = ids.index(self.active) if self.active in ids else -1
        self.focus(self.clients[ids[(index + (-1 if reverse else 1)) % len(ids)]]['w'])

    def snapshot(self):
        lines = []
        for i, c in self.clients.items():
            w = c['w']
            if c['kind'] != 'normal' or len(self.prop(w, '_WIN4WINE_HWND')):
                continue
            if w.get_wm_transient_for():
                continue
            title = self.prop(w, '_NET_WM_NAME')
            if len(title):
                name = bytes(title).decode('utf-8', 'replace')
            else:
                name = w.get_wm_name() or ''
            name = name.replace('\t', ' ').replace('\n', ' ').replace('\r', ' ')[:500]
            lines.append(f'{i:x}\t{int(c["hidden"])}\t{int(i == self.active)}\t{name}\n')
        text = ''.join(lines)
        if text != self.previous_snapshot:
            temp = self.ipc / 'windows.tmp'
            temp.write_text(text, encoding='utf-8')
            temp.replace(self.ipc / 'windows.tsv')
            self.previous_snapshot = text
        for command in self.ipc.glob('*.cmd'):
            try:
                op, value = command.read_text().split()
                i = int(value, 16)
                c = self.clients.get(i)
                if c and c['kind'] == 'normal' and not len(self.prop(c['w'], '_WIN4WINE_HWND')):
                    {'activate': self.focus, 'minimize': self.minimize, 'close': self.close}[op](c['w'])
            except (ValueError, KeyError, OSError):
                pass
            finally:
                command.unlink(missing_ok=True)

    def event(self, e):
        if e.type == X.MapRequest:
            self.manage(e.window)
        elif e.type == X.ConfigureRequest:
            values = {name: getattr(e, name) for bit, name in
                      ((X.CWX, 'x'), (X.CWY, 'y'), (X.CWWidth, 'width'), (X.CWHeight, 'height'),
                       (X.CWBorderWidth, 'border_width'), (X.CWSibling, 'sibling'), (X.CWStackMode, 'stack_mode'))
                      if e.value_mask & bit}
            e.window.configure(**values)
            self.restack()
            self.workarea()
        elif e.type == X.DestroyNotify:
            self.clients.pop(e.window.id, None)
            if self.active == e.window.id:
                self.cycle()
            self.restack()
            self.workarea()
        elif e.type == X.UnmapNotify:
            c = self.clients.get(e.window.id)
            if c and not c['hidden'] and c['kind'] != 'proxy':
                self.clients.pop(e.window.id, None)
                self.restack()
                self.workarea()
        elif e.type == X.PropertyNotify:
            c = self.clients.get(e.window.id)
            if c:
                c['kind'] = self.kind(e.window)
                if c['kind'] == 'proxy':
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
        elif e.type == X.FocusIn and e.window.id in self.clients:
            if self.clients[e.window.id]['kind'] == 'normal':
                self.active = e.window.id
                self.setprop(self.root, '_NET_ACTIVE_WINDOW', Xatom.WINDOW, [self.active])
        elif e.type == X.ConfigureNotify and e.window.id == self.root.id:
            self.workarea()
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
                maximized = self.atom('_NET_WM_STATE_MAXIMIZED_VERT') in states or self.atom('_NET_WM_STATE_MAXIMIZED_HORZ') in states
                c = self.clients[e.window.id]
                if maximized:
                    if c['restore'] is None:
                        r = e.window.get_geometry()
                        c['restore'] = (r.x, r.y, r.width, r.height)
                    x, y, width, height = self.area
                    e.window.configure(x=x, y=y, width=width, height=height)
                elif c['restore']:
                    x, y, width, height = c['restore']
                    e.window.configure(x=x, y=y, width=width, height=height)
                    c['restore'] = None

    def run(self):
        while self.running:
            while self.d.pending_events():
                try:
                    self.event(self.d.next_event())
                except error.XError:
                    pass  # A client may disappear between any two requests.
            try:
                self.snapshot()
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
