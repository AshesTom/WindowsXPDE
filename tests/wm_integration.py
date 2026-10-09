#!/usr/bin/env python3
"""Run against an isolated X display where bridge-wm is already running."""
import os
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
                           background_pixel=0x406080)
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
    desktop = make('test desktop', 0, 0, g.width, g.height, 'Progman')
    dock = make('test tray', 0, g.height-32, g.width, 32, 'Shell_TrayWnd')
    first = make('native first', 30, 40, 300, 200)
    second = make('native second', 360, 40, 300, 200)
    windows = [desktop, dock, first, second]
    second.set_wm_protocols([atom('WM_DELETE_WINDOW')])
    wait(lambda: prop(root, '_NET_WORKAREA') == [0, 0, g.width, g.height-32], 'taskbar workarea')
    wait(lambda: f'{first.id:x}\t' in (ipc / 'windows.tsv').read_text(), 'native snapshot')
    assert f'{dock.id:x}\t' not in (ipc / 'windows.tsv').read_text()
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
            wait(lambda: d.pending_events() > 0, 'close protocol event')
            found = False
            while d.pending_events():
                event = d.next_event()
                if event.type == X.ClientMessage and event.client_type == atom('WM_PROTOCOLS'):
                    found = event.data[1][0] == atom('WM_DELETE_WINDOW')
            assert found, 'WM_DELETE_WINDOW not delivered'
    print('PASS: workarea, native snapshot, minimize, restore, activation')
finally:
    for w in windows:
        try:
            w.destroy()
        except Exception:
            pass
    d.flush()
    d.close()
