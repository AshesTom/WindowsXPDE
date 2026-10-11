# Debian 13 / LightDM の独立 VM 試験

実測した構成は QEMU/KVM、2 vCPU、2 GiB RAM、16 GiB sparse ext4、std VGA、
Debian kernel 6.12.107+deb13、Xorg、LightDM。ネットワークデバイスは付けない。
ISO・抽出物・prefix・ディスク・画面・ログはすべて `.local/` / `localres/` に保存する。

以下はホストで行う。先に [再現手順](reproduce.md) の Debian chroot、ソース Wine、
bridge、新規 `.local/prefix-clean` を作成し、その prefix の専用 wineserver を終了する。
ホストに `qemu-system-x86`、`rsync`、`e2fsprogs` が必要。
出力先が既にある場合は別名を使い、稼働中のディスクを再作成しない。

```sh
mkdir -p .local/guest-root .local/vm-test/mnt
sudo rsync -aH --exclude=/work/*** --exclude=/proc/*** --exclude=/dev/*** \
  --exclude=/sys/*** --exclude=/run/*** --exclude=/tmp/*** \
  .local/debian/ .local/guest-root/
sudo mkdir -p .local/guest-root/{dev,proc,sys,tmp,work}
sudo chmod 1777 .local/guest-root/tmp
```

ゲストのパッケージ導入時だけ dev / proc を mount する。サービスの起動を止める
policy-rc.d はこのゲストルートの中だけに置く。

```sh
sudo sh -c 'printf "#!/bin/sh\nexit 101\n" > .local/guest-root/usr/sbin/policy-rc.d'
sudo chmod +x .local/guest-root/usr/sbin/policy-rc.d
sudo mount --rbind /dev .local/guest-root/dev
sudo mount --make-rslave .local/guest-root/dev
sudo mount -t proc proc .local/guest-root/proc
sudo chroot .local/guest-root apt-get update
sudo chroot .local/guest-root apt-get install -y linux-image-amd64 systemd-sysv \
  dbus-user-session lightdm lightdm-gtk-greeter xserver-xorg xserver-xorg-video-fbdev \
  xserver-xorg-video-vesa x11-utils procps
sudo umount .local/guest-root/proc
sudo umount -R .local/guest-root/dev
sudo rm .local/guest-root/usr/sbin/policy-rc.d
sudo rsync -a --exclude=/.git --exclude=/.local --exclude=/localres ./ .local/guest-root/work/
sudo mkdir -p .local/guest-root/work/.local
sudo rsync -a .local/wine-install .local/bridge .local/prefix-clean .local/guest-root/work/.local/
sudo chown -R 1000:1000 .local/guest-root/work
```

chroot の builder UID が 1000 でない場合は最後の所有者を合わせる。
ゲストルート内に次の設定を作る（ホスト自身の `/etc` へ書かない）。

- `/usr/local/bin/xp-shell-session`: executable shell script、`exec python3 /work/session/supervisor.py "$@"`。
- `/usr/share/xsessions/xp-shell.desktop`: リポジトリの `session/xp-shell.desktop` をコピー。
- `/home/builder/.config/win4wine/session.json`: builder 所有。
  `{"wine":"/work/.local/wine-install/bin/wine","prefix":"/work/.local/prefix-clean","readiness_timeout":45}`。
- `/etc/lightdm/lightdm.conf.d/90-xp-test.conf`:

```ini
[Seat:*]
autologin-user=builder
autologin-user-timeout=0
autologin-session=xp-shell
user-session=xp-shell
greeter-session=lightdm-gtk-greeter
```

`sudo systemctl --root="$PWD/.local/guest-root" enable lightdm` を実行する。
`/etc/fstab` は `/dev/vda / ext4 defaults 0 1`、`/etc/machine-id` は空にする。
検証用の自動ログインはこのネットワークなし VM だけの設定。
必要なら VM の serial-getty@ttyS0 に `agetty --autologin root` の override を設定する。

```sh
truncate -s 16G .local/vm-test/root.ext4
mkfs.ext4 -F .local/vm-test/root.ext4
sudo mount -o loop .local/vm-test/root.ext4 .local/vm-test/mnt
sudo rsync -aH --exclude=/proc/*** --exclude=/dev/*** --exclude=/sys/*** \
  --exclude=/run/*** --exclude=/tmp/*** .local/guest-root/ .local/vm-test/mnt/
sudo mkdir -p .local/vm-test/mnt/{proc,dev,sys,run,tmp}
sudo chmod 1777 .local/vm-test/mnt/tmp
sudo umount .local/vm-test/mnt
```

インストールされた kernel / initrd のファイル名を指定し、直接 kernel boot する。
GRUB やホストの起動設定は変更しない。

```sh
sudo qemu-system-x86_64 -enable-kvm -cpu host -m 2048 -smp 2 \
  -kernel .local/guest-root/boot/vmlinuz-6.12.107+deb13-amd64 \
  -initrd .local/guest-root/boot/initrd.img-6.12.107+deb13-amd64 \
  -append 'root=/dev/vda rw console=ttyS0 systemd.unit=graphical.target' \
  -drive file=.local/vm-test/root.ext4,format=raw,if=virtio \
  -vga std -display vnc=127.0.0.1:5 -nic none -serial mon:stdio
```

VNC viewer でホストの loopback 5905 を開く。LightDM が XP Shell を選択し、
XP のデスクトップとタスクバーが現れることを確認する。
ゲストの `~/.local/state/win4wine/status.json` は `xp-shell`、
セッション環境は `DESKTOP_SESSION=xp-shell` / `XDG_SESSION_TYPE=x11` となる。

ゲストの GUI 端末、または serial console から builder の DISPLAY / XAUTHORITY を指定して、
`tests/recovery_integration.py ~/.config/win4wine/session.json` を実行する。
この試験は専用テストセッションの XP Shell を 3 回強制終了する。
2 回の再起動後に `fallback` となり、builtin explorer のホームと xterm が見えることを検査する。
終了はゲストで `poweroff`。再試験には LightDM の再起動または VM の再起動を使う。

実測ログと画像の一覧は [検証状況](status.md)。実際の今回の検証では headless QMP
screendump と、ネットワークを使わない VM 専用 virtio-serial の試験用コマンド経路も使った。
それらは `.local/` のみに置き、製品セッションには組み込まない。

## ログイン画面（段階 9）

ゲストに `gir1.2-lightdm-1`、`python3-cairo`、`python3-gi-cairo` を入れ（ネットワークが無いので .deb をホストで取得して
`dpkg -i`）、新しい抽出物（logonui.exe などを含むもの）をゲストに置いて `greeter/install.sh` を root で実行する。
上の `90-xp-test.conf` は自動ログインで、greeter はログオフの後に出る。greeter から試すときは、ゲストにだけ
`/etc/lightdm/lightdm.conf.d/96-s9-test.conf`（`[Seat:*]` と空の `autologin-user=`）を置き、builder に試験用のパスワードを付けて
LightDM を再起動する。終わったらこのファイルを消す。操作は `DISPLAY=:0 XAUTHORITY=/var/run/lightdm/root/:0 xdotool ...`。
