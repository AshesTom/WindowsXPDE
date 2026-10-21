#!/usr/bin/env python3
"""Fill the left column of XP's Start menu.

On XP the top of the left column holds "Internet" and "E-mail": the Start
Menu Internet Applications registered under Software\\Clients\\StartMenuInternet
and Software\\Clients\\Mail (https://learn.microsoft.com/windows/win32/shell/start-menu-reg).
They are registered here as the Linux desktop's web browser (xdg-settings) and
mailto handler (xdg-mime), started through linux-launch.exe with their own
icons. Below them, as on XP, come the most used programs: XP Explorer leaves
its own list empty under Wine, so startmenu.exe recent (run by xpstart.exe at
each logon) keeps them pinned, counted from XP's usage data. Until programs
have been used, a few KDE essentials fill the list. To pin programs of your
own instead, list desktop file IDs in pins.txt (one per line, at the project
root); the most used programs then follow them. The Start menu keeps the
height of its right column, which has XP's full set of items.
Run with the session stopped.
"""
import argparse
import json
import os
from pathlib import Path, PureWindowsPath
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
PROGRAMS = r'C:\ProgramData\Microsoft\Windows\Start Menu\Programs'
STATE = r'C:\xp\linux-apps'
LAUNCH = STATE + r'\linux-launch.exe'
RECENT = r'HKEY_CURRENT_USER\Software\XP Explorer\Recent'
ICONS = r'C:\xp\linux-apps\icons'
# Our stand-in (helpers/iexplore.c), which opens the Linux browser; the real
# Internet Explorer 6 stays in Program Files for the desktop's Internet icon.
IEXPLORE = r'C:\xp\bin\iexplore.exe'
DEFAULTS = (
    # The seeds of the most used programs: file manager, terminal, text editor, settings.
    ('org.kde.dolphin.desktop', 'org.gnome.Nautilus.desktop', 'thunar.desktop'),
    ('org.kde.konsole.desktop', 'org.gnome.Console.desktop', 'xterm.desktop'),
    ('org.kde.kate.desktop', 'org.kde.kwrite.desktop', 'org.gnome.TextEditor.desktop'),
    ('systemsettings.desktop', 'kdesystemsettings.desktop'),
)
BROWSERS = ('firefox.desktop', 'chromium.desktop', 'google-chrome.desktop', 'org.kde.falkon.desktop')
MAILERS = ('org.mozilla.Thunderbird.desktop', 'thunderbird.desktop', 'org.kde.kmail2.desktop',
           'org.gnome.Evolution.desktop', 'geary.desktop')


def xdg_default(*args):
    try:
        return subprocess.run(args, capture_output=True, text=True, timeout=10).stdout.strip()
    except (OSError, subprocess.SubprocessError):
        return ''


def pick(links, preferred, fallbacks):
    return next((i for i in (preferred, *fallbacks) if i and i in links), None)


def pinned(links):
    path = ROOT / 'pins.txt'
    if path.is_file():
        return [line.strip() for line in path.read_text().splitlines() if line.strip() and not line.startswith('#')]
    return []


def name_of(entry, file_id):
    return PureWindowsPath(entry['link']).stem if entry else file_id


def client_keys(root, key, entry, file_id, command):
    """Registry lines of one Start Menu Internet Application."""
    name = name_of(entry, file_id).replace('"', '')
    icon = f'{ICONS}\\{file_id}.ico,0'
    base = f'[{root}\\{key}]'
    command = command.replace('\\', '\\\\').replace('"', '\\"')
    return [base, f'@="{name}"', f'"LocalizedString"="{name}"', '',
            f'[{root}\\{key}\\DefaultIcon]', '@="' + icon.replace('\\', '\\\\') + '"', '',
            f'[{root}\\{key}\\shell\\open\\command]', f'@="{command}"', '',
            # Set Program Access and Computer Defaults: the icons are shown.
            f'[{root}\\{key}\\InstallInfo]', '"IconsVisible"=dword:00000001', '']


