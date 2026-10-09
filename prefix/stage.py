#!/usr/bin/env python3
"""Restore native shell DLLs atomically after Wine prefix updates."""
import filecmp
import os
from pathlib import Path
import shutil
import sys
import tempfile

def stage(prefix):
    if not (prefix / 'xp-shell.json').is_file():
        raise SystemExit('Not a dedicated XP prefix')
    system = prefix / 'drive_c/windows/system32'
    for name in ('shell32', 'shdocvw', 'browseui', 'shlwapi', 'linkinfo'):
        source = prefix / f'drive_c/xp/native/{name}.dll'
        target = system / f'{name}.dll'
        if target.is_file() and filecmp.cmp(source, target, shallow=False):
            continue
        # Never truncate an image that a running process may have mapped.
        with tempfile.NamedTemporaryFile(dir=system, prefix='.xp-stage-', delete=False) as temp:
            path = Path(temp.name)
        try:
            shutil.copyfile(source, path)
            os.replace(path, target)
        finally:
            path.unlink(missing_ok=True)

if __name__ == '__main__':
    stage(Path(sys.argv[1]).resolve(strict=True))
