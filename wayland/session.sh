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
# labwc's configuration directory, with the keyboard layout of XP's input
# languages in its environment file; the supervisor rewrites it when they
# change and labwc rereads it on SIGHUP (session/keyboard.py).
labwc_config=$ipc/labwc
mkdir -p "$labwc_config"
for file in "$root"/wayland/labwc/*; do
    ln -s "$file" "$labwc_config/$(basename "$file")"
done
prefix=$(python3 -c 'import json, sys; print(json.load(open(sys.argv[1]))["prefix"])' "$config" 2>/dev/null || true)
if [[ -n $prefix && -f $prefix/xp-shell.json ]]; then
    python3 "$root/session/keyboard.py" env "$prefix" > "$labwc_config/environment" || true
fi
export WIN4WINE_LABWC_ENV=$labwc_config/environment
status=0
"$labwc" -C "$labwc_config" -c "$rc" -S "$command" || status=$?
rm -rf -- "$ipc"
exit $status
