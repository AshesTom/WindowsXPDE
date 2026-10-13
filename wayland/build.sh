#!/bin/bash
# Build the Wayland session's compositor: labwc with win4wine's patches, and
# wlroots with its own patch built into it (a static subproject), so that the
# system's wlroots and labwc stay as they are.
set -euo pipefail
root=$(cd -- "$(dirname -- "$0")/.." && pwd)
source "$root/wayland/upstream.env"
src=${LABWC_SOURCE:-$root/.local/labwc-upstream}
build=${LABWC_BUILD:-$root/.local/labwc-build}
install=${LABWC_INSTALL:-$root/.local/labwc-install}
fetch() {  # fetch DIR URL TAG COMMIT
    if [[ ! -d $1/.git ]]; then
        echo ">>> Fetching $(basename "$2" .git)"
        git -c advice.detachedHead=false clone -q --depth 1 --branch "$3" "$2" "$1"
    fi
    [[ $(git -C "$1" rev-parse HEAD) == "$4" ]] || { echo "Unexpected upstream commit in $1" >&2; exit 1; }
}
fetch "$src" "$LABWC_URL" "$LABWC_TAG" "$LABWC_COMMIT"
fetch "$src/subprojects/wlroots" "$WLROOTS_URL" "$WLROOTS_TAG" "$WLROOTS_COMMIT"
fetch "$src/subprojects/libsfdo" "$LIBSFDO_URL" "$LIBSFDO_TAG" "$LIBSFDO_COMMIT"
echo '>>> Patching'
# This checkout's own sources may be returned to upstream for a changed
# series; a source given with LABWC_SOURCE keeps its edits.
reset=(--reset)
[[ -z ${LABWC_SOURCE:-} ]] || reset=()
python3 "$root/wine/patch-state.py" "$src" "$root/wayland/patches/labwc" --apply "${reset[@]}"
python3 "$root/wine/patch-state.py" "$src/subprojects/wlroots" "$root/wayland/patches/wlroots" --apply "${reset[@]}"
if [[ ! -f $build/build.ninja ]]; then
    echo '>>> Configuring'
    meson setup "$build" "$src" --prefix="$install" --buildtype=release \
        --force-fallback-for=wlroots-0.18,libsfdo-basedir,libsfdo-desktop,libsfdo-icon \
        -Dxwayland=enabled -Dman-pages=disabled -Dwlroots:xwayland=enabled -Dwlroots:backends=drm,libinput,x11 -Dwlroots:renderers=gles2
fi
echo '>>> Compiling'
ninja -C "$build" -j"${JOBS:-6}"
echo '>>> Installing'
ninja -C "$build" install
# The patches this install was built with, for scripts/setup.sh
python3 "$root/wine/patch-state.py" --digest "$root/wayland/patches/labwc" "$root/wayland/patches/wlroots" \
    > "$install/win4wine-patches"
"$install/bin/labwc" --version
