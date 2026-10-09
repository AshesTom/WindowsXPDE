#!/usr/bin/env python3
"""Verify native xterm backing-content recovery without keyboard input."""
import os
from pathlib import Path
import subprocess
import time
from Xlib import X, display, error

ipc = Path(os.environ['XP_BRIDGE_DIR'])
d = display.Display()
root = d.screen().root
title = f'win4wine-redraw-{os.getpid()}'
terminal = subprocess.Popen(['xterm', '-title', title, '-fg', 'white', '-bg', 'black',
                             '-geometry', '60x15+70+70', '-e', '/bin/sh', '-c',
                             'printf "REDRAW-CONTENT-1234567890\\n"; exec sleep 120'])
w = other = None

def wait(predicate, label):
    deadline = time.monotonic() + 8
    while time.monotonic() < deadline:
        d.sync()
        result = predicate()
        if result:
            return result
        time.sleep(.05)
    raise AssertionError(label)

def activate(window):
    path = ipc / 'redraw-test.tmp'
    path.write_text(f'activate {window.id:x}\n')
    path.replace(ipc / 'redraw-test.cmd')

def pixels(window):
    try:
        return window.get_image(0, 0, 250, 18, X.ZPixmap, 0xffffffff).data
    except error.BadMatch:
        return b''  # The client can exist before its first map/resize finishes.

try:
    w = wait(lambda: next((c for c in root.query_tree().children if c.get_wm_name() == title), None),
             'xterm maps')
    child = wait(lambda: next(iter(w.query_tree().children), None), 'xterm drawing child')
    wait(lambda: len(set(pixels(child))) > 1, 'terminal marker appears')
    original = pixels(child)
    other = root.create_window(700, 60, 100, 100, 0, X.CopyFromParent,
                               background_pixel=0x123456)
    other.set_wm_name('redraw focus target')
    other.map()
    d.sync()
    time.sleep(.2)
    # Model stale framebuffer contents while xterm retains its text buffer.
    # No Expose is requested here, and no terminal input is sent during recovery.
    child.clear_area(exposures=False)
    d.sync()
    assert len(set(pixels(child))) == 1, 'damage simulation did not clear the marker'
    activate(w)
    wait(lambda: pixels(child) == original, 'focus restores text without input')
    subprocess.run(['xdotool', 'windowsize', str(w.id), '550', '300'], check=True)
    wait(lambda: len(set(pixels(child))) > 1, 'text survives resize')
    activate(other)
    time.sleep(.2)
    child.clear_area(exposures=False)
    d.sync()
    activate(w)
    wait(lambda: len(set(pixels(child))) > 1, 'text recovers after resize and focus')
    print('PASS: xterm descendant redraw after stale pixels, resize, focus; no input')
finally:
    if other:
        other.destroy()
    terminal.terminate()
    terminal.wait(timeout=5)
    d.close()
