#!/usr/bin/env python3
"""Check greeter/welcome.py's Welcome screen against numbers measured on the reference XP.

python3 tests/greeter_layout.py ASSETS   (extract/extract.py's output, with logonui.exe)

Each state is drawn the way XP showed it (accounts Administrator, builder and
taro with XP's beach and palm tree pictures, 96 DPI) and the bounding box of
one kind of pixel in a region is compared with the box measured on XP's
screen (stage 9). At 120 and 144 DPI (stage 10, XP changed to the DPI from 96
with Display Properties) the boxes may be a pixel off: DirectUI rounds some
parts of its scaled layout differently. Only these numbers are kept, no XP pixels.
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
# dpi, state, size, what, region, kind, box measured on XP at that DPI (within a pixel).
DPI_CHECKS = [
    (144, 'list', (1280, 800), 'logo', (300, 280, 540, 420), 'not background', (300, 280, 511, 419)),
    (144, 'list', (1280, 800), 'help text', (150, 415, 540, 475), 'white', (170, 423, 475, 468)),
    (144, 'list', (1280, 800), 'first picture', (580, 200, 700, 320), 'not background', (594, 219, 699, 302)),
    (144, 'list', (1280, 800), 'first name', (690, 212, 900, 245), 'not background', (693, 220, 825, 236)),
    (144, 'list', (1280, 800), 'third name', (690, 452, 900, 485), 'not background', (693, 463, 730, 476)),
    (144, 'list', (1280, 800), 'turn off label', (80, 680, 300, 740), 'white', (85, 690, 216, 711)),
    (144, 'list', (1280, 800), 'instructions', (760, 680, 1270, 740), 'white', (785, 689, 1212, 723)),
    (144, 'logon', (1280, 800), 'status', (690, 350, 1000, 400), 'status blue', (692, 360, 909, 376)),
    (144, 'logoff', (1280, 800), 'logo', (560, 250, 800, 420), 'not background', (764, 373, 799, 403)),
    (144, 'logoff', (1280, 800), 'status', (500, 420, 800, 470), 'not background', (642, 436, 799, 456)),
    (144, 'password', (1280, 800), 'password box', (680, 400, 945, 450), 'white', (690, 405, 932, 445)),
    (120, 'password', (1280, 800), 'password box', (700, 398, 925, 440), 'white', (713, 398, 914, 436)),
    (120, 'list', (1280, 800), 'logo', (380, 300, 590, 410), 'not background', (431, 318, 564, 398)),
    (120, 'list', (1280, 800), 'help text', (180, 415, 590, 470), 'white', (225, 424, 535, 461)),
    (120, 'list', (1280, 800), 'first picture', (620, 235, 712, 330), 'not background', (635, 250, 704, 319)),
    (120, 'list', (1280, 800), 'first name', (712, 245, 900, 272), 'not background', (717, 250, 829, 263)),
    (120, 'list', (1280, 800), 'third name', (712, 445, 900, 472), 'not background', (717, 452, 749, 463)),
    (120, 'list', (1280, 800), 'turn off label', (60, 700, 300, 740), 'white', (71, 706, 180, 723)),
    (120, 'list', (1280, 800), 'instructions', (800, 700, 1270, 745), 'white', (869, 707, 1225, 734)),
]
# The bottom band's gradient on XP at 1280 pixels wide (row 760).
GRADIENT = {0: (57, 52, 173), 1: (56, 51, 172), 100: (52, 51, 171), 320: (42, 50, 168),
            640: (28, 49, 164), 960: (14, 48, 160), 1100: (8, 48, 158)}


def screen(welcome, assets, state, dpi=96):
    pictures = assets.pictures
    users = [welcome.User('Administrator', picture=pictures / 'beach.bmp'),
             welcome.User('builder', picture=pictures / 'beach.bmp'),
             welcome.User('taro', picture=pictures / 'palmtree.bmp')]
    s = welcome.Screen(assets, users, dpi=dpi)
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
        welcome.use_fonts(args.assets, cache)
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
        for dpi, state, size, what, region, kind, expected in DPI_CHECKS:
            if (state, size, dpi) not in renders:
                renders[state, size, dpi] = pixels(screen(welcome, assets, state, dpi).render(*size))
            got = bbox(renders[state, size, dpi], region, kind)
            ok = got is not None and max(abs(a - b) for a, b in zip(got, expected)) <= 1
            failures += not ok
            print(f'{"ok" if ok else "FAIL"}: {state} {dpi} DPI {what}: {got}, XP {expected}')
        pixel = renders['list', (1280, 800)]
        got = {x: pixel(x, 760) for x in GRADIENT}
        ok = got == GRADIENT
        failures += not ok
        print(f'{"ok" if ok else "FAIL"}: bottom gradient' + ('' if ok else f': {got}, XP {GRADIENT}'))
    print('FAILED' if failures else 'PASSED')
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
