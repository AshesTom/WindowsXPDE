#!/usr/bin/env python3
"""Check the ReactOS logon screen's layout (greeter/rosdialog.py) without LightDM.

python3 tests/reactos_greeter.py ASSETS [OUTDIR]   (extract/reactos.py's output)

The dialogs are laid out from msgina.dll's templates; at 1024x768 in English
their client areas and places are those measured on ReactOS 0.4.16 (the
Logon dialog's client is the banner's width, 413 pixels). With OUTDIR the
pages are also written as PNG files, in English and Japanese."""
import os
from pathlib import Path
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'greeter'))
from welcome import fontconfig  # noqa: E402

assets = Path(sys.argv[1]).resolve(strict=True)
out = Path(sys.argv[2]) if len(sys.argv) > 2 else None
os.environ['FONTCONFIG_FILE'] = str(fontconfig(assets / 'fonts', Path(tempfile.mkdtemp(prefix='ros-greeter-'))))
import rosdialog  # noqa: E402

for lang in ('en', 'ja'):
    screen = rosdialog.Screen(rosdialog.Assets(assets, lang), 'user', 'host')
    for page in ('welcome', 'logon', 'shutdown'):
        screen.page = page
        (x, y, w, h), (cx, cy), caption = screen.geometry(1024, 768)
        assert caption, f'{lang} {page} caption'
        assert w - 6 == screen.assets.banner.width, f'{lang} {page}: client width {w - 6}'
        if out:
            out.mkdir(parents=True, exist_ok=True)
            screen.render(1024, 768).write_to_png(str(out / f'{lang}-{page}.png'))
        if lang == 'en':
            # Measured on ReactOS 0.4.16 (1024x768): the frames' outer top-left corners.
            expected = {'welcome': (302, 294), 'logon': (302, 238)}.get(page)
            assert expected is None or (x, y) == expected, f'{page} at {(x, y)}, ReactOS {expected}'
    screen.page = 'logon'
    order = screen.tab_order()
    assert order[:2] == [rosdialog.IDC_USERNAME, rosdialog.IDC_PASSWORD], order
print('PASS: ReactOS logon screen layout (English and Japanese)')
