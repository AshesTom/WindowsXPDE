#!/usr/bin/env python3
"""Stage a dedicated 64-bit (WoW64) prefix for Windows 7's shell (experimental).

  win7.py stage PREFIX [--wine WINE]

Windows 7's x64 explorer runs as C:\\w7\\w7-explorer.exe, a 64-bit process.
Its shell DLLs (with their MUI resources merged, since Wine does not look for
.mui files) are kept in C:\\w7\\native and placed in system32; Common
Controls 6 replaces Wine's assembly file. The prefix keeps Wine's DLLs for
every other program (DllOverrides builtin) and gives w7-explorer.exe the
native ones (AppDefaults), as prefix/create.py does for XP's explorer.

The session runs "stage" before each start: a Wine update of the prefix
(wineboot -u, from a newer wine.inf) replaces DLLs and rewrites
registrations that Windows 7's shell reads, such as the name of Computer.
"""
import argparse
import filecmp
import json
import os
from pathlib import Path
import pwd
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
MARKER = 'win7-shell.json'
# Bump when the registry settings below change; stage then applies them again.
VERSION = 3
LOCALES = {'ja': 'ja_JP.UTF-8', 'en': 'en_US.UTF-8'}
SXS = 'amd64_microsoft.windows.common-controls_6595b64144ccf1df_6.0.2600.2982_none_deadbeef'
# Windows 7's names of shell items whose registration wine.inf writes with Wine's own string.
NAMES = (
    ('{20D04FE0-3AEA-1069-A2D8-08002B30309D}', r'@%SystemRoot%\system32\shell32.dll,-9216'),  # Computer
    ('{21EC2020-3AEA-1069-A2DD-08002B30309D}', r'@%SystemRoot%\system32\shell32.dll,-4161'),  # Control Panel
    # Help and Support names itself in explorer.exe; %SystemRoot%\explorer.exe is Wine's.
    ('{2559a1f1-21d7-11d4-bdaf-00c04f60b9f0}', r'@C:\w7\w7-explorer.exe,-7021'),
)
# Wine's own desktop items that Windows 7's desktop does not show: My Documents,
# the Unix root folder and Internet Explorer.
HIDDEN_DESKTOP_ITEMS = ('{450D8FBA-AD25-11D0-98A8-0800361B1103}', '{9D20AAE8-0625-44B0-9CA7-71889C2254D9}',
                        '{871C5380-42A0-1069-A2EA-08002B30309D}')


class Prefix:
    def __init__(self, path, wine, lang):
        self.path = path
        self.wine = Path(shutil.which(str(wine)) or wine).resolve(strict=True)
        self.lang = lang
        locale = LOCALES[lang]
        self.env = dict(os.environ, WINEPREFIX=str(path), WINEDEBUG=os.environ.get('WINEDEBUG', '-all'),
                        WINEDLLOVERRIDES='mscoree,mshtml=', LANG=locale, LC_ALL=locale)
        self.env.pop('WINEARCH', None)

    @property
    def system(self):
        return self.path / 'drive_c/windows/system32'

    def run(self, *args, timeout=300, check=True):
        return subprocess.run([str(self.wine), *args], env=self.env, check=check, timeout=timeout,
                              stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    def reg(self, key, name, value, kind='REG_SZ'):
        self.run('reg', 'add', key, *(('/ve',) if name is None else ('/v', name)), '/t', kind, '/d', str(value), '/f')

    def wait(self):
        server = str(self.wine.with_name('wineserver'))
        subprocess.run([server, '-w'], env=self.env, check=False, timeout=120)

    def stop(self):
        # Wine's services stay until the server times out; end them so that a
        # wait does not block, and the registry is saved.
        server = str(self.wine.with_name('wineserver'))
        subprocess.run([server, '-k'], env=self.env, check=False, timeout=15)

    def place(self, source, target):
        """Copy SOURCE over TARGET unless equal, without truncating a mapped image."""
        if target.is_file() and filecmp.cmp(source, target, shallow=False):
            return
        target.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.NamedTemporaryFile(dir=target.parent, prefix='.w7-stage-', delete=False) as temp:
            path = Path(temp.name)
        try:
            shutil.copyfile(source, path)
            os.replace(path, target)
        finally:
            path.unlink(missing_ok=True)

    def place_files(self):
        native = self.path / 'drive_c/w7/native'
        for source in sorted(native.glob('*.dll')):
            self.place(source, self.system / source.name)
        # Same assembly version as Wine's own Common Controls 6: the file is
        # replaced, and the prefix-wide builtin override keeps Wine's for others.
        if (native / 'sxs/comctl32.dll').is_file():
            self.place(native / 'sxs/comctl32.dll', self.path / f'drive_c/windows/winsxs/{SXS}/comctl32.dll')

    def registrations(self):
        """What a Wine update of the prefix rewrites."""
        for clsid, name in NAMES:
            self.reg(rf'HKCR\CLSID\{clsid}', 'LocalizedString', name, 'REG_EXPAND_SZ')
        # Linux tray icons and the input method's mode stay in the notification
        # area (Windows 7's "always show all icons and notifications").
        self.reg(r'HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer', 'EnableAutoTray', 0, 'REG_DWORD')
        icons = r'HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\HideDesktopIcons\NewStartPanel'
        for clsid in HIDDEN_DESKTOP_ITEMS:
            self.reg(icons, clsid, 1, 'REG_DWORD')
        user = pwd.getpwuid(os.getuid()).pw_name
        wallpaper = Path(f'users/{user}/AppData/Roaming/Microsoft/Windows/Themes/TranscodedWallpaper.bmp')
        if (self.path / 'drive_c' / wallpaper).is_file():
            desktop = r'HKCU\Control Panel\Desktop'
            self.reg(desktop, 'Wallpaper', 'C:\\' + str(wallpaper).replace('/', '\\'))
            self.reg(desktop, 'WallpaperStyle', '10')
            self.reg(desktop, 'TileWallpaper', '0')


def stage(dest, wine=None):
    config = json.loads((dest / MARKER).read_text())
    prefix = Prefix(dest, wine or config['wine'], config.get('lang', 'ja'))
    inf = prefix.wine.parent.parent / 'share/wine/wine.inf'
    timestamp = dest / '.update-timestamp'
    version = dest / '.w7-registry-version'
    current = timestamp.read_text().strip() if timestamp.is_file() else ''
    wine_current = inf.is_file() and current == str(int(inf.stat().st_mtime))
    applied = version.read_text().split() if version.is_file() else []
    if wine_current and applied == [str(VERSION), current]:
        # Nothing changed, but a first wineboot or an update may have replaced files.
        prefix.place_files()
        return
    print('Updating the Windows 7 shell prefix', flush=True)
    try:
        if not wine_current:
            prefix.run('wineboot', '-u', timeout=900)
        prefix.place_files()
        prefix.registrations()
    finally:
        prefix.stop()
    current = timestamp.read_text().strip() if timestamp.is_file() else ''
    temp = version.with_suffix('.tmp')
    temp.write_text(f'{VERSION} {current}\n')
    temp.replace(version)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest='command', required=True)
    p = commands.add_parser('stage')
    p.add_argument('prefix', type=Path)
    p.add_argument('--wine')
    args = parser.parse_args()
    path = args.prefix.resolve(strict=True)
    if not (path / MARKER).is_file():
        raise SystemExit('Not a Windows 7 shell prefix')
    stage(path, args.wine)
