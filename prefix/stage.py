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
    # msgina supplies XP's own shut down dialog; older extractions lack it.
    for name in ('shell32', 'shdocvw', 'browseui', 'shlwapi', 'linkinfo', 'browselc', 'shdoclc', 'msgina'):
        source = prefix / f'drive_c/xp/native/{name}.dll'
        target = system / f'{name}.dll'
        if name == 'msgina' and not source.is_file():
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
        subprocess.run([str(wine), str(converter), 'Z:' + str(jpeg).replace('/', '\\'),
                        'Z:' + str(bitmap).replace('/', '\\')], env=env, check=False, timeout=120,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


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
