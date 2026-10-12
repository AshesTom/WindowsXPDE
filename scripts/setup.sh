#!/bin/bash
# Set up the XP shell session on this Debian 13 machine, from a fresh install.
#
#   scripts/setup.sh --media PATH [--greeter | --no-greeter] [--theme luna|classic]
#                    [--dpi N] [--wayland] [--wine-install DIR | --jobs N]
#   scripts/setup.sh --shell reactos [--lang ja|en] [--dpi N] [--wine-install DIR | --jobs N]
#
#   --media PATH   the user's Windows XP SP3 (x86) ISO or Windows directory
#   --shell S      xp (the default) or reactos: ReactOS's shell (classic look),
#                  downloaded from the ReactOS release, so no XP media is needed.
#                  It is registered as its own session, "ReactOS Shell (Wine, X11)",
#                  beside the XP shell's if that is set up too
#   --lang L       ja or en: the language of the ReactOS shell and its session
#                  (default: ja when the current locale is Japanese, else en);
#                  with --greeter, LightDM shows ReactOS's logon screen instead
#                  of XP's Welcome screen
#   --greeter      also make LightDM show XP's Welcome screen (greeter/), with XP's
#                  cursors; LightDM becomes the display manager
#   --no-greeter   keep the display manager's own login screen (the default); XP's
#                  cursors are still installed for the session
#   --theme T      luna (XP's default, the default here, with XP's Start menu) or
#                  classic (with the classic Start menu), for a new prefix; given
#                  explicitly, it is applied to an existing prefix too
#   --dpi N        the screen's DPI as XP sets it (LogPixels): 96 (normal), 120 (large)
#                  or any value from 19 to 480; for the XP shell, the Linux
#                  applications in its session and the Welcome screen
#   --wayland      also build win4wine's labwc (wayland/build.sh) and register the
#                  "XP Shell (Wine, Wayland)" session: per-monitor and fractional
#                  scales (session.json "outputs"), Wayland applications with XP frames
#   --wine-install DIR
#                  use a Wine built elsewhere with this repository's patches (the
#                  .local/wine-install of another Debian 13 machine, copied anywhere)
#                  instead of building it here
#   --jobs N       parallel jobs for the Wine build (default: the number of CPUs)
#
# Run it as the user who will log in to the XP shell (not as root); it uses sudo
# for packages and system files. It builds Wine with this repository's patches,
# extracts the XP files, creates the XP prefix and registers the "XP Shell"
# session. Steps already done are skipped, so it can be run again after a failure.
# Everything it builds stays in .local/ of this checkout.
set -euo pipefail
root=$(cd -- "$(dirname -- "$0")/.." && pwd)
local=$root/.local
media=
shell=xp
lang=
greeter=no
jobs=$(nproc)
theme=luna
theme_given=no
dpi=
wayland=no
prebuilt=
while (($#)); do
    case $1 in
        --media) media=${2:?--media needs a path}; shift 2 ;;
        --shell) shell=${2:?--shell needs xp or reactos}; shift 2 ;;
        --lang) lang=${2:?--lang needs ja or en}; shift 2 ;;
        --greeter) greeter=yes; shift ;;
        --no-greeter) greeter=no; shift ;;
        --jobs) jobs=${2:?--jobs needs a number}; shift 2 ;;
        --theme) theme=${2:?--theme needs luna or classic}; theme_given=yes; shift 2 ;;
        --dpi) dpi=${2:?--dpi needs a number}; shift 2 ;;
        --wayland) wayland=yes; shift ;;
        --wine-install) prebuilt=$(realpath -e -- "${2:?--wine-install needs a directory}"); shift 2 ;;
        -h|--help) sed -n '2,/^set -euo/p' "$0" | sed '$d; s/^# \{0,1\}//'; exit 0 ;;
        *) printf 'Unknown option %s (see --help)\n' "$1" >&2; exit 2 ;;
    esac
