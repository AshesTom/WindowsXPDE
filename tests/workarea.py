#!/usr/bin/env python3
"""Exercise public work-area APIs against reservations on an owned Xvfb.

No Microsoft DLLs or assets are needed. The default prefix is newly created
and discarded; --prefix-template optionally copies an existing test fixture.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time

from Xlib import Xatom, display


def wait_line(log, needle, process, timeout=20):
    until = time.monotonic() + timeout
    while time.monotonic() < until:
        text = log.read_text(errors="replace")
        if needle in text:
            return
        if process.poll() is not None:
            raise AssertionError(f"Missing {needle}, exit {process.returncode}:\n{text}")
        time.sleep(0.05)
    raise AssertionError(f"Timed out waiting for {needle}:\n{log.read_text(errors='replace')}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--wine", required=True, type=Path)
    parser.add_argument("--probe", type=Path, default=Path(".local/bridge/workarea-test.exe"))
    parser.add_argument("--prefix-template", type=Path)
    parser.add_argument("--opt-out", action="store_true")
    parser.add_argument("--expect-stale", action="store_true", help="Verify original cached API behavior")
    args = parser.parse_args()
    wine, probe = args.wine.resolve(), args.probe.resolve()
    with tempfile.TemporaryDirectory(prefix="workarea-", dir=".local") as directory:
        base = Path(directory).resolve()
        prefix, control, log = base / "prefix", base / "control.txt", base / "public.log"
        if args.prefix_template:
            shutil.copytree(args.prefix_template, prefix, symlinks=True)
        env = os.environ.copy()
        env.update(WINEPREFIX=str(prefix), WINEARCH="win64", WINEDEBUG="-all",
                   WINEDLLOVERRIDES="mscoree,mshtml=")
        env.pop("WIN4WINE_EXTERNAL_SHELL", None)
        if not args.opt_out:
            env["WIN4WINE_EXTERNAL_SHELL"] = "1"
        read_fd, write_fd = os.pipe()
        with (base / "xvfb.log").open("w") as xlog:
            xvfb = subprocess.Popen(["Xvfb", "-displayfd", str(write_fd), "-screen", "0",
                                     "1024x768x24", "-nolisten", "tcp"],
                                    pass_fds=(write_fd,), stdout=xlog, stderr=xlog)
        os.close(write_fd)
        connection = None
        try:
            with os.fdopen(read_fd) as number:
                chosen = number.readline().strip()
            if not chosen or xvfb.poll() is not None:
                raise AssertionError((base / "xvfb.log").read_text())
            env["DISPLAY"] = ":" + chosen
            connection = display.Display(env["DISPLAY"])
            root = connection.screen().root
            net = connection.intern_atom("_NET_WORKAREA")
            gtk = connection.intern_atom("_GTK_WORKAREAS_D0")

            def area(height):
                root.change_property(net, Xatom.CARDINAL, 32, [0, 0, 1024, height])
                connection.sync()

            area(768)
            with log.open("w") as output:
                command = [str(wine), str(probe), str(control)]
                if args.expect_stale:
                    command.append("--expect-stale")
                process = subprocess.Popen(command, env=env, stdout=output, stderr=output)
                wait_line(log, "READY", process, timeout=60)
                for phase, height in [(1, 734), (2, 734), (3, 738), (4, 738), (5, 718),
                                      (6, 738), (7, 768), (8, 734), (9, 738), (10, 738)]:
                    if phase in (2, 4):
                        if phase == 2:
                            # Invalid or truncated arrays must not refresh shared
                            # monitor state or create notification storms.
                            for kind, bits, values in [
                                (Xatom.STRING, 8, b"invalid"),
                                (Xatom.CARDINAL, 16, [0, 0, 1024, height]),
                                (Xatom.CARDINAL, 32, [0, 0, 1024, height, 1]),
                                (Xatom.CARDINAL, 32, [0, 0, 1024, height] + [0] * 4096),
                            ]:
                                root.change_property(net, kind, bits, values)
                                connection.sync()
                                time.sleep(0.15)
                                area(height)
                                time.sleep(0.15)
                        for _ in range(20):
                            area(height)
                            time.sleep(0.01)
                    elif phase == 5:
                        root.change_property(gtk, Xatom.CARDINAL, 32, [0, 0, 1024, height])
                        connection.sync()
                    elif phase == 6:
                        root.delete_property(gtk)
                        connection.sync()
                    elif phase == 7:
                        root.delete_property(net)
                        connection.sync()
                    elif phase < 10:
                        area(height)
                    temporary = control.with_suffix(".tmp")
                    temporary.write_text(f"{phase} 0 0 1024 {height}\n")
                    temporary.replace(control)
                    wait_line(log, f"STEP {phase}" if phase < 10 else "PASS:", process)
                process.wait(timeout=10)
                if process.returncode:
                    raise AssertionError(log.read_text())
            print(log.read_text(), end="")
        finally:
            try:
                subprocess.run([str(wine.parent / "wineserver"), "-k"], env=env,
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=10)
                subprocess.run([str(wine.parent / "wineserver"), "-w"], env=env,
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=10)
            finally:
                if connection:
                    connection.close()
                xvfb.terminate()
                xvfb.wait(timeout=5)


if __name__ == "__main__":
    main()
