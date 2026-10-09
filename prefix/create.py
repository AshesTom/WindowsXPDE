#!/usr/bin/env python3
"""Create a dedicated win32 prefix from locally extracted XP files."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess


def create(assets, dest, home, wine):
    assets, dest, home = assets.resolve(strict=True), dest.resolve(), home.resolve(strict=True)
    if dest.exists():
        raise SystemExit(f'Refusing existing prefix: {dest}')
    records = json.loads((assets / 'manifest.json').read_text())
    if not records or not (assets / 'windows/explorer.exe').is_file():
        raise SystemExit('Incomplete XP assets')
    dest.parent.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, WINEPREFIX=str(dest), WINEARCH='win32',
               WINEDLLOVERRIDES='mscoree,mshtml=', WINEDEBUG=os.environ.get('WINEDEBUG', '-all'))
    def run(*args):
        subprocess.run([wine, *args], env=env, check=True)
    # Disable optional runtime installers: never obtain Microsoft binaries.
    run('wineboot', '-u')
    system = dest / 'drive_c/windows/system32'
    xp = dest / 'drive_c/xp'
    xp.mkdir()
    shutil.copyfile(assets / 'windows/explorer.exe', xp / 'explorer.exe')
    native = xp / 'native'
    native.mkdir()
    for dll in (assets / 'system32').glob('*.dll'):
        shutil.copyfile(dll, native / dll.name)
    for font in (assets / 'fonts').glob('*'):
        shutil.copyfile(font, dest / 'drive_c/windows/Fonts' / font.name)
    if (assets / 'themes').exists():
        shutil.copytree(assets / 'themes', dest / 'drive_c/windows/Resources/Themes', dirs_exist_ok=True)
    (dest / 'dosdevices/h:').symlink_to(home)
    # Preserve Wine's generated namespace registrations. Add only explicit XP settings.
    def reg(key, name, value, kind='REG_SZ'):
        run('reg', 'add', key, '/v', name, '/t', kind, '/d', value, '/f')
    reg(r'HKCU\Software\Wine', 'Version', 'winxp')
    reg(r'HKCU\Software\Wine\X11 Driver', 'Managed', 'Y')
    reg(r'HKCU\Software\Wine\X11 Driver', 'Decorated', 'N')
    for suffix in ('Shell Folders', 'User Shell Folders'):
        reg('HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\' + suffix,
            'Personal', 'H:\\')
    reg(r'HKCU\Software\Microsoft\Windows\CurrentVersion\ThemeManager', 'ThemeActive', '0')
    reg(r'HKCU\Control Panel\Desktop', 'LogPixels', '96', 'REG_DWORD')
    reg(r'HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced', 'WebView', '0', 'REG_DWORD')
    (dest / 'xp-shell.json').write_text(json.dumps({'assets': str(assets), 'home': str(home),
                                                   'wine': wine, 'profile': 'builtin'}, indent=2) + '\n')
    print(f'Prefix ready: {dest}')

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('assets', type=Path)
    parser.add_argument('prefix', type=Path)
    parser.add_argument('--home', type=Path, default=Path.home())
    parser.add_argument('--wine', default=os.environ.get('WINE', 'wine'))
    args = parser.parse_args()
    create(args.assets, args.prefix, args.home, args.wine)
