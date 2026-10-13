#!/usr/bin/env python3
"""The registry profile follows the language of the XP media (Japanese or English XP).

  tests/xp_language.py [ja=ASSETS] [en=ASSETS]

With extract/extract.py outputs of Japanese or English XP media, it also checks
the language detected from them (extract/media.py).
"""
from pathlib import Path
import struct
import sys
import unittest
root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / 'prefix'))
sys.path.insert(0, str(root / 'extract'))
import media
import registry

ASSETS = {}


def commands(language, existing=False):
    """The reg commands of a profile install into an empty (or, with EXISTING, a filled) prefix."""
    issued = []
    registry.install(lambda *args: issued.append(args), lambda key, name: existing, language)
    return issued


def values(issued, key, name):
    return [args[args.index('/d') + 1] for args in issued
            if args[:3] == ('reg', 'add', key) and '/v' in args and args[args.index('/v') + 1] == name]


class Language(unittest.TestCase):
    def test_japanese_profile(self):
        issued = commands('ja')
        self.assertTrue(values(issued, registry.FONT_PROFILE, 'CaptionFont'))
        self.assertEqual(values(issued, r'HKCU\Control Panel\Desktop\WindowMetrics', 'IconSpacing'), ['-1170'])
        self.assertEqual(values(issued, r'HKCU\Console', 'FaceName'), ['MS Gothic'])
        self.assertFalse([args for args in issued if 'MS Shell Dlg' in args])

    def test_english_profile(self):
        issued = commands('en')
        # English Luna: Wine reads the theme's own fonts; the 75px icon spacing is Wine's default.
        self.assertFalse([args for args in issued if registry.FONT_PROFILE in args])
        self.assertFalse(values(issued, r'HKCU\Control Panel\Desktop\WindowMetrics', 'IconSpacing'))
        self.assertEqual(values(issued, r'HKCU\Console', 'FaceName'), ['Terminal'])
        self.assertEqual(values(issued, r'HKCU\Console', 'FontSize'), ['0x000c0008'])
        shell_dlg = [args for args in issued if 'MS Shell Dlg' in args]
        self.assertEqual(len(shell_dlg), 1)
        self.assertIn('Microsoft Sans Serif', shell_dlg[0])
        # Wine reads font substitutes in the 64-bit view; the explicit view is kept.
        self.assertEqual(registry.shell_view(shell_dlg[0]), shell_dlg[0])
        self.assertEqual(shell_dlg[0][-1], '/reg:64')

    def test_existing_values_kept(self):
        for language in ('ja', 'en'):
            issued = commands(language, existing=True)
            self.assertFalse(values(issued, r'HKCU\Console', 'FaceName'))

    def test_shell_view(self):
        self.assertEqual(registry.shell_view(('reg', 'add', 'K', '/f')), ('reg', 'add', 'K', '/f', '/reg:32'))
        self.assertEqual(registry.shell_view(('wineboot', '-u')), ('wineboot', '-u'))

    def test_classic_fonts(self):
        for language, face, height in (('ja', 'MS UI Gothic', -12), ('en', 'Tahoma', -11)):
            issued = []
            registry.install_classic_fonts(lambda *args: issued.append(args), lambda key, name: False, language)
            self.assertEqual(len(issued), 6)
            for args in issued:
                fields = struct.unpack('<5l8B64s', bytes.fromhex(args[args.index('/d') + 1]))
                self.assertEqual(fields[0], height)
                self.assertEqual(fields[13].decode('utf-16le').rstrip('\0'), face)
                self.assertEqual(fields[4], 700 if args[4] in ('CaptionFont', 'SmallCaptionFont') else 400)

    def test_media_language(self):
        if not ASSETS:
            self.skipTest('no extracted assets given')
        for expected, path in ASSETS.items():
            self.assertEqual(media.language(path), expected)
            self.assertEqual(media.LOCALES[expected].split('_')[0], expected)
            self.assertIsNone(media.warning(path))

    def test_untested_language(self):
        # German XP's shell32: set up as English XP, with a warning.
        saved = media.resource_languages
        media.resource_languages = lambda path, rtype: [0x407]
        try:
            self.assertEqual(media.language('assets'), 'en')
            self.assertIn('0x0407', media.warning('assets'))
        finally:
            media.resource_languages = saved


if __name__ == '__main__':
    for arg in sys.argv[1:]:
        name, _, path = arg.partition('=')
        ASSETS[name] = path
    unittest.main(argv=sys.argv[:1])
