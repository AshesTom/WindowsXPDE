#!/usr/bin/env python3
"""Check a running ReactOS shell session (session.json "shell": "reactos").

Run on the display of a session that session/supervisor.py --config CONFIG
started, with the same CONFIG; build the probes first (tests/build.sh). It
checks that ReactOS's explorer is the taskbar and the desktop, that a folder
opened by running explorer.exe opens in ReactOS's explorer (Wine patch 0075),
and that a log off ends the session (patch 0018). The session ends."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import time
from Xlib import X, display, error, protocol

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('config', type=Path)
args = parser.parse_args()
config = json.loads(args.config.read_text())
if config.get('shell') != 'reactos':
    raise SystemExit('Expected a ReactOS shell session configuration')
prefix = Path(config['prefix']).resolve(strict=True)
state = Path(config.get('state', str(Path.home() / '.local/state/win4wine')))
env = dict(os.environ, WINEPREFIX=str(prefix), WINEDEBUG='-all', WIN4WINE_EXTERNAL_SHELL='1',
           WIN4WINE_FOLDER_SHELL=r'C:\ros\ros-explorer.exe',
           WIN4WINE_SESSION_REQUEST='Z:' + str((state / 'session-request').resolve()).replace('/', '\\'))
env.pop('WINEARCH', None)
d = display.Display()
class_atom = d.intern_atom('_WIN4WINE_CLASS')


def wait(predicate, label, timeout=60):
    until = time.monotonic() + timeout
    while time.monotonic() < until:
        if predicate():
            return
        time.sleep(.2)
    raise AssertionError(label)


def status():
    try:
        return json.loads((state / 'status.json').read_text())
    except (OSError, ValueError):
        return {}


def windows(win32_class):
    """Viewable top-level X windows of this Win32 class, with their X class."""
    found = []
    for w in d.screen().root.query_tree().children:
        try:
            if w.get_attributes().map_state != X.IsViewable:
                continue
            p = w.get_full_property(class_atom, X.AnyPropertyType)
            if p and bytes(p.value).rstrip(b'\0').lower() == win32_class.lower().encode():
                found.append((w, (w.get_wm_class() or ('', ''))[0]))
        except error.XError:
            pass
    return found


try:
    wait(lambda: status().get('state') == 'xp-shell', 'shell readiness')
    for name in ('Shell_TrayWnd', 'Progman'):
        wait(lambda: windows(name), name)
        assert all(owner == 'ros-explorer.exe' for _, owner in windows(name)), f'{name} of ReactOS explorer'
    before = {w.id for w, _ in windows('CabinetWClass')}
    subprocess.Popen([config['wine'], 'explorer.exe', 'C:\\'], env=env)
    wait(lambda: {w.id for w, _ in windows('CabinetWClass')} - before, 'folder window')
    for w, owner in windows('CabinetWClass'):
        if w.id not in before:
            assert owner == 'ros-explorer.exe', f'folder window of {owner}'
            w.send_event(protocol.event.ClientMessage(window=w, client_type=d.intern_atom('WM_PROTOCOLS'),
                                                      data=(32, [d.intern_atom('WM_DELETE_WINDOW'), X.CurrentTime, 0, 0, 0])))
            d.flush()
    assert not windows('ExplorerWClass'), "Wine's own folder browser"
    subprocess.run([config['wine'], str(ROOT / '.local/bridge/exit-windows-test.exe'), 'logoff'], env=env, timeout=60)
    wait(lambda: status().get('state') == 'stopped', 'session end after log off', timeout=90)
    assert (state / 'session-request').read_text().strip() == 'logoff'
    print('PASS: ReactOS taskbar and desktop, folders in ReactOS explorer, log off')
finally:
    d.close()
