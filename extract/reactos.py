#!/usr/bin/env python3
"""Download a ReactOS release and extract the files of its shell.

ReactOS is free software (GPL-2.0-or-later as a whole; the shell DLLs used
here are mostly LGPL-2.1-or-later). The release is fetched from the ReactOS
project's GitHub release page and checked against a fixed SHA-256; its
source is the tag named in the manifest. Only files that the ReactOS shell
prefix needs are kept (prefix/reactos.py).
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import urllib.request

VERSION = '0.4.16'
URL = f'https://github.com/reactos/reactos/releases/download/{VERSION}-release/ReactOS-{VERSION}-i386.zip'
SHA256 = 'e5851510cf5b79ef51a76b8155dd5ef7faca1155f44d0a1d41c35b77ece4ce2a'
SOURCE = f'https://github.com/reactos/reactos/tree/{VERSION}-release'
ISO = f'ReactOS-{VERSION}-i386.iso'

# Path inside the ISO (under reactos/) -> path in the output.
FILES = {
    'explorer.exe': 'shell/ros-explorer.exe',
    # The shell family. Wine's own DLLs of these names stay the default for
    # every other program; only ros-explorer.exe loads these.
    **{f'system32/{name}': f'system32/{name}' for name in (
        'shell32.dll', 'browseui.dll', 'shdocvw.dll', 'shlwapi.dll', 'comctl32.dll', 'zipfldr.dll',
        # The Shut Down and Log Off dialogs (Wine has no msgina).
        'msgina.dll')},
    # Common Controls 6; its manifest names the assembly.
    'winsxs/x86_microsoft.windows.common-controls_6595b64144ccf1df_6.0.2600.2982_none_deadbeef/comctl32.dll':
        'sxs/comctl32.dll',
    'winsxs/manifests/x86_microsoft.windows.common-controls_6595b64144ccf1df_6.0.2600.2982_none_deadbeef.manifest':
        'sxs/comctl32.manifest',
    # ReactOS's own fonts (free: Bitstream Vera based Tahoma, the Marlett glyphs
    # of its menus and the CJK fallback of its LiveCD).
    **{f'Fonts/{name}': f'fonts/{name}' for name in (
        'tahoma.ttf', 'tahomabd.ttf', 'Marlett.ttf', 'DroidSansFallback.ttf')},
    # The cursors are read from user32.dll's resources (greeter/cursors.py).
    'system32/user32.dll': 'resources/user32.dll',
    'Web/Wallpaper/Nebula.bmp': 'wallpaper/Nebula.bmp',
    # ReactOS's visual styles, for the shell's optional themes (classic is the default).
    **{f'Resources/Themes/{name}/{name.lower()}.msstyles': f'themes/{name}/{name.lower()}.msstyles'
       for name in ('Lautus', 'Lunar', 'Mizu')},
}


def download(url, target, sha256):
    if target.is_file() and hashlib.sha256(target.read_bytes()).hexdigest() == sha256:
        return
    partial = target.with_name(target.name + '.part')
    print(f'Downloading {url}')
    with urllib.request.urlopen(url) as response, partial.open('wb') as out:
        shutil.copyfileobj(response, out)
    digest = hashlib.sha256(partial.read_bytes()).hexdigest()
    if digest != sha256:
        partial.unlink()
        raise SystemExit(f'Unexpected SHA-256 {digest} for {url}')
    partial.replace(target)


def extract(output, archive):
    output = output.resolve()
    if output.exists():
        raise SystemExit(f'Refusing existing destination: {output}')
    if hashlib.sha256(archive.read_bytes()).hexdigest() != SHA256:
        raise SystemExit(f'{archive} is not ReactOS {VERSION} (SHA-256 differs)')
    with tempfile.TemporaryDirectory(prefix='reactos-extract-') as temp:
        root = Path(temp)
        subprocess.run(['7z', 'x', '-y', f'-o{root}', str(archive), ISO], check=True, stdout=subprocess.DEVNULL)
        names = ['reactos/' + name for name in FILES]
        subprocess.run(['7z', 'x', '-y', f'-o{root / "iso"}', str(root / ISO), *names],
                       check=True, stdout=subprocess.DEVNULL)
        staging = output.with_name(output.name + '.part')
        if staging.exists():
            shutil.rmtree(staging)
        manifest = []
        for name, target in FILES.items():
            source = root / 'iso/reactos' / name
            if not source.is_file():
                raise SystemExit(f'Missing {name} in {ISO}')
            dest = staging / target
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, dest)
            manifest.append({'name': target, 'source': f'{ISO}/reactos/{name}',
                             'sha256': hashlib.sha256(dest.read_bytes()).hexdigest()})
        (staging / 'manifest.json').write_text(json.dumps(
            {'reactos': VERSION, 'url': URL, 'sha256': SHA256, 'source': SOURCE, 'files': manifest},
            indent=2) + '\n')
        staging.rename(output)
    print(f'Extracted {len(manifest)} files of ReactOS {VERSION} to {output}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path, help='new directory for the files')
    parser.add_argument('--zip', type=Path,
                        help=f'a local copy of ReactOS-{VERSION}-i386.zip (otherwise it is downloaded)')
    parser.add_argument('--cache', type=Path, help='where to keep the downloaded zip (default: beside OUTPUT)')
    args = parser.parse_args()
    archive = args.zip
    if archive is None:
        cache = (args.cache or args.output.resolve().parent)
        cache.mkdir(parents=True, exist_ok=True)
        archive = cache / URL.rsplit('/', 1)[1]
        download(URL, archive, SHA256)
    extract(args.output, archive.resolve(strict=True))
