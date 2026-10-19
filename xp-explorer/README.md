<p align="center">
  <strong>The Windows XP shell, in a window on your Linux desktop.</strong>
</p>
<p align="center">
  <a href="../LICENSE"><img src="https://img.shields.io/badge/license-LGPL--2.1--or--later-blue?style=flat" alt="License: LGPL-2.1-or-later"></a>
  <img src="https://img.shields.io/badge/Arch%20%2F%20CachyOS-KDE%20Plasma-1793D1?style=flat&logo=archlinux&logoColor=white" alt="Arch / CachyOS, KDE Plasma">
  <img src="https://img.shields.io/badge/Wine-11.0-8B0000?style=flat" alt="Wine 11.0">
  <img src="https://img.shields.io/badge/status-experimental-orange?style=flat" alt="Status: experimental">
</p>

> [!IMPORTANT]
> **XP Explorer is a fork of [win4wine](https://github.com/mikuta0407/win4wine) by mikuta0407**, trimmed down
> and extended with quality-of-life additions. It lives in `xp-explorer/` of this win4wine checkout and builds
> on its Wine patches (`../wine/patches`), so it gets win4wine's XP fixes as they land. All the hard work (the Wine patches, the ISO extraction, the
> prefix profile, the Linux applications in the Start menu) is theirs.
> Like win4wine, this is a **personal, experimental project**. It has only been tried on CachyOS with KDE Plasma,
> inside a Wine virtual desktop.

## Install

1. Get a Windows XP Professional SP3 (x86) ISO **yourself**.
2. Install the build tools, then build, set up and run:

```sh
sudo pacman -S --needed wine cabextract mingw-w64-gcc libisoburn
cd xp-explorer
./xp.sh wine                          # optional but recommended: win4wine's patched Wine, 20-40 min
./xp.sh setup /path/to/WindowsXPSP3.iso
./xp.sh run                           # or: ./xp.sh run 1280x720
```

The desktop opens in a window. Its size must be a resolution your screen supports (1600x900 by default).

| Command | Does |
|---|---|
| `./xp.sh wine` | builds Wine 11.0 with win4wine's patches (`../wine/patches`) and XP Explorer's 13 (`wine/patches`) |
| `./xp.sh setup ISO` | extracts the XP files, builds the helpers, creates the prefix |
| `./xp.sh run [WxH]` | opens the XP desktop |
| `./xp.sh apps` | refreshes the Linux applications in All Programs |
| `./xp.sh pins` | re-pins the Start menu's left column (see `pins.txt` below) |
| `./xp.sh kill` | stops everything running in the prefix |
| `./xp.sh reset` | deletes the prefix (the extracted files are kept) |

`XP_PREFIX`, `XP_DESKTOP` and `XP_LOG` run `xp.sh` on another prefix, under another desktop name and log, for
example a test copy beside your own session.

## What it is

It runs Windows XP's own `explorer.exe` on Wine, in a **Wine virtual desktop**: the taskbar, Start menu,
desktop and folder windows are XP's real ones. Unlike win4wine, it does not replace your Linux session.
It is something to open next to KDE, not a login session.

No Microsoft files are included. Only the files that are needed are extracted locally from your own ISO,
into `.local/`, which Git ignores.

## What it adds to win4wine

- **A full left column in the Start menu.** Internet and E-mail are your Linux web browser and mail client
  (`scripts/pins.py`). Below them come your most used programs: XP Explorer leaves its own list of them empty
  under Wine, so `startmenu.exe recent` (`helpers/startmenu.c`, run by `xpstart.exe` at each logon) counts them
  from XP's own usage data as XP does, per program rather than per shortcut, leaving out helpers such as
  Notepad, set-up programs and your own pins, and keeps the first six pinned. Until programs have been used,
  your file manager, terminal, text editor and System Settings fill the list. They are pins, not XP's list, so
  there is no separator line above them and the list changes at the next logon rather than at once. Each Linux
  application starts through a launcher of its own (`C:\xp\linux-apps\apps\<desktop file ID>.exe`, a hard
  link), since XP counts the uses of a program by the executable its shortcut runs. To pin programs of your own,
  list desktop file IDs (one per line, such as `org.kde.dolphin.desktop`) in `pins.txt`, then run
  `./xp.sh pins`. The right column has XP's full set (My Network Places, Help and Support, Search, Run), and the
  menu keeps the height of that column.
