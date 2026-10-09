#!/usr/bin/env python3
"""Exercise patch detection on overlapping revisions and retained local edits."""
import difflib
import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location(
    "patch_state", Path(__file__).resolve().parents[1] / "wine/patch-state.py")
patch_state = importlib.util.module_from_spec(spec)
spec.loader.exec_module(patch_state)


class PatchStateTests(unittest.TestCase):
    def exercise(self, applied, custom=False, conflict=False):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source, patches = root / "source", root / "patches"
            source.mkdir()
            patches.mkdir()
            def contents(number):
                return f"revision={number}\n" + "unchanged\n" * 30 + "local=original\n"
            names = []
            for number in range(1, 19):
                name = f"{number:04}.patch"
                names.append(name)
                (patches / name).write_text("".join(difflib.unified_diff(
                    contents(number - 1).splitlines(True),
                    contents(number).splitlines(True), "a/code.c", "b/code.c")))
            (patches / "series").write_text("\n".join(names) + "\n")
            original = contents(applied)
            if custom:
                original = original.replace("local=original", "local=retained")
            target = source / "code.c"
            if conflict:
                # Earlier missing patches are valid; a late, independent hunk
                # conflicts. Preflight must prevent applying those earlier ones.
                conflict_file = source / "late.c"
                conflict_file.write_text("local conflicting edit\n")
                late = "0019.patch"
                (patches / late).write_text("".join(difflib.unified_diff(
                    ["original\n"], ["patched\n"], "a/late.c", "b/late.c")))
                with (patches / "series").open("a") as series:
                    series.write(late + "\n")
            target.write_text(original)
            if conflict:
                with self.assertRaises(RuntimeError):
                    patch_state.ensure_series(source, patches, apply=True)
                self.assertEqual(target.read_bytes(), original.encode())
                self.assertEqual(conflict_file.read_bytes(), b"local conflicting edit\n")
                return
            self.assertEqual(patch_state.ensure_series(source, patches), names[applied:])
            self.assertEqual(target.read_text(), original)
            self.assertEqual(patch_state.ensure_series(source, patches, apply=True), names[applied:])
            expected = contents(18)
            if custom:
                expected = expected.replace("local=original", "local=retained")
            self.assertEqual(target.read_text(), expected)
            self.assertEqual(patch_state.ensure_series(source, patches, apply=True), [])

    def test_fresh(self):
        self.exercise(0)

    def test_partial_eleven(self):
        self.exercise(11)

    def test_complete_eighteen(self):
        self.exercise(18)

    def test_incremental_seventeen(self):
        self.exercise(17)

    def test_retains_local_edit(self):
        self.exercise(17, custom=True)

    def test_conflict_preserves_actual_source(self):
        self.exercise(11, conflict=True)


if __name__ == "__main__":
    unittest.main()
