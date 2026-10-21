#!/usr/bin/env python3
"""Real Wine frame gestures on a native X client; run with WM + proxy active."""
import os
import re
from itertools import count
from pathlib import Path
import subprocess
import time
from Xlib import X, Xatom, Xutil, display, error, protocol

ipc = Path(os.environ['XP_BRIDGE_DIR'])
d = display.Display()
root = d.screen().root
atom = d.intern_atom
sequence = count()

def prop(w, name):
    p = w.get_full_property(atom(name), X.AnyPropertyType)
    return list(p.value) if p else []

def wait(predicate, message):
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        d.sync()
        value = predicate()
        if value:
            return value
        time.sleep(.05)
    raise AssertionError(message)

def command(op):
    temporary = ipc / f'frame-test-{os.getpid()}-{next(sequence)}.tmp'
    temporary.write_text(f'{op} {native.id:x}\n')
    temporary.replace(temporary.with_suffix('.cmd'))

def hidden(w):
    try:
        return w.get_attributes().map_state != X.IsViewable
    except error.XError:
        return True

def geometry(w):
    g = w.get_geometry()
    return g.x, g.y, g.width, g.height

def frame_for(w):
    try:
        pairs = {int(hwnd, 16): int(xid, 16) for hwnd, xid, *_ in
                 (line.split() for line in (ipc / 'frames.tsv').read_text().splitlines())}
    except OSError:
        return None
    for child in root.query_tree().children:
        try:
            hwnd = prop(child, '_WIN4WINE_HWND')
            if hwnd and pairs.get(hwnd[0]) == w.id and child.get_attributes().map_state == X.IsViewable:
                return child
        except error.BadWindow:
            pass  # destroyed since the query, e.g. during withdrawal and remap

def frame_icon():
    hwnd = prop(frame, '_WIN4WINE_HWND')[0]
    wine = os.environ.get('WINE', 'wine')
    inspect = Path(__file__).resolve().parents[1] / '.local/bridge/inspect.exe'
    result = subprocess.run([wine, str(inspect), 'icon', f'{hwnd:x}'],
                            stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                            env=dict(os.environ, WINEDEBUG='-all'))
    if result.returncode:
        return {}
    return {key: int(value, 10 if key in ('width', 'height') else 16)
            for key, value in re.findall(r'(icon|width|height|hash|first|center|last)=([0-9a-f]+)',
                                         result.stdout.decode('ascii', 'replace'))}

def icon_hash(pixels):
    value = 2166136261
    for pixel in pixels:
        rgb = pixel & 0xffffff if pixel >> 24 else 0xffffff
        value = ((value ^ rgb) * 16777619) & 0xffffffff
    return value

def expect_icon(width, height, pixels, message):
    return wait(lambda: (actual := frame_icon()) and
                (actual.get('width'), actual.get('height'), actual.get('hash')) ==
                (width, height, icon_hash(pixels)), message)

