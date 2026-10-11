#!/usr/bin/env python3
"""Reject executable/font/media/hive material from the Git index.

PNG images under docs/images/ (the README screenshot) are the only binaries allowed."""
from pathlib import Path
import subprocess
import sys

paths = subprocess.check_output(['git', 'diff', '--cached', '--name-only', '-z', '--diff-filter=ACMR']).split(b'\0')
for raw in paths:
    if not raw:
        continue
    name = raw.decode()
    content = subprocess.check_output(['git', 'show', ':' + name])
    image = Path(name).parent == Path('docs/images') and content[:8] == b'\x89PNG\r\n\x1a\n'
    if (Path(name).parts[0] in ('localres', '.local', 'runtime', 'build', 'vendor') or
            Path(name).suffix.lower() in ('.exe', '.dll', '.msstyles', '.ttf', '.ttc', '.fon', '.iso', '.cab', '.hiv') or
            content[:2] == b'MZ' or content[:4] == b'regf' or (b'\0' in content and not image)):
        print(f'Refusing local or binary artifact in index: {name}', file=sys.stderr)
        sys.exit(1)
