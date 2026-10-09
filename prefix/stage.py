#!/usr/bin/env python3
"""Restore native shell DLLs atomically after Wine prefix updates."""
import filecmp
import argparse
import os
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
    # Wine upgrades may replace both DLL images and shell COM registrations.
    # Complete that update before restoring the native shell profile.
    print('Updating dedicated prefix Wine/XP registry profile', flush=True)
    try:
        if not wine_current:
            run('wineboot', '-u')
        registry.install(run)
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
    for name in ('shell32', 'shdocvw', 'browseui', 'shlwapi', 'linkinfo', 'browselc', 'shdoclc'):
        source = prefix / f'drive_c/xp/native/{name}.dll'
        target = system / f'{name}.dll'
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
