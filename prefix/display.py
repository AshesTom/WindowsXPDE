#!/usr/bin/env python3
"""Set classic/Luna and DPI in a dedicated prefix; restart the session afterwards."""
import argparse
import os
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--theme', choices=('classic', 'luna'), default='classic')
parser.add_argument('--menu', choices=('classic', 'xp'), help='Start menu layout, independent of theme')
parser.add_argument('--dpi', type=int, choices=(96, 144, 192), default=96)
parser.add_argument('--font-rendering', choices=('xp', 'wine'),
                    help='Prefix-wide Wine synthetic bold/italic antialiasing policy')
args = parser.parse_args()
prefix = Path(os.environ['WINEPREFIX']).resolve(strict=True)
if not (prefix / 'xp-shell.json').is_file():
    raise SystemExit('Not a dedicated XP prefix')
if args.theme == 'luna' and not (prefix / 'drive_c/windows/Resources/Themes/Luna/luna.msstyles').is_file():
    raise SystemExit('Luna must be extracted from your local XP media first')
theme_helper = Path(__file__).resolve().parents[1] / '.local/bridge/theme-settings.exe'
if not theme_helper.is_file():
    raise SystemExit('Build bridge-wm before selecting a theme')
wine = os.environ.get('WINE', 'wine')
env = dict(os.environ, WINEDLLOVERRIDES='explorer,shell32,shdocvw,browseui,shlwapi,uxtheme=b;mscoree,mshtml=')
def reg(key, name, value, kind='REG_SZ'):
    subprocess.run([wine, 'reg', 'add', key, '/v', name, '/t', kind, '/d', str(value), '/f'], env=env, check=True)
if args.font_rendering:
    reg(r'HKCU\Software\Wine\Fonts', 'AntialiasFakeBoldOrItalic',
        'N' if args.font_rendering == 'xp' else 'Y')
if args.menu:
    helper = Path(__file__).resolve().parents[1] / '.local/bridge/menu-settings.exe'
    if not helper.is_file():
        raise SystemExit('Build bridge-wm before selecting a Start menu')
    reg(r'HKCU\Software\Microsoft\Windows\CurrentVersion\Policies\Explorer',
        'NoSimpleStartMenu', int(args.menu == 'classic'), 'REG_DWORD')
    # n,b: in a 64-bit prefix Wine starts the 32-bit helper through the 64-bit
    # start.exe, which has no native shell32 and must keep the builtin one.
    menu_env = dict(env, WINEDLLOVERRIDES='shell32,shlwapi=n,b;explorer=b;mscoree,mshtml=')
    subprocess.run([wine, str(helper), args.menu], env=menu_env, check=True)
reg(r'HKCU\Control Panel\Desktop', 'LogPixels', args.dpi, 'REG_DWORD')
subprocess.run([wine, str(theme_helper), args.theme],
               env=dict(env, WIN4WINE_EXTERNAL_SHELL='1'), check=True)
print('Display settings saved. Restart the dedicated Wine session to apply.')
