#!/usr/bin/env python3
"""Measure bridge latency and idle wakeups; run with WM + proxy active.

Reports, in milliseconds, how long the genuine Wine frame takes to follow a
moved or resized native client, how long the native client takes to follow
its frame moved as Wine's move loop moves it, how long a native title takes
to reach the Win32 proxy (what the XP taskbar reads), and how long a proxy
command takes to reach the native client. Then counts the context switches and CPU time of
the WM, the proxy and their wineserver while nothing happens.
"""
import os
from pathlib import Path
import statistics
import time
from Xlib import X, Xatom, display

ipc = Path(os.environ['XP_BRIDGE_DIR']).resolve()
prefix = os.environ.get('WINEPREFIX', '')
rounds = int(os.environ.get('XP_LATENCY_ROUNDS', '20'))
idle_seconds = float(os.environ.get('XP_IDLE_SECONDS', '10'))
d = display.Display()
root = d.screen().root
atom = d.intern_atom


def prop(w, name):
    p = w.get_full_property(atom(name), X.AnyPropertyType)
    return list(p.value) if p else []


def wait(predicate, message, timeout=5):
    start = time.monotonic()
    while time.monotonic() - start < timeout:
        d.sync()
        if predicate():
            return time.monotonic() - start
        time.sleep(.0005)
    raise AssertionError(message)


def frame_for(w):
    try:
        pairs = {int(hwnd, 16): int(xid, 16) for hwnd, xid, *_ in
                 (line.split() for line in (ipc / 'frames.tsv').read_text().splitlines())}
    except OSError:
        return None
    for child in root.query_tree().children:
        hwnd = prop(child, '_WIN4WINE_HWND')
        if hwnd and pairs.get(hwnd[0]) == w.id and child.get_attributes().map_state == X.IsViewable:
            return child


def paired():
    f, n = frame.get_geometry(), native.get_geometry()
    metrics = prop(native, '_NET_FRAME_EXTENTS')
    if len(metrics) != 4:
        return False
    left, right, top, bottom = metrics
    return (f.x, f.y, f.width, f.height) == (n.x-left, n.y-top, n.width+left+right, n.height+top+bottom)


def summary(label, samples):
    ms = sorted(s * 1000 for s in samples)
    print(f'{label}: median {statistics.median(ms):.1f} ms, max {ms[-1]:.1f} ms, n={len(ms)}', flush=True)


def processes():
    """The WM, proxy and wineserver serving this IPC directory and prefix."""
    found = {}
    for entry in Path('/proc').iterdir():
        if not entry.name.isdigit():
            continue
        try:
            argv = (entry / 'cmdline').read_bytes().split(b'\0')
            environ = (entry / 'environ').read_bytes().split(b'\0')
        except OSError:
            continue
        text = b' '.join(argv).decode('utf-8', 'replace')
        if 'wm' in text and str(ipc) in text and '.exe' not in text:
            found['wm'] = int(entry.name)
        elif f'WINEPREFIX={prefix}'.encode() in environ:
            if 'proxy' in text and '.exe' in text:
                found['proxy'] = int(entry.name)
            elif 'wineserver' in text:
                found['wineserver'] = int(entry.name)
    return found


def sample(pid):
    status = Path(f'/proc/{pid}/status').read_text()
    switches = sum(int(line.split()[1]) for line in status.splitlines()
                   if line.startswith(('voluntary_ctxt_switches', 'nonvoluntary_ctxt_switches')))
    # Threads count too: Wine processes wait in more than one thread.
    for task in Path(f'/proc/{pid}/task').iterdir():
        if task.name == str(pid):
            continue
        for line in (task / 'status').read_text().splitlines():
            if line.startswith(('voluntary_ctxt_switches', 'nonvoluntary_ctxt_switches')):
                switches += int(line.split()[1])
    fields = Path(f'/proc/{pid}/stat').read_text().rsplit(')', 1)[1].split()
    return switches, int(fields[11]) + int(fields[12])


native = root.create_window(200, 200, 400, 250, 0, X.CopyFromParent, X.InputOutput, X.CopyFromParent,
                            background_pixel=0x208040, event_mask=X.StructureNotifyMask)
native.set_wm_name('latency probe')
native.map()
d.flush()
try:
    frame = None
    def found():
        global frame
        frame = frame_for(native)
        return frame
    wait(found, 'frame appears', 15)
    wait(paired, 'frame pairs', 15)
    time.sleep(1)

    moves, resizes, titles, commands = [], [], [], []
    for i in range(rounds):
        x, y = 150 + (i % 2) * 120 + i, 160 + (i % 3) * 40
        native.configure(x=x, y=y)
        d.flush()
        moves.append(wait(lambda: native.get_geometry().x == x and native.get_geometry().y == y and paired(),
                          'frame follows move'))
        time.sleep(.15)
    for i in range(rounds):
        width, height = 360 + (i % 2) * 80 + i, 220 + (i % 3) * 30
        native.configure(width=width, height=height)
        d.flush()
        resizes.append(wait(lambda: native.get_geometry().width == width and
                            native.get_geometry().height == height and paired(), 'frame follows resize'))
        time.sleep(.15)
    for i in range(rounds):
        name = f'latency probe {i}'
        native.change_property(atom('_NET_WM_NAME'), atom('UTF8_STRING'), 8, name.encode())
        d.flush()
        titles.append(wait(lambda: bytes(prop(frame, '_NET_WM_NAME')).decode('utf-8', 'replace') == name,
                           'proxy title follows native title'))
        time.sleep(.15)
    for i in range(rounds):
        x, y = 180 + (i % 2) * 90 + i, 190 + (i % 3) * 20
        temporary = ipc / f'latency-{os.getpid()}-{i}.tmp'
        temporary.write_text(f'geometry {native.id:x} {x} {y} 400 250\n')
        temporary.replace(temporary.with_suffix('.cmd'))
        commands.append(wait(lambda: (native.get_geometry().x, native.get_geometry().y) == (x, y),
                             'native follows proxy command'))
        time.sleep(.15)
    follows = []
    for i in range(rounds):
        # A request to move the frame's X window, as Wine makes for each
        # step of a drag; the WM moves the client in the same pass.
        f = frame.get_geometry()
        x, y = f.x + (12 if i % 2 else -12), f.y + (6 if i % 3 else -6)
        frame.configure(x=x, y=y)
        d.flush()
        follows.append(wait(lambda: (frame.get_geometry().x, frame.get_geometry().y) == (x, y) and paired(),
                            'native follows frame move'))
        time.sleep(.15)
    summary('frame follows native move', moves)
    summary('frame follows native resize', resizes)
    summary('native title reaches proxy', titles)
    summary('proxy command reaches native', commands)
    summary('native follows frame move', follows)

    pids = processes()
    time.sleep(2)
    before = {name: sample(pid) for name, pid in pids.items()}
    time.sleep(idle_seconds)
    after = {name: sample(pid) for name, pid in pids.items()}
    ticks = os.sysconf('SC_CLK_TCK')
    for name in sorted(pids):
        switches = after[name][0] - before[name][0]
        cpu = (after[name][1] - before[name][1]) / ticks
        print(f'idle {name}: {switches / idle_seconds:.1f} context switches/s, '
              f'CPU {100 * cpu / idle_seconds:.2f}%', flush=True)
finally:
    native.destroy()
    d.flush()
    d.close()