done
[[ $shell == xp || $shell == reactos ]] || { printf -- '--shell is xp or reactos\n' >&2; exit 2; }
if [[ $shell == reactos ]]; then
    [[ -z $media ]] || { printf -- '--media is for the XP shell; the ReactOS shell needs no media\n' >&2; exit 2; }
    [[ $theme_given == no ]] || { printf -- 'The ReactOS shell has the classic look only; leave out --theme\n' >&2; exit 2; }
    [[ $wayland == no ]] || { printf -- 'The ReactOS shell has no Wayland session yet\n' >&2; exit 2; }
    if [[ -z $lang ]]; then
        [[ ${LANG:-} == ja* ]] && lang=ja || lang=en
    fi
    [[ $lang == ja || $lang == en ]] || { printf -- '--lang is ja or en\n' >&2; exit 2; }
else
    [[ -z $lang ]] || { printf -- '--lang is for the ReactOS shell; the XP shell follows its media\n' >&2; exit 2; }
    [[ -n $media ]] || { printf 'Give the XP media with --media (see --help)\n' >&2; exit 2; }
fi
[[ $theme == luna || $theme == classic ]] || { printf -- '--theme is luna or classic\n' >&2; exit 2; }
[[ -z $dpi || ( $dpi =~ ^[0-9]+$ && $dpi -ge 19 && $dpi -le 480 ) ]] ||
    { printf -- '--dpi is a number from 19 to 480\n' >&2; exit 2; }
[[ -z $media ]] || media=$(realpath -e -- "$media")
[[ $EUID -ne 0 ]] || { printf 'Run this as the user who will log in, not as root\n' >&2; exit 2; }
[[ $root != *[[:space:]]* ]] || { printf 'The checkout path must not contain spaces: %s\n' "$root" >&2; exit 2; }
if ! grep -q '^VERSION_CODENAME=trixie' /etc/os-release; then
    printf 'Warning: written for Debian 13 (trixie); this is %s\n' "$(. /etc/os-release; echo "$PRETTY_NAME")" >&2
fi
step() { printf '\n=== %s\n' "$*"; }
mkdir -p "$local"

step 'Packages'
packages=(
    # Wine build (WoW64: 64-bit Unix libraries, i686 and x86_64 MinGW for the PE modules)
    build-essential flex bison pkg-config git ca-certificates
    libx11-dev libxext-dev libxrender-dev libxrandr-dev libxi-dev libxcursor-dev libxinerama-dev
    libfreetype-dev libfontconfig-dev libgnutls28-dev libgl-dev libasound2-dev libdbus-1-dev libudev-dev
    gcc-mingw-w64-i686 gcc-mingw-w64-x86-64
    # extraction, prefix creation
    cabextract p7zip-full xvfb xauth
    # the session: X, the WM and tray bridge, recovery terminal, root cursor
    xserver-xorg x11-xserver-utils xterm python3 python3-xlib
    python3-gi gir1.2-glib-2.0 gir1.2-gdkpixbuf-2.0 librsvg2-common
    # Japanese input (session.json "input_method": "fcitx5")
    locales fcitx5 fcitx5-mozc fcitx5-frontend-gtk3 fcitx5-frontend-gtk4
    fcitx5-frontend-qt5 fcitx5-frontend-qt6 fcitx5-config-qt
)
if [[ $wayland == yes ]]; then
    packages+=(
        # labwc and wlroots (wayland/build.sh), Xwayland, the outputs' settings
        meson ninja-build wayland-protocols libwayland-dev libxkbcommon-dev libxml2-dev
        libcairo2-dev libpango1.0-dev libglib2.0-dev libinput-dev libpixman-1-dev libpng-dev
        librsvg2-dev libdrm-dev libgbm-dev libegl-dev libgles-dev libseat-dev libdisplay-info-dev
        libliftoff-dev liblcms2-dev hwdata libxcb1-dev libxcb-composite0-dev libxcb-dri3-dev
        libxcb-errors-dev libxcb-ewmh-dev libxcb-icccm4-dev libxcb-present-dev libxcb-randr0-dev
        libxcb-render0-dev libxcb-render-util0-dev libxcb-res0-dev libxcb-shm0-dev
        libxcb-xfixes0-dev libxcb-xinput-dev xwayland wlr-randr
    )
