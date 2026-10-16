#!/usr/bin/env python3
"""Detect an ordered, overlapping patch series without changing local edits."""
import argparse
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def git_apply(source, patch, *options, check=False):
    # A source without its own .git (an unpacked tarball) must not be taken
    # for a subdirectory of an enclosing repository: git apply would then
    # skip every path outside it and still succeed.
    env = dict(os.environ, GIT_CEILING_DIRECTORIES=str(Path(source).resolve().parent))
    result = subprocess.run(
        ["git", "-C", str(source), "apply", *options, str(patch)],
        text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env)
    if result.returncode and not check:
        raise RuntimeError(f"{patch.name}: {result.stderr.strip()}")
    return result.returncode == 0


def patch_paths(patches):
    paths = set()
    for patch in patches:
        for line in patch.read_text().splitlines():
            # Include old names too, so deletion/rename patches remain safe.
            if line.startswith(("+++ b/", "--- a/")):
                name = line[6:].split("\t", 1)[0]
                path = Path(name)
                if path.is_absolute() or ".." in path.parts:
                    raise RuntimeError(f"Unsafe patch path: {name}")
                paths.add(path)
    return paths


def ensure_series(source, directory, apply=False):
    source, directory = Path(source).resolve(), Path(directory).resolve()
    patches = [directory / line.strip()
               for line in (directory / "series").read_text().splitlines()
               if line.strip() and not line.lstrip().startswith("#")]
    missing = []
    with tempfile.TemporaryDirectory(prefix="win4wine-patches-") as temp:
        scratch = Path(temp)
        subprocess.run(["git", "init", "-q", str(scratch)], check=True)
        for relative in patch_paths(patches):
            original, copied = source / relative, scratch / relative
            if original.is_file():
                copied.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(original, copied)
        # Later patches can modify earlier hunks. Undo them on the copy first.
        for patch in reversed(patches):
            if git_apply(scratch, patch, "--reverse", "--check", check=True):
                git_apply(scratch, patch, "--reverse")
            else:
                missing.append(patch)
        # Validate the entire reconstruction before touching the real source.
        for patch in patches:
            git_apply(scratch, patch, "--check")
            git_apply(scratch, patch)
    missing.reverse()
    if apply:
        for patch in missing:
            git_apply(source, patch, "--check")
            git_apply(source, patch)
    return [patch.name for patch in missing]


def digest(directories):
    """A digest of the series and patches in DIRECTORIES, to tell a build's patches."""
    hasher = hashlib.sha256()
    for directory in directories:
        series = (directory / "series").read_text()
        hasher.update(series.encode())
        for line in series.splitlines():
            if line.strip() and not line.lstrip().startswith("#"):
                hasher.update(line.strip().encode() + b"\0" + (directory / line.strip()).read_bytes())
    return hasher.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, nargs="?")
    parser.add_argument("patches", type=Path, nargs="?")
    parser.add_argument("--apply", action="store_true")
    parser.add_argument("--digest", type=Path, nargs="+", metavar="PATCHES",
                        help="print a digest of these patch directories and exit")
    args = parser.parse_args()
    if args.digest:
        print(digest(args.digest))
        return
    if not args.source or not args.patches:
        parser.error("give the source and the patch directory")
    try:
        missing = ensure_series(args.source, args.patches, args.apply)
    except (OSError, RuntimeError) as error:
        parser.exit(1, f"Patch series validation failed: {error}\n")
    for name in missing:
        print(f"{'Applied' if args.apply else 'Missing'} {name}")


if __name__ == "__main__":
    main()
