#!/usr/bin/env python3
"""A Wine window minimized and restored from Win32 maps again (bridge-wm running).

Wine 11 restores a minimized managed window through WithdrawnState: it
withdraws it (XWithdrawWindow), waits until the WM removes WM_STATE, then
maps it. Run like wm_integration.py, with WINE and WINEPREFIX set; build
the probe first with tests/build.sh."""
import os
from pathlib import Path
import subprocess
import time
from Xlib import X, display, error

ROOT = Path(__file__).resolve().parents[1]
d = display.Display()
root = d.screen().root
hwnd_atom = d.intern_atom('_WIN4WINE_HWND')


def x_window(hwnd):
    for w in root.query_tree().children:
        try:
            p = w.get_full_property(hwnd_atom, X.AnyPropertyType)
        except error.XError:
            continue
        if p and p.value and int(p.value[0]) == hwnd:
            return w
    return None


def wait(predicate, label, timeout=5):
    until = time.monotonic() + timeout
    while time.monotonic() < until:
        if predicate():
            return
        time.sleep(.05)
    raise AssertionError(label)


probe = subprocess.Popen([os.environ.get('WINE', 'wine'), str(ROOT / '.local/bridge/wine-restore-test.exe')],
                         stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, env=dict(os.environ, WIN4WINE_EXTERNAL_SHELL='1'))
try:
    hwnd = int(probe.stdout.readline().split()[1], 16)
    wait(lambda: x_window(hwnd) is not None, 'X window of the probe')
    w = x_window(hwnd)
    viewable = lambda: w.get_attributes().map_state == X.IsViewable  # the probe keeps it until it is told to end
    wait(viewable, 'shown')
    assert probe.stdout.readline().split() == ['minimized', '1'], 'Win32 minimize'
    wait(lambda: not viewable(), 'unmapped while minimized')
    assert probe.stdout.readline().split() == ['restored', '1'], 'Win32 restore'
    wait(viewable, 'mapped again after a Win32 restore')
    print('PASS: Win32 minimize and restore of a Wine window')
finally:
    probe.stdin.close()
    probe.wait(timeout=30)
