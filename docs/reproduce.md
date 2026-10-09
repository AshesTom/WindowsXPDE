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
./tests/build.sh
./extract/extract.py localres/WindowsXPSP3.iso .local/assets
Xvfb :95 -screen 0 1280x800x24 -nolisten tcp > .local/xvfb.log 2>&1 &
export DISPLAY=:95
export WINE=/work/.local/wine-install/bin/wine
./prefix/create.py .local/assets .local/prefix-clean --home /home/builder --wine "$WINE" > .local/prefix-clean.log 2>&1
export WINEPREFIX=/work/.local/prefix-clean
```

出力先は既存ディレクトリを拒否する。やり直すときは別名で作る。既存 prefix を無条件で消す手順は用意しない。`.local/wine-build/config.log` には構成・依存の実測が残る。ビルドは 32-bit Unix ライブラリと i686 MinGW を使う。DirectX ヘルパーの版違いと Wine 回帰テストのビルドを省く。通常の Wine テストを追加する場合は別 build directory を使う。

新規 prefix は自作 `theme-settings.exe` で Wine の保存済み classic metrics も復元する。
旧手順の `ThemeActive=0` だけでは Luna の caption height が残り、クラシックのボタンが縦長になる。
既存 prefix は `WINEPREFIX=... WINE=... python3 prefix/display.py --theme classic --dpi 96` で復元し、専用 session を再起動する。
96 DPI の新規 prefix では公開 SPI が caption width/height とも18、caption font は -11 Tahoma を返した。
これは Wine の classic 既定値の確認であり、実機 XP の寸法を検証したものではない。

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

## 復旧と表示の検証

[独立 VM の手順](vm.md) で Debian の実カーネル / Xorg / LightDM を確認できる。
復旧は `tests/recovery_integration.py CONFIG.json` を使う。このテストは専用の試験セッションで
XP Shell を強制終了し、再起動回数と builtin ブラウザ / xterm の表示を検査する。
復旧 prefix は XP 用 prefix 内の `recovery/` に作る。MS ファイルや native COM 登録は導入しない。

表示設定は専用 prefix を指定して保存し、専用セッションを再起動する。

```sh
WINE="$WINE" WINEPREFIX="$WINEPREFIX" ./prefix/display.py --theme classic --dpi 144
# 200% は --dpi 192。通常 XP Start と Luna は --theme luna --menu xp。
```

`tests/build.sh` 後、`wine .local/bridge/display-test.exe 144 0` で実際の system DPI と
classic を検査できる。第2引数 1 は Luna API / テーマファイルの読み込み検査であり、
完全な見た目の合格判定ではない。DPI を戻すには `--theme classic --dpi 96`。

`wine/build.sh` は重なる Wine パッチも `wine/patch-state.py` で判定する。
一時コピーで逆順に戻してから全系列を検証し、不足分だけを適用する。
既存 Wine ソースを reset したり、利用者の変更を破棄したりしない。衝突はビルド前に報告する。

Wine の更新時は `prefix/stage.py --wine WINE PREFIX` が wine.inf の timestamp を照合し、
wineboot 更新 → XP COM 再登録 → 専用 wineserver 終了 → native DLL の atomic 配置を行う。
supervisor / profile.sh から自動実行される。初回の更新には通常起動より時間がかかる。

## 表示修正後の既存 prefix の更新

以前の抽出には `browselc.dll` / `shdoclc.dll` や Luna 用 Trebuchet 4種がないため、
同じ利用者のローカルメディアを
**新しい出力先**へ再抽出する。シェルセッションを終了してから以下を実行する。
`WINEPREFIX` は更新する専用 XP prefix、`WINE` はソースビルド版の絶対パス。
Microsoft のファイルは元の内容のままコピーする。

```sh
./extract/extract.py localres/WindowsXPSP3.iso .local/assets-luna
./wine/build.sh > .local/wine-luna-build.log 2>&1
./bridge-wm/build.sh
./tests/build.sh
WINEPREFIX="$WINEPREFIX" "$(dirname "$WINE")/wineserver" -k
WINEPREFIX="$WINEPREFIX" "$(dirname "$WINE")/wineserver" -w
cp .local/assets-luna/system32/{browselc,shdoclc}.dll "$WINEPREFIX/drive_c/xp/native/"
cp .local/assets-luna/fonts/* "$WINEPREFIX/drive_c/windows/Fonts/"
./prefix/stage.py --wine "$WINE" "$WINEPREFIX"
```

stage は資源 DLL と自作 Wine controls の別名モジュールを atomic 配置し、
registry profile version の変更を Wine 更新と別に検出する。version 4 は All Programs の
最小 COM 登録、My Computer の XP Explorer アイコン参照、利用者が pin するための shortcut handler を復元する。既に version 4 かつ Wine が同じなら registry 更新を繰り返さない。
新規 prefix は `create.py` が資源不足を開始前に拒否する。全体の override は builtin、
XP explorer の AppDefaults だけ native。復旧 prefix には導入しない。

クラシックな外観とスタートメニューの形式は独立して指定できる。
通常 XP Start の保存画像、フォント、owner-draw の clip を Wine 側で修正した。
初期設定のクラシック Start から、以下で通常 XP Start と Luna を選択できる。

```sh
WINE="$WINE" WINEPREFIX="$WINEPREFIX" ./prefix/display.py --theme luna --dpi 96 --menu xp
# クラシックへ戻す場合は --theme classic --menu classic。その後専用セッションを再起動する。
```

`--menu` は専用 helper が native shell32 の公開 SHGetSetSettings / SSF_STARTPANELON を呼び、
設定を読み戻す。レジストリの ShellState バイナリを手作業で書き換えない。
`--theme` は自作 theme-settings helper が Wine builtin uxtheme のテーマ適用を呼び、
色・font・非クライアント寸法をまとめて反映する。単に ThemeActive の registry 値を変更する手順ではない。
helper の ordinal とシグネチャは Wine 自身の公開ソースに合わせ、native uxtheme は使用しない。
DPI 変更の反映には専用 Wine セッションの再起動が必要。

表示 API 試験は同じ専用 prefix、独立 X display で行う。

```sh
WIN4WINE_EXTERNAL_SHELL=1 WINEDLLOVERRIDES='explorer=b;mscoree,mshtml=' \
  "$WINE" .local/bridge/system-menu-test.exe
WIN4WINE_EXTERNAL_SHELL=1 WINEDLLOVERRIDES='comctl32=n;explorer=b;mscoree,mshtml=' \
  "$WINE" .local/bridge/rebar-test.exe
WIN4WINE_EXTERNAL_SHELL=1 WINEDLLOVERRIDES='explorer=b;mscoree,mshtml=' \
  "$WINE" .local/bridge/print-window-test.exe
# Luna を選択して再起動した prefix で、NULL DC を許す公開 font API を検査する。
WINEDLLOVERRIDES='uxtheme,explorer=b;mscoree,mshtml=' \
  "$WINE" .local/bridge/theme-font-test.exe
XP_BRIDGE_DIR=/absolute/private/ipc python3 tests/xterm_redraw.py
```

最後の試験には起動済み WM が必要。xterm の画面だけを一度消して、フォーカス変更で
文字が入力なしで戻ることを検査する。対象は試験自身が起動した xterm に限定する。
実 XP の左クリック操作、フォルダ移動、通常 Start / All Programs の実際の項目と操作は
別途画面で確認する。API 試験だけでメニュー全体の完成とは判断しない。


## native アプリの XP 枠の検証

`bridge-wm/build.sh` で更新した proxy と WM を同時に再起動する。
専用 prefix は `HKCU\Software\Wine\X11 Driver` の `Decorated=N` を使う。
create.py はこの設定を作成する。Wine に非クライアント領域を描画させるための設定である。
枠の classic / Luna は同じ prefix の表示設定に従う。

`tests/native_frames.py` は、指定した独立 X display 上で WM と proxy が既に動いている状態で実行する。
`XP_BRIDGE_DIR` はその WM / proxy が共有している所有者専用の IPC ディレクトリを指定する。
テストは自身の native X ウィンドウを作り、実際にマウスを動かすので通常の作業用 display では実行しない。

```sh
DISPLAY=:93 XP_BRIDGE_DIR=/absolute/private/ipc \
  WINE=/absolute/path/to/wine WINEPREFIX=/same/prefix/as/proxy python3 tests/native_frames.py
```

枠と native の正確な位置・サイズ、積み順、単一タスク ID、代理への EWMH activation、
Start popup の除外、タイトルバー移動、角リサイズ、最大化と復元、最小化と復元、
閉じるボタンの WM_DELETE_WINDOW、withdraw と再表示を検査する。
同時に EWMH / ICCCM のアイコン取得、更新・削除、Wine 既定アイコン、実 HICON と可視 caption の画素、
half alpha を検査する。Wine / prefix は proxy と同じものを指定し、build 済み inspect.exe を使う。
`XP_FRAME_ICON_SCREENSHOT=/absolute/local/output.png` を指定すれば実アイコン表示中の画面も保存できる。
classic と Luna は表示設定を保存して専用 Wine セッションを再起動し、それぞれ実行する。
これは proxy と native の組の操作試験であり、スタートメニュー全体、Wine アプリ全体のテーマ、
native アプリとの clipboard 相互運用の合格を代用しない。


## テーマ適用と system font の往復試験

`tests/theme-metrics.c` はテーマ設定を変更するため、作業用ではなく専用の使い捨て
XP prefix で実行する。Wine builtin のテーマ適用を2往復し、実際の caption / menu 等の
6種類の SPI font と theme API の値を比較する。終了時は classic へ戻る。

```sh
WINE="$WINE" WINEPREFIX="$TEST_PREFIX" ./prefix/display.py --theme luna --dpi 144
WINEPREFIX="$TEST_PREFIX" "$(dirname "$WINE")/wineserver" -k
WINEPREFIX="$TEST_PREFIX" "$(dirname "$WINE")/wineserver" -w
WINEPREFIX="$TEST_PREFIX" WINEDLLOVERRIDES='uxtheme,explorer=b;mscoree,mshtml=' \
  "$WINE" .local/bridge/theme-metrics-test.exe 144
```

96 / 192 DPI も同じ手順で設定・再起動し、試験の引数を合わせる。
保存された DPI と起動中プロセスの DPI は、専用セッションの再起動までは同じとは限らない。
