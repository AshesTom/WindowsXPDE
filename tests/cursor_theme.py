#!/usr/bin/env python3
"""Check greeter/cursors.py: the X cursor theme made from the user's XP user32.dll.

python3 tests/cursor_theme.py ASSETS   (extract/extract.py's output, with resources/user32.dll)

Every name that winex11 asks for Windows' standard cursors
(dlls/winex11.drv/mouse.c) must be in the theme; the arrow's hot spot is its
tip; the shadow is at most 0.318 of black, as measured on the reference XP,
and the inverting I-beam has none but a white edge.
"""
import argparse
from pathlib import Path
import struct
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'greeter'))
import cursors  # noqa: E402

WINEX11 = ('left_ptr', 'xterm', 'watch', 'cross', 'center_ptr', 'fleur', 'top_left_corner', 'top_right_corner',
           'h_double_arrow', 'v_double_arrow', 'not-allowed', 'hand2', 'left_ptr_watch', 'question_arrow')
TOOLKITS = ('default', 'text', 'pointer', 'wait', 'progress', 'help', 'crosshair', 'move', 'ew-resize',
            'ns-resize', 'nwse-resize', 'nesw-resize')


def read(path):
    data = path.read_bytes()
    magic, header, version, count = struct.unpack_from('<4sIII', data)
    assert magic == b'Xcur' and count == 1
    _, kind, nominal, _, width, height, hot_x, hot_y, _ = struct.unpack_from('<9I', data, 28)
    pixels = struct.unpack_from(f'<{width * height}I', data, 28 + 36)
    return nominal, width, height, hot_x, hot_y, pixels


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('assets', type=Path)
    args = parser.parse_args()
    failures = []

    def check(ok, what):
        print(('ok: ' if ok else 'FAIL: ') + what)
        if not ok:
            failures.append(what)
    with tempfile.TemporaryDirectory(prefix='cursor-theme-') as temp:
        theme = Path(temp) / 'win4wine-xp'
        cursors.write_theme(args.assets / 'resources/user32.dll', theme)
        for name in WINEX11 + TOOLKITS:
            check((theme / 'cursors' / name).is_file(), f'cursor {name}')
        nominal, width, height, hot_x, hot_y, pixels = read(theme / 'cursors/left_ptr')
        check((nominal, width, height) == (32, 40, 40), f'arrow size: nominal {nominal}, image {width}x{height}')
        tip = pixels[hot_y * width + hot_x]
        check(tip >> 24 == 255 and tip & 0xffffff == 0, f'arrow tip at the hot spot ({hot_x},{hot_y}): {tip:08x}')
        alphas = [p >> 24 for p in pixels]
        shadow = [a for a, p in zip(alphas, pixels) if 0 < a < 255 and p & 0xffffff == 0]
        # Under the arrow's body the blurred shadow is full: 0.318 of black, as on XP (32%).
        check(bool(shadow) and 75 <= max(shadow) <= 82,
              f'arrow shadow: {len(shadow)} pixels, darkest alpha {max(shadow) if shadow else None}')
        _, width, height, _, _, pixels = read(theme / 'cursors/xterm')
        partial = [p for p in pixels if 0 < p >> 24 < 255]
        check(not partial, f'I-beam (inverting) without shadow: {len(partial)} partly transparent pixels')
        black = sum(1 for p in pixels if p == 0xff000000)
        white = sum(1 for p in pixels if p == 0xffffffff)
        check(black and white, f'I-beam black with a white edge: {black} black, {white} white pixels')
    print('FAILED' if failures else 'PASSED')
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
