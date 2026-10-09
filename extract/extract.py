#!/usr/bin/env python3
"""Extract only from a user supplied local XP SP3 ISO or Windows directory."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import xml.etree.ElementTree as ET

FILES = {
    'explorer.exe': 'windows',
    **dict.fromkeys(('shell32.dll', 'browseui.dll', 'shdocvw.dll', 'shlwapi.dll',
                     'browselc.dll', 'shdoclc.dll', 'comctl32.dll', 'linkinfo.dll', 'uxtheme.dll', 'shellstyle.dll', 'explorer.exe.manifest'), 'system32'),
    **dict.fromkeys(('tahoma.ttf', 'tahomabd.ttf', 'msgothic.ttc'), 'fonts'),
    'luna.msstyles': 'themes/Luna', 'luna.theme': 'themes',
    'blue_ss.dll': 'themes/Luna/Shell/NormalColor',
    'home_ss.dll': 'themes/Luna/Shell/Homestead',
    'metal_ss.dll': 'themes/Luna/Shell/Metallic',
}

def run(*args):
    subprocess.run(args, check=True, stdout=subprocess.DEVNULL)

def extract(source, output):
    source, output = source.resolve(strict=True), output.resolve()
    if output.exists():
        raise SystemExit(f'Refusing existing destination: {output}')
    with tempfile.TemporaryDirectory(prefix='xp-extract-') as temp:
        root = Path(temp)
        if source.is_file():
            media = root / 'media'
            run('7z', 'x', '-y', f'-o{media}', str(source))
        else:
            media = source
        paths = sorted(p for p in media.rglob('*') if p.is_file())
        output.mkdir(parents=True)
        manifest = []
        for name, group in FILES.items():
            stem, extension = name.rsplit('.', 1)
            compressed = stem + '.' + extension[:2] + '_'
            matches = [p for p in paths if p.name.lower() in (name, compressed)]
            # System32 controls v5 must not accidentally be replaced with SxS v6.
            matches.sort(key=lambda p: (any(s.lower() in ('winsxs', 'asms') for s in p.parts), len(p.parts)))
            if not matches:
                continue
            original = matches[0]
            target = output / group / ('shellstyle.dll' if name.endswith('_ss.dll') else name)
            target.parent.mkdir(parents=True, exist_ok=True)
            if original.name.lower() == compressed:
                stage = root / 'expanded'
                if stage.exists():
                    shutil.rmtree(stage)
                stage.mkdir()
                run('cabextract', '-q', '-d', str(stage), str(original))
                expanded = list(stage.iterdir())
                if len(expanded) != 1:
                    raise RuntimeError(f'Expected one expanded file: {original}')
                shutil.copyfile(expanded[0], target)
            else:
                shutil.copyfile(original, target)
            manifest.append({'name': str(target.relative_to(output)),
                             'source': str(original.relative_to(media)),
                             'sha256': hashlib.sha256(target.read_bytes()).hexdigest()})
        for original in paths:
            if original.name.lower() != 'shellstyle.dll':
                continue
            parent = original.parent.name.lower()
            if parent not in ('normalcolor', 'homestead', 'metallic'):
                continue
            target = output / 'themes/Luna/Shell' / parent / 'shellstyle.dll'
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(original, target)
            manifest.append({'name': str(target.relative_to(output)),
                             'source': str(original.relative_to(media)),
                             'sha256': hashlib.sha256(target.read_bytes()).hexdigest()})
        # Keep SxS material separate, including its assembly identity manifest.
        for p in paths:
            if p.name.lower() not in ('comctl32.dll', 'comctl32.dl_', 'controls.man'):
                continue
            if not any(s.lower() in ('asms', 'winsxs') for s in p.parts):
                continue
            dest = output / 'sxs' / str(p.relative_to(media)).lower()
            if p.name.lower() == 'comctl32.dl_':
                dest = dest.with_name('comctl32.dll')
                stage = root / 'sxs-expanded'
                if stage.exists():
                    shutil.rmtree(stage)
                stage.mkdir()
                run('cabextract', '-q', '-d', str(stage), str(p))
                source_file = next(stage.iterdir())
            else:
                source_file = p
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source_file, dest)
            manifest.append({'name': str(dest.relative_to(output)), 'source': str(p.relative_to(media)),
                             'sha256': hashlib.sha256(dest.read_bytes()).hexdigest()})
        # Installed XP keeps manifests in WinSxS/Manifests, separate from DLLs.
        for original in paths:
            if original.suffix.lower() != '.manifest' or original.parent.name.lower() != 'manifests':
                continue
            if not any(part.lower() == 'winsxs' for part in original.parts):
                continue
            try:
                identity = ET.parse(original).getroot().find('{urn:schemas-microsoft-com:asm.v1}assemblyIdentity')
            except ET.ParseError:
                continue
            if identity is None or identity.get('name', '').lower() != 'microsoft.windows.common-controls':
                continue
            # Match the actual sibling assembly folder without inferring its hash.
            assembly_name = original.stem.lower()
            siblings = [folder for folder in original.parent.parent.iterdir()
                        if folder.is_dir() and folder.name.lower() == assembly_name]
            if not siblings:
                continue
            target = output / 'sxs' / str(siblings[0].relative_to(media)).lower() / 'controls.man'
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(original, target)
            manifest.append({'name': str(target.relative_to(output)),
                             'source': str(original.relative_to(media)),
                             'sha256': hashlib.sha256(target.read_bytes()).hexdigest()})
        required = ['windows/explorer.exe'] + ['system32/' + n for n in ('shell32.dll', 'browseui.dll', 'shdocvw.dll', 'shlwapi.dll')]
        missing = [n for n in required if not (output / n).is_file()]
        if missing:
            raise SystemExit('Incomplete XP assets: ' + ', '.join(missing))
        # Read PE header architecture; no disassembly.
        pe = (output / 'windows/explorer.exe').read_bytes()
        offset = int.from_bytes(pe[60:64], 'little')
        if pe[offset:offset+6] != b'PE\0\0\x4c\x01':
            raise SystemExit('Only x86 XP media is supported')
        (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
        print(f'Extracted {len(manifest)} files to {output}')

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    extract(args.source, args.output)
