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
    # Under a native client the Windows key and Ctrl+Esc go to the proxy,
    # which enters them into Wine; the client keeps the keyboard.
    def shell_keys(text):
        path = ipc / 'shellkeys.tsv'
        return lambda: path.exists() and path.read_text() == text
    subprocess.run(['xdotool', 'key', 'super+r'], check=True)
    wait(shell_keys('1 windows 52\n'), 'Win+R passed to the proxy')
    subprocess.run(['xdotool', 'key', 'super'], check=True)
    wait(shell_keys('2 windows\n'), 'Windows key passed to the proxy')
    subprocess.run(['xdotool', 'key', 'ctrl+Escape'], check=True)
    wait(shell_keys('3 ctrl-escape\n'), 'Ctrl+Esc passed to the proxy')
    assert d.get_input_focus().focus.id == second.id, 'native client keeps the keyboard'
    # A click on the desktop gives it the keyboard, and no task stays active.
    subprocess.run(['xdotool', 'mousemove', str(g.width - 100), str(g.height - 100), 'click', '1'], check=True)
    wait(lambda: d.get_input_focus().focus.id == desktop.id, 'desktop click takes the keyboard')
    wait(lambda: prop(root, '_NET_ACTIVE_WINDOW') == [0], 'no active task on the desktop')
    subprocess.run(['xdotool', 'key', 'super'], check=True)
    time.sleep(.3)
    assert (ipc / 'shellkeys.tsv').read_text() == '3 ctrl-escape\n', 'Wine windows get the Windows key themselves'
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
    # A full-screen client (Spectacle's selection, a video) gets the whole
    # screen over the taskbar, and its rectangle back when it leaves it.
    full = root.create_window(100, 100, 400, 300, 0, X.CopyFromParent, X.InputOutput, X.CopyFromParent,
                              background_pixel=0x204060)
    windows.append(full)
    full.set_wm_name('native full screen')
    full.change_property(atom('_NET_WM_STATE'), Xatom.ATOM, 32, [atom('_NET_WM_STATE_FULLSCREEN')])
    full.map()
    d.flush()
    def geometry(w):
        r = w.get_geometry()
        return r.x, r.y, r.width, r.height
    wait(lambda: geometry(full) == (0, 0, g.width, g.height), 'full screen geometry')
    wait(lambda: prop(root, '_NET_ACTIVE_WINDOW') == [full.id], 'full screen window active')
    def above_dock():
        order = [w.id for w in root.query_tree().children]
        return order.index(full.id) > order.index(dock.id)
    wait(above_dock, 'active full screen window covers the taskbar')
    root.send_event(protocol.event.ClientMessage(window=full, client_type=atom('_NET_WM_STATE'),
                    data=(32, [0, atom('_NET_WM_STATE_FULLSCREEN'), 0, 1, 0])),
                    event_mask=X.SubstructureRedirectMask | X.SubstructureNotifyMask)
    d.flush()
    wait(lambda: geometry(full) == (100, 100, 400, 300), 'windowed geometry back')
    wait(lambda: not above_dock(), 'taskbar above the windowed client')
    # A notification balloon (an unmanaged tooltip) stays above the taskbar:
    # restacks raising the dock must not bury it, or its stem disappears under it.
    balloon = root.create_window(g.width-340, g.height-113, 250, 69, 0, X.CopyFromParent,
                                 X.InputOutput, X.CopyFromParent, background_pixel=0xffffe1,
                                 override_redirect=True)
    windows.append(balloon)
    balloon.change_property(atom('_WIN4WINE_CLASS'), Xatom.STRING, 8, b'tooltips_class32')
    balloon.change_property(atom('_WIN4WINE_HWND'), Xatom.CARDINAL, 32, [balloon.id])
    balloon.map()
    d.flush()
    def balloon_above_dock():
        order = [w.id for w in root.query_tree().children]
        return order.index(balloon.id) > order.index(dock.id)
    wait(balloon_above_dock, 'balloon above the taskbar')
    balloon.configure(stack_mode=X.Below, sibling=dock)
    d.flush()
    third = make('native third', 60, 80, 300, 200)
    windows.append(third)
    wait(balloon_above_dock, 'balloon still above the taskbar after a restack')
    print('PASS: workarea, native snapshot, Alt+Tab, shell keys, desktop focus, minimize, restore, activation, close, '
          'full screen, balloon above the taskbar')
finally:
    for w in windows:
        try:
            w.destroy()
        except Exception:
            pass
    d.flush()
    d.close()
