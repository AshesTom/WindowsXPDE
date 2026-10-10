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
import sys
import tempfile
import time
from xml.sax.saxutils import escape
from Xlib import X, display, error

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CONFIG = Path.home() / '.config/win4wine/session.json'
IM_VARIABLES = ('XMODIFIERS', 'GTK_IM_MODULE', 'QT_IM_MODULE', 'SDL_IM_MODULE', 'CLUTTER_IM_MODULE')


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
        self.env = dict(os.environ, WINEPREFIX=str(self.prefix),
                        WIN4WINE_EXTERNAL_SHELL='1', WINEDEBUG=config.get('debug', '+timestamp,+seh,+loaddll'),
                        WINEDLLOVERRIDES='explorer=b;xp-explorer=n;mscoree,mshtml=;uxtheme=b')
        # The prefix decides its architecture; the recovery prefix takes Wine's default.
        self.env.pop('WINEARCH', None)
        # Wine derives its user locale and ANSI code page from the Unix locale.
        # A Japanese XP shell expects Japanese formats and code page 932 even
        # when the Linux session itself runs in another locale.
        if config.get('locale'):
            self.env.update(LANG=config['locale'], LC_ALL=config['locale'])
        self.input_method = config.get('input_method')
        if self.input_method not in (None, 'fcitx5'):
            raise SystemExit('Unsupported input_method')
        if self.input_method:
            # An input method framework already chosen for this login (for
            # example by im-config from Xsession) keeps its variables.
            if not any(os.environ.get(name) for name in IM_VARIABLES):
                self.env.update(XMODIFIERS='@im=fcitx', GTK_IM_MODULE='fcitx', QT_IM_MODULE='fcitx',
                                SDL_IM_MODULE='fcitx')
        self.linux_font = config.get('linux_font', 'MS UI Gothic')
        # Wine's ExitWindowsEx records log off / shut down / restart here,
        # so that the shell's exit ends the session instead of restarting it.
        self.request = self.state / 'session-request'
        self.env['WIN4WINE_SESSION_REQUEST'] = 'Z:' + str(self.request.resolve()).replace('/', '\\')
        self.power_commands = {'shutdown': config.get('shutdown_command', ['systemctl', 'poweroff']),
                               'reboot': config.get('reboot_command', ['systemctl', 'reboot'])}
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
        if self.linux_font:
            self.env['FONTCONFIG_FILE'] = str(self.font_config())
        self.status('starting')

    def font_config(self):
        """Prefer one family for Linux applications' sans-serif text.

        The prefix's Fonts directory is added so that the XP shell's own
        fonts, such as MS UI Gothic, serve Linux applications too. The
        system configuration, and through it the user's, still applies."""
        directory = self.state / 'fontconfig'
        directory.mkdir(mode=0o700, exist_ok=True)
        family = escape(self.linux_font)
        aliases = ''.join(f'  <alias binding="strong"><family>{generic}</family>'
                          f'<prefer><family>{family}</family></prefer></alias>\n'
                          for generic in ('sans-serif', 'Sans', 'system-ui'))
        text = ('<?xml version="1.0"?>\n<!DOCTYPE fontconfig SYSTEM "urn:fontconfig:fonts.dtd">\n<fontconfig>\n'
                '  <include ignore_missing="yes">/etc/fonts/fonts.conf</include>\n'
                f'  <dir>{escape(str(self.prefix / "drive_c/windows/Fonts"))}</dir>\n'
                f'{aliases}</fontconfig>\n')
        target = directory / 'fonts.conf'
        tmp = directory / 'fonts.tmp'
        tmp.write_text(text)
        tmp.replace(target)
        return target

    def xim_server(self, name):
        """Whether an XIM server of this name is already registered on the display."""
        servers = self.d.screen().root.get_full_property(self.d.intern_atom('XIM_SERVERS'), X.AnyPropertyType)
        wanted = '@server=' + name
        return bool(servers) and any(self.d.get_atom_name(atom) == wanted for atom in servers.value)

    def start_input_method(self):
        if not self.input_method or self.xim_server('fcitx'):
            return None  # Not configured, or started by the login already.
        return self.spawn(['fcitx5'], 'fcitx5')

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
        # Linux tray icons (StatusNotifierItem) reach the XP notification
        # area through the proxy; the input method shows its mode there.
        self.spawn(['python3', str(ROOT / 'bridge-wm/tray.py'), '--ipc', str(self.ipc)], 'tray')
        im = self.start_input_method()
        im_restarts = deque()
        # Install only the shell family in this dedicated prefix. Runtime overrides select them.
        subprocess.run(['python3', str(ROOT / 'prefix/stage.py'), str(self.prefix),
                        '--wine', self.wine], check=True)
        crashes = deque()
        attempt = 0
        while not self.stopping:
            if wm.poll() is not None:
                raise RuntimeError('WM exited during session')
            attempt += 1
            self.request.unlink(missing_ok=True)
            ipc_win = 'Z:' + str(self.ipc).replace('/', '\\')
            proxy = self.spawn([self.wine, str(self.proxy), ipc_win], 'proxy')
            shell = self.spawn([self.wine, r'C:\xp\xp-explorer.exe'], 'xp-shell')
            started = time.monotonic()
            ready = False
            while not self.stopping:
                if wm.poll() is not None:
                    raise RuntimeError('WM exited during session')
                if im and im.poll() is not None:
                    im = self.restart_input_method(im_restarts)
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
            if self.end_requested(shell):
                return
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

    def restart_input_method(self, restarts):
        """Restart an input method that exited, at most three times in two minutes."""
        now = time.monotonic()
        restarts.append(now)
        while restarts and now-restarts[0] > 120:
            restarts.popleft()
        if len(restarts) > 3:
            print('input method keeps exiting; see fcitx5.log', file=sys.stderr)
            return None
        return self.start_input_method()

    def end_requested(self, shell):
        """Finish the session after an XP log off, shut down or restart."""
        try:
            request = self.request.read_text().strip()
        except OSError:
            return False
        if request not in ('logoff', 'shutdown', 'reboot'):
            return False
        try:
            shell.wait(timeout=30)  # let wineboot end the applications first
        except subprocess.TimeoutExpired:
            pass
        self.status('ending', request=request)
        self.stopping = True
        command = self.power_commands.get(request)
        if command:
            subprocess.Popen(command, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                             stderr=subprocess.DEVNULL, start_new_session=True)
        return True

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
