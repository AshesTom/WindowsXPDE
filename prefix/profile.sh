#!/bin/bash
set -euo pipefail
: "${WINEPREFIX:?Set WINEPREFIX to the dedicated XP prefix}"
[[ -f $WINEPREFIX/xp-shell.json ]] || { echo 'Not an XP prefix' >&2; exit 1; }
profile=${1:-builtin}
case "$profile" in
    builtin) overrides='explorer=b;xp-explorer=n;mscoree,mshtml=;shell32,shdocvw,browseui,shlwapi,comctl32,uxtheme=b;linkinfo=' ;;
    shell) overrides='explorer=b;xp-explorer=n;mscoree,mshtml=;uxtheme=b' ;;
    *) echo 'Profile must be builtin or shell' >&2; exit 2 ;;
esac
if [[ $profile == shell ]]; then
    python3 "$(dirname -- "$0")/stage.py" "$WINEPREFIX"
fi
export WINEDLLOVERRIDES=$overrides
shift || true
exec "${WINE:-wine}" "${@:-C:\\xp\\xp-explorer.exe}"
