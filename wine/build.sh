#!/bin/bash
set -euo pipefail
root=$(cd -- "$(dirname -- "$0")/.." && pwd)
source "$root/wine/upstream.env"
src=${WINE_SOURCE:-$root/.local/wine-upstream}
build=${WINE_BUILD:-$root/.local/wine-build}
install=${WINE_INSTALL:-$root/.local/wine-install}
if [[ ! -d $src/.git ]]; then
    git clone --depth 1 --branch "$WINE_TAG" "$WINE_URL" "$src"
fi
[[ $(git -C "$src" rev-parse HEAD) == "$WINE_COMMIT" ]] || { echo 'Unexpected upstream commit' >&2; exit 1; }
python3 "$root/wine/patch-state.py" "$src" "$root/wine/patches" --apply
mkdir -p "$build" "$install"
cd "$build"
# DirectX helper version copies are outside the XP desktop milestones.
extra=()
for version in {24..43}; do extra+=("--disable-d3dx9_$version"); done
for version in {33..43}; do extra+=("--disable-d3dx10_$version"); done
for version in {33..42} 46 47; do extra+=("--disable-d3dcompiler_$version"); done
# New WoW64: 64-bit Unix libraries only, with i386 and x86_64 PE modules, so
# the host needs no i386 libraries and runs 32-bit and 64-bit applications.
"$src/configure" --prefix="$install" --enable-archs=i386,x86_64 --without-wayland --disable-tests "${extra[@]}"
make -j"${JOBS:-6}"
make install
# Isolate Wine's themed controls from the XP common-control module. Since
# Wine 11 they are in comctl32_v6; comctl32 is the version 5 module.
# Both architectures: 64-bit applications in the session load it too.
for arch in i386 x86_64; do
    controls_tmp=$(mktemp "$install/lib/wine/$arch-windows/win4wine-controls.dll.XXXXXX")
    cp -p -- "$install/lib/wine/$arch-windows/comctl32_v6.dll" "$controls_tmp"
    mv -f -- "$controls_tmp" "$install/lib/wine/$arch-windows/win4wine-controls.dll"
done
"$install/bin/wine" --version
