#!/bin/bash
set -euo pipefail
root=$(cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$root/.local/bridge"
compile() {
    local target=$1 temporary
    shift
    temporary=$(mktemp "$target.XXXXXX")
    if "${MINGW_CC:-i686-w64-mingw32-gcc}" -std=c11 -Wall -Wextra -Werror -O2 \
        "$@" -o "$temporary"; then
        chmod 755 "$temporary"
        mv -f -- "$temporary" "$target"
    else
        rm -f -- "$temporary"
        return 1
    fi
}
# The reference probe can stay open while a new version is built.
# Publish a new inode instead of truncating a PE mapped by Wine.
for name in shellhook button-size system-menu rebar print-window foreign-icons; do
    compile "$root/.local/bridge/$name-test.exe" -municode "$root/tests/$name.c" -luser32 -lgdi32
done
compile "$root/.local/bridge/class-probe.exe" -municode "$root/prefix/class-probe.c" -lole32 -luuid

compile "$root/.local/bridge/display-test.exe" -s "$root/tests/display.c" -luxtheme -lgdi32

for name in start-button theme-font theme-metrics taskbar-background clock-theme scrollbar-theme rebar-theme reference-metrics; do
    compile "$root/.local/bridge/$name-test.exe" -municode "$root/tests/$name.c" -luxtheme -lgdi32
done

compile "$root/.local/bridge/nonclient-theme-test.exe" -municode "$root/tests/nonclient-theme.c" -luxtheme -lgdi32

# Synthetic strings exercise the source parser without reading theme assets.
python3 "$root/tests/font_parser.py"
