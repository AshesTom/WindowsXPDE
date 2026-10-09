#!/bin/bash
set -euo pipefail
root=$(cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$root/.local/bridge"
for name in shellhook button-size; do
    "${MINGW_CC:-i686-w64-mingw32-gcc}" -std=c11 -Wall -Wextra -Werror -O2 -municode \
        "$root/tests/$name.c" -o "$root/.local/bridge/$name-test.exe" -luser32 -lgdi32
done
"${MINGW_CC:-i686-w64-mingw32-gcc}" -std=c11 -Wall -Wextra -Werror -O2 -municode \
    "$root/prefix/class-probe.c" -o "$root/.local/bridge/class-probe.exe" -lole32 -luuid

"${MINGW_CC:-i686-w64-mingw32-gcc}" -std=c11 -Wall -Wextra -Werror -O2 -s \
    "$root/tests/display.c" -o "$root/.local/bridge/display-test.exe" -luxtheme -lgdi32
