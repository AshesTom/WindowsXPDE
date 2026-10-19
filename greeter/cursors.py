#!/usr/bin/env python3
"""Make an X cursor theme from the cursors in the user's XP user32.dll.

python3 greeter/cursors.py USER32.DLL THEME_DIRECTORY [--reactos]

With --reactos the user32.dll is ReactOS's: its cursors get no shadow, as in
ReactOS and Windows 2000. (Windows 7's Aero cursors are on the win7 branch.)

Wine shows Windows' standard cursors through the X cursor theme (winex11
maps them to names such as left_ptr), Linux applications and the greeter use
the theme too; with this theme they all show XP's cursors. XP draws a
shadow under the cursor; it is put into the images (measured on the
reference XP). Pixels that XP inverts are drawn black, which X cursors can
show. Only the resources are read (greeter/xpres.py).
"""
import os
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from xpres import Resources

RT_CURSOR, RT_GROUP_CURSOR = 1, 12
# XP user32's cursor groups and the X cursor names that stand for them: the
# names winex11 asks for (dlls/winex11.drv/mouse.c) first, then the names
# that toolkits use (freedesktop cursor-spec and the old X names).
NAMES = {
    100: ('left_ptr', 'default', 'arrow', 'top_left_arrow', 'X_cursor', 'right_ptr', 'draft_large',
          'draft_small'),
    101: ('xterm', 'text', 'ibeam', 'vertical-text'),
    102: ('watch', 'wait'),
    103: ('cross', 'crosshair', 'tcross', 'cell'),
    104: ('center_ptr', 'up-arrow', 'sb_up_arrow'),
    105: ('top_left_corner', 'nw-resize', 'se-resize', 'nwse-resize', 'size_fdiag', 'bottom_right_corner'),
    106: ('top_right_corner', 'ne-resize', 'sw-resize', 'nesw-resize', 'size_bdiag', 'bottom_left_corner'),
    107: ('h_double_arrow', 'size_hor', 'ew-resize', 'e-resize', 'w-resize', 'sb_h_double_arrow',
          'col-resize', 'left_side', 'right_side'),
    108: ('v_double_arrow', 'size_ver', 'ns-resize', 'n-resize', 's-resize', 'sb_v_double_arrow',
          'row-resize', 'top_side', 'bottom_side'),
    109: ('fleur', 'size_all', 'move', 'all-scroll', 'grabbing', 'dnd-move'),
    110: ('not-allowed', 'forbidden', 'no-drop', 'circle', 'crossed_circle', 'dnd-none'),
    111: ('left_ptr_watch', 'progress', 'half-busy'),
    112: ('question_arrow', 'help', 'whats_this', 'left_ptr_help'),
    114: ('hand2', 'pointer', 'pointing-hand', 'hand', 'hand1', 'openhand', 'grab'),
}
# XP's cursor shadow (SPI_GETCURSORSHADOW), fitted on the reference XP's arrow
# (largest error 0.003 in alpha): the cursor's opaque shape, moved 3px right and
# 1px down, blurred by [1, 2, 3, 2, 1] both ways and darkening by 0.318 of it.
SHADOW_OFFSET = (3, 1)
SHADOW_KERNEL = (1, 2, 3, 2, 1)
SHADOW_STRENGTH = 0.318
SIZE = 40  # the image, with room for the shadow
# XP's cursors above 144 DPI (SM_CXCURSOR 64): each pixel doubled, the
# shadow as at 32 (measured at 192 DPI, largest error 0.002 in alpha).
LARGE, LARGE_SIZE = 64, 72


def group_entries(data):
    _, kind, count = struct.unpack_from('<HHH', data)
    for i in range(count):
        width, height, planes, bits, size, number = struct.unpack_from('<HHHHIH', data, 6 + 14 * i)
        yield width, height // 2, bits, number


INVERT = (0, 0, 0, 254)


