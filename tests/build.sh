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
for name in shellhook title-change shell-activation shell-layout-sync wine-maximize workarea no-redraw button-size system-menu rebar print-window foreign-icons mono-advance text-vcenter taskbar-fonts nc-context-menu marlett-caption frame-control default-position ui-state screen-capture text-mask; do
    compile "$root/.local/bridge/$name-test.exe" -municode "$root/tests/$name.c" -luser32 -lgdi32
done
compile "$root/.local/bridge/toolbar-layout-test.exe" "$root/tests/toolbar-layout.c" -luser32 -lgdi32
compile "$root/.local/bridge/stretch-map-test.exe" "$root/tests/stretch-map.c" -lgdi32 -lmsimg32
compile "$root/.local/bridge/margin-shrink-test.exe" "$root/tests/margin-shrink.c" -luxtheme -lgdi32 -luser32
compile "$root/.local/bridge/text-wrap-test.exe" "$root/tests/text-wrap.c" -lgdi32 -luser32
compile "$root/.local/bridge/alpha-blend-test.exe" "$root/tests/alpha-blend.c" -lgdi32
compile "$root/.local/bridge/part-black-test.exe" "$root/tests/part-black.c" -luxtheme -lgdi32 -luser32
compile "$root/.local/bridge/menu-layout-test.exe" "$root/tests/menu-layout.c" -luser32 -lgdi32
compile "$root/.local/bridge/class-probe.exe" -municode "$root/prefix/class-probe.c" -lole32 -luuid

compile "$root/.local/bridge/display-test.exe" -s "$root/tests/display.c" -luxtheme -lgdi32

for name in start-button start-sizing start-metrics theme-font theme-text-shadow theme-metrics classic-fonts font-profile taskbar-background clock-theme scrollbar-theme rebar-theme rebar-gripper rebar-tail reference-metrics shell-layout caption-text nonclient-caption caption-icons gradient-clip halftone-clip clock-metrics clock-pixels taskbar-text theme-part-pixels window-theme-empty theme-props resource-languages; do
    compile "$root/.local/bridge/$name-test.exe" -municode "$root/tests/$name.c" -luxtheme -lgdi32
done

compile "$root/.local/bridge/shell-names-test.exe" -municode "$root/tests/shell-names.c" -ladvapi32
compile "$root/.local/bridge/desktop-icons.exe" -municode "$root/tests/desktop-icons.c" -ladvapi32 -lshell32
compile "$root/.local/bridge/text-widths-test.exe" -municode "$root/tests/text-widths.c" -lgdi32 -luser32
compile "$root/.local/bridge/folder-icons-test.exe" -municode "$root/tests/folder-icons.c" -lshell32 -lole32 -luuid
compile "$root/.local/bridge/reg-dump-test.exe" -municode "$root/tests/reg-dump.c" -ladvapi32
compile "$root/.local/bridge/string-id-test.exe" -municode "$root/tests/string-id.c"
compile "$root/.local/bridge/transparent-shrink-test.exe" -municode "$root/tests/transparent-shrink.c" -lmsimg32 -lgdi32

compile "$root/.local/bridge/nonclient-theme-test.exe" -municode "$root/tests/nonclient-theme.c" -luxtheme -lgdi32

# Synthetic strings exercise the source parser without reading theme assets.
python3 "$root/tests/font_parser.py"

# Self-authored activation manifest selects the public v6 Button API on XP.
"${MINGW_WINDRES:-i686-w64-mingw32-windres}" -I "$root/tests" \
    "$root/tests/classic-start-sizing.rc" "$root/.local/bridge/classic-start-sizing-res.o"
compile "$root/.local/bridge/classic-start-sizing-test.exe" -municode \
    "$root/tests/classic-start-sizing.c" "$root/.local/bridge/classic-start-sizing-res.o" \
    -luxtheme -lcomctl32 -lgdi32
