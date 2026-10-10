#!/usr/bin/env python3
"""Restore native shell DLLs atomically after Wine prefix updates."""
import filecmp
import argparse
import json
import os
import pwd
from pathlib import Path
import shutil
import subprocess
import tempfile
import registry

def prepare(prefix, wine):
    inf = wine.parent.parent / 'share/wine/wine.inf'
    timestamp = prefix / '.update-timestamp'
    current = timestamp.read_text().strip() if timestamp.is_file() else ''
    version = prefix / '.xp-registry-version'
    wine_current = inf.is_file() and current == str(int(inf.stat().st_mtime))
    if wine_current and version.is_file() and version.read_text().strip() == str(registry.VERSION):
        return
    env = dict(os.environ, WINEPREFIX=str(prefix), WINEARCH='win32',
               WINEDLLOVERRIDES='explorer,shell32,shdocvw,browseui,shlwapi,comctl32,uxtheme=b;mscoree,mshtml=',
               WINEDEBUG=os.environ.get('WINEDEBUG', '-all'))
    def run(*args):
        subprocess.run([str(wine), *args], env=env, check=True, timeout=120,
                       stdout=subprocess.DEVNULL)
    def exists(key, name):
        return subprocess.run([str(wine), 'reg', 'query', key, '/v', name], env=env,
                              stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                              timeout=120).returncode == 0
    # Wine upgrades may replace both DLL images and shell COM registrations.
    # Complete that update before restoring the native shell profile.
    print('Updating dedicated prefix Wine/XP registry profile', flush=True)
    try:
        if not wine_current:
            run('wineboot', '-u')
        registry.install(run, exists)
        temp = version.with_suffix('.tmp')
        temp.write_text(str(registry.VERSION) + '\n')
        temp.replace(version)
    finally:
        server = str(wine.with_name('wineserver'))
        subprocess.run([server, '-k'], env=env, check=True, timeout=15)
        subprocess.run([server, '-w'], env=env, check=True, timeout=15)

def stage(prefix, wine=None):
    if not (prefix / 'xp-shell.json').is_file():
        raise SystemExit('Not a dedicated XP prefix')
    for name in ('browselc', 'shdoclc'):
        if not (prefix / f'drive_c/xp/native/{name}.dll').is_file():
            raise SystemExit(f'Missing {name}.dll: re-extract local XP media and add it to C:\\xp\\native')
    system = prefix / 'drive_c/windows/system32'
    if wine:
        source = wine.parent.parent / 'lib/wine/i386-windows/win4wine-controls.dll'
        if not source.is_file():
            raise SystemExit('Rebuild patched Wine: missing self-built themed controls module')
        target = system / 'win4wine-controls.dll'
        if not target.is_file() or not filecmp.cmp(source, target, shallow=False):
            with tempfile.NamedTemporaryFile(dir=system, prefix='.wine-controls-', delete=False) as temp:
                path = Path(temp.name)
            try:
                shutil.copyfile(source, path)
                os.replace(path, target)
            finally:
                path.unlink(missing_ok=True)
    # msgina supplies XP's own shut down dialog and mydocs the icons of My
    # Documents and My Pictures; older extractions lack them.
    for name in ('shell32', 'shdocvw', 'browseui', 'shlwapi', 'linkinfo', 'browselc', 'shdoclc', 'msgina', 'mydocs'):
        source = prefix / f'drive_c/xp/native/{name}.dll'
        target = system / f'{name}.dll'
        if name in ('msgina', 'mydocs') and not source.is_file():
            continue
        if target.is_file() and filecmp.cmp(source, target, shallow=False):
            continue
        # Never truncate an image that a running process may have mapped.
        with tempfile.NamedTemporaryFile(dir=system, prefix='.xp-stage-', delete=False) as temp:
            path = Path(temp.name)
        try:
            shutil.copyfile(source, path)
            os.replace(path, target)
        finally:
            path.unlink(missing_ok=True)

