#!/bin/bash
# session.sh LABWC [SUPERVISOR OPTIONS] : the Wayland session. win4wine's
# labwc runs the session supervisor and ends with it. install.sh writes
# /usr/local/bin/xp-shell-wayland-session, which runs this with the
# compositor's path.
set -euo pipefail
labwc=${1:?Usage: session.sh LABWC [SUPERVISOR OPTIONS]}
shift
root=$(cd -- "$(dirname -- "$0")/.." && pwd)
ipc=$(mktemp -d "${XDG_RUNTIME_DIR:-/tmp}/win4wine-$(id -u)-XXXXXX")
export WIN4WINE_IPC=$ipc XDG_CURRENT_DESKTOP=XP-Shell XDG_SESSION_DESKTOP=xp-shell
command=$(printf ' %q' python3 "$root/session/supervisor.py" "$@")
status=0
"$labwc" -C "$root/wayland/labwc" -S "$command" || status=$?
rm -rf -- "$ipc"
exit $status
