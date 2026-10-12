# Installation

[🇯🇵 日本語](../../INSTALL.md) · 🇬🇧 English

> [!WARNING]
> - Obtain the Windows XP ISO yourself, and use this at your own risk (see [Caution in the README](README.en.md#caution)).
> - This is an experimental project. It has only been tested on a **freshly installed Debian 13** set up with `scripts/setup.sh`;
>   installing it on a system already in use has not been tried. The script installs packages and may change LightDM's
>   settings (see "2. What the script does"). A virtual machine or a spare machine is recommended.

## Requirements

- A **Debian 13 (trixie) amd64** machine (real or virtual). No desktop environment is needed.
- A **Japanese Windows XP Professional SP3 (x86) ISO** (or an XP Windows directory).
- Network access (packages and the Wine source) and **about 40 GB of free space** (Wine's source and build).
- A user who can use sudo.

## 1. Set everything up

Run it as the user who will use the XP shell (not as root). It uses sudo for packages and system files.

```sh
git clone https://github.com/mikuta0407/win4wine.git ~/win4wine
cd ~/win4wine
scripts/setup.sh --media /path/to/WindowsXPSP3.iso --greeter
```

| Option | Meaning |
|---|---|
| `--media PATH` | the XP ISO or Windows directory (required) |
| `--greeter` | make the login screen XP's Welcome screen (LightDM becomes the display manager) |
| `--no-greeter` | keep the current login screen (default); only XP's cursors are installed |
| `--theme luna\|classic` | `luna` (default, XP-style Start menu) or `classic` (classic Start menu) |
| `--dpi N` | the screen's DPI (XP's "DPI setting"): 96 (normal), 120 (large) or any value from 19 to 480; for the XP shell, the Linux applications in its session and the Welcome screen |
| `--wine-install DIR` | use a Wine built on another Debian 13 machine (a copy of its `.local/wine-install`) instead of building it here |
| `--jobs N` | parallel jobs for the Wine build (default: the number of CPUs) |

If it fails, just run it again: finished steps are skipped. Everything it builds stays in `.local/` of this checkout.

## 2. What the script does

1. Installs packages (Wine's build dependencies, Xorg, python3-xlib, fcitx5 + Mozc, …), and LightDM if there is no display manager.
2. Generates the `ja_JP.UTF-8` locale.
3. Fetches Wine 11.0, applies the patches and builds it (`.local/wine-install`; about 90 minutes on 4 cores; log in `.local/wine-build.log`).
4. Builds the bridge (the WM and the proxy windows).
5. Extracts only the needed files from the ISO (`.local/assets`).
6. Creates the XP prefix (`.local/prefix`).
7. Writes `~/.config/win4wine/session.json` and registers the session "XP Shell (Wine, X11)".
8. With `--greeter`, installs XP's Welcome screen.

## 3. Log in

Restart the display manager (or reboot).

- With `--greeter`: choose your user on XP's Welcome screen and type the password.
- Otherwise: choose the session "**XP Shell (Wine, X11)**" on the login screen.

## Usage

| To | Do |
|---|---|
| toggle Japanese input | Ctrl+Space or Zenkaku/Hankaku, or click the icon in the notification area |
| start a Linux application | Start → All Programs (names end in "(Linux)") |
| install a Windows application | `wine setup.exe` in a terminal; the session's `PATH` starts with this Wine |
| open the Start menu | the Windows key, Ctrl+Esc |
| Run / Explorer | Win+R / Win+E |
| log off or shut down | Start → Log Off / Turn Off Computer |

## Settings (`~/.config/win4wine/session.json`)

| Key | Default | Meaning |
|---|---|---|
| `wine`, `prefix` | (written by setup.sh) | absolute paths of the Wine and prefix to use |
| `locale` | `ja_JP.UTF-8` | the session's locale |
| `input_method` | `fcitx5` | the input method; leave it out to start none |
| `linux_font` | `MS UI Gothic` | font of Linux applications; `null` leaves it alone |
| `linux_menu` | `true` | list Linux applications in the Start menu |
| `welcome_screen` | `true` | show XP's screens after login and while logging off |
| `cursor_theme` | `win4wine-xp` | cursor theme; `null` for none |
| `shutdown_command`, `reboot_command` | `systemctl poweroff` / `reboot` | commands run by Turn Off Computer |

To change the look later, stop the session and run `scripts/setup.sh --media ISO --theme classic` (or `luna`).
To change the DPI (the size of text and screen elements), run `scripts/setup.sh --media ISO --dpi 120` and log in again.
As on XP, text and metrics grow while icons stay 32×32. Linux applications get the same DPI for their text (GTK also doubles
its widgets from 192).

## Troubleshooting

- Session logs: `~/.local/state/win4wine/` (the previous run's in `*.log.1`).
- If the XP shell keeps crashing, the session switches to Wine's own explorer and an xterm.
- If the Welcome screen does not appear: `/var/log/lightdm/seat0-greeter.log`. When something it needs is missing, it falls back to LightDM's usual greeter.

## Uninstall

```sh
greeter/install.sh --remove                     # stop using the Welcome screen (back to LightDM's previous greeter)
sudo rm /usr/share/xsessions/xp-shell.desktop /usr/local/bin/xp-shell-session   # unregister the session
python3 session/linux_apps.py .local/prefix --wine .local/wine-install/bin/wine --remove   # remove the Start menu entries it made
```

The builds, extracted files and prefix go away with `.local/` of this checkout.
Packages the script installed (LightDM included) and the switch of display manager are not undone.
