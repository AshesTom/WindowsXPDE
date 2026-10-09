#!/usr/bin/env python3
"""Real Wine frame gestures on a native X client; run with WM + proxy active."""
import os
from itertools import count
from pathlib import Path
import subprocess
import time
from Xlib import X, Xatom, display, error, protocol

ipc = Path(os.environ['XP_BRIDGE_DIR'])
d = display.Display()
root = d.screen().root
atom = d.intern_atom
sequence = count()

def prop(w, name):
    p = w.get_full_property(atom(name), X.AnyPropertyType)
    return list(p.value) if p else []

def wait(predicate, message):
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        d.sync()
        value = predicate()
        if value:
            return value
        time.sleep(.05)
    raise AssertionError(message)

def command(op):
    temporary = ipc / f'frame-test-{os.getpid()}-{next(sequence)}.tmp'
    temporary.write_text(f'{op} {native.id:x}\n')
    temporary.replace(temporary.with_suffix('.cmd'))

def hidden(w):
    try:
        return w.get_attributes().map_state != X.IsViewable
    except error.XError:
        return True

def geometry(w):
    g = w.get_geometry()
    return g.x, g.y, g.width, g.height

def frame_for(w):
    try:
        pairs = {int(hwnd, 16): int(xid, 16) for hwnd, xid, *_ in
                 (line.split() for line in (ipc / 'frames.tsv').read_text().splitlines())}
    except OSError:
        return None
    for child in root.query_tree().children:
        hwnd = prop(child, '_WIN4WINE_HWND')
        if hwnd and pairs.get(hwnd[0]) == w.id and child.get_attributes().map_state == X.IsViewable:
            return child

def paired():
    f, n = frame.get_geometry(), native.get_geometry()
    metrics = prop(native, '_NET_FRAME_EXTENTS')
    if len(metrics) != 4:
        return False
    left, right, top, bottom = metrics
    return (f.x, f.y, f.width, f.height) == (n.x-left, n.y-top, n.width+left+right, n.height+top+bottom)

def pointer(*args):
    subprocess.run(['xdotool', *map(str, args)], check=True)

def caption():
    f, n = frame.get_geometry(), native.get_geometry()
    return f.x + 80, f.y + (n.y-f.y)//2

native = root.create_window(0, 0, 400, 250, 0, X.CopyFromParent,
                            X.InputOutput, X.CopyFromParent,
                            background_pixel=0x208040, event_mask=X.StructureNotifyMask)