def register_clients(prefix, wine, links):
    browser = pick(links, xdg_default('xdg-settings', 'get', 'default-web-browser'), BROWSERS)
    mailer = pick(links, xdg_default('xdg-mime', 'query', 'default', 'x-scheme-handler/mailto'), MAILERS)
    lines = ['Windows Registry Editor Version 5.00', '']
    clients = r'HKEY_LOCAL_MACHINE\Software\Clients'
    # Start over: earlier registrations (Wine's winebrowser.exe among them).
    lines += [f'[-{clients}\\StartMenuInternet]', '', f'[-{clients}\\Mail]', '']
    if browser:
        # XP's explorer realigns the Internet item with the program that opens
        # http links; both are our stand-in (also named iexplore.exe), which
        # opens this browser.
        lines += [f'[{clients}\\StartMenuInternet]', '@="IEXPLORE.EXE"', '']
        lines += client_keys(clients + '\\StartMenuInternet', 'IEXPLORE.EXE', links[browser], browser, f'"{IEXPLORE}"')
        for protocol in ('http', 'https', 'ftp'):
            lines += [f'[HKEY_CLASSES_ROOT\\{protocol}\\shell\\open\\command]',
                      '@="\\"' + IEXPLORE.replace('\\', '\\\\') + '\\" \\"%1\\""', '']
    if mailer:
        key = mailer.removesuffix('.desktop')
        lines += [f'[{clients}\\Mail]', f'@="{key}"', '']
        # A mail client opens its inbox; a browser handling mailto opens a new message.
        # Through the mailer's own launcher, so that it is not also counted as
        # a most used program.
        launcher = links[mailer].get('launcher')
        program = f'{STATE}\\apps\\{launcher}' if launcher else LAUNCH
        command = (f'"{program}" {links[mailer]["arguments"]}' if mailer in MAILERS or mailer != browser
                   else f'"{LAUNCH}" xdg-open mailto:')
        lines += client_keys(clients + '\\Mail', key, links[mailer], mailer, command)
    # Room for two most used programs under the pins (XP: 6), and XP's full
    # right column, so the menu is no taller than on XP.
    advanced = r'HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced'
    # No empty rows for XP's own list of most used programs, which stays empty
    # (our pins stand in for it), and XP's full right column. StartMenuInit:
    # XP's first logon would clear the usage data and the pins.
    lines += [f'[{advanced}]', '"Start_MinMFU"=dword:00000000', '"Start_ShowNetPlaces"=dword:00000001',
              '"StartMenuInit"=dword:00000002', '']
    # The user's choice wins over the machine's on XP: leave none behind.
    lines += [r'[-HKEY_CURRENT_USER\Software\Clients\StartMenuInternet]', '',
              r'[-HKEY_CURRENT_USER\Software\Clients\Mail]', '']
    env = dict(os.environ, WINEPREFIX=str(prefix), WINEDEBUG='-all')
    with tempfile.TemporaryDirectory() as temp:
        reg = Path(temp) / 'clients.reg'
        reg.write_text('\r\n'.join(lines) + '\r\n', encoding='utf-16')
        # The 32-bit view, which XP Explorer reads.
        subprocess.run([wine, r'C:\windows\syswow64\regedit.exe', '/S', 'Z:' + str(reg).replace('/', '\\')],
                       env=env, check=True)
    return browser, mailer


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('prefix', type=Path)
    parser.add_argument('--wine', default=os.environ.get('WINE', 'wine'))
    args = parser.parse_args()
    prefix = args.prefix.resolve(strict=True)
    try:
        links = json.loads((prefix / 'drive_c/xp/linux-apps/state.json').read_text())['links']
    except (OSError, ValueError, KeyError):
        raise SystemExit('Run scripts/linux_apps.py first')
    browser, mailer = register_clients(prefix, args.wine, links)
    print(f'Internet: {browser or "none"}, E-mail: {mailer or "none"}')
    env = dict(os.environ, WINEPREFIX=str(prefix), WINEDEBUG='-all')
    current = subprocess.run([args.wine, r'C:\xp\bin\startmenu.exe', 'list'], env=env,
                             capture_output=True, text=True).stdout.lower()
    defaults = [PROGRAMS + '\\' + links[i]['link']
                for i in (next((c for c in choices if c in links), '') for choices in DEFAULTS) if i]
    own = [PROGRAMS + '\\' + links[i]['link'] for i in pinned(links) if i in links]
    # Earlier versions pinned the KDE essentials; they are seeds now.
    if not (ROOT / 'pins.txt').is_file():
        old = [p for p in defaults if p.lower() in current and not managed(prefix, args.wine, p)]
        if old:
            subprocess.run([args.wine, r'C:\xp\bin\startmenu.exe', 'unpin', *old], env=env)
    # XP's own Internet and E-mail items stay first; the others follow them.
    missing = [p for p in own if p.lower() not in current]
    if missing:
        subprocess.run([args.wine, r'C:\xp\bin\startmenu.exe', 'pin', *missing], env=env)
    write_seeds(prefix, args.wine, defaults)


def managed(prefix, wine, path):
    """Whether startmenu.exe recent pinned PATH (its Pins list)."""
    env = dict(os.environ, WINEPREFIX=str(prefix), WINEDEBUG='-all')
    out = subprocess.run([wine, 'reg', 'query', RECENT.replace('HKEY_CURRENT_USER', 'HKCU'), '/v', 'Pins'],
                         env=env, capture_output=True, text=True).stdout
    return path.lower() in out.lower()


def write_seeds(prefix, wine, seeds):
    """The shortcuts that fill the most used programs until programs are used."""
    data = ''.join(s + '\0' for s in seeds).encode('utf-16le') + b'\0\0'
    lines = ['Windows Registry Editor Version 5.00', '', f'[{RECENT}]',
             '"Seeds"=hex(7):' + ','.join(f'{b:02x}' for b in data), '']
    env = dict(os.environ, WINEPREFIX=str(prefix), WINEDEBUG='-all')
    with tempfile.TemporaryDirectory() as temp:
        reg = Path(temp) / 'seeds.reg'
        reg.write_text('\r\n'.join(lines) + '\r\n', encoding='utf-16')
        subprocess.run([wine, r'C:\windows\syswow64\regedit.exe', '/S', 'Z:' + str(reg).replace('/', '\\')],
                       env=env, check=True)

if __name__ == '__main__':
    main()