def root_image(left, top, width, height):
    image = root.get_image(left, top, width, height, X.ZPixmap, 0xffffffff)
    fmt = next(fmt for fmt in d.display.info.pixmap_formats if fmt.depth == root.get_geometry().depth)
    visual = next(v for level in d.screen().allowed_depths for v in level.visuals
                  if v.visual_id == d.screen().root_visual)
    stride = ((width*fmt.bits_per_pixel+fmt.scanline_pad-1)//fmt.scanline_pad)*(fmt.scanline_pad//8)
    result = []
    for y in range(height):
        row = []
        for x in range(width):
            offset = y*stride+x*(fmt.bits_per_pixel//8)
            value = int.from_bytes(image.data[offset:offset+fmt.bits_per_pixel//8],
                                   'little' if d.display.info.image_byte_order == X.LSBFirst else 'big')
            rgb = 0
            for mask, shift in ((visual.red_mask,16),(visual.green_mask,8),(visual.blue_mask,0)):
                low = mask & -mask
                rgb |= ((value & mask)//low*255//(mask//low)) << shift
            row.append(rgb)
        result.append(row)
    return result

def caption_image():
    f, n = frame.get_geometry(), native.get_geometry()
    return root_image(f.x, f.y, min(64, f.width), n.y-f.y)

def visible_task_icon(pixels):
    for child in root.query_tree().children:
        cls = child.get_full_property(atom('_WIN4WINE_CLASS'), X.AnyPropertyType)
        if not cls or bytes(cls.value).rstrip(b'\0').lower() != b'shell_traywnd' or hidden(child):
            continue
        g, screen = child.get_geometry(), root.get_geometry()
        origin_x, origin_y = max(0, g.x), max(0, g.y)
        width = min(screen.width, g.x + g.width) - origin_x
        height = min(screen.height, g.y + g.height) - origin_y
        if width < 16 or height < 16:
            continue
        image = root_image(origin_x, origin_y, width, height)
        def matches(actual, expected):
            return all(abs(((actual >> shift) & 255) - ((expected >> shift) & 255)) <= 1
                       for shift in (16, 8, 0))
        for top in range(len(image)-15):
            for left in range(len(image[0])-15):
                if not matches(image[top][left+1], pixels[1]):
                    continue
                if all(matches(image[top+y][left+x], pixels[y*16+x])
                       for y in range(16) for x in range(16) if pixels[y*16+x] >> 24 == 255):
                    return left + origin_x, top + origin_y
    return None

def visible_icon(pixels):
    """Where the 16x16 icon shows in the caption, drawn at SM_CXSMICON: 16 at 96 DPI, larger above
    (each screen pixel takes the icon pixel under its left edge or its centre)."""
    image = caption_image()
    for size, centre in ((size, centre) for size in range(16, 33, 2) for centre in (0, 1)):
        source = [(2 * i + centre) * 16 // (2 * size) for i in range(size)]
        scaled = [pixels[source[y] * 16 + source[x]] for y in range(size) for x in range(size)]
        for top in range(len(image)-size+1):
            for left in range(len(image[0])-size+1):
                if image[top][left+size-1] != (scaled[size-1] & 0xffffff):
                    continue
                if all(image[top+y][left+x] == (scaled[y*size+x] & 0xffffff)
                       for y in range(size) for x in range(size) if scaled[y*size+x] >> 24 == 255):
                    return left, top
    return None

def paired():
    f, n = frame.get_geometry(), native.get_geometry()
    metrics = prop(native, '_NET_FRAME_EXTENTS')
    if len(metrics) != 4:
        return False
    left, right, top, bottom = metrics
    return (f.x, f.y, f.width, f.height) == (n.x-left, n.y-top, n.width+left+right, n.height+top+bottom)

def pointer(*args):
    subprocess.run(['xdotool', *map(str, args)], check=True)

def caption():
    f, n = frame.get_geometry(), native.get_geometry()
    return f.x + 80, f.y + (n.y-f.y)//2

native = root.create_window(0, 0, 400, 250, 0, X.CopyFromParent,
                            X.InputOutput, X.CopyFromParent,
                            background_pixel=0x208040, event_mask=X.StructureNotifyMask)
native.set_wm_name('XP native frame integration')
native.set_wm_protocols([atom('WM_DELETE_WINDOW')])
native.map()
d.flush()
try:
    frame = wait(lambda: frame_for(native), 'genuine Wine proxy frame is mapped')
    wait(paired, 'frame encloses native pixels')
    fallback = wait(frame_icon, 'Wine generic application icon is set when native icon is absent')
    pixels = [0xff00aa33] * 256
    pixels[0], pixels[8*16+8], pixels[-1] = 0, 0xffe03020, 0xff2030e0
    native.change_property(atom('_NET_WM_ICON'), Xatom.CARDINAL, 32, [16, 16] + pixels)
    d.flush()
    expect_icon(16, 16, pixels, 'EWMH ARGB, transparent mask and colors reach genuine Wine HICON')
    icon_position = wait(lambda: visible_icon(pixels), 'actual visible caption pixels show native EWMH icon')
    if os.environ.get('XP_FRAME_TASKBAR_ICONS'):
        wait(lambda: visible_task_icon(pixels), 'actual XP taskbar pixels show the native icon after asynchronous WM_GETICON')
    if os.environ.get('XP_FRAME_ICON_SCREENSHOT'):
        subprocess.run(['import', '-window', 'root', os.environ['XP_FRAME_ICON_SCREENSHOT']], check=True)
    sidecar = ipc / f'icon-{native.id:x}.argb'
    unchanged = sidecar.stat().st_mtime_ns
    native.set_wm_name('XP native frame integration renamed')
    d.flush()
    time.sleep(.5)
    assert sidecar.stat().st_mtime_ns == unchanged, 'title change must not recopy icon pixels'
    # Wine's CreateIconIndirect makes its own premultiplied alpha bitmap.
    # Supplying premultiplied EWMH channels would multiply them twice.
    transparent = [0] * 256
    native.change_property(atom('_NET_WM_ICON'), Xatom.CARDINAL, 32, [16,16] + transparent)
    d.flush()
    expect_icon(16, 16, transparent, 'fully transparent HICON preserves background')
    left, top = icon_position
    time.sleep(.2)
    background = caption_image()[top+8][left+8]
    half = [0x8000aa33] * 256
    native.change_property(atom('_NET_WM_ICON'), Xatom.CARDINAL, 32, [16,16] + half)
    d.flush()
    wait(lambda: (actual := frame_icon()) and actual.get('width') == 16 and
         all(abs(a-b) <= 1 for a,b in zip(((actual.get('center',0)>>16)&255,
                                          (actual.get('center',0)>>8)&255,actual.get('center',0)&255),
                                         (127,212,153))), 'half-alpha HICON blends straight ARGB once')
    expected = tuple((source*128+127)//255 + ((background>>shift)&255)*127//255
                     for source,shift in ((0,16),(170,8),(51,0)))
    def visible_half_alpha():
        pixel = caption_image()[top+8][left+8]
        return all(abs(((pixel>>shift)&255)-target) <= 1
                   for shift,target in zip((16,8,0),expected))
    wait(visible_half_alpha, 'actual caption image blends half-alpha native icon against Luna background')
    small = [0xff902050] * 256
    large = [0xff502090] * (32*32)
    native.change_property(atom('_NET_WM_ICON'), Xatom.CARDINAL, 32,
                           [16, 16] + small + [32, 32] + large)
    d.flush()
    expect_icon(16, 16, small, 'updated EWMH icon gives the caption its own 16x16 image and replaces HICON')
    if os.environ.get('XP_FRAME_TASKBAR_ICONS'):
        wait(lambda: visible_task_icon(small), 'actual XP taskbar replaces its icon after native property update')
        print('PASS: actual XP taskbar pixels show initial and updated native icons')
    native.change_property(atom('_NET_WM_ICON'), Xatom.CARDINAL, 32, [32, 32] + large)
    d.flush()
    expect_icon(16, 16, [large[0]] * 256, 'a 32px-only EWMH icon is shrunk to the caption\'s 16x16 image')
    pixmap = root.create_pixmap(16, 16, root.get_geometry().depth)
    mask = root.create_pixmap(16, 16, 1)
    gc = pixmap.create_gc(foreground=0x112233)
    pixmap.fill_rectangle(gc, 0, 0, 16, 16)
    mask_gc = mask.create_gc(foreground=1)
    mask.fill_rectangle(mask_gc, 0, 0, 16, 16)
    mask_gc.change(foreground=0)
    mask.fill_rectangle(mask_gc, 0, 0, 8, 16)
    native.set_wm_hints(flags=Xutil.IconPixmapHint | Xutil.IconMaskHint,
                        icon_pixmap=pixmap, icon_mask=mask)
    native.change_property(atom('_NET_WM_ICON'), Xatom.CARDINAL, 32, [0xffffffff, 0xffffffff])
    d.flush()
    hints_pixels = [0 if x < 8 else 0xff112233 for y in range(16) for x in range(16)]
    expect_icon(16, 16, hints_pixels, 'malformed EWMH safely falls back to ICCCM pixmap and mask')
    native.delete_property(atom('_NET_WM_ICON'))
    native.delete_property(atom('WM_HINTS'))
    d.flush()
    wait(lambda: frame_icon() == fallback and not sidecar.exists(), 'removed native icon restores shared Wine default')
    pixmap.free(); mask.free(); gc.free(); mask_gc.free()
    print('PASS: genuine HICON default, EWMH image selection/update, visible caption pixels and half-alpha, ICCCM pixmap mask and removal', flush=True)
    area = prop(root, '_NET_WORKAREA')
    assert frame.get_geometry().x >= area[0] and frame.get_geometry().y >= area[1], 'initial caption stays inside workarea'
    wait(lambda: prop(root, '_NET_ACTIVE_WINDOW') == [native.id], 'native focus identity')
    assert prop(root, '_NET_CLIENT_LIST').count(native.id) == 1
    assert frame.id not in prop(root, '_NET_CLIENT_LIST')
    popup = root.create_window(10, 10, 60, 60, 0, X.CopyFromParent,
                               X.InputOutput, X.CopyFromParent, background_pixel=0x505080)
    popup.change_property(atom('_WIN4WINE_CLASS'), Xatom.STRING, 8, b'DV2ControlHost')
    popup.set_wm_name('Start popup must not become a task')
    popup.map()
    d.flush()
    wait(lambda: popup.get_attributes().map_state == X.IsViewable, 'Start popup maps')
    assert prop(root, '_NET_ACTIVE_WINDOW') == [native.id]
    wait(lambda: f'{popup.id:x}\t' not in (ipc / 'windows.tsv').read_text(), 'Start popup excluded from bridge')
    popup.destroy()
    d.flush()
    d.set_input_focus(X.PointerRoot, X.RevertToPointerRoot, X.CurrentTime)
    root.send_event(protocol.event.ClientMessage(window=frame, client_type=atom('_NET_ACTIVE_WINDOW'),
                    data=(32, [1, 0, 0, 0, 0])), event_mask=X.SubstructureRedirectMask | X.SubstructureNotifyMask)
    d.flush()
    wait(lambda: getattr(d.get_input_focus().focus, 'id', 0) == native.id, 'proxy EWMH activation focuses native client')
    # The border must sit immediately below its client, not above its pixels.
    stack = [w.id for w in root.query_tree().children]
    assert stack.index(frame.id) + 1 == stack.index(native.id)
    before = geometry(native)
    x, y = caption()
    pointer('mousemove', x, y, 'mousedown', 1, 'mousemove', x+60, y+45, 'sleep', .3, 'mouseup', 1)
    wait(lambda: geometry(native)[:2] == (before[0]+60, before[1]+45), 'caption drag moves native client')
    wait(paired, 'frame follows caption drag')
    before = geometry(native)
    f = frame.get_geometry()
    pointer('mousemove', f.x+f.width-2, f.y+f.height-2, 'mousedown', 1,
            'mousemove', f.x+f.width+58, f.y+f.height+38, 'sleep', .3, 'mouseup', 1)
    wait(lambda: native.get_geometry().width > before[2] and native.get_geometry().height > before[3],
         'resize corner changes native client dimensions')
    wait(paired, 'frame follows resize')
    before = geometry(native)
    x, y = caption()
    pointer('mousemove', x, y, 'click', '--repeat', 2, '--delay', 100, 1)
    wait(lambda: prop(native, '_NET_WM_STATE') and native.get_geometry().width > before[2], 'caption double click maximizes')
    wait(paired, 'maximized frame encloses native client')
    hwnd=prop(frame,'_WIN4WINE_HWND')[0]
    def public_frame():
        result=subprocess.run([os.environ.get('WINE', 'wine'), str(Path(__file__).resolve().parents[1] / '.local/bridge/inspect.exe'), 'frame', f'{hwnd:x}'],
            stdout=subprocess.PIPE,stderr=subprocess.DEVNULL,env=dict(os.environ,WINEDEBUG='-all'), timeout=10)
        return result.stdout.decode('ascii','replace')
    wait(lambda:'zoomed=1' in public_frame(),'native proxy has genuine Win32 maximized state')
    area=prop(root,'_NET_WORKAREA')
    records=[row.split() for row in (ipc/'frames.tsv').read_text().splitlines()]
    record=next(row for row in records if int(row[1],16)==native.id)
    bx,by=map(int,record[8:10])
    wanted=(area[0]-bx,area[1]-by,area[2]+2*bx,area[3]+2*by)
    wait(lambda:geometry(frame)==wanted,'true maximized outer clips measured border outside workarea')
    f,n=frame.get_geometry(),native.get_geometry()
    visible=(n.x-max(f.x,area[0]), max(0,min(f.x+f.width,area[0]+area[2])-n.x-n.width),
             n.y-max(f.y,area[1]), max(0,min(f.y+f.height,area[1]+area[3])-n.y-n.height))
    assert visible==(0,0,int(record[4])-by,0),visible
    print('PASS real IsZoomed/WS_MAXIMIZE and clipped NC max geometry',geometry(frame),geometry(native),'visible',visible,flush=True)

    x, y = caption()
    pointer('mousemove', x, y, 'click', '--repeat', 2, '--delay', 100, 1)
    wait(lambda: geometry(native) == before, 'caption double click restores original geometry')
    wait(lambda:'zoomed=0' in public_frame(),'restore clears genuine Win32 maximized state')
    restored = geometry(native)
    x, y = caption()
    pointer('mousemove', x, y, 'mousedown', 1, 'mousemove', x+25, y+18, 'sleep', .3, 'mouseup', 1)
    wait(lambda: geometry(native)[:2] != restored[:2], 'restored caption remains draggable after true maximize')
    wait(paired, 'frame follows drag after maximize/restore')
    command('minimize')
    wait(lambda: prop(native, 'WM_STATE')[:1] == [3] and frame.get_attributes().map_state != X.IsViewable,
         'minimize hides native and frame together')
    command('activate')
    wait(lambda: native.get_attributes().map_state == X.IsViewable and frame.get_attributes().map_state == X.IsViewable,
         'restore maps native and frame together')
    wait(paired, 'restored frame encloses native client')
    f, n = frame.get_geometry(), native.get_geometry()
    pointer('mousemove', f.x+f.width-12, f.y+(n.y-f.y)//2, 'click', 1)
    def closed():
        while d.pending_events():
            e = d.next_event()
            if e.type == X.ClientMessage and e.client_type == atom('WM_PROTOCOLS') and e.data[1][0] == atom('WM_DELETE_WINDOW'):
                return True
    wait(closed, 'close button sends WM_DELETE_WINDOW to native client')
    native.unmap()
    d.flush()
    wait(lambda: native.id not in prop(root, '_NET_CLIENT_LIST') and hidden(frame) and not prop(native, 'WM_STATE') and not (ipc / f'icon-{native.id:x}.argb').exists(),
         'withdrawal deletes WM_STATE, removes task and hides orphan frame')
    root.send_event(protocol.event.ClientMessage(window=frame, client_type=atom('_NET_ACTIVE_WINDOW'),
                    data=(32, [1, 0, 0, 0, 0])), event_mask=X.SubstructureRedirectMask | X.SubstructureNotifyMask)
    native.map()
    d.flush()
    frame = wait(lambda: frame_for(native), 'withdrawn client can map with an associated frame')
    wait(paired, 'remapped native client has matching frame')
    wait(lambda: prop(root, '_NET_ACTIVE_WINDOW') == [native.id], 'queued orphan activation does not break WM')
    print('PASS: genuine Wine frame, single task identity, stacking, caption drag, resize, maximize/restore, minimize, close, withdrawal/remap')
except Exception:
    for label, window in [('native', native), ('frame', locals().get('frame'))]:
        if window:
            try:
                print(label, 'map=', window.get_attributes().map_state,
                      'geometry=', geometry(window), 'WM_STATE=', prop(window, 'WM_STATE'),
                      'NET_STATE=', prop(window, '_NET_WM_STATE'), flush=True)
            except error.XError:
                print(label, 'already destroyed', flush=True)
    for name in ('windows.tsv', 'geometry.tsv', 'frames.tsv'):
        print(name, (ipc / name).read_text() if (ipc / name).exists() else 'missing', flush=True)
    raise
finally:
    native.destroy()
    d.flush()
    d.close()
