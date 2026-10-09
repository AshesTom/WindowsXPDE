#!/bin/bash
set -euo pipefail
root=$(cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$root/.local/bridge"
"${MINGW_CC:-i686-w64-mingw32-gcc}" -std=c11 -Wall -Wextra -Werror -O2 -municode \
    "$root/bridge-wm/proxy.c" -o "$root/.local/bridge/proxy.exe" -luser32
"${MINGW_CC:-i686-w64-mingw32-gcc}" -std=c11 -Wall -Wextra -Werror -O2 -municode \
    "$root/bridge-wm/inspect.c" -o "$root/.local/bridge/inspect.exe" -luser32 -lole32 -lshell32 -luuid

"${MINGW_CC:-i686-w64-mingw32-gcc}" -std=c11 -Wall -Wextra -Werror -O2 -municode \
    "$root/prefix/menu-settings.c" -o "$root/.local/bridge/menu-settings.exe" -lshell32
