#!/usr/bin/env python3
"""End-to-end test of X11 identities, Win32 proxies and shell notifications."""
import os
from pathlib import Path
import re
import subprocess
import time
from Xlib import X, display

rootdir = Path(__file__).resolve().parents[1]
wine = os.environ['WINE']
ipc = Path(os.environ['XP_BRIDGE_DIR'])
env = dict(os.environ, WIN4WINE_EXTERNAL_SHELL='1', WINEDEBUG='-all',
           WINEDLLOVERRIDES='explorer=b;mscoree,mshtml=')
log = (ipc / 'shellhook.log').open('w')
proxy = subscriber = None
d = display.Display()
w = None

def wait(predicate, label):
    until = time.monotonic() + 10
    while time.monotonic() < until:
        d.sync()
        if predicate():
            return
        time.sleep(.1)
    raise AssertionError(label)

def inspect(*args):
    return subprocess.check_output([wine, str(rootdir / '.local/bridge/inspect.exe'), *args], env=env, timeout=10).decode('utf-8', 'replace')

def prop(window, name):
    p = window.get_full_property(d.intern_atom(name), X.AnyPropertyType)
    return list(p.value) if p else []

try:
    subscriber = subprocess.Popen([wine, str(rootdir / '.local/bridge/shellhook-test.exe')], env=env, stdout=log)
    wait(lambda: 'subscribed' in (ipc / 'shellhook.log').read_text(), 'shell hook registration')
    proxy = subprocess.Popen([wine, str(rootdir / '.local/bridge/proxy.exe'), 'Z:' + str(ipc).replace('/', '\\')], env=env)
    w = d.screen().root.create_window(60, 60, 400, 250, 0, X.CopyFromParent, X.InputOutput, X.CopyFromParent,
                                      background_pixel=0x508060)
    w.set_wm_name('Linux proxy integration')
    w.set_wm_protocols([d.intern_atom('WM_DELETE_WINDOW')])
    w.map()
    d.flush()
    captured = []
    def find_proxy():
        for line in inspect('dump').splitlines():
            if 'class=Win4WineProxy title=Linux proxy integration' in line:
                captured[:] = [line.split()[0]]
                assert 'visible=1' in line
                return True
        return False
    wait(find_proxy, 'Win32 proxy creation and visibility')
    hwnd = captured[0]
    wait(lambda: 'class=Win4WineProxy' in (ipc / 'shellhook.log').read_text(), 'shell created notification')
    inspect('minimize', hwnd)
    wait(lambda: prop(w, 'WM_STATE')[:1] == [3], 'proxy minimize -> X11')
    inspect('restore', hwnd)
    wait(lambda: prop(d.screen().root, '_NET_ACTIVE_WINDOW') == [w.id] and w.get_attributes().map_state == X.IsViewable,
         'proxy restore -> X11 focus')
    wait(lambda: 'class=Win4WineProxy title=Linux proxy integration' in inspect('foreground'),
         'native focus mirrors Win32 foreground identity')
    w.set_wm_name('Linux renamed')
    d.flush()
    wait(lambda: 'class=Win4WineProxy title=Linux renamed' in inspect('dump'), 'title propagation')
    inspect('close', hwnd)
    received = []
    def closed():
        while d.pending_events():
            event = d.next_event()
            if event.type == X.ClientMessage and event.client_type == d.intern_atom('WM_PROTOCOLS'):
                received.append(event.data[1][0])
        return d.intern_atom('WM_DELETE_WINDOW') in received
    wait(closed, 'proxy close -> X11 WM_DELETE_WINDOW')
    w.destroy()
    w = None
    d.flush()
    wait(lambda: 'class=Win4WineProxy title=Linux renamed' not in inspect('dump'), 'proxy destruction')
    wait(lambda: re.search(r'event=2 hwnd=' + hwnd, (ipc / 'shellhook.log').read_text(), re.I), 'shell destroyed notification')
    print('PASS: shell hook subscription, proxy creation, minimize, restore, focus, rename, close, destruction')
finally:
    if w:
        w.destroy()
    d.flush()
    d.close()
    for p in (proxy, subscriber):
        if p and p.poll() is None:
            p.terminate()
            try:
                p.wait(timeout=5)
            except subprocess.TimeoutExpired:
                p.kill()
                p.wait()
    log.close()
