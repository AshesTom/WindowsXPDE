#!/usr/bin/env python3
"""Build synthetic property fixtures against the actual Wine source parser.

No theme assets or Microsoft implementation are read. Parser source is included
only in an ignored temporary translation unit, retaining its Wine license.
"""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source', type=Path, default=root / '.local/wine-upstream/dlls/uxtheme/msstyles.c')
parser.add_argument('--output', type=Path, default=root / '.local/bridge/font-parser-test.exe')
args = parser.parse_args()
source = args.source.read_text()
start = source.rindex('static BOOL MSSTYLES_GetNextLong(')
end = source.index('/***********************************************************************', source.index('HRESULT MSSTYLES_GetPropertyFont('))
block = source[start:end]
# Keep tokenizer/integer helpers and font accessors, omit unrelated accessors.
a = block.index('/***********************************************************************')
b = block.index('static HRESULT MSSTYLES_GetFont')
block = block[:a] + block[b:]
license_text = source[:source.index('#include')]
property_header = args.source.with_suffix('.h').read_text()
property_start = property_header.index('typedef struct _THEME_PROPERTY')
property_end = property_header.index('} THEME_PROPERTY, *PTHEME_PROPERTY;', property_start) + len('} THEME_PROPERTY, *PTHEME_PROPERTY;')
property_type = property_header[property_start:property_end]
header = '''#include <windows.h>
#include <uxtheme.h>
#include <stdio.h>
#include <string.h>
#define TRACE(...) ((void)0)
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#define E_PROP_ID_UNSUPPORTED ((HRESULT)0x80070490)
''' + property_type + '\n'
args.output.parent.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix='font-parser-', dir=root / '.local') as temporary:
    unit = Path(temporary) / 'font-parser.c'
    unit.write_text(license_text + header + block + (root / 'tests/font-parser.c').read_text())
    subprocess.run([os.environ.get('MINGW_CC', 'i686-w64-mingw32-gcc'), '-std=c11',
                    '-Wall', '-Wextra', '-Werror', '-O2', '-municode', str(unit),
                    '-o', str(args.output), '-lgdi32', '-luser32'], check=True)
