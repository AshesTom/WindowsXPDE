#!/bin/bash
# install.sh [--wayland LABWC] : the X11 session, and with a labwc built by
# wayland/build.sh also the Wayland session.
set -euo pipefail
root=$(cd -- "$(dirname -- "$0")/.." && pwd)
[[ $root != *$'\n'* ]] || exit 1
labwc=
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
if [[ -n $labwc ]]; then
    printf '#!/bin/bash\nexec %q %q\n' "$root/wayland/session.sh" "$labwc" > "$wrapper"
    as_root install -Dm755 "$wrapper" /usr/local/bin/xp-shell-wayland-session
    as_root install -Dm644 "$root/session/xp-shell-wayland.desktop" /usr/share/wayland-sessions/xp-shell-wayland.desktop
    printf 'Installed the Wayland session (%s)\n' "$labwc"
fi