- **Network, battery and volume icons** in the notification area (`helpers/xptray.c`), with XP's own icons,
  tooltips and balloons ("Wireless Network Connection is now connected", "Your battery is running low"), fed by
  the Linux state (`/sys/class/power_supply`, `/sys/class/net`, `nmcli`, `wpctl` or `pactl`). Clicking them opens
  **XP's own flyouts**, built from the dialog templates in your XP files: the Volume slider (`sndvol32.exe`),
  the Power Meter (`stobject.dll` + `batmeter.dll`) and the "Wireless Network Connection Status" sheet with its
  General and Support pages (`netshell.dll`: duration, speed, signal strength, bytes, IP address, gateway;
  Disable and Repair go through NetworkManager). **Network Connections** in Accessories\Communications and on
  the network icon lists saved NetworkManager profiles with their connection state and can connect or disconnect
  them; its Settings button opens KDE's network settings. Clicking the network notification icon,
  **Wireless Network Connection** in Accessories\Communications, or **View Available Wireless Networks** opens an XP-style
  chooser populated by nearby NetworkManager access points; Connect activates the selected SSID through `nmcli`.
  Right-click menus also open the matching settings.
- **Running applications on the taskbar.** Wine sends no shell hook messages; like win4wine's proxy,
  `xpstart.exe` follows the top-level windows and tells XP's taskbar about them. It also restarts Explorer when
  it ends without Log Off or Turn Off Computer (it watches for `WM_QUERYENDSESSION`), and points the desktop
  icons that XP's tools store as `explorer.exe,N` (My Computer's, for one) at XP's Explorer.
- **Internet Explorer 6** from your ISO (`iexplore.exe`, `mshtml.dll`, `jscript.dll`, `urlmon.dll`,
  `wininet.dll`…) for the desktop's Internet Explorer icon, installed beside its `iexplore.exe` and loaded
  native in it only. It opens plain `http://` pages; most of today's HTTPS sites need newer TLS than IE6 has, so
  the Start menu's Internet item and web links still open the Linux browser (`helpers/iexplore.c`). Assets
  extracted before this can add it with `XP_ISO=/path/to/the.iso ./xp.sh run`.
- **XP's minimize, maximize and restore animation**: the caption flies to the taskbar and back (Display
  Properties, Appearance, Effects, "Animate windows when minimizing and maximizing").
