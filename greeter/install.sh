#!/bin/bash
# Make LightDM show XP's Welcome screen (greeter/xp-greeter.py).
#   greeter/install.sh ASSETS    copy the greeter and the XP files it reads from ASSETS
#                                (extract/extract.py's output), then make it LightDM's greeter
#   greeter/install.sh --remove  go back to the greeter LightDM used before
# Restart LightDM afterwards. Without the XP files the greeter starts lightdm-gtk-greeter.
set -euo pipefail
root=$(cd -- "$(dirname -- "$0")/.." && pwd)
share=/usr/local/share/win4wine/greeter
seat=/etc/lightdm/lightdm.conf.d/95-xp-greeter.conf
sudo() { if [[ $EUID -eq 0 ]]; then "$@"; else command sudo "$@"; fi; }
if [[ ${1:-} == --remove ]]; then
    sudo rm -f "$seat"
    printf 'Removed %s; LightDM uses its previous greeter after a restart.\n' "$seat"
    exit 0
fi
assets=${1:?Usage: greeter/install.sh ASSETS | --remove}
for f in system32/logonui.exe system32/msgina.dll system32/winlogon.exe fonts/msgothic.ttc; do
    [[ -f $assets/$f ]] || { printf 'Missing %s: extract again with extract/extract.py\n' "$assets/$f" >&2; exit 1; }
done
# LightDM runs the greeter as its own user, which cannot read a private home or checkout.
sudo install -d -m755 "$share/code" "$share/assets/system32" "$share/assets/fonts" "$share/assets/pictures" \
    "$share/assets/themes/Luna"
sudo install -m644 "$root"/greeter/{welcome,xpres,xdraw}.py "$share/code/"
sudo install -m755 "$root/greeter/xp-greeter.py" "$share/code/"
sudo install -m644 "$assets"/system32/{logonui.exe,msgina.dll,winlogon.exe} "$share/assets/system32/"
for f in fonts/msgothic.ttc fonts/arial.ttf themes/Luna/luna.msstyles; do
    [[ ! -f $assets/$f ]] || sudo install -m644 "$assets/$f" "$share/assets/$f"
done
shopt -s nullglob
pictures=("$assets"/pictures/*.bmp)
[[ ${#pictures[@]} -eq 0 ]] || sudo install -m644 "${pictures[@]}" "$share/assets/pictures/"
if [[ ! -f /etc/win4wine/greeter.json ]]; then
    printf '{"assets": "%s/assets", "fallback": "lightdm-gtk-greeter"}\n' "$share" |
        sudo install -Dm644 /dev/stdin /etc/win4wine/greeter.json
fi
# XP's cursors as an X cursor theme, for the greeter, Wine and Linux applications in the session.
if [[ -f $assets/resources/user32.dll ]]; then
    theme=$(mktemp -d)
    python3 "$root/greeter/cursors.py" "$assets/resources/user32.dll" "$theme/win4wine-xp"
    sudo rm -rf /usr/share/icons/win4wine-xp
    sudo cp -r "$theme/win4wine-xp" /usr/share/icons/
    sudo chmod -R a+rX /usr/share/icons/win4wine-xp
    rm -rf "$theme"
else
    printf 'No resources/user32.dll in %s: XP cursors are not installed (extract again)\n' "$assets" >&2
fi
sudo install -Dm644 "$root/greeter/xp-greeter.desktop" /usr/share/xgreeters/xp-greeter.desktop
# The Welcome screen has no session menu: users without a last session get the XP shell.
printf '[Seat:*]\ngreeter-session=xp-greeter\nuser-session=xp-shell\n' | sudo install -Dm644 /dev/stdin "$seat"
printf 'Installed the XP Welcome screen as %s. Settings: /etc/win4wine/greeter.json\n' "$seat"
