# クリーン Debian 13 での再現

ホストで git、sudo、debootstrap が使え、約 15 GB 以上の空きがあること。XP SP3 x86 ISO または XP Windows ディレクトリは利用者が用意する。Microsoft バイナリの取得処理はない。chroot は VM 内で実行する開発検証用で、独立した VM カーネルの試験ではない。

```sh
scripts/debian.sh init
scripts/debian.sh run /bin/bash
```

以後 chroot 内で実行する。`/work` がこのリポジトリ。Wine の upstream 取得・パッケージ取得だけはネットワークを使う。

```sh
cd /work
./wine/build.sh > .local/wine-build.log 2>&1
./bridge-wm/build.sh
./extract/extract.py localres/WindowsXPSP3.iso .local/assets
Xvfb :95 -screen 0 1280x800x24 -nolisten tcp > .local/xvfb.log 2>&1 &
export DISPLAY=:95
export WINE=/work/.local/wine-install/bin/wine
./prefix/create.py .local/assets .local/prefix-clean --home /home/builder --wine "$WINE" > .local/prefix-clean.log 2>&1
export WINEPREFIX=/work/.local/prefix-clean
```

出力先は既存ディレクトリを拒否する。やり直すときは別名で作る。既存 prefix を無条件で消す手順は用意しない。`.local/wine-build/config.log` には構成・依存の実測が残る。ビルドは 32-bit Unix ライブラリと i686 MinGW を使う。DirectX ヘルパーの版違いと Wine 回帰テストのビルドを省く。通常の Wine テストを追加する場合は別 build directory を使う。

XP の実行ファイルは内容を変えず `C:\xp\xp-explorer.exe` に配置する。Wine 自身の `explorer.exe` と AppDefaults を分けるため。native DLL の override はそのファイル名に限定する。起動時には新しい desktop manager に外部 shell フラグを渡す必要があるため、単体試験の前にその専用 prefix の wineserver を終了する。

```sh
WINEPREFIX="$WINEPREFIX" /work/.local/wine-install/bin/wineserver -k
export WIN4WINE_EXTERNAL_SHELL=1
export WINEDEBUG=+seh,+loaddll
mkdir -m700 .local/ipc-clean
python3 bridge-wm/wm.py --ipc .local/ipc-clean > .local/wm-clean.log 2>&1 &
./prefix/profile.sh shell > .local/xp-shell-clean.log 2>&1 &
"$WINE" .local/bridge/proxy.exe 'Z:\work\.local\ipc-clean' > .local/proxy-clean.log 2>&1 &
"$WINE" .local/bridge/inspect.exe dump
"$WINE" .local/bridge/inspect.exe shortcuts
```

検証項目は `status.md` を参照。Xvfb 上のスクリーンショットには Microsoft 由来の表示内容が含まれるため Git に入れない。

```sh
import -window root .local/desktop.png
```

WM 単体の統合試験は XP shell と分け、独立 X display / IPC directory で行う。

```sh
XP_BRIDGE_DIR=/work/.local/ipc-clean python3 tests/wm_integration.py
```

## ログイン用 Xsession

実機 Debian 13 / Ubuntu 24.04 に Xorg、ディスプレイマネージャ、python3-xlib、xterm を導入し、上記と同じ 32-bit ビルド依存を入れて Wine をビルドする。ホスト上の prefix はその利用者のホームを `--home` で指定する。chroot の `/work` パスの prefix はホスト用に流用しない。

`~/.config/win4wine/session.json` に、その実機上の絶対パスで設定する。

```json
{
  "wine": "/absolute/path/win4wine/.local/wine-install/bin/wine",
  "prefix": "/absolute/path/win4wine/.local/prefix",
  "debug": "+timestamp,+seh,+loaddll"
}
```

```sh
./session/install.sh
```

次回ログイン画面で **XP Shell (Wine, X11)** を選ぶ。既存セッション定義は変更しない。復旧ログは `~/.local/state/win4wine/`。Wine builtin explorer と native xterm へフォールバックする。これは native shell の安定性とディスプレイマネージャのログイン試験が完了してから常用する。

削除は `/usr/share/xsessions/xp-shell.desktop` と `/usr/local/bin/xp-shell-session` の 2 ファイルだけ。インストールされた wrapper はこのリポジトリを参照するため、移動後は再インストールする。

## 既存ハイブがある場合

`python3-hivex` を追加インストールし、次のようにローカルの読み取り専用変換を行う。

```sh
./prefix/hive-to-reg.py --software /local/XP/system32/config/SOFTWARE --system /local/XP/system32/config/SYSTEM --output .local/shell-hive.reg
```

出力を確認してから専用 prefix に `wine reg import` する。4 個の名前空間 CLSID と NLS の限定した値だけを扱う。サービス、ドライバ、ログオン、製品情報は移植しない。ハイブなしでのクリーン prefix が基準で、この補助経路は実ハイブの検証状況を `status.md` に明記する。

## Git の隔離

```sh
git config core.hooksPath .githooks
```

pre-commit はローカル専用領域、PE、ハイブ、メディア、フォント、NUL を含むバイナリを拒否する。`git add -f` でもこの検査を通す。ソースとパッチはリポジトリ、ISO・抽出結果・prefix・ビルド・生ログ・画像は `.local/` または `localres/`。
