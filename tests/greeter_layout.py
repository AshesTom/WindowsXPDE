#!/usr/bin/env python3
"""Check greeter/welcome.py's Welcome screen against numbers measured on the reference XP.

python3 tests/greeter_layout.py ASSETS   (extract/extract.py's output, with logonui.exe)

Each state is drawn the way XP showed it (accounts Administrator, builder and
taro with XP's beach and palm tree pictures, 96 DPI) and the bounding box of
one kind of pixel in a region is compared with the box measured on XP's
screen (docs/status.md, stage 9). Only these numbers are kept, no XP pixels.
Needs python3-gi, gir1.2-pango-1.0 and python3-gi-cairo (and GdkPixbuf).
"""
import argparse
import os
from pathlib import Path
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'greeter'))

BACKGROUND = (90, 126, 220)
KINDS = {
    'not background': lambda p: p != BACKGROUND,
    'white': lambda p: min(p) > 230,
    'dark': lambda p: max(p) < 90,
    'status blue': lambda p: p[0] < 40 and 140 < p[2] < 170 and p[1] < 70,
    'green': lambda p: p[1] > 150 and p[0] < 120,
    'red': lambda p: p[0] > 180 and p[1] < 120,
    'yellow': lambda p: p == (255, 255, 225),
    'black': lambda p: max(p) < 40,
}
# state, size, what, region (x0, y0, x1, y1), kind, box measured on XP (inclusive).
CHECKS = [
    ('list', (1280, 800), 'logo', (400, 300, 636, 425), 'not background', (484, 336, 617, 416)),
    ('list', (1280, 800), 'help text', (250, 425, 630, 470), 'not background', (310, 437, 589, 451)),
    ('list', (1280, 800), 'first picture', (660, 270, 735, 340), 'not background', (675, 282, 729, 336)),
    ('list', (1280, 800), 'first name', (735, 276, 900, 300), 'not background', (739, 282, 829, 292)),
    ('list', (1280, 800), 'third name', (735, 436, 900, 460), 'not background', (739, 444, 765, 452)),
    ('list', (1280, 800), 'turn off label', (50, 715, 200, 760), 'white', (55, 726, 142, 740)),
    ('list', (1280, 800), 'instructions', (900, 715, 1270, 760), 'white', (951, 726, 1239, 748)),
    ('list', (1024, 768), 'logo', (280, 290, 508, 410), 'not background', (356, 320, 489, 400)),
    ('list', (1024, 768), 'first picture', (530, 250, 605, 330), 'not background', (547, 266, 601, 320)),
    ('list', (800, 600), 'help text', (40, 325, 395, 360), 'not background', (70, 337, 349, 351)),
    ('list', (800, 600), 'first picture', (420, 170, 495, 240), 'not background', (435, 182, 489, 236)),
    ('password', (1280, 800), 'password box', (730, 400, 903, 436), 'white', (736, 403, 896, 429)),
    ('password', (1280, 800), 'go button', (900, 395, 960, 440), 'green', (910, 405, 930, 424)),
    ('password', (1280, 800), 'password label', (736, 380, 900, 400), 'white', (741, 388, 819, 398)),
    ('wrong', (1280, 800), 'balloon', (780, 410, 1100, 520), 'yellow', (801, 420, 1064, 498)),
    ('wrong', (1280, 800), 'balloon title', (830, 440, 1060, 462), 'dark', (836, 447, 985, 457)),
    ('wrong', (1280, 800), 'balloon text', (803, 462, 1062, 497), 'dark', (812, 466, 1048, 488)),
    ('logon', (1280, 800), 'picture', (660, 340, 734, 430), 'not background', (675, 362, 729, 416)),
    ('logon', (1280, 800), 'status', (736, 374, 900, 392), 'status blue', (739, 375, 886, 385)),
    ('logoff', (1280, 800), 'logo', (600, 320, 830, 425), 'not background', (678, 336, 811, 416)),
    ('logoff', (1280, 800), 'status', (600, 430, 830, 456), 'not background', (641, 437, 787, 451)),
    ('dialog', (800, 600), 'frame', (210, 130, 600, 330), 'black', (218, 139, 583, 322)),
    ('dialog', (800, 600), 'cancel face', (480, 285, 590, 325), 'white', (503, 297, 568, 310)),
    ('dialog', (800, 600), 'turn off button', (330, 190, 420, 240), 'red', (383, 215, 419, 239)),
    ('dialog', (800, 600), 'labels', (330, 240, 560, 262), 'white', (362, 240, 528, 261)),
    ('tooltip', (1280, 800), 'balloon', (620, 330, 960, 410), 'yellow', (626, 330, 946, 401)),
    ('tooltip', (1280, 800), 'title', (628, 334, 944, 360), 'dark', (637, 342, 713, 353)),
    ('tooltip', (1280, 800), 'text', (628, 362, 944, 400), 'dark', (637, 369, 933, 391)),
]
# The bottom band's gradient on XP at 1280 pixels wide (row 760).
GRADIENT = {0: (57, 52, 173), 1: (56, 51, 172), 100: (52, 51, 171), 320: (42, 50, 168),
            640: (28, 49, 164), 960: (14, 48, 160), 1100: (8, 48, 158)}


