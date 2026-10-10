#!/usr/bin/env python3
"""Move a dedicated 32-bit XP prefix to a new 64-bit (WoW64) prefix.

A WoW64 Wine cannot run a 32-bit prefix. The new prefix is created with
create.py from the same local XP extraction, then receives the old prefix's
user settings (HKCU, which both registry views share) and the files of its
user profiles. The old prefix is only read and stays as it is. Windows
applications installed in the old prefix are not moved; install them again.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import create
import stage


def server_running(prefix):
    """Whether a wineserver serves PREFIX (Wine's socket for its directory)."""
    info = prefix.stat()
    return (Path(f'/tmp/.wine-{os.getuid()}') / f'server-{info.st_dev:x}-{info.st_ino:x}' / 'socket').exists()


def arch(prefix):
    with (prefix / 'system.reg').open(encoding='utf-8', errors='replace') as registry:
        for line in registry:
            if line.startswith('#arch='):
                return line.strip()[6:]
    return None


def merge(source, target, replace=True):
    """Copy SOURCE's tree into TARGET without writing through symbolic links.

    Links are copied as links, except over a real directory. Files replace
    those of the new prefix unless REPLACE is false. Attributes, including
    Wine's DOS attributes in extended attributes, are kept."""
    target.mkdir(exist_ok=True)
    for entry in os.scandir(source):
        dest = target / entry.name
        if entry.is_symlink():
            if dest.is_symlink() or not dest.exists():
                dest.unlink(missing_ok=True)
                os.symlink(os.readlink(entry.path), dest)
        elif entry.is_dir():
            if not dest.is_symlink():
                merge(Path(entry.path), dest, replace)
        elif not dest.is_symlink() and (replace or not dest.exists()):
            shutil.copy2(entry.path, dest)
    shutil.copystat(source, target)


def migrate(old, new, wine, assets=None, home=None):
    old, new = old.resolve(strict=True), new.resolve()
    config = json.loads((old / 'xp-shell.json').read_text())
    if arch(old) != 'win32':
        raise SystemExit(f'Not a 32-bit prefix: {old}')
    if server_running(old):
        raise SystemExit('End the session and the wineserver of the old prefix first')
    assets = Path(assets or config['assets'])
    home = Path(home or config['home'])
    create.create(assets, new, home, wine)
    wine_path = Path(shutil.which(wine) or wine).resolve(strict=True)
    server = str(wine_path.with_name('wineserver'))
    env = dict(os.environ, WINEPREFIX=str(new))
    subprocess.run([server, '-k'], env=env, check=False, timeout=15)
    subprocess.run([server, '-w'], env=env, check=False, timeout=15)
    # HKCU is the same in both views; only the file's architecture differs.
    text = (old / 'user.reg').read_text(encoding='utf-8')
    if '\n#arch=win32\n' not in text:
        raise SystemExit('Unexpected user.reg in the old prefix')
    temp = new / 'user.reg.tmp'
    temp.write_text(text.replace('\n#arch=win32\n', '\n#arch=win64\n', 1), encoding='utf-8')
    temp.replace(new / 'user.reg')
    # User profiles, the shared Start menu and the account pictures.
    for name in ('users', 'ProgramData'):
        merge(old / 'drive_c' / name, new / 'drive_c' / name)
    # Fonts and XP files added to the old prefix by hand, such as msgina.dll.
    merge(old / 'drive_c/windows/Fonts', new / 'drive_c/windows/Fonts', replace=False)
    merge(old / 'drive_c/xp/native', new / 'drive_c/xp/native', replace=False)
    (new / 'xp-shell.json').write_text(json.dumps(dict(config, assets=str(assets), home=str(home), wine=wine,
                                                       migrated_from=str(old)), indent=2) + '\n')
    stage.prepare(new, wine_path)
    stage.stage(new, wine_path)
    stage.stage_media(new, wine_path)
    stage.stage_folders(new, wine_path)
    print(f'Moved to the 64-bit prefix: {new}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('old', type=Path, help='the 32-bit XP prefix (left unchanged)')
    parser.add_argument('new', type=Path, help='the 64-bit XP prefix to create')
    parser.add_argument('--wine', default=os.environ.get('WINE', 'wine'), help='the WoW64 build of the patched Wine')
    parser.add_argument('--assets', type=Path, help='local XP extraction (default: the old prefix\'s)')
    parser.add_argument('--home', type=Path, help='home for H: (default: the old prefix\'s)')
    args = parser.parse_args()
    migrate(args.old, args.new, args.wine, args.assets, args.home)
