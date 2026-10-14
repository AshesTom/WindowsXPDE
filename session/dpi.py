"""The XP shell's DPI (LogPixels) and font smoothing, and the matching settings for Linux applications.

XP scales text and metrics with LogPixels and keeps 32x32 icons; Linux
toolkits follow Xft.dpi for their text the same way. GTK 3 scales its
widgets only by whole factors (GDK_SCALE), so from 192 DPI on it doubles
them and keeps the text at Xft.dpi. With XP's ClearType, Linux applications
get subpixel rendering in XP's RGB order too (Xft.rgba and fontconfig)."""
from pathlib import Path
import re

DEFAULT = 96


def _registry_value(hive, key, name):
    """A REG_DWORD (int) or REG_SZ (str) from a Wine registry file, or None."""
    try:
        text = Path(hive).read_text(encoding='utf-8', errors='replace')
    except OSError:
        return None
    header = '[' + key.replace('\\', '\\\\') + ']'
    for section in text.split('\n\n'):
        lines = section.strip().splitlines()
        if not lines or not lines[0].startswith(header + ' ') and lines[0] != header:
            continue
        for line in lines[1:]:
            match = re.fullmatch(r'"%s"=(?:dword:([0-9a-fA-F]{8})|"([^"\\]*)")' % re.escape(name), line.strip())
            if match:
                return int(match.group(1), 16) if match.group(1) else match.group(2)
    return None


def _registry_dword(hive, key, name):
    value = _registry_value(hive, key, name)
    return value if isinstance(value, int) else None


def prefix_dpi(prefix):
    """LogPixels as Wine reads it: the user's, then the hardware profile's, else 96."""
    prefix = Path(prefix)
    value = (_registry_dword(prefix / 'user.reg', r'Control Panel\Desktop', 'LogPixels')
             or _registry_dword(prefix / 'system.reg',
                                r'System\ControlSet001\Hardware Profiles\Current\Software\Fonts', 'LogPixels'))
    return value if value and 0 < value <= 960 else DEFAULT


def prefix_cleartype(prefix):
    """Whether XP's font smoothing is ClearType (Display Properties, Effects) in the prefix."""
    hive = Path(prefix) / 'user.reg'
    smoothing = _registry_value(hive, r'Control Panel\Desktop', 'FontSmoothing')
    try:
        smoothing = int(smoothing)
    except (TypeError, ValueError):
        return False
    return bool(smoothing) and _registry_dword(hive, r'Control Panel\Desktop', 'FontSmoothingType') == 2


def cursor_size(dpi):
    """XP's SM_CXCURSOR: 32 up to 144 DPI, 64 above (measured at 144 and 168 DPI)."""
    return 64 if dpi > 144 else 32


def environment(dpi, wayland=False):
    """Variables for Linux applications started in the session.

    On Wayland, applications take each output's scale from the compositor."""
    env = {'XCURSOR_SIZE': str(cursor_size(dpi))}
    scale = dpi // DEFAULT
    if scale >= 2 and not wayland:
        env.update(GDK_SCALE=str(scale), GDK_DPI_SCALE=f'{1 / scale:g}')
    return env


# The resources set for ClearType; the marker lets a later session with
# standard smoothing on the same X server take them back.
CLEARTYPE_RESOURCES = {'Xft.rgba': 'rgb', 'Xft.lcdfilter': 'lcddefault', 'win4wine.cleartype': '1'}


def set_xft(d, dpi, cleartype=False):
    """Set Xft.dpi, and Xft.rgba for ClearType, in the root window's resources (what xrdb -merge does) on Xlib display D."""
    from Xlib import Xatom
    root = d.screen().root
    atom = d.intern_atom('RESOURCE_MANAGER')
    prop = root.get_full_property(atom, Xatom.STRING)
    value = prop.value if prop else b''
    if isinstance(value, str):
        value = value.encode()
    lines = value.decode('latin-1').splitlines()
    names = {'Xft.dpi'}
    if cleartype or any(line.split(':')[0] == 'win4wine.cleartype' for line in lines):
        names.update(CLEARTYPE_RESOURCES)
    lines = [line for line in lines if line.split(':')[0] not in names]
    lines.append(f'Xft.dpi:\t{dpi}')
    if cleartype:
        lines.extend(f'{name}:\t{value}' for name, value in CLEARTYPE_RESOURCES.items())
    root.change_property(atom, Xatom.STRING, 8, ('\n'.join(lines) + '\n').encode('latin-1'))
    d.flush()