def decode(data):
    """A cursor resource: (width, height, hot x, hot y, rows of (r, g, b, a), whether XP shades it)."""
    hot_x, hot_y = struct.unpack_from('<HH', data)
    dib = data[4:]
    header, width, height, _, bits, _, _, _, _, used = struct.unpack_from('<IiiHHIIiiI', dib)
    height //= 2
    colors = used or (1 << bits if bits <= 8 else 0)
    palette = [struct.unpack_from('<BBB', dib, header + 4 * i)[::-1] for i in range(colors)]
    xor = dib[header + 4 * colors:]
    xor_stride = (width * bits + 31) // 32 * 4
    and_mask = xor[xor_stride * height:]
    and_stride = (width + 31) // 32 * 4
    rows = []
    for y in range(height):
        line = xor[(height - 1 - y) * xor_stride:]
        mask = and_mask[(height - 1 - y) * and_stride:]
        row = []
        for x in range(width):
            transparent = mask[x // 8] >> (7 - x % 8) & 1
            if bits == 32:
                b, g, r, a = line[4 * x:4 * x + 4]
                row.append((r, g, b, a))
                continue
            if bits == 24:
                color = tuple(line[3 * x:3 * x + 3][::-1])
            else:
                index = line[x * bits // 8] >> (8 - bits - x * bits % 8) & ((1 << bits) - 1)
                color = palette[index]
            if transparent:
                # Mask 1 with a colour inverts the screen, which X cursors cannot do.
                row.append(INVERT if color != (0, 0, 0) else (0, 0, 0, 0))
            else:
                row.append(color + (255,))
        rows.append(row)
    if any(INVERT in row for row in rows):
        # Black with a white edge: the same as XP's inversion on white, still seen on black.
        edge = {(x + i, y + j) for y, row in enumerate(rows) for x, p in enumerate(row) if p is INVERT
                for i in (-1, 0, 1) for j in (-1, 0, 1)}
        rows = [[(0, 0, 0, 255) if p is INVERT else
                 (255, 255, 255, 255) if p[3] == 0 and (x, y) in edge else p
                 for x, p in enumerate(row)] for y, row in enumerate(rows)]
        return width, height, hot_x, hot_y, rows, False
    return width, height, hot_x, hot_y, rows, True


def doubled(width, height, rows):
    """The cursor with each pixel doubled, as XP shows it at 64."""
    return 2 * width, 2 * height, [[row[x // 2] for x in range(2 * width)] for row in rows for _ in (0, 1)]


def with_shadow(width, height, rows, shadowed=True, size=SIZE):
    """The cursor over its soft shadow, in a SIZE x SIZE image (straight alpha)."""
    dx, dy = SHADOW_OFFSET
    reach = len(SHADOW_KERNEL) // 2
    total = sum(SHADOW_KERNEL) ** 2
    mask = [[rows[y][x][3] / 255 if 0 <= y < height and 0 <= x < width else 0 for x in range(size)]
            for y in range(size)]
    out = []
    for y in range(size):
        row = []
        for x in range(size):
            shadow = 0
            for j, wy in enumerate(SHADOW_KERNEL):
                for i, wx in enumerate(SHADOW_KERNEL):
                    sx, sy = x - dx + i - reach, y - dy + j - reach
                    if 0 <= sx < size and 0 <= sy < size:
                        shadow += wx * wy * mask[sy][sx]
            shadow = shadow / total * SHADOW_STRENGTH if shadowed else 0
            r, g, b, a = rows[y][x] if y < height and x < width else (0, 0, 0, 0)
            alpha = a / 255
            combined = alpha + shadow * (1 - alpha)
            if combined <= 0:
                row.append((0, 0, 0, 0))
                continue
            # Colour over a black shadow, as straight alpha.
            row.append(tuple(round(c * alpha / combined) for c in (r, g, b)) + (round(combined * 255),))
        out.append(row)
    return out


def xcursor(*images):
    """An Xcursor file with images (nominal, width, height, hot x, hot y, rows), premultiplied ARGB."""
    chunks = []
    for nominal, width, height, hot_x, hot_y, rows in images:
        pixels = bytearray()
        for row in rows:
            for r, g, b, a in row:
                pixels += struct.pack('<I', a << 24 | (r * a // 255) << 16 | (g * a // 255) << 8 | (b * a // 255))
        # The nominal size is XP's cursor size; the image is larger only for the shadow.
        chunks.append((nominal, struct.pack('<IIIIIIIII', 36, 0xfffd0002, nominal, 1, width, height, hot_x, hot_y, 0)
                       + pixels))
    position = 16 + 12 * len(chunks)
    table = b''
    for nominal, chunk in chunks:
        table += struct.pack('<III', 0xfffd0002, nominal, position)
        position += len(chunk)
    return struct.pack('<4sIII', b'Xcur', 16, 0x10000, len(chunks)) + table + b''.join(c for _, c in chunks)


def cursors(user32):
    resources = Resources(user32)
    for number, names in NAMES.items():
        group = resources.find(RT_GROUP_CURSOR, number)
        if group is None:
            continue
        entries = sorted(group_entries(group), key=lambda e: (e[0] != 32, -e[2]))
        width, height, _, resource = entries[0]
        yield number, names, decode(resources.find(RT_CURSOR, resource))


def write_theme(user32, directory, reactos=False):
    directory = Path(directory)
    (directory / 'cursors').mkdir(parents=True, exist_ok=True)
    for _, names, (width, height, hot_x, hot_y, rows, shadowed) in cursors(user32):
        shadowed = shadowed and not reactos
        large = doubled(width, height, rows)
        data = xcursor((32, SIZE, SIZE, hot_x, hot_y, with_shadow(width, height, rows, shadowed)),
                       (LARGE, LARGE_SIZE, LARGE_SIZE, 2 * hot_x, 2 * hot_y,
                        with_shadow(*large, shadowed, size=LARGE_SIZE)))
        first = directory / 'cursors' / names[0]
        first.write_bytes(data)
        for alias in names[1:]:
            link = directory / 'cursors' / alias
            if link.is_symlink() or link.exists():
                link.unlink()
            link.symlink_to(names[0])
    if reactos:
        (directory / 'index.theme').write_text('[Icon Theme]\nName=win4wine-reactos\nComment=ReactOS cursors\n')
    else:
        (directory / 'index.theme').write_text('[Icon Theme]\nName=win4wine-xp\nComment=Windows XP cursors from the user\'s media\n')


if __name__ == '__main__':
    if len(sys.argv) not in (3, 4) or sys.argv[3:] not in ([], ['--reactos']):
        sys.exit(__doc__)
    write_theme(sys.argv[1], sys.argv[2], reactos=len(sys.argv) == 4)
