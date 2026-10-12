#!/usr/bin/env python3
"""win4wine's labwc without Wine: the bridge's rules with stand-in windows.

Starts the compositor nested on the X display in DISPLAY (Xvfb is enough)
with one output at scale 1.5, then checks with X windows that carry Wine's
identity (_WIN4WINE_CLASS, _WIN4WINE_HWND) and plain X windows (Linux
applications):
- the X space: the output in pixels, the taskbar's edge in the work area
- the IPC: windows.tsv, geometry.tsv, commands, frames paired from frames.tsv
- stacking: the desktop at the bottom, the taskbar above windows, a frame right
  below its window, in the scene and in X's order (Xwayland picks by it)
- maximizing a framed window into the work area less its frame

  LABWC=.local/labwc-install/bin/labwc python3 tests/wayland_compositor.py
"""
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import tempfile
import time
from Xlib import X, Xatom, Xutil, display

LABWC = os.environ.get('LABWC', str(Path(__file__).resolve().parents[1] / '.local/labwc-install/bin/labwc'))
SCALE = 1.5
WIDTH, HEIGHT = 1280, 800  # the output in pixels


def wait(predicate, label, timeout=10, show=None):
    until = time.monotonic() + timeout
    while time.monotonic() < until:
        value = predicate()
        if value:
            return value
        time.sleep(.05)
    raise AssertionError(label + (f': {show()}' if show else ''))


def main():
    if not os.environ.get('DISPLAY'):
        print('needs an X display (Xvfb) for the nested compositor', file=sys.stderr)
        return 77
    work = Path(tempfile.mkdtemp(prefix='w4w-labwc-'))
    runtime = work / 'runtime'
    ipc = work / 'ipc'
    config = work / 'config'
    runtime.mkdir(mode=0o700)
    ipc.mkdir(mode=0o700)
    config.mkdir()
    root = Path(__file__).resolve().parents[1]
    for name in ('rc.xml', 'menu.xml'):
        shutil.copy(root / 'wayland/labwc' / name, config / name)
    started = work / 'started'
    startup = work / 'startup.sh'
    startup.write_text(f'#!/bin/sh\nwlr-randr --output X11-1 --custom-mode {WIDTH}x{HEIGHT} --scale {SCALE}\n'
                       f'echo "$DISPLAY" > {started}\n')
    startup.chmod(0o755)
    env = dict(os.environ, XDG_RUNTIME_DIR=str(runtime), WIN4WINE_IPC=str(ipc), WLR_BACKENDS='x11',
               WLR_X11_OUTPUTS='1', WLR_RENDERER='pixman', WLR_NO_HARDWARE_CURSORS='1')
    log = (work / 'labwc.log').open('w')
    labwc = subprocess.Popen([LABWC, '-C', str(config), '-s', str(startup)], env=env, stdout=log,
                             stderr=subprocess.STDOUT, start_new_session=True)
    try:
        xdisplay = wait(lambda: started.exists() and started.read_text().strip(), 'compositor started', 30)
        time.sleep(1)  # the scale applies
        d = display.Display(xdisplay)
        run(d, ipc)
        print('ok')
        return 0
    finally:
        os.killpg(labwc.pid, signal.SIGTERM)
        labwc.wait(10)
        log.close()
        if os.environ.get('KEEP'):
            print(f'kept {work}')
        else:
            shutil.rmtree(work, ignore_errors=True)


