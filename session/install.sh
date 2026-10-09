#!/bin/bash
set -euo pipefail
root=$(cd -- "$(dirname -- "$0")/.." && pwd)
[[ $root != *$'\n'* ]] || exit 1
wrapper=$(mktemp)
trap 'rm -f "$wrapper"' EXIT
printf '#!/bin/bash\nexec python3 %q "$@"\n' "$root/session/supervisor.py" > "$wrapper"
sudo install -Dm755 "$wrapper" /usr/local/bin/xp-shell-session
sudo install -Dm644 "$root/session/xp-shell.desktop" /usr/share/xsessions/xp-shell.desktop
printf 'Installed Xsession. Configuration: ~/.config/win4wine/session.json\n'
