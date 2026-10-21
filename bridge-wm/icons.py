"""Bounded public X11 icon extraction for the native-window bridge."""
import struct
from Xlib import X, Xatom, Xutil, error

MAX_SIDE = 256
MAX_WORDS = 262144


def net_icon(window, atom, side=32):
    prop = window.get_property(atom, Xatom.CARDINAL, 0, MAX_WORDS)
    if not prop or prop.format != 32 or prop.bytes_after:
        return None
    values = prop.value
    offset, best = 0, None
    while offset < len(values):
        if len(values) - offset < 2:
            return None
        width, height = map(int, values[offset:offset+2])
        offset += 2
        if not width or not height or width * height > len(values) - offset:
            return None
        if width <= MAX_SIDE and height <= MAX_SIDE:
            candidate = (abs(max(width, height)-side), -width*height, offset, width, height)
            if best is None or candidate < best:
                best = candidate
        offset += width * height
    if best is None:
        return None
    _, _, offset, width, height = best
    return width, height, [int(p) for p in values[offset:offset+width*height]]


def pixmap_pixels(display, pixmap):
    if not hasattr(pixmap, 'get_geometry'):
        pixmap = display.create_resource_object('pixmap', int(pixmap))
    geometry = pixmap.get_geometry()
    if not 0 < geometry.width <= MAX_SIDE or not 0 < geometry.height <= MAX_SIDE:
        return None
    info = display.display.info
    formats = [f for f in info.pixmap_formats if f.depth == geometry.depth]
    if not formats:
        return None
    fmt = formats[0]
    if fmt.bits_per_pixel not in (1, 8, 16, 24, 32):
        return None
    stride = ((geometry.width * fmt.bits_per_pixel + fmt.scanline_pad - 1) // fmt.scanline_pad) * (fmt.scanline_pad // 8)
    image = pixmap.get_image(0, 0, geometry.width, geometry.height, X.ZPixmap, 0xffffffff)
    if not image or len(image.data) < stride * geometry.height:
        return None
    pixels = []
    for y in range(geometry.height):
        for x in range(geometry.width):
            offset = y * stride + (x * fmt.bits_per_pixel // 8)
            if fmt.bits_per_pixel == 1:
                shift = x % 8 if info.bitmap_format_bit_order == X.LSBFirst else 7 - x % 8
                pixel = image.data[offset] >> shift & 1
            else:
                pixel = int.from_bytes(image.data[offset:offset+fmt.bits_per_pixel//8],
                                       'little' if info.image_byte_order == X.LSBFirst else 'big')
            pixels.append(pixel)
    return geometry.width, geometry.height, geometry.depth, pixels


def hints_icon(display, window):
    hints = window.get_wm_hints()
    if not hints or not (hints.flags & Xutil.IconPixmapHint):
        return None
    image = pixmap_pixels(display, hints.icon_pixmap)
    if not image:
        return None
    width, height, depth, pixels = image
    mask = None
    if hints.flags & Xutil.IconMaskHint:
        mask = pixmap_pixels(display, hints.icon_mask)
        if not mask or mask[:3] != (width, height, 1):
            return None
        mask = mask[3]
    screen = display.screen()
    if depth == 1:
        colors = {0: 0xff000000, 1: 0xffffffff}
    else:
        visuals = [v for d in screen.allowed_depths if d.depth == depth for v in d.visuals
                   if v.visual_class == X.TrueColor]
        if visuals:
            visual = next((v for v in visuals if v.visual_id == screen.root_visual), visuals[0])
            def rgb(pixel):
                color = 0xff000000
                for component, shift in ((visual.red_mask, 16), (visual.green_mask, 8), (visual.blue_mask, 0)):
                    low = component & -component
                    if not low:
                        return None
                    color |= ((pixel & component) // low * 255 // (component // low)) << shift
                return color
            colors = {pixel: rgb(pixel) for pixel in set(pixels)}
        else:
            # Indexed pixmaps use the client's colormap (the default when unset).
            unique = list(set(pixels))
            if len(unique) > 256:
                return None
            attrs = window.get_attributes()
            cmap = attrs.colormap or screen.default_colormap
            if not hasattr(cmap, 'query_colors'):
                cmap = display.create_resource_object('colormap', int(cmap))
            queried = cmap.query_colors(unique)
            colors = {p: 0xff000000 | (c.red >> 8) << 16 | (c.green >> 8) << 8 | (c.blue >> 8)
                      for p, c in zip(unique, queried)}
    if None in colors.values():
        return None
    return width, height, [colors[p] if mask is None or mask[i] else 0 for i, p in enumerate(pixels)]


def shrink(width, height, pixels, side):
    """The image fitted in SIDE x SIDE by averaging its pixels (alpha
    weighted), centred: a caption draws its small icon at that size, and
    stretching it there would only keep some of the pixels."""
    scale = side / max(width, height)
    w, h = max(1, round(width * scale)), max(1, round(height * scale))
    left, top = (side - w) // 2, (side - h) // 2
    out = [0] * (side * side)
    for y in range(h):
        y0, y1 = y * height // h, max(y * height // h + 1, (y + 1) * height // h)
        for x in range(w):
            x0, x1 = x * width // w, max(x * width // w + 1, (x + 1) * width // w)
            a = r = g = b = 0
            for sy in range(y0, y1):
                for p in pixels[sy * width + x0:sy * width + x1]:
                    alpha = p >> 24
                    a += alpha
                    r += (p >> 16 & 0xff) * alpha
                    g += (p >> 8 & 0xff) * alpha
                    b += (p & 0xff) * alpha
            count = (y1 - y0) * (x1 - x0)
            if a:
                out[(top + y) * side + left + x] = (a // count) << 24 | (r // a) << 16 | (g // a) << 8 | b // a
    return side, side, out


def payload(display, window, icon_atom, side=32):
    """The icon nearest SIDE pixels; a small one (side 16) is shrunk to
    16x16 when the client has none that small."""
    try:
        image = net_icon(window, icon_atom, side) or hints_icon(display, window)
        if image and side < 32 and max(image[:2]) > side:
            image = shrink(*image, side)
        if image:
            width, height, pixels = image
            return b'W4I1' + struct.pack('<II', width, height) + struct.pack(f'<{len(pixels)}I', *pixels)
    except (error.XError, ValueError, TypeError, KeyError, AttributeError):
        pass
    return None
