#!/usr/bin/env python3
"""Run against an isolated X display where bridge-wm is already running."""
import os
import subprocess
from pathlib import Path
import time
from Xlib import X, Xatom, display, protocol

ipc = Path(os.environ['XP_BRIDGE_DIR'])
d = display.Display()
root = d.screen().root
atom = d.intern_atom

def wait(predicate, label):
    until = time.monotonic() + 5
    while time.monotonic() < until:
        d.sync()
        if predicate():
            return
        time.sleep(.05)
    raise AssertionError(label)

def prop(w, name):
    p = w.get_full_property(atom(name), X.AnyPropertyType)
    return list(p.value) if p else []

def make(title, x, y, width, height, cls=None):
    w = root.create_window(x, y, width, height, 0, X.CopyFromParent, X.InputOutput, X.CopyFromParent,
                           background_pixel=0x406080, event_mask=X.StructureNotifyMask)
    w.set_wm_name(title)
    if cls:
        w.change_property(atom('_WIN4WINE_CLASS'), Xatom.STRING, 8, cls.encode())
        w.change_property(atom('_WIN4WINE_HWND'), Xatom.CARDINAL, 32, [w.id])
    w.map()
    d.flush()
    return w

windows = []
try:
    g = root.get_geometry()
    desktop = make('test desktop', 0, 0, g.width, g.height, 'PROGMAN')
    dock = make('test tray', 0, g.height-32, g.width, 32, 'Shell_TrayWnd')
    first = make('native first', 30, 40, 300, 200)
    second = make('native second', 360, 40, 300, 200)
    windows = [desktop, dock, first, second]
    second.set_wm_protocols([atom('WM_DELETE_WINDOW')])
    wait(lambda: prop(root, '_NET_WORKAREA') == [0, 0, g.width, g.height-32], 'taskbar workarea')
    owner = d.get_selection_owner(atom('_XSETTINGS_S%d' % d.get_default_screen()))
    assert owner != X.NONE, 'XSETTINGS owner'
    settings = bytes(owner.get_full_property(atom('_XSETTINGS_SETTINGS'), X.AnyPropertyType).value)
    assert b'Gtk/DecorationLayout' in settings and settings.endswith(b':\0\0\0'), 'GTK decoration layout'
    wait(lambda: f'{first.id:x}\t' in (ipc / 'windows.tsv').read_text(), 'native snapshot')
    assert f'{dock.id:x}\t' not in (ipc / 'windows.tsv').read_text()
    wait(lambda: prop(root, '_NET_ACTIVE_WINDOW') == [second.id], 'initial focus')
    first.configure(x=70, y=80)
    d.flush()
    def configured():
        found = False
        while d.pending_events():
            event = d.next_event()
            if event.type == X.ConfigureNotify and event.window.id == first.id and event.send_event:
                found |= (event.x, event.y, event.width, event.height) == (70, 80, 300, 200)
        return found
    wait(configured, 'synthetic ConfigureNotify has root coordinates')
    subprocess.run(['xdotool', 'key', 'alt+Tab'], check=True)
    wait(lambda: prop(root, '_NET_ACTIVE_WINDOW') == [first.id], 'Alt+Tab cycles native windows')
    subprocess.run(['xdotool', 'key', 'alt+shift+Tab'], check=True)
    wait(lambda: prop(root, '_NET_ACTIVE_WINDOW') == [second.id], 'reverse Alt+Tab')
    for index, (op, target) in enumerate((('minimize', first), ('activate', first), ('close', second))):
        (ipc / f'test-{index}.tmp').write_text(f'{op} {target.id:x}\n')
        (ipc / f'test-{index}.tmp').replace(ipc / f'test-{index}.cmd')
        if op == 'minimize':
            wait(lambda: prop(target, 'WM_STATE')[:1] == [3], 'minimize')
            wait(lambda: f'{target.id:x}\t1\t' in (ipc / 'windows.tsv').read_text(), 'retain iconic identity')
        elif op == 'activate':
            wait(lambda: prop(root, '_NET_ACTIVE_WINDOW') == [target.id], 'restore/focus')
            assert target.get_attributes().map_state == X.IsViewable
        else:
            def received_close():
                found = False
                while d.pending_events():
                    event = d.next_event()
                    if event.type == X.ClientMessage and event.client_type == atom('WM_PROTOCOLS'):
                        found |= event.data[1][0] == atom('WM_DELETE_WINDOW')
                return found
            wait(received_close, 'WM_DELETE_WINDOW delivered')
    second.destroy()
    windows.remove(second)
    d.flush()
    wait(lambda: f'{second.id:x}\t' not in (ipc / 'windows.tsv').read_text(), 'second removed')
    d.set_input_focus(X.PointerRoot, X.RevertToPointerRoot, X.CurrentTime)
    d.flush()
    (ipc / 'single.tmp').write_text(f'minimize {first.id:x}\n')
    (ipc / 'single.tmp').replace(ipc / 'single.cmd')
    wait(lambda: prop(first, 'WM_STATE')[:1] == [3], 'single window stays minimized')
    time.sleep(.3)
    assert first.get_attributes().map_state != X.IsViewable
    print('PASS: workarea, native snapshot, Alt+Tab, minimize, restore, activation, close')
finally:
    for w in windows:
        try:
            w.destroy()
        except Exception:
            pass
    d.flush()
    d.close()
