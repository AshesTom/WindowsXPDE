#!/usr/bin/env python3
"""Verify installed-directory extraction with user-provided, already extracted assets."""
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('xp_extract', root / 'extract/extract.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
assets = Path(sys.argv[1]).resolve(strict=True)
with tempfile.TemporaryDirectory(prefix='xp-extract-test-', dir=root / '.local') as name:
    temp = Path(name)
    source = temp / 'Windows'
    source.mkdir()
    shutil.copyfile(assets / 'windows/explorer.exe', source / 'EXPLORER.EXE')
    shutil.copytree(assets / 'system32', source / 'SYSTEM32')
    shutil.copytree(assets / 'fonts', source / 'Fonts')
    shutil.copytree(assets / 'themes', source / 'Resources/Themes')
    manifests = list((assets / 'sxs').rglob('controls.man'))
    sxs = source / 'WinSxS'
    (sxs / 'Manifests').mkdir(parents=True)
    for index, manifest in enumerate(manifests):
        name = f'x86_microsoft.windows.common-controls_fixture_{index}'
        (sxs / name).mkdir()
        shutil.copyfile(manifest, sxs / 'Manifests' / (name + '.manifest'))
        shutil.copyfile(manifest.with_name('comctl32.dll'), sxs / name / 'COMCTL32.DLL')
    module.extract(source, temp / 'output')
    for row in json.loads((temp / 'output/manifest.json').read_text()):
        path = temp / 'output' / row['name']
        assert hashlib.sha256(path.read_bytes()).hexdigest() == row['sha256']
    assert (temp / 'output/themes/Luna/luna.msstyles').read_bytes() == (assets / 'themes/Luna/luna.msstyles').read_bytes()
    assert len(list((temp / 'output/sxs').rglob('controls.man'))) == len(manifests)
    assert (temp / 'output/windows/explorer.exe').read_bytes() == (assets / 'windows/explorer.exe').read_bytes()
    for font in (assets / 'fonts').iterdir():
        if font.is_file():
            assert (temp / 'output/fonts' / font.name).read_bytes() == font.read_bytes()
    for name in ('browselc.dll', 'shdoclc.dll'):
        assert (temp / 'output/system32' / name).read_bytes() == (assets / 'system32' / name).read_bytes()
    try:
        module.extract(source, temp / 'output')
    except SystemExit:
        pass
    else:
        raise AssertionError('Existing output was overwritten')
print('PASS: installed-directory layout, case, fonts, long theme extension, hashes, overwrite refusal')
