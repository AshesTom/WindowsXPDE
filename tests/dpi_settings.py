#!/usr/bin/env python3
"""session/dpi.py: the prefix's LogPixels and the settings Linux applications get from it.

With DISPLAY set (Xvfb is enough), also checks that Xft.dpi lands in the root
window's resources next to the ones already there."""
import os
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'session'))
import dpi

USER = '''WINE REGISTRY Version 2
;; All keys relative to \\\\User\\\\S-1-5-21-0-0-0-1000

#arch=win64

[Control Panel\\\\Desktop] 1791724368
#time=1dd598236b925d0
"ActiveWndTrackTimeout"=dword:00000000
{logpixels}"Wallpaper"="C:\\\\windows\\\\x.bmp"

[Control Panel\\\\Desktop\\\\WindowMetrics] 1791736648
"LogPixels"=dword:000000c0
'''
SYSTEM = '''WINE REGISTRY Version 2

[System\\\\ControlSet001\\\\Hardware Profiles\\\\Current\\\\Software\\\\Fonts] 1791739461
#time=1dd59a55ab20b14
"FONTS.FON"="jvgasys.fon"
{logpixels}
'''


class PrefixDpi(unittest.TestCase):
    def prefix(self, user, system):
        directory = Path(tempfile.mkdtemp())
        (directory / 'user.reg').write_text(USER.format(logpixels=user))
        (directory / 'system.reg').write_text(SYSTEM.format(logpixels=system))
        return directory

    def test_user_value_wins(self):
        self.assertEqual(dpi.prefix_dpi(self.prefix('"LogPixels"=dword:00000078\n', '"LogPixels"=dword:00000090')), 120)

    def test_hardware_profile_fallback(self):
        # A value under another key (WindowMetrics) is not Wine's.
        self.assertEqual(dpi.prefix_dpi(self.prefix('', '"LogPixels"=dword:00000090')), 144)

    def test_default(self):
        self.assertEqual(dpi.prefix_dpi(self.prefix('', '')), 96)
        self.assertEqual(dpi.prefix_dpi(Path(tempfile.mkdtemp())), 96)


class Environment(unittest.TestCase):
    def test_xp_cursor_sizes(self):
        # XP's SM_CXCURSOR at 96, 108, 120, 132, 144, 168, 192 and 216 DPI.
        for value, size in ((96, 32), (108, 32), (120, 32), (132, 32), (144, 32), (168, 64), (192, 64), (216, 64)):
            self.assertEqual(dpi.cursor_size(value), size, value)

    def test_gtk_whole_factors(self):
        self.assertNotIn('GDK_SCALE', dpi.environment(144))
        self.assertEqual(dpi.environment(192)['GDK_SCALE'], '2')
        self.assertEqual(dpi.environment(192)['GDK_DPI_SCALE'], '0.5')
        self.assertEqual(dpi.environment(288)['GDK_SCALE'], '3')

    def test_wayland_leaves_scale_to_outputs(self):
        self.assertNotIn('GDK_SCALE', dpi.environment(192, wayland=True))
        self.assertEqual(dpi.environment(192, wayland=True)['XCURSOR_SIZE'], '64')


@unittest.skipUnless(os.environ.get('DISPLAY'), 'needs an X display')
class XftDpi(unittest.TestCase):
    def test_merge(self):
        from Xlib import Xatom, display
        d = display.Display()
        root = d.screen().root
        atom = d.intern_atom('RESOURCE_MANAGER')
        saved = root.get_full_property(atom, Xatom.STRING)
        try:
            root.change_property(atom, Xatom.STRING, 8, b'*customization:\t-color\nXft.dpi:\t96\n')
            dpi.set_xft_dpi(d, 144)
            value = root.get_full_property(atom, Xatom.STRING).value
            value = value.decode() if isinstance(value, bytes) else value
            self.assertEqual(value.splitlines(), ['*customization:\t-color', 'Xft.dpi:\t144'])
        finally:
            if saved:
                root.change_property(atom, Xatom.STRING, 8, saved.value)
            else:
                root.delete_property(atom)
            d.flush()


if __name__ == '__main__':
    unittest.main()
