#!/bin/bash
# install.sh [--reactos] [--wayland LABWC] : the X11 session, and with a
# labwc built by wayland/build.sh also the Wayland session. With --reactos, the
# "ReactOS Shell" session, which reads ~/.config/win4wine/reactos-session.json
# (with --wayland also its Wayland session). The Windows 7 shell's sessions are
# on the win7 branch.
set -euo pipefail
root=$(cd -- "$(dirname -- "$0")/.." && pwd)
[[ $root != *$'\n'* ]] || exit 1
labwc=
reactos=no
if [[ ${1:-} == --reactos ]]; then
    reactos=yes
    shift
fi
if [[ ${1:-} == --wayland ]]; then
    labwc=$(realpath -e -- "${2:?--wayland needs the labwc binary}")
    [[ $labwc != *$'\n'* ]] || exit 1
fi
# As root (a fresh system without sudo) or through sudo
as_root() { if [[ $EUID -eq 0 ]]; then "$@"; else sudo "$@"; fi; }
wrapper=$(mktemp)
trap 'rm -f "$wrapper"' EXIT
printf '#!/bin/bash\nexec python3 %q "$@"\n' "$root/session/supervisor.py" > "$wrapper"
as_root install -Dm755 "$wrapper" /usr/local/bin/xp-shell-session
as_root install -Dm644 "$root/session/xp-shell.desktop" /usr/share/xsessions/xp-shell.desktop
printf 'Installed Xsession. Configuration: ~/.config/win4wine/session.json\n'
if [[ $reactos == yes ]]; then
    printf '#!/bin/bash\nexec python3 %q --config "${XDG_CONFIG_HOME:-$HOME/.config}/win4wine/reactos-session.json" "$@"\n' \
        "$root/session/supervisor.py" > "$wrapper"
    as_root install -Dm755 "$wrapper" /usr/local/bin/reactos-shell-session
    as_root install -Dm644 "$root/session/reactos-shell.desktop" /usr/share/xsessions/reactos-shell.desktop
    printf 'Installed the ReactOS shell Xsession. Configuration: ~/.config/win4wine/reactos-session.json\n'
fi
if [[ -n $labwc ]]; then
    printf '#!/bin/bash\nexec %q %q\n' "$root/wayland/session.sh" "$labwc" > "$wrapper"
    as_root install -Dm755 "$wrapper" /usr/local/bin/xp-shell-wayland-session
    as_root install -Dm644 "$root/session/xp-shell-wayland.desktop" /usr/share/wayland-sessions/xp-shell-wayland.desktop
    printf 'Installed the Wayland session (%s)\n' "$labwc"
    if [[ $reactos == yes ]]; then
        printf '#!/bin/bash\nexec %q %q --config "${XDG_CONFIG_HOME:-$HOME/.config}/win4wine/reactos-session.json" "$@"\n' \
            "$root/wayland/session.sh" "$labwc" > "$wrapper"
        as_root install -Dm755 "$wrapper" /usr/local/bin/reactos-shell-wayland-session
        as_root install -Dm644 "$root/session/reactos-shell-wayland.desktop" \
            /usr/share/wayland-sessions/reactos-shell-wayland.desktop
        printf 'Installed the ReactOS shell Wayland session\n'
    fi
fi