def run(d, ipc):
    root = d.screen().root
    atom = d.intern_atom

    def prop(w, name):
        p = w.get_full_property(atom(name), X.AnyPropertyType)
        return list(p.value) if p else []

    def make(title, x, y, width, height, cls=None, hwnd=None):
        w = root.create_window(x, y, width, height, 0, X.CopyFromParent, X.InputOutput, X.CopyFromParent,
                               background_pixel=0x406080)
        w.set_wm_name(title)
        # Wine gives its windows' positions (labwc would place them otherwise)
        w.set_wm_normal_hints(flags=Xutil.USPosition | Xutil.USSize, x=x, y=y, width=width, height=height)
        if cls:
            w.change_property(atom('_WIN4WINE_CLASS'), Xatom.STRING, 8, cls.encode())
            w.change_property(atom('_WIN4WINE_HWND'), Xatom.CARDINAL, 32, [hwnd or w.id])
        w.map()
        d.flush()
        return w

    def geometry(w):
        g = w.get_geometry()
        t = w.translate_coords(root, 0, 0)
        return (-t.x, -t.y, g.width, g.height)

    def stacking():
        return [c.id for c in root.query_tree().children if c.get_attributes().map_state == X.IsViewable]

    def read(name):
        try:
            return (ipc / name).read_text()
        except FileNotFoundError:
            return ''

    def command(text):
        temporary = ipc / 'test.tmp'
        temporary.write_text(text + '\n')
        temporary.replace(ipc / f'{time.monotonic_ns()}.cmd')

    # The output is in pixels for X; the root is that big.
    wait(lambda: root.get_geometry().width == WIDTH and root.get_geometry().height == HEIGHT, 'X space in pixels')

    desktop = make('desktop', 0, 0, WIDTH, HEIGHT, 'Progman')
    dock = make('taskbar', 0, HEIGHT - 45, WIDTH, 45, 'Shell_TrayWnd')
    wait(lambda: prop(root, '_NET_WORKAREA') == [0, 0, WIDTH, HEIGHT - 45], 'work area less the taskbar',
         show=lambda: prop(root, '_NET_WORKAREA'))
    wait(lambda: prop(root, '_GTK_WORKAREAS_D0') == [0, 0, WIDTH, HEIGHT - 45], 'work area per output',
         show=lambda: prop(root, '_GTK_WORKAREAS_D0'))

    native = make('native', 300, 200, 600, 300)
    wait(lambda: f'{native.id:x}\t0\t' in read('windows.tsv'), 'a Linux window is a task')
    assert f'{desktop.id:x}\t' not in read('windows.tsv') and f'{dock.id:x}\t' not in read('windows.tsv')
    wait(lambda: f'{native.id:x} 300 200 600 300 0 0 0 {WIDTH} {HEIGHT - 45}' in read('geometry.tsv'),
         'geometry in Win32 coordinates, exact X pixels')
    order = stacking()
    assert order.index(desktop.id) < order.index(native.id) < order.index(dock.id), ('stacking', order)

    # A frame from the proxy: its HWND in frames.tsv pairs it with the window.
    frame_hwnd = 0x400a0
    frame = make('frame', -32000, -32000, 1, 1, 'Win4WineProxy', frame_hwnd)
    (ipc / 'frames.tmp').write_text(f'{frame_hwnd:x} {native.id:x} 5 5 44 5 0 0 5 5\n')
    (ipc / 'frames.tmp').replace(ipc / 'frames.tsv')
    wait(lambda: geometry(frame) == (295, 156, 610, 349), 'the frame around its window')
    order = stacking()
    assert order.index(frame.id) + 1 == order.index(native.id), ('frame below its window', order)

    command(f'geometry {native.id:x} 100 120 500 250')
    wait(lambda: geometry(native) == (100, 120, 500, 250), 'geometry command')
    wait(lambda: geometry(frame) == (95, 76, 510, 299), 'the frame follows')

    command(f'maximize {native.id:x}')
    # The work area less the frame, whose outer border goes past it when maximized (5 5 hidden)
    wait(lambda: geometry(native) == (0, 39, WIDTH, HEIGHT - 45 - 39), 'maximized into the work area')
    wait(lambda: f'{native.id:x} 0 39 {WIDTH} {HEIGHT - 45 - 39} 1 ' in read('geometry.tsv'), 'maximized flag')
    command(f'restore {native.id:x}')
    wait(lambda: geometry(native) == (100, 120, 500, 250), 'restored')

    command(f'minimize {native.id:x}')
    wait(lambda: f'{native.id:x}\t1\t' in read('windows.tsv'), 'minimized')
    command(f'activate {native.id:x}')
    wait(lambda: f'{native.id:x}\t0\t' in read('windows.tsv'), 'activated again')

    # A Win32 topmost window stays above the others
    topmost = make('topmost', 50, 50, 200, 100, 'Topmost')
    d.flush()
    root.send_event(_state_message(d, topmost, 1, '_NET_WM_STATE_ABOVE'),
                    event_mask=X.SubstructureRedirectMask | X.SubstructureNotifyMask)
    d.flush()
    wait(lambda: atom('_NET_WM_STATE_ABOVE') in prop(topmost, '_NET_WM_STATE'), 'above state kept')
    native.configure(stack_mode=X.Above)
    d.flush()
    command(f'activate {native.id:x}')
    time.sleep(.5)
    order = stacking()
    assert order.index(native.id) < order.index(topmost.id), ('topmost above', order)
    assert order[0] == desktop.id, ('desktop at the bottom', order)


def _state_message(d, w, action, name):
    from Xlib.protocol import event
    return event.ClientMessage(window=w, client_type=d.intern_atom('_NET_WM_STATE'),
                               data=(32, [action, d.intern_atom(name), 0, 1, 0]))


if __name__ == '__main__':
    sys.exit(main())
