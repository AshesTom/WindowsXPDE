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
# Opt-in window snapping (session.json "window_snap"): labwc moves Wine's
# windows and snaps them itself when they are dragged to an edge.
config=${XDG_CONFIG_HOME:-$HOME/.config}/win4wine/session.json
previous=
for argument in "$@"; do
    [[ $previous == --config ]] && config=$argument
    [[ $argument == --config=* ]] && config=${argument#--config=}
    previous=$argument
done
rc=$root/wayland/labwc/rc.xml
if python3 -c 'import json, sys; sys.exit(not json.load(open(sys.argv[1])).get("window_snap"))' "$config" 2>/dev/null; then
    rc=$ipc/rc.xml
    sed -e 's|<range>0</range>|<range>1</range>|' -e 's|<topMaximize>no</topMaximize>|<topMaximize>yes</topMaximize>|' \
        "$root/wayland/labwc/rc.xml" > "$rc"
fi
status=0
"$labwc" -C "$root/wayland/labwc" -c "$rc" -S "$command" || status=$?
rm -rf -- "$ipc"
exit $status
