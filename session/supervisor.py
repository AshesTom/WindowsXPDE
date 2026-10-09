#!/usr/bin/env python3
"""Xsession supervisor with shell readiness checks and bounded recovery."""
import argparse
from collections import deque
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import tempfile
import time
from Xlib import X, display, error

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CONFIG = Path.home() / '.config/win4wine/session.json'


class Session:
    def __init__(self, config):
        self.wine = str(Path(config['wine']).resolve(strict=True))
        self.prefix = Path(config['prefix']).resolve(strict=True)
        if not (self.prefix / 'xp-shell.json').is_file():
            raise SystemExit('Expected a dedicated XP prefix')
        self.proxy = ROOT / '.local/bridge/proxy.exe'
        if not self.proxy.is_file():
            raise SystemExit('Build bridge-wm first')
        self.state = Path(config.get('state', str(Path.home() / '.local/state/win4wine')))
        self.state.mkdir(mode=0o700, parents=True, exist_ok=True)
        runtime = Path(os.environ.get('XDG_RUNTIME_DIR', '/tmp'))
        self.ipc = Path(tempfile.mkdtemp(prefix=f'win4wine-{os.getuid()}-', dir=runtime))
        self.env = dict(os.environ, WINEPREFIX=str(self.prefix), WINEARCH='win32',
                        WIN4WINE_EXTERNAL_SHELL='1', WINEDEBUG=config.get('debug', '+timestamp,+seh,+loaddll'),
                        WINEDLLOVERRIDES='explorer=b;xp-explorer=n;mscoree,mshtml=;uxtheme=b')
        # Wine derives its user locale and ANSI code page from the Unix locale.
        # A Japanese XP shell expects Japanese formats and code page 932 even
        # when the Linux session itself runs in another locale.
        if config.get('locale'):
            self.env.update(LANG=config['locale'], LC_ALL=config['locale'])
        self.processes = []
        self.wine_envs = [self.env]
        self.logs = []
        self.stopping = False
        self.readiness_timeout = float(config.get('readiness_timeout', 30))
        self.failures = int(config.get('failures', 3))
        if self.failures < 1 or self.readiness_timeout <= 0:
            raise SystemExit('Invalid recovery limits')
        self.d = display.Display()
        self.class_atom = self.d.intern_atom('_WIN4WINE_CLASS')
        self.status('starting')

    def status(self, state, **extra):
        record = dict(state=state, time=time.time(), **extra)
        target = self.state / 'status.json'
        tmp = self.state / 'status.tmp'
        tmp.write_text(json.dumps(record, indent=2) + '\n')
        tmp.replace(target)

    def spawn(self, args, name, env=None):
        log = (self.state / f'{name}.log').open('ab', buffering=0)
        log.write(f'\n--- start {time.time()} ---\n'.encode())
        process = subprocess.Popen(args, env=env or self.env, stdout=log, stderr=subprocess.STDOUT,
                                   start_new_session=True)
        self.logs.append(log)
        self.processes.append(process)
        return process

    def tray(self):
        for w in self.d.screen().root.query_tree().children:
            try:
                p = w.get_full_property(self.class_atom, X.AnyPropertyType)
                if p and bytes(p.value).rstrip(b'\0').lower() == b'shell_traywnd' and w.get_attributes().map_state == X.IsViewable:
                    geometry = w.get_geometry()
                    return geometry.width >= 200 and geometry.height >= 20
            except error.XError:
                pass
        return False

    def halt(self, process):
        if process.poll() is None:
            try:
                os.killpg(process.pid, signal.SIGTERM)
                process.wait(timeout=3)
            except (ProcessLookupError, subprocess.TimeoutExpired):
                try:
                    os.killpg(process.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass

    def stop_wine(self):
        server = Path(self.wine).with_name('wineserver')
        for env in self.wine_envs:
            subprocess.run([str(server), '-k'], env=env, timeout=10, check=False)
            subprocess.run([str(server), '-w'], env=env, timeout=10, check=False)

    def run(self):
        wm = self.spawn(['python3', str(ROOT / 'bridge-wm/wm.py'), '--ipc', str(self.ipc)], 'wm')
        time.sleep(.5)
        if wm.poll() is not None:
            raise RuntimeError('WM failed; see wm.log')
        # Install only the shell family in this dedicated prefix. Runtime overrides select them.
        subprocess.run(['python3', str(ROOT / 'prefix/stage.py'), str(self.prefix),
                        '--wine', self.wine], check=True)
        crashes = deque()
        attempt = 0
        while not self.stopping:
            if wm.poll() is not None:
                raise RuntimeError('WM exited during session')
            attempt += 1
            ipc_win = 'Z:' + str(self.ipc).replace('/', '\\')
            proxy = self.spawn([self.wine, str(self.proxy), ipc_win], 'proxy')
            shell = self.spawn([self.wine, r'C:\xp\xp-explorer.exe'], 'xp-shell')
            started = time.monotonic()
            ready = False
            while not self.stopping:
                if wm.poll() is not None:
                    raise RuntimeError('WM exited during session')
                has_tray = self.tray()
                if has_tray and not ready:
                    ready = True
                    self.status('xp-shell', attempt=attempt)
                if ready and not has_tray:
                    break
                if not ready and (time.monotonic()-started > self.readiness_timeout or shell.poll() is not None):
                    break
                if proxy.poll() is not None:
                    break
                time.sleep(.25)
            if self.stopping:
                break
            self.halt(shell)
            self.halt(proxy)
            # Release the prior desktop manager too; environment is inherited at its creation.
            self.stop_wine()
            now = time.monotonic()
            crashes.append(now)
            while crashes and now-crashes[0] > 120:
                crashes.popleft()
            self.status('retry', attempt=attempt, failures=len(crashes))
            if len(crashes) >= self.failures:
                self.fallback(wm)
                return
            time.sleep(1)

    def fallback(self, wm):
        env = dict(self.env)
        env.pop('WIN4WINE_EXTERNAL_SHELL', None)
        # Native shell COM registrations can also affect builtin explorer. Keep
        # recovery independent of both the XP files and their registry entries.
        env['WINEPREFIX'] = str(self.prefix / 'recovery')
        env['WINEDLLOVERRIDES'] = 'explorer,shell32,shdocvw,browseui,shlwapi,comctl32,uxtheme=b;mscoree,mshtml='
        self.wine_envs.append(env)
        self.status('fallback')
        # A Wine desktop plus file browser and a native terminal gives an escape route.
        home = 'Z:' + str(Path.home()).replace('/', '\\')
        fallback = self.spawn([self.wine, 'explorer', home], 'fallback', env)
        terminal = self.spawn(['xterm', '-title', 'XP shell recovery'], 'recovery-terminal', env)
        while not self.stopping:
            if wm.poll() is not None:
                raise RuntimeError('WM exited during fallback')
            if fallback.poll() is not None and terminal.poll() is not None:
                break
            time.sleep(.5)

    def close(self):
        self.stopping = True
        for p in reversed(self.processes):
            self.halt(p)
        try:
            self.stop_wine()
        finally:
            for f in self.logs:
                f.close()
            self.d.close()
            shutil.rmtree(self.ipc, ignore_errors=True)
            self.status('stopped')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', type=Path, default=DEFAULT_CONFIG)
    args = parser.parse_args()
    session = Session(json.loads(args.config.read_text()))
    for sig in (signal.SIGTERM, signal.SIGINT, signal.SIGHUP):
        signal.signal(sig, lambda *_: setattr(session, 'stopping', True))
    try:
        session.run()
    finally:
        session.close()