def screen(welcome, assets, state):
    pictures = assets.pictures
    users = [welcome.User('Administrator', picture=pictures / 'beach.bmp'),
             welcome.User('builder', picture=pictures / 'beach.bmp'),
             welcome.User('taro', picture=pictures / 'palmtree.bmp')]
    s = welcome.Screen(assets, users)
    s.caret = False
    if state in ('password', 'wrong', 'tooltip'):
        s.selected = 1
    s.error = state == 'wrong'
    if state == 'logon':
        s.mode, s.selected, s.status = 'logon', 2, assets.string('winlogon', 1679)
    if state == 'logoff':
        s.mode, s.status = 'status', assets.string('winlogon', 1687)
    if state in ('dialog', 'tooltip'):
        s.dialog = True
    if state == 'tooltip':
        s.dialog_hover, s.tooltip = 'off', ('off', 641, 296)
    return s


def pixels(surface):
    data, stride = surface.get_data(), surface.get_stride()
    return lambda x, y: (data[y * stride + 4 * x + 2], data[y * stride + 4 * x + 1], data[y * stride + 4 * x])


def bbox(pixel, region, kind):
    test = KINDS[kind]
    x0, y0, x1, y1 = region
    found = [(x, y) for y in range(y0, y1) for x in range(x0, x1) if test(pixel(x, y))]
    if not found:
        return None
    xs, ys = [p[0] for p in found], [p[1] for p in found]
    return min(xs), min(ys), max(xs), max(ys)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('assets', type=Path)
    args = parser.parse_args()
    import welcome
    with tempfile.TemporaryDirectory(prefix='greeter-layout-') as cache:
        os.environ['FONTCONFIG_FILE'] = str(welcome.fontconfig(args.assets / 'fonts', Path(cache)))
        assets = welcome.Assets(args.assets)
        failures = 0
        renders = {}
        for state, size, what, region, kind, expected in CHECKS:
            if (state, size) not in renders:
                renders[state, size] = pixels(screen(welcome, assets, state).render(*size))
            got = bbox(renders[state, size], region, kind)
            ok = got == expected
            failures += not ok
            print(f'{"ok" if ok else "FAIL"}: {state} {size[0]}x{size[1]} {what}: {got}'
                  + ('' if ok else f', XP {expected}'))
        pixel = renders['list', (1280, 800)]
        got = {x: pixel(x, 760) for x in GRADIENT}
        ok = got == GRADIENT
        failures += not ok
        print(f'{"ok" if ok else "FAIL"}: bottom gradient' + ('' if ok else f': {got}, XP {GRADIENT}'))
    print('FAILED' if failures else 'PASSED')
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
