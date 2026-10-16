#!/bin/bash
# Disposable Debian 13 build and X11 validation environment; binaries stay local.
set -euo pipefail
root=$(cd -- "$(dirname -- "$0")/.." && pwd)
fs=$root/.local/debian
mounts() {
    sudo mkdir -p "$fs/work" "$fs/proc" "$fs/dev" "$fs/sys"
    mountpoint -q "$fs/work" || sudo mount --bind "$root" "$fs/work"
    mountpoint -q "$fs/proc" || sudo mount -t proc proc "$fs/proc"
    if ! mountpoint -q "$fs/sys"; then
        sudo mount --bind /sys "$fs/sys"
        sudo mount -o remount,bind,ro "$fs/sys"
    fi
    if ! mountpoint -q "$fs/dev"; then
        sudo mount --rbind /dev "$fs/dev"
        sudo mount --make-rslave "$fs/dev"
    fi
}
case ${1:-help} in
    init)
        command -v debootstrap >/dev/null || sudo apt-get install -y debootstrap
        mkdir -p "$root/.local"
        if [[ ! -f $fs/etc/debian_version ]]; then
            sudo debootstrap --variant=minbase trixie "$fs" http://deb.debian.org/debian
        fi
        mounts
        sudo chroot "$fs" apt-get update
        # WoW64 Wine: 64-bit Unix libraries only, i686 and x86_64 MinGW for the PE modules.
        sudo chroot "$fs" apt-get install -y --no-install-recommends \
            build-essential flex bison pkg-config git ca-certificates gettext \
            python3 python3-xlib cabextract p7zip-full xvfb xauth xterm x11-utils xdotool imagemagick \
            python3-gi-cairo gir1.2-pango-1.0 \
            libx11-dev libxext-dev libxrender-dev libxrandr-dev libxi-dev libxcursor-dev libxinerama-dev \
            libfreetype-dev libfontconfig-dev libgnutls28-dev libgl-dev libasound2-dev libdbus-1-dev \
            libudev-dev gcc-mingw-w64-i686 gcc-mingw-w64-x86-64
        if ! sudo chroot "$fs" getent passwd "$(id -u)" >/dev/null; then
            sudo chroot "$fs" useradd -m -u "$(id -u)" builder
        fi
        ;;
    run)
        shift
        mounts
        exec sudo chroot --userspec="$(id -u):$(id -g)" "$fs" /usr/bin/env \
            HOME=/home/builder USER=builder PATH=/usr/local/bin:/usr/bin:/bin \
            "${@:?Supply a command, e.g. /bin/bash}"
        ;;
    stop)
        # Stop processes you launched inside the chroot before unmounting.
        mountpoint -q "$fs/work" && sudo umount "$fs/work"
        mountpoint -q "$fs/proc" && sudo umount "$fs/proc"
        mountpoint -q "$fs/sys" && sudo umount "$fs/sys"
        mountpoint -q "$fs/dev" && sudo umount -R "$fs/dev"
        true
        ;;
    *) echo 'Usage: scripts/debian.sh init | run COMMAND [ARGS...] | stop' ;;
esac
