#!/usr/bin/env python3
"""Keep XP's desktop window at the bottom of the Wine virtual desktop.

In a Wine virtual desktop each Win32 top-level window is an X child window of
the desktop window, and Wine only ever raises them. When XP's desktop
(Progman, as large as the screen) is activated, Wine raises it above the
taskbar, which Win32 still has topmost, and the taskbar vanishes until it is
activated itself. XP keeps Progman at the bottom; this does the same on the
X side: whenever Progman is not the lowest mapped child, it is lowered, and
the windows it covered are asked to paint again (an X expose).
Windows are recognised by the _WIN4WINE_CLASS property of Wine patch 0019.

Usage: xstack.py [TITLE]   (default: "XP - Wine Desktop"); exits with the desktop.
"""
import ctypes
import ctypes.util
import sys
import time

TITLE = (sys.argv[1] if len(sys.argv) > 1 else 'XP - Wine Desktop').encode()
x = ctypes.cdll.LoadLibrary(ctypes.util.find_library('X11') or 'libX11.so.6')
x.XOpenDisplay.restype = ctypes.c_void_p
x.XDefaultRootWindow.restype = ctypes.c_ulong
x.XInternAtom.restype = ctypes.c_ulong
Window = ctypes.c_ulong


class Attributes(ctypes.Structure):
    _fields_ = [(n, ctypes.c_int) for n in ('x', 'y', 'width', 'height', 'border_width', 'depth')] + [
        ('visual', ctypes.c_void_p), ('root', Window), ('c_class', ctypes.c_int), ('bit_gravity', ctypes.c_int),
        ('win_gravity', ctypes.c_int), ('backing_store', ctypes.c_int), ('backing_planes', ctypes.c_ulong),
        ('backing_pixel', ctypes.c_ulong), ('save_under', ctypes.c_int), ('colormap', ctypes.c_ulong),
        ('map_installed', ctypes.c_int), ('map_state', ctypes.c_int), ('all_event_masks', ctypes.c_long),
        ('your_event_mask', ctypes.c_long), ('do_not_propagate_mask', ctypes.c_long),
        ('override_redirect', ctypes.c_int), ('screen', ctypes.c_void_p)]


# Windows come and go; ignore BadWindow and friends instead of exiting.
ERROR_HANDLER = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p)(lambda display, event: 0)
x.XSetErrorHandler(ERROR_HANDLER)

display = x.XOpenDisplay(None)
if not display:
    sys.exit('xstack: cannot open the X display')
display = ctypes.c_void_p(display)
root = x.XDefaultRootWindow(display)
CLASS = x.XInternAtom(display, b'_WIN4WINE_CLASS', False)
NAME = x.XInternAtom(display, b'WM_NAME', False)


def children(window):
    root_return, parent, kids, count = Window(), Window(), ctypes.POINTER(Window)(), ctypes.c_uint()
    if not x.XQueryTree(display, Window(window), ctypes.byref(root_return), ctypes.byref(parent),
                        ctypes.byref(kids), ctypes.byref(count)):
        return []
    result = [kids[i] for i in range(count.value)]  # bottom to top
    if kids:
        x.XFree(kids)
    return result


def string_property(window, atom):
    kind, size, count, after = ctypes.c_ulong(), ctypes.c_int(), ctypes.c_ulong(), ctypes.c_ulong()
    data = ctypes.POINTER(ctypes.c_ubyte)()
    if x.XGetWindowProperty(display, Window(window), ctypes.c_ulong(atom), 0, 256, False, 0,
                            ctypes.byref(kind), ctypes.byref(size), ctypes.byref(count), ctypes.byref(after),
                            ctypes.byref(data)) != 0 or not data:
        return b''
    value = ctypes.string_at(data, count.value)
    x.XFree(data)
    return value


def mapped(window):
    attributes = Attributes()
    return x.XGetWindowAttributes(display, Window(window), ctypes.byref(attributes)) and attributes.map_state == 2


def find_desktop():
    for window in children(root):
        if string_property(window, NAME) == TITLE:
            return window
    return None


desktop, classes = None, {}
# The previous session's window may still be closing when this starts: only
# leave once no XP desktop has been there for a while.
last_seen = time.monotonic() + 105
while True:
    if desktop is None or not mapped(desktop):
        desktop = find_desktop()
        if desktop is None:
            if time.monotonic() - last_seen > 15:
                break
            time.sleep(0.5)
            continue
    last_seen = time.monotonic()
    kids = [w for w in children(desktop) if mapped(w)]
    for window in kids:
        if window not in classes:
            classes[window] = string_property(window, CLASS)
    progman = [w for w in kids if classes.get(w) == b'Progman']
    if progman and kids and kids[0] != progman[0]:
        x.XLowerWindow(display, Window(progman[0]))
        # What Progman covered may not have been painted: have the windows
        # that were under it (the taskbar first) draw themselves again.
        for window in kids[:kids.index(progman[0])]:
            x.XClearArea(display, Window(window), 0, 0, 0, 0, True)
        x.XFlush(display)
    if len(classes) > 4096:
        classes.clear()
    time.sleep(0.2)
