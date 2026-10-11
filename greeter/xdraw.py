"""Put cairo images on X drawables with python-xlib (no toolkit needed)."""
from Xlib import X, Xatom


def pixmap(d, surface):
    """A server pixmap holding SURFACE (cairo RGB24/ARGB32, 32 bits per pixel)."""
    screen = d.screen()
    width, height = surface.get_width(), surface.get_height()
    result = screen.root.create_pixmap(width, height, screen.root_depth)
    gc = result.create_gc()
    surface.flush()
    data, stride = bytes(surface.get_data()), surface.get_stride()
    # Keep each PutImage request well under the server's request size.
    rows = max(1, 200000 // stride)
    for y in range(0, height, rows):
        count = min(rows, height - y)
        result.put_image(gc, 0, y, width, count, X.ZPixmap, screen.root_depth, 0,
                         data[y * stride:(y + count) * stride])
    gc.free()
    return result


def set_root_background(d, surface):
    """Leave SURFACE as the root window's background after this client exits.

    The display manager keeps the X server for the session, so the screen shows
    this image instead of black until the session draws. The previous image set
    this way is freed first, as Esetroot does."""
    root = d.screen().root
    property = d.intern_atom('ESETROOT_PMAP_ID')
    previous = root.get_full_property(property, Xatom.PIXMAP)
    if previous and previous.value:
        try:
            d.kill_client(previous.value[0])
        except Exception:
            pass
    image = pixmap(d, surface)
    root.change_attributes(background_pixmap=image)
    root.clear_area()
    for name in ('_XROOTPMAP_ID', 'ESETROOT_PMAP_ID'):
        root.change_property(d.intern_atom(name), Xatom.PIXMAP, 32, [image.id])
    d.set_close_down_mode(X.RetainPermanent)
    d.sync()
