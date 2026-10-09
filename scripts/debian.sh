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
        sudo chroot "$fs" dpkg --add-architecture i386
        sudo chroot "$fs" apt-get update
        sudo chroot "$fs" apt-get install -y --no-install-recommends \
            build-essential gcc-multilib g++-multilib flex bison pkg-config git ca-certificates \
            python3 python3-xlib cabextract p7zip-full xvfb xauth xterm x11-utils xdotool imagemagick \
            libx11-dev libx11-dev:i386 libxext-dev:i386 libxrender-dev:i386 libxrandr-dev:i386 \
            libxi-dev:i386 libxcursor-dev:i386 libxinerama-dev:i386 libfreetype-dev:i386 \
            libfontconfig-dev:i386 libgnutls28-dev:i386 libgl-dev:i386 libasound2-dev:i386 \
            libdbus-1-dev:i386 libudev-dev:i386 gcc-mingw-w64-i686
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