fi
if [[ $greeter == yes ]]; then
    packages+=(lightdm lightdm-gtk-greeter gir1.2-lightdm-1 gir1.2-gtk-3.0 python3-gi-cairo)
elif [[ ! -e /etc/X11/default-display-manager ]]; then
    # No display manager yet: LightDM with its usual login screen.
    packages+=(lightdm lightdm-gtk-greeter)
fi
missing=()
for package in "${packages[@]}"; do
    dpkg-query -W -f='${Status}' "$package" 2>/dev/null | grep -q 'install ok installed' || missing+=("$package")
done
if ((${#missing[@]})); then
    sudo apt-get update
    sudo env DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends "${missing[@]}"
else
    printf 'All installed\n'
fi
if [[ $greeter == yes && $(cat /etc/X11/default-display-manager 2>/dev/null) != /usr/sbin/lightdm ]]; then
    printf 'Making LightDM the display manager (it was %s)\n' "$(cat /etc/X11/default-display-manager 2>/dev/null || echo none)"
    echo /usr/sbin/lightdm | sudo tee /etc/X11/default-display-manager >/dev/null
    sudo systemctl enable --force lightdm.service
fi

locale=ja_JP.UTF-8
[[ $shell == xp || $lang == ja ]] || locale=en_US.UTF-8
step "Locale $locale"
if ! locale -a | grep -qix "${locale%.UTF-8}.utf8"; then
    sudo sed -i "s/^# *\\($locale UTF-8\\)/\\1/" /etc/locale.gen
    grep -q "^$locale UTF-8" /etc/locale.gen || echo "$locale UTF-8" | sudo tee -a /etc/locale.gen >/dev/null
    sudo locale-gen
else
    printf '%s exists\n' "$locale"
fi

step 'Wine'
wine=$local/wine-install/bin/wine
if [[ -n $prebuilt ]]; then
    [[ -f $prebuilt/lib/wine/x86_64-windows/win4wine-controls.dll && -x $prebuilt/bin/wine ]] ||
        { printf '%s is not a Wine install built by wine/build.sh\n' "$prebuilt" >&2; exit 1; }
    wine=$prebuilt/bin/wine
    printf 'Using %s: %s\n' "$prebuilt" "$("$wine" --version)"
elif [[ -x $wine && -f $local/wine-install/lib/wine/x86_64-windows/win4wine-controls.dll ]]; then
    printf 'Built: %s\n' "$("$wine" --version)"
else
    printf 'Building Wine with %s jobs (log: .local/wine-build.log); this takes a long time\n' "$jobs"
    JOBS=$jobs "$root/wine/build.sh" > "$local/wine-build.log" 2>&1 ||
        { tail -20 "$local/wine-build.log" >&2; printf 'Wine build failed; see .local/wine-build.log\n' >&2; exit 1; }
    "$wine" --version
fi

step 'Bridge'
"$root/bridge-wm/build.sh"
printf 'Built .local/bridge\n'

labwc=$local/labwc-install/bin/labwc
if [[ $wayland == yes ]]; then
    step 'Wayland compositor'
    if [[ -x $labwc ]]; then
        printf 'Built: %s\n' "$("$labwc" --version)"
    else
        printf 'Building labwc and wlroots (log: .local/labwc-build.log)\n'
        JOBS=$jobs "$root/wayland/build.sh" > "$local/labwc-build.log" 2>&1 ||
            { tail -20 "$local/labwc-build.log" >&2; printf 'labwc build failed; see .local/labwc-build.log\n' >&2; exit 1; }
        "$labwc" --version
    fi
fi

if [[ $shell == xp ]]; then
    step 'XP files'
    assets=$local/assets
    if [[ -f $assets/manifest.json ]]; then
        printf 'Extracted already: %s\n' "$assets"
    else
        # 7z needs room for the whole medium; keep it out of a small /tmp.
        mkdir -p "$local/tmp"
        TMPDIR=$local/tmp python3 "$root/extract/extract.py" "$media" "$assets"
    fi

    step 'XP prefix'
    prefix=$local/prefix
    if [[ -f $prefix/xp-shell.json ]]; then
        printf 'Created already: %s\n' "$prefix"
    else
        [[ ! -e $prefix ]] || { printf '%s exists but is incomplete; move it away and run again\n' "$prefix" >&2; exit 1; }
        xvfb-run -a -s '-screen 0 1280x800x24' python3 "$root/prefix/create.py" "$assets" "$prefix" \
            --home "$HOME" --wine "$wine" > "$local/prefix.log" 2>&1 ||
            { tail -20 "$local/prefix.log" >&2; printf 'Prefix creation failed; see .local/prefix.log\n' >&2; exit 1; }
        printf 'Created %s\n' "$prefix"
        theme_given=yes
    fi
    if [[ $theme_given == yes ]]; then
        WINEPREFIX=$prefix WINE=$wine xvfb-run -a -s '-screen 0 1280x800x24' \
            python3 "$root/prefix/display.py" --theme "$theme" --menu "$([[ $theme == luna ]] && echo xp || echo classic)" \
            >> "$local/prefix.log" 2>&1 ||
            { tail -20 "$local/prefix.log" >&2; printf 'Setting the theme failed; see .local/prefix.log\n' >&2; exit 1; }
        "$(dirname "$wine")/wineserver" -w 2>/dev/null || true
        printf 'Theme: %s\n' "$theme"
    fi
    if [[ -n $dpi ]]; then
        WINEPREFIX=$prefix WINE=$wine python3 "$root/prefix/display.py" --dpi "$dpi" >> "$local/prefix.log" 2>&1 ||
            { tail -20 "$local/prefix.log" >&2; printf 'Setting the DPI failed; see .local/prefix.log\n' >&2; exit 1; }
        "$(dirname "$wine")/wineserver" -w 2>/dev/null || true
        printf 'DPI: %s\n' "$dpi"
    fi
else
    step 'ReactOS files'
    assets=$local/assets-reactos
    if [[ -f $assets/manifest.json ]]; then
        printf 'Extracted already: %s\n' "$assets"
    else
        # The release zip is kept in .local/dl; 7z needs room for the ISO, out of a small /tmp.
        mkdir -p "$local/tmp"
        TMPDIR=$local/tmp python3 "$root/extract/reactos.py" "$assets" --cache "$local/dl"
    fi

    step 'ReactOS shell prefix'
    prefix=$local/prefix-reactos
    if [[ -f $prefix/reactos-shell.json ]]; then
        printf 'Created already: %s\n' "$prefix"
    else
        [[ ! -e $prefix ]] || { printf '%s exists but is incomplete; move it away and run again\n' "$prefix" >&2; exit 1; }
        xvfb-run -a -s '-screen 0 1280x800x24' python3 "$root/prefix/reactos.py" create "$assets" "$prefix" \
            --home "$HOME" --wine "$wine" --lang "$lang" > "$local/prefix-reactos.log" 2>&1 ||
            { tail -20 "$local/prefix-reactos.log" >&2; printf 'Prefix creation failed; see .local/prefix-reactos.log\n' >&2; exit 1; }
        printf 'Created %s\n' "$prefix"
    fi
    if [[ -n $dpi ]]; then
        WINEPREFIX=$prefix WINE=$wine python3 "$root/prefix/display.py" --dpi "$dpi" >> "$local/prefix-reactos.log" 2>&1 ||
            { tail -20 "$local/prefix-reactos.log" >&2; printf 'Setting the DPI failed; see .local/prefix-reactos.log\n' >&2; exit 1; }
        "$(dirname "$wine")/wineserver" -w 2>/dev/null || true
        printf 'DPI: %s\n' "$dpi"
    fi
fi

step 'Session'
config=${XDG_CONFIG_HOME:-$HOME/.config}/win4wine/$([[ $shell == xp ]] && echo session.json || echo reactos-session.json)
if [[ -f $config ]]; then
    printf 'Keeping %s\n' "$config"
else
    mkdir -p "$(dirname "$config")"
    python3 - "$config" "$wine" "$prefix" "$shell" "$locale" <<'EOF'
import json, sys
config, wine, prefix, shell, locale = sys.argv[1:]
settings = {'wine': wine, 'prefix': prefix, 'locale': locale}
if locale.startswith('ja'):
    settings['input_method'] = 'fcitx5'
if shell == 'reactos':
    settings.update(shell='reactos', cursor_theme='win4wine-reactos')
open(config, 'w').write(json.dumps(settings, indent=2) + '\n')
EOF
    printf 'Wrote %s\n' "$config"
fi
if [[ $wayland == yes ]]; then
    "$root/session/install.sh" --wayland "$labwc"
elif [[ $shell == reactos ]]; then
    "$root/session/install.sh" --reactos
else
    "$root/session/install.sh"
fi

step 'Login screen and cursors'
if [[ $shell == reactos ]]; then
    if [[ $greeter == yes ]]; then
        "$root/greeter/install.sh" --reactos "$assets" "$lang"
    fi
    # ReactOS's cursors for the session (and the logon screen).
    theme=$(mktemp -d)
    python3 "$root/greeter/cursors.py" "$assets/resources/user32.dll" "$theme/win4wine-reactos" --reactos
    sudo rm -rf /usr/share/icons/win4wine-reactos
    sudo cp -r "$theme/win4wine-reactos" /usr/share/icons/
    sudo chmod -R a+rX /usr/share/icons/win4wine-reactos
    rm -rf "$theme"
    printf 'Installed ReactOS cursors\n'
elif [[ $greeter == yes ]]; then
    "$root/greeter/install.sh" "$assets" "$([[ $wayland == yes ]] && echo xp-shell-wayland || echo xp-shell)"
    if [[ -n $dpi ]]; then
        # XP's Welcome screen takes the machine's DPI.
        sudo python3 - "$dpi" <<'EOF'
import json, sys
path = '/etc/win4wine/greeter.json'
config = json.load(open(path))
config['dpi'] = int(sys.argv[1])
open(path, 'w').write(json.dumps(config) + '\n')
EOF
    fi
else
    # XP's cursors for the session only; LightDM, if any, keeps its own greeter.
    theme=$(mktemp -d)
    python3 "$root/greeter/cursors.py" "$assets/resources/user32.dll" "$theme/win4wine-xp"
    sudo rm -rf /usr/share/icons/win4wine-xp
    sudo cp -r "$theme/win4wine-xp" /usr/share/icons/
    sudo chmod -R a+rX /usr/share/icons/win4wine-xp
    rm -rf "$theme"
    printf 'Installed XP cursors; the display manager keeps its login screen\n'
fi

step 'Done'
if [[ $shell == reactos && $greeter == yes ]]; then
    printf 'Restart the display manager (or reboot): the ReactOS logon screen logs in to the ReactOS shell.\n'
elif [[ $shell == reactos ]]; then
    printf 'Restart the display manager (or reboot) and choose "ReactOS Shell (Wine, X11)" at the login screen.\n'
elif [[ $greeter == yes ]]; then
    printf 'Restart the display manager (or reboot): the XP Welcome screen logs in to the XP shell.\n'
else
    printf 'Restart the display manager (or reboot) and choose "XP Shell (Wine, X11)" at the login screen.\n'
fi
if [[ $wayland == yes ]]; then
    printf 'The Wayland session is "XP Shell (Wine, Wayland)"; outputs and scales: session.json "outputs".\n'
fi
printf 'Logs of the session: ~/.local/state/win4wine/\n'