native.set_wm_name('XP native frame integration')
native.set_wm_protocols([atom('WM_DELETE_WINDOW')])
native.map()
d.flush()
try:
    frame = wait(lambda: frame_for(native), 'genuine Wine proxy frame is mapped')
    wait(paired, 'frame encloses native pixels')
    area = prop(root, '_NET_WORKAREA')
    assert frame.get_geometry().x >= area[0] and frame.get_geometry().y >= area[1], 'initial caption stays inside workarea'
    wait(lambda: prop(root, '_NET_ACTIVE_WINDOW') == [native.id], 'native focus identity')
    assert prop(root, '_NET_CLIENT_LIST').count(native.id) == 1
    assert frame.id not in prop(root, '_NET_CLIENT_LIST')
    popup = root.create_window(10, 10, 60, 60, 0, X.CopyFromParent,
                               X.InputOutput, X.CopyFromParent, background_pixel=0x505080)
    popup.change_property(atom('_WIN4WINE_CLASS'), Xatom.STRING, 8, b'DV2ControlHost')
    popup.set_wm_name('Start popup must not become a task')
    popup.map()
    d.flush()
    wait(lambda: popup.get_attributes().map_state == X.IsViewable, 'Start popup maps')
    assert prop(root, '_NET_ACTIVE_WINDOW') == [native.id]
    wait(lambda: f'{popup.id:x}\t' not in (ipc / 'windows.tsv').read_text(), 'Start popup excluded from bridge')
    popup.destroy()
    d.flush()
    d.set_input_focus(X.PointerRoot, X.RevertToPointerRoot, X.CurrentTime)
    root.send_event(protocol.event.ClientMessage(window=frame, client_type=atom('_NET_ACTIVE_WINDOW'),
                    data=(32, [1, 0, 0, 0, 0])), event_mask=X.SubstructureRedirectMask | X.SubstructureNotifyMask)
    d.flush()
    wait(lambda: getattr(d.get_input_focus().focus, 'id', 0) == native.id, 'proxy EWMH activation focuses native client')
    # The border must sit immediately below its client, not above its pixels.
    stack = [w.id for w in root.query_tree().children]
    assert stack.index(frame.id) + 1 == stack.index(native.id)
    before = geometry(native)
    x, y = caption()
    pointer('mousemove', x, y, 'mousedown', 1, 'mousemove', x+60, y+45, 'sleep', .3, 'mouseup', 1)
    wait(lambda: geometry(native)[:2] == (before[0]+60, before[1]+45), 'caption drag moves native client')
    wait(paired, 'frame follows caption drag')
    before = geometry(native)
    f = frame.get_geometry()
    pointer('mousemove', f.x+f.width-2, f.y+f.height-2, 'mousedown', 1,
            'mousemove', f.x+f.width+58, f.y+f.height+38, 'sleep', .3, 'mouseup', 1)
    wait(lambda: native.get_geometry().width > before[2] and native.get_geometry().height > before[3],
         'resize corner changes native client dimensions')
    wait(paired, 'frame follows resize')
    before = geometry(native)
    x, y = caption()
    pointer('mousemove', x, y, 'click', '--repeat', 2, '--delay', 100, 1)
    wait(lambda: prop(native, '_NET_WM_STATE') and native.get_geometry().width > before[2], 'caption double click maximizes')
    wait(paired, 'maximized frame encloses native client')
    x, y = caption()
    pointer('mousemove', x, y, 'click', '--repeat', 2, '--delay', 100, 1)
    wait(lambda: geometry(native) == before, 'caption double click restores original geometry')
    command('minimize')
    wait(lambda: prop(native, 'WM_STATE')[:1] == [3] and frame.get_attributes().map_state != X.IsViewable,
         'minimize hides native and frame together')
    command('activate')
    wait(lambda: native.get_attributes().map_state == X.IsViewable and frame.get_attributes().map_state == X.IsViewable,
         'restore maps native and frame together')
    wait(paired, 'restored frame encloses native client')
    f, n = frame.get_geometry(), native.get_geometry()
    pointer('mousemove', f.x+f.width-12, f.y+(n.y-f.y)//2, 'click', 1)
    def closed():
        while d.pending_events():
            e = d.next_event()
            if e.type == X.ClientMessage and e.client_type == atom('WM_PROTOCOLS') and e.data[1][0] == atom('WM_DELETE_WINDOW'):
                return True
    wait(closed, 'close button sends WM_DELETE_WINDOW to native client')
    native.unmap()
    d.flush()
    wait(lambda: native.id not in prop(root, '_NET_CLIENT_LIST') and hidden(frame) and not prop(native, 'WM_STATE'),
         'withdrawal deletes WM_STATE, removes task and hides orphan frame')
    root.send_event(protocol.event.ClientMessage(window=frame, client_type=atom('_NET_ACTIVE_WINDOW'),
                    data=(32, [1, 0, 0, 0, 0])), event_mask=X.SubstructureRedirectMask | X.SubstructureNotifyMask)
    native.map()
    d.flush()
    frame = wait(lambda: frame_for(native), 'withdrawn client can map with an associated frame')
    wait(paired, 'remapped native client has matching frame')
    wait(lambda: prop(root, '_NET_ACTIVE_WINDOW') == [native.id], 'queued orphan activation does not break WM')
    print('PASS: genuine Wine frame, single task identity, stacking, caption drag, resize, maximize/restore, minimize, close, withdrawal/remap')
except Exception:
    for label, window in [('native', native), ('frame', locals().get('frame'))]:
        if window:
            try:
                print(label, 'map=', window.get_attributes().map_state,
                      'geometry=', geometry(window), 'WM_STATE=', prop(window, 'WM_STATE'),
                      'NET_STATE=', prop(window, '_NET_WM_STATE'), flush=True)
            except error.XError:
                print(label, 'already destroyed', flush=True)
    for name in ('windows.tsv', 'geometry.tsv', 'frames.tsv'):
        print(name, (ipc / name).read_text() if (ipc / name).exists() else 'missing', flush=True)
    raise
finally:
    native.destroy()
    d.flush()
    d.close()