def stage_media(prefix, wine=None):
    """Copy XP's default wallpaper and user picture from the local extraction.

    The files are copied unchanged to the places XP uses; existing files,
    including a user's own picture, are never replaced.
    """
    try:
        assets = Path(json.loads((prefix / 'xp-shell.json').read_text())['assets'])
    except (OSError, ValueError, KeyError):
        return
    user = pwd.getpwuid(os.getuid()).pw_name
    pictures = prefix / 'drive_c/ProgramData/Microsoft/User Account Pictures'
    for source, target in ((assets / 'web/wallpaper/bliss.jpg', prefix / 'drive_c/windows/Web/Wallpaper/Bliss.jpg'),
                           (assets / 'pictures/beach.bmp', pictures / 'Default Pictures/beach.bmp'),
                           (assets / 'pictures/beach.bmp', pictures / f'{user}.bmp')):
        if source.is_file() and not target.exists():
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)
    # XP converts a JPEG wallpaper to Wallpaper1.bmp; without it, XP's desktop
    # switches to the HTML Active Desktop that this shell does not provide.
    jpeg = prefix / 'drive_c/windows/Web/Wallpaper/Bliss.jpg'
    bitmap = prefix / f'drive_c/users/{user}/AppData/Local/Microsoft/Wallpaper1.bmp'
    converter = Path(__file__).resolve().parents[1] / '.local/bridge/wallpaper-convert.exe'
    if wine and jpeg.is_file() and converter.is_file() and not bitmap.exists():
        bitmap.parent.mkdir(parents=True, exist_ok=True)
        env = dict(os.environ, WINEPREFIX=str(prefix), WINEARCH='win32', WINEDEBUG='-all')
        try:
            subprocess.run([str(wine), str(converter), 'Z:' + str(jpeg).replace('/', '\\'),
                            'Z:' + str(bitmap).replace('/', '\\')], env=env, check=False, timeout=120,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        finally:
            # Do not leave a desktop started without the external shell flag.
            server = str(wine.with_name('wineserver'))
            subprocess.run([server, '-k'], env=env, check=False, timeout=15)
            subprocess.run([server, '-w'], env=env, check=False, timeout=15)


# XP's default user marks its special folders in desktop.ini, as read on the
# reference machine: [DeleteOnCopy] names a folder by its CSIDL while it keeps
# its created name, and [.ShellClassInfo] gives its icon and tip.
SPECIAL_FOLDERS = (
    ('Pictures', [('DeleteOnCopy', [('Owner', None), ('Personalized', '39'), ('PersonalizedName', None)]),
                  ('.ShellClassInfo', [('InfoTip', '@Shell32.dll,-12688'),
                                       ('IconFile', r'%SystemRoot%\system32\mydocs.dll'), ('IconIndex', '-101')])]),
    ('Music', [('DeleteOnCopy', [('Owner', None), ('Personalized', '13'), ('PersonalizedName', None)]),
               ('.ShellClassInfo', [('InfoTip', '@Shell32.dll,-12689'),
                                    ('IconFile', r'%SystemRoot%\system32\SHELL32.dll'), ('IconIndex', '-237')])]),
    ('AppData/Roaming/Microsoft/Windows/Recent',
     [('.ShellClassInfo', [('InfoTip', '@Shell32.dll,-12692'), ('IconFile', r'%SystemRoot%\system32\SHELL32.dll'),
                           ('IconIndex', '-21'), ('LocalizedResourceName', '@shell32.dll,-12691')])]),
    ('Favorites',
     [('.ShellClassInfo', [('IconFile', r'%SystemRoot%\system32\SHELL32.dll'), ('IconIndex', '-173'),
                           ('LocalizedResourceName', '@shell32.dll,-12693')])]),
)

def stage_folders(prefix, wine):
    """Give the prefix's own special folders XP's names and icons.

    Only real directories inside the prefix are marked, never a link to the
    user's home, and an existing desktop.ini is kept.
    """
    user = pwd.getpwuid(os.getuid()).pw_name
    profile = prefix / f'drive_c/users/{user}'
    mydocs = (prefix / 'drive_c/windows/system32/mydocs.dll').is_file()
    marked = []
    for relative, sections in SPECIAL_FOLDERS:
        folder = profile / relative
        ini = folder / 'desktop.ini'
        if not folder.is_dir() or folder.is_symlink() or ini.exists() or ini.is_symlink():
            continue
        if any(value.endswith('mydocs.dll') for _, entries in sections for _, value in entries if value) and not mydocs:
            continue
        lines = []
        for section, entries in sections:
            lines.append(f'[{section}]')
            for key, value in entries:
                lines.append(f'{key}={value or (user if key == "Owner" else folder.name)}')
        ini.write_text('\r\n'.join(lines) + '\r\n', encoding='ascii')
        marked.append('C:\\users\\' + user + '\\' + relative.replace('/', '\\'))
    icon = prefix / '.xp-mydocs-icon'
    if not marked and (not mydocs or icon.is_file()):
        return
    env = dict(os.environ, WINEPREFIX=str(prefix), WINEARCH='win32', WINEDEBUG='-all')
    try:
        # The shell reads desktop.ini only in system or read-only folders.
        for folder in marked:
            subprocess.run([str(wine), 'attrib', '+s', folder], env=env, check=False, timeout=120,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            subprocess.run([str(wine), 'attrib', '+h', '+s', folder + '\\desktop.ini'], env=env, check=False,
                           timeout=120, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        if mydocs:
            subprocess.run([str(wine), 'reg', 'add',
                            r'HKCR\CLSID\{450D8FBA-AD25-11D0-98A8-0800361B1103}\DefaultIcon', '/ve', '/t',
                            'REG_EXPAND_SZ', '/d', r'%SystemRoot%\system32\mydocs.dll,0', '/f'],
                           env=env, check=False, timeout=120, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            icon.write_text('1\n')
    finally:
        server = str(wine.with_name('wineserver'))
        subprocess.run([server, '-k'], env=env, check=False, timeout=15)
        subprocess.run([server, '-w'], env=env, check=False, timeout=15)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('prefix', type=Path)
    parser.add_argument('--wine', type=Path)
    args = parser.parse_args()
    prefix = args.prefix.resolve(strict=True)
    if not (prefix / 'xp-shell.json').is_file():
        raise SystemExit('Not a dedicated XP prefix')
    if args.wine:
        prepare(prefix, args.wine.resolve(strict=True))
    stage(prefix, args.wine.resolve(strict=True) if args.wine else None)
    stage_media(prefix, args.wine.resolve(strict=True) if args.wine else None)
    if args.wine:
        stage_folders(prefix, args.wine.resolve(strict=True))
