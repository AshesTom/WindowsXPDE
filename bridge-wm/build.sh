#!/bin/bash
set -euo pipefail
root=$(cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$root/.local/bridge"
compile() {
    local target=$1
    shift
    local temporary
    temporary=$(mktemp "$target.XXXXXX")
    if "${MINGW_CC:-i686-w64-mingw32-gcc}" -std=c11 -Wall -Wextra -Werror -O2 -municode \
        "$@" -o "$temporary"; then
        chmod 755 "$temporary"
        mv -f -- "$temporary" "$target"
    else
        rm -f -- "$temporary"
        return 1
    fi
}
# Sessions may still map these PE files. Publish new inodes without truncating
# an executable that another Wine process is using.
compile "$root/.local/bridge/proxy.exe" "$root/bridge-wm/proxy.c" -luser32 -lgdi32 -lshell32 -luxtheme
compile "$root/.local/bridge/inspect.exe" "$root/bridge-wm/inspect.c" -luser32 -lgdi32 -lole32 -lshell32 -luuid
compile "$root/.local/bridge/menu-settings.exe" "$root/prefix/menu-settings.c" -lshell32
compile "$root/.local/bridge/theme-settings.exe" "$root/prefix/theme-settings.c" -luxtheme
compile "$root/.local/bridge/wallpaper-convert.exe" -municode "$root/prefix/wallpaper-convert.c" -lole32 -luuid -lwindowscodecs
# Linux applications in the XP Start menu (session/linux_apps.py).
compile "$root/.local/bridge/linux-launch.exe" "$root/session/linux-launch.c" -mwindows -luser32
compile "$root/.local/bridge/linux-links.exe" "$root/session/linux-links.c" -lole32 -lshell32 -luuid
# XP's logon and log off sounds (session/supervisor.py).
compile "$root/.local/bridge/playsound.exe" "$root/session/playsound.c" -mwindows -lwinmm -lshell32
# The session's keyboard layout follows XP's input languages (session/keyboard.py).
compile "$root/.local/bridge/keyboard-watch.exe" "$root/session/keyboard-watch.c" -mwindows -lshell32 -ladvapi32
# XP Explorer's helpers (xp-explorer/helpers), for the session too: the
# notification area's network, battery and volume icons, Start menu pin
# management, and the stand-in iexplore.exe that opens the Linux browser.
helpers=$root/xp-explorer/helpers
i686-w64-mingw32-windres -I "$helpers" "$helpers/xptray.rc" -O coff -o "$root/.local/bridge/xptray.res"
compile "$root/.local/bridge/xptray.exe" -std=gnu11 "$helpers/xptray.c" "$root/.local/bridge/xptray.res" \
    -mwindows -lcomctl32 -lgdi32 -luser32 -lkernel32
rm -f "$root/.local/bridge/xptray.res"
# XP's Volume Control (sndvol32) on the Linux sound server, for xptray.exe.
compile "$root/.local/bridge/xpmix.dll" -std=gnu11 -shared "$helpers/xpmix.c" "$helpers/xpmix.def" -lwinmm
# XP's Security Center service (wscsvc.dll) loads these from beside it: its
# firewall and network imports, which XP's own DLLs bring only with theirs.
mkdir -p "$root/.local/bridge/wsc"
compile "$root/.local/bridge/wsc/HNetCfg.dll" -std=gnu11 -shared "$helpers/wscfirewall.c" "$helpers/wscfirewall.def" -ladvapi32
compile "$root/.local/bridge/wsc/netshell.dll" -std=gnu11 -shared "$helpers/wscnetshell.c" "$helpers/wscnetshell.def" -lole32
compile "$root/.local/bridge/startmenu.exe" -std=gnu11 "$helpers/startmenu.c" -lshell32 -lole32 -luuid -ladvapi32 -luser32
compile "$root/.local/bridge/startmenu-view.exe" "$helpers/startmenu-view.c" -mwindows -lshell32 -lole32 -luuid \
    -luxtheme -lgdi32 -luser32 -ladvapi32
compile "$root/.local/bridge/iexplore.exe" -std=gnu11 "$helpers/iexplore.c" -mwindows -lshell32
