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
while IFS= read -r patch; do
    [[ -z $patch || $patch == \#* ]] && continue
    if git -C "$src" apply --reverse --check "$root/wine/patches/$patch" 2>/dev/null; then
        continue
    fi
    git -C "$src" apply --check "$root/wine/patches/$patch"
    git -C "$src" apply "$root/wine/patches/$patch"
done < "$root/wine/patches/series"
mkdir -p "$build" "$install"
cd "$build"
export PKG_CONFIG_LIBDIR=${PKG_CONFIG_LIBDIR:-/usr/lib/i386-linux-gnu/pkgconfig:/usr/share/pkgconfig}
# DirectX helper version copies are outside the XP desktop milestones.
extra=()
for version in {24..43}; do extra+=("--disable-d3dx9_$version"); done
for version in {33..43}; do extra+=("--disable-d3dx10_$version"); done
for version in {33..42} 46 47; do extra+=("--disable-d3dcompiler_$version"); done
"$src/configure" --prefix="$install" --without-wayland --disable-tests "${extra[@]}" CC='gcc -m32' CXX='g++ -m32'
make -j"${JOBS:-6}"
make install
"$install/bin/wine" --version
