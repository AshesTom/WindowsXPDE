#!/usr/bin/env python3
"""Create and stage a dedicated 64-bit (WoW64) prefix for Windows 7's shell (experimental).

  win7.py create ASSETS PREFIX [--home HOME] [--wine WINE]
  win7.py stage PREFIX [--wine WINE]

ASSETS is the output of extract/win7.py (the image's language decides the
prefix's: Japanese or English).

Windows 7's x64 explorer runs as C:\\w7\\w7-explorer.exe, a 64-bit process.
Its shell DLLs (with their MUI resources merged, since Wine does not look for
.mui files) are kept in C:\\w7\\native and placed in system32; Common
Controls 6 replaces Wine's assembly file. The prefix keeps Wine's DLLs for
every other program (DllOverrides builtin) and gives w7-explorer.exe the
native ones (AppDefaults), as prefix/create.py does for XP's explorer.
DLLs that the shell only reads for their strings and icons (imageres) are
placed in system32 without overrides. The registry gets what the components
of these files register (their manifests, extract/manifests.py), not the
registry of an installed Windows.

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
VERSION = 4
LOCALES = {'ja': 'ja_JP.UTF-8', 'en': 'en_US.UTF-8'}
SXS = 'amd64_microsoft.windows.common-controls_6595b64144ccf1df_6.0.2600.2982_none_deadbeef'
# Windows 7's names of shell items whose registration wine.inf writes with Wine's own string.
NAMES = (
    ('{20D04FE0-3AEA-1069-A2D8-08002B30309D}', r'@%SystemRoot%\system32\shell32.dll,-9216'),  # Computer
    ('{21EC2020-3AEA-1069-A2DD-08002B30309D}', r'@%SystemRoot%\system32\shell32.dll,-4161'),  # Control Panel
    # Network names itself in NetworkExplorer.dll, which is not installed; shell32 has the same name.
    ('{F02C1A0D-BE21-4350-88B0-7367FC96EF3C}', r'@%SystemRoot%\system32\shell32.dll,-9217'),  # Network
    # Help and Support names itself in explorer.exe; %SystemRoot%\explorer.exe is Wine's.
    ('{2559a1f1-21d7-11d4-bdaf-00c04f60b9f0}', r'@C:\w7\w7-explorer.exe,-7021'),
)
# Loaded as native by the shell (and by dllhost.exe, which runs Windows 7's
# thumbnail cache server); Wine's builtin stays every other program's.
NATIVE = ('explorerframe', 'shell32', 'shlwapi', 'propsys', 'shdocvw', 'dui70', 'duser', 'profapi',
          'sndvolsso', 'winbrand', 'shacct', 'thumbcache', 'authui', 'linkinfo')
# Read only for their resources (strings and icons): placed in system32, no override.
RESOURCES = ('imageres', 'sud', 'DeviceCenter')
# Windows 7's interface proxies, under another name: Wine's own actxprxy.dll
# keeps the proxies of Wine's builtin DLLs.
PROXIES = ('actxprxy', 'w7actxprxy', '{C90250F3-4D7D-4991-9B69-A5C5BC1C2AE6}')
# Japanese text in the UI fonts' fallback (Windows 7 links Meiryo UI too).
FONT_LINKS = ('Segoe UI', 'Tahoma', 'MS Shell Dlg', 'MS Shell Dlg 2', 'Microsoft Sans Serif', 'MS UI Gothic')
FONT_SUBSTITUTES = ('MS Shell Dlg', 'MS Shell Dlg 2', 'MS UI Gothic')
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

    def run(self, *args, timeout=300, check=True, env=None):
        return subprocess.run([str(self.wine), *args], env=env or self.env, check=check, timeout=timeout,
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

    def merge(self, dll, mui, target):
        """Copy DLL to TARGET with the resources of its MUI file (Wine reads no .mui files)."""
        helper = ROOT / '.local/bridge/mui-merge.exe'
        if not mui.is_file():
            shutil.copyfile(dll, target)
            return
        self.run(str(helper), windows_path(dll), windows_path(mui), windows_path(target))

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


def windows_path(path):
    return 'Z:' + str(Path(path).resolve()).replace('/', '\\')


def create(assets, dest, home, wine):
    assets, dest, home = assets.resolve(strict=True), dest.resolve(), home.resolve(strict=True)
    if dest.exists():
        raise SystemExit(f'Refusing existing prefix: {dest}')
    for tool in ('mui-merge.exe', 'theme-fonts.exe', 'theme-settings.exe', 'wallpaper-convert.exe'):
        if not (ROOT / '.local/bridge' / tool).is_file():
            raise SystemExit(f'Run bridge-wm/build.sh first: missing {tool}')
    media = json.loads((assets / 'media.json').read_text())
    lang, ui = media['lang'], media['ui_language']
    prefix = Prefix(dest, wine, lang)
    dest.parent.mkdir(parents=True, exist_ok=True)
    prefix.run('wineboot', '-u', timeout=900, env=dict(prefix.env, WINEARCH='win64'))
    # A new prefix's own first update may still be writing the system directories.
    prefix.wait()
    if not (dest / 'drive_c/windows/syswow64').is_dir():
        raise SystemExit('Expected a 64-bit prefix: use the WoW64 build of the patched Wine')
    c = dest / 'drive_c'
    native = c / 'w7/native'
    (native / 'sxs').mkdir(parents=True)
    user = pwd.getpwuid(os.getuid()).pw_name
    try:
        prefix.merge(assets / 'shell/explorer.exe', assets / f'shell/{ui}/explorer.exe.mui', c / 'w7/w7-explorer.exe')
        for name in NATIVE:
            prefix.merge(assets / f'system32/{name}.dll', assets / f'system32/{ui}/{name}.dll.mui',
                         native / f'{name}.dll')
        source, name, _ = PROXIES
        prefix.merge(assets / f'system32/{source}.dll', assets / f'system32/{ui}/{source}.dll.mui',
                     native / f'{name}.dll')
        for name in RESOURCES:
            prefix.merge(assets / f'system32/{name}.dll', assets / f'system32/{ui}/{name}.dll.mui',
                         prefix.system / f'{name}.dll')
        prefix.merge(assets / 'sxs/comctl32.dll', assets / f'sxs/{ui}/comctl32.dll.mui', native / 'sxs/comctl32.dll')
        for font in (assets / 'fonts').iterdir():
            shutil.copyfile(font, c / 'windows/Fonts' / font.name)
        shutil.copytree(assets / 'themes', c / 'windows/Resources/Themes', dirs_exist_ok=True)
        shutil.copytree(assets / 'pictures', c / 'ProgramData/Microsoft/User Account Pictures', dirs_exist_ok=True)
        wallpaper = c / 'windows/Web/Wallpaper/Windows/img0.jpg'
        wallpaper.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(assets / 'web/img0.jpg', wallpaper)
        # Windows 7 shows a JPEG wallpaper from a bitmap it converts it to.
        themes = c / f'users/{user}/AppData/Roaming/Microsoft/Windows/Themes'
        themes.mkdir(parents=True, exist_ok=True)
        prefix.run(str(ROOT / '.local/bridge/wallpaper-convert.exe'), windows_path(wallpaper),
                   windows_path(themes / 'TranscodedWallpaper.bmp'))
        start_menu(prefix, assets, user)
        (dest / 'dosdevices/h:').symlink_to(home)
        prefix.run('regedit', '/s', windows_path(assets / 'registry.reg'), timeout=600)
        prefix.wait()
        settings(prefix, lang)
        prefix.wait()
        appearance(prefix)
    finally:
        prefix.stop()
    prefix.place_files()
    update = (dest / '.update-timestamp').read_text().strip()
    (dest / '.w7-registry-version').write_text(f'{VERSION} {update}\n')
    # The registry settings the session's stage keeps.
    stage_registrations(prefix)
    (dest / MARKER).write_text(json.dumps({'assets': str(assets), 'home': str(home), 'wine': str(prefix.wine),
                                           'lang': lang, 'edition': media['edition']}, indent=2) + '\n')
    print(f'Prefix ready: {dest}')


def start_menu(prefix, assets, user):
    """The Start menu folders' Windows 7 names (desktop.ini), in the shared and the user's menu."""
    roots = ('ProgramData', f'users/{user}/AppData/Roaming')
    for source in sorted((assets / 'startmenu').rglob('desktop.ini')):
        folder = source.parent.relative_to(assets / 'startmenu')
        for root in roots:
            # Wine names the startup folder StartUp, Windows 7 Startup.
            target = prefix.path / 'drive_c' / root / 'Microsoft/Windows'
            for part in folder.parts:
                target = next((p for p in target.iterdir() if p.name.lower() == part.lower()), target / part) \
                    if target.is_dir() else target / part
            if not target.is_dir() or (target / 'desktop.ini').exists():
                continue
            shutil.copyfile(source, target / 'desktop.ini')
            path = 'C:\\' + str(target.relative_to(prefix.path / 'drive_c')).replace('/', '\\')
            # A folder with desktop.ini is a system folder; the file is hidden and
            # system (Wine's attrib applies only the last of several flags).
            prefix.run('attrib', '+s', path, check=False)
            prefix.run('attrib', '+h', path + '\\desktop.ini', check=False)
            prefix.run('attrib', '+s', path + '\\desktop.ini', check=False)


def settings(prefix, lang):
    """DLL overrides, the shell, fonts and Wine settings of a new prefix."""
    _, proxies, clsid = PROXIES
    prefix.reg(rf'HKCR\CLSID\{clsid}\InProcServer32', None, rf'%SystemRoot%\system32\{proxies}.dll', 'REG_EXPAND_SZ')
    builtin = prefix.wine.parent.parent / 'lib/wine/x86_64-windows'
    for name in NATIVE + ('comctl32',):
        prefix.reg(r'HKCU\Software\Wine\AppDefaults\w7-explorer.exe\DllOverrides', name, 'native')
        # dllhost.exe has no manifest for Common Controls 6, so it keeps Wine's comctl32.
        if name != 'comctl32':
            prefix.reg(r'HKCU\Software\Wine\AppDefaults\dllhost.exe\DllOverrides', name, 'native')
        if (builtin / f'{name}.dll').is_file():
            prefix.reg(r'HKCU\Software\Wine\DllOverrides', name, 'builtin')
    for view in ('/reg:32', '/reg:64'):
        prefix.run('reg', 'add', r'HKLM\Software\Microsoft\Windows NT\CurrentVersion\Winlogon', '/v', 'Shell',
                   '/d', 'w7-explorer.exe', '/f', view)
    if lang == 'ja':
        nt = r'HKLM\Software\Microsoft\Windows NT\CurrentVersion'
        for name in FONT_LINKS:
            prefix.reg(nt + r'\FontLink\SystemLink', name, r'MEIRYO.TTC,Meiryo UI\0MEIRYO.TTC,Meiryo', 'REG_MULTI_SZ')
        for name in FONT_SUBSTITUTES:
            prefix.reg(nt + r'\FontSubstitutes', name, 'Meiryo UI')
    # ClearType, Windows 7's default.
    prefix.reg(r'HKCU\Control Panel\Desktop', 'FontSmoothing', '2')
    prefix.reg(r'HKCU\Control Panel\Desktop', 'FontSmoothingType', 2, 'REG_DWORD')
    prefix.reg(r'HKCU\Control Panel\Desktop', 'LogPixels', 96, 'REG_DWORD')
    prefix.reg(r'HKCU\Software\Wine\X11 Driver', 'Managed', 'Y')
    prefix.reg(r'HKCU\Software\Wine\X11 Driver', 'Decorated', 'N')
    # The user's documents are the Linux home (H:), as in the XP session.
    for suffix in ('Shell Folders', 'User Shell Folders'):
        prefix.reg('HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\' + suffix, 'Personal', 'H:\\')


def appearance(prefix):
    """Windows 7 Basic: Aero's visual style, and its fonts saved as the user's."""
    env = dict(prefix.env, WIN4WINE_EXTERNAL_SHELL='1', WIN4WINE_SHELL='win7')
    prefix.run(str(ROOT / '.local/bridge/theme-settings.exe'), 'msstyles',
               r'C:\windows\Resources\Themes\Aero\aero.msstyles', 'NormalColor', 'NormalSize', env=env)
    prefix.wait()
    prefix.run(str(ROOT / '.local/bridge/theme-fonts.exe'), env=env)


def stage_registrations(prefix):
    try:
        prefix.registrations()
    finally:
        prefix.stop()


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
        prefix.registrations()
    finally:
        prefix.stop()
    # After every Wine process of the prefix has ended: a Wine update can go on
    # writing system32 after wineboot returns.
    prefix.place_files()
    current = timestamp.read_text().strip() if timestamp.is_file() else ''
    temp = version.with_suffix('.tmp')
    temp.write_text(f'{VERSION} {current}\n')
    temp.replace(version)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest='command', required=True)
    p = commands.add_parser('create')
    p.add_argument('assets', type=Path)
    p.add_argument('prefix', type=Path)
    p.add_argument('--home', type=Path, default=Path.home())
    p.add_argument('--wine', default=os.environ.get('WINE', 'wine'))
    p = commands.add_parser('stage')
    p.add_argument('prefix', type=Path)
    p.add_argument('--wine')
    args = parser.parse_args()
    if args.command == 'create':
        create(args.assets, args.prefix, args.home, args.wine)
        raise SystemExit(0)
    path = args.prefix.resolve(strict=True)
    if not (path / MARKER).is_file():
        raise SystemExit('Not a Windows 7 shell prefix')
    stage(path, args.wine)