- **16x16 icons in title bars**, taken from each program's icon resource rather than its big icon shrunk.
- **XP's own registry defaults** from your ISO's setup files: the sound scheme (`HIVEDEF.INF`, with the sounds of
  `Windows\Media`: Explorer's navigation click, balloons, errors, startup and logoff) and Explorer's settings
  (`HIVESFT.INF`), converted by `scripts/xpreg.py`. Your folders keep pointing at your Linux ones.
- **Image thumbnails and previews** (Thumbnails and Filmstrip views, the task pane's preview) with your ISO's
  `shimgvw.dll`, and XP's names and icons for My Pictures, My Music, My Recent Documents and Shared Documents
  (a `desktop.ini` in each, as XP's setup writes it).
- **Windows Picture and Fax Viewer** from your ISO opens the common image formats from XP folders.
- **Windows Media Player 9** from your ISO is in Accessories\Entertainment and in Open With for its supported
  audio and video formats; Play commands open it directly. Wine's DirectShow uses its GStreamer backend for
  playback.
- **The Security Center balloon** ("Your computer might be at risk") with XP's red shield, from `xpsp2res.dll`.
- `C:\xp\xpstart.log` notes why each session ended, for bug reports.
- **XP's Display Properties** (Themes, Desktop, Screen Saver, Appearance, Settings) from your ISO's `desk.cpl`
  and `themeui.dll`, so the wallpaper can be changed.
- **The folder task pane** (System Tasks, Other Places, Details) with your ISO's DirectUser (`duser.dll`).
- **Thirteen more Wine patches** on top of win4wine's series, each described at the top of its file in
  `wine/patches` (applied after `../wine/patches`): `0001` (apphelp/advapi32 checks XP's shell32 makes before
  running a program), `0002`, `0005`, `0006` and `0008` (uxtheme exports and ordinals XP's Display Properties
  uses), `0003`, `0004` and `0011` (XP's common controls beside the program), `0007` (`SetSysColorsTemp`),
  `0009` (the desktop window stays under the taskbar), `0010` (small caption icons), `0012` (the caption
  animation) and `0013` (sound paths with environment variables). The message pump hook for DirectUser is now
  win4wine's own (its patch 0080).
- **win4wine's later XP fixes**, through its series: XP's menu bar, control and message box metrics, system
  icons (from your ISO's `user32.dll`, kept as `C:\xp\res\user32-xp.dll`), bitmap font metrics, and more.
- **A Wine virtual desktop instead of a session.** The desktop is not named "Default", so Wine's own desktop
  does not claim the shell window and XP Explorer becomes the shell without a window manager or greeter.
- **Runs on the system's Wine too, in a degraded mode.** Without `./xp.sh wine`, a launcher (`xpstart.exe`) loads
  a small compatibility DLL (`xpcompat.dll`) into XP Explorer. It fills in at run time the essentials that
  win4wine patches into Wine: the SCM readiness event (0009), the dock station, Automatic Updates and shut down
  reason exports (0010–0012), hiding WOW64 (0017) and the Start button (0055). The taskbar background, the
  stretched wallpaper and icon transparency still need the patched Wine.
- **Much smaller.** The X11 session, window manager bridge, greeter, Wayland compositor, ReactOS shell,
  Japanese profile and test suite are left out. `xp.sh` replaces `scripts/setup.sh`.
- **Arch-friendly.** Extraction uses `xorriso`. Wine is built outside the project folder, because Wine's
  `configure` does not accept paths with spaces. The system's Wine is never replaced: only `xp.sh` uses the
  patched one, through the `.local/wine-install` link.

## Known limits

- Linux applications started from the Start menu open as normal KDE windows, outside the virtual desktop.
  Bringing them into XP's frames and taskbar is what win4wine's window manager bridge does in its full session.
- "Turn Off Computer" closes the virtual desktop.
- XP's own list of most used programs stays empty under Wine (explorer never reads the usage data for it);
  pins stand in for it (see above).
- Menus and tooltips do not fade in: a Wine virtual desktop cannot blend windows over others. Restoring a window
  from its taskbar button is not animated (only the title bar buttons and the window menu are).
- Applying another visual style or the Windows Classic theme in Display Properties is not finished.
- Only the English XP SP3 ISO has been tried.

## How it works

```text
xp.sh run
 └─ wine explorer /desktop=XP,WxH          (Wine's virtual desktop, not named "Default")
     └─ xpstart.exe                        (sets Global\ScmCreatedEvent, restarts Explorer if it crashes)
         ├─ xpcompat.dll                   (system Wine only: run-time fixes)
         └─ C:\xp\xp-explorer.exe          (XP's explorer.exe, shell32, browseui... from your ISO)
             ├─ Run key: xptray.exe        (network, battery, volume)
             └─ Start menu: Linux apps via linux-launch.exe, pins via startmenu.exe
```

| Path | Contents |
|---|---|
| `xp.sh` | the entry point |
| `wine/patches/` | XP Explorer's Wine patches, applied after win4wine's (`../wine/`) |
| `scripts/extract.py`, `scripts/media.py` | extracts the needed files from the ISO (win4wine) |
| `scripts/prefix.py` | creates the prefix and its registry profile (trimmed from win4wine's `prefix/`) |
| `scripts/linux_apps.py` | Linux applications in All Programs (win4wine) |
| `scripts/pins.py` | pinned programs |
| `helpers/` | small Win32 programs, built with mingw-w64 into `.local/bin` |

## Caution

> [!WARNING]
> - **Obtain the Windows XP ISO yourself.** Neither this repository nor its scripts download any Microsoft files.
>   Follow Microsoft's license terms for the ISO and the files extracted from it.
> - **Use at your own risk.**
> - This is an unofficial project, not affiliated with Microsoft, the Wine project or win4wine.

## Ground rules

Inherited from win4wine:

- Binaries from Windows are never modified; differences are handled in Wine and around it.
- Microsoft binaries are never disassembled. Behavior is studied through public APIs. `xpcompat.dll` only
  replaces entries of import tables and Wine's own stub exports.
- Microsoft files, extracted files and prefixes live only in `.local/`, which Git ignores.

## License

[LGPL-2.1-or-later](../LICENSE), the same as win4wine and Wine. It covers only the code and patches in this repository.
The Windows XP files are copyrighted by Microsoft.

## Credits

- [win4wine](https://github.com/mikuta0407/win4wine) by mikuta0407: the Wine patches, the extraction, the prefix
  profile, `linux_apps.py`, and `linux-launch.c`, `linux-links.c` and `theme-settings.c` (built from
  `../session/` and `../prefix/`).
- [Wine](https://www.winehq.org/).
