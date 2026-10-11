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

出力先は既存ディレクトリを拒否する。やり直すときは別名で作る。既存 prefix を無条件で消す手順は用意しない。`.local/wine-build/config.log` には構成・依存の実測が残る。ビルドは Wine の新しい WoW64 方式で、64-bit の Unix ライブラリと i686・x86_64 の MinGW を使う（段階 7。i386 のライブラリは要らない）。DirectX ヘルパーの版違いと Wine 回帰テストのビルドを省く。通常の Wine テストを追加する場合は別 build directory を使う。

新規 prefix は自作 `theme-settings.exe` で Wine の保存済み classic metrics も復元する。
旧手順の `ThemeActive=0` だけでは Luna の caption height が残り、クラシックのボタンが縦長になる。
既存 prefix は `WINEPREFIX=... WINE=... python3 prefix/display.py --theme classic --dpi 96` で復元し、専用 session を再起動する。
初期実装では96 DPI の新規 prefix の caption font は -11 Tahoma だった。
現在の日本語 XP profile（version 7）では新規 prefix の Classic 字体を MS UI Gothic -12 にする。
既存 prefix の独自字体は移行時に上書きしない。Luna の字体は公開 API 実測に基づく別 profile を使用する。
version 6 は `ClockFont`（96 DPI で MS UI Gothic -12 regular）を未存在時だけ追加する。
外部シェルの日本語 Luna profile が有効かつテーマ選択が一致する場合に、TrayNotify の Clock 字体へ適用する。
独自の ClockFont、無効な値、profile の無効化はそのまま保持し、値が欠落・不正なら Wine のテーマ字体へ戻る。
version 18 は XP 既定の `ListviewShadow=1`（壁紙上のアイコン名に影）を未存在時だけ入れる。
version 19 はフォルダ オプションの「全般」「表示」ページ（XP shell32 の {6D5313C0-…}）を登録する。
version 20 は XP 既定の視覚効果（UserPreferencesMask `be 3e 07 80`、メニュー等の影を含む）を一度だけ入れる。
version 21 は Folder の explore 動詞（Win+E と「エクスプローラ」、XP と同じ DDE `[ExploreFolder(...)]`）と、
コントロール パネルの名前（XP shell32 の文字列 4161）を入れる。
version 22 は Folder の open / explore に XP と同じ形の command（`C:\xp\xp-explorer.exe /idlist,%I,%L`）を加え、
「ファイル名を指定して実行」からフォルダを開けるようにする。
version 23 は「新規作成」のハンドラー（{D969A300-…}）の CLSID に XP と同じ `flags=0` を加え、デスクトップの右クリックメニューの
「ショートカットの貼り付け」と「新規作成」の間に区切り線を出す。
version 17 は日本語 Luna profile に Start のユーザー名と「すべてのプログラム」の字体を加える。
version 15/16 は壁紙（Bliss を XP と同じく Wallpaper1.bmp に変換、拡大表示）と `LogonType=1` を一度だけ設定する。
画像は再抽出（30ファイル）で入り、session 開始時の stage で `C:\windows\Web\Wallpaper` とユーザー画像へ置く。
version 12/13 は XP Explorer にだけ msgina を native で読ませ、`Logon User Name` に利用者名を入れる。
（msgina.dll は再抽出で `C:\xp\native` に入る。無い既存 prefix ではシャットダウンは従来どおり使えない。）
特殊フォルダの名前とアイコン（desktop.ini、マイ ドキュメントの DefaultIcon）は session 開始時の stage が入れる。
mydocs.dll は再抽出（31ファイル）で入る。既存 prefix では `C:\xp\native` に置くとマイ ピクチャのアイコンも入る。
version 11 は日本語 XP のデスクトップアイコン横間隔78px（`IconSpacing=-1170`）を一度だけ設定する。
version 10 は My Computer 等の LocalizedString/InfoTip を XP shell32 のリソース参照にする。
version 9 は XP shdocvw の Explorer Band（`{EFA24E64-B078-11D0-89E4-00C04FC9E26E}`）を登録し、
フォルダウィンドウの「フォルダ」ボタンでツリーを表示する。
version 8 は `TaskbarSizeMove=0`（タスクバーを固定）を未存在時だけ追加する。
XP の既定ユーザーと同じく、グリッパーと上端のサイズ変更帯がない30px高（96 DPI）で始まる。
固定を解除すると explorer 自身が値を1にし、34px高でサイズ変更帯が上端の二重線として見える。
version 7 は `TaskFont`（96 DPI で MS UI Gothic -12 regular）を未存在時だけ追加する。
外部シェルの同じ日本語 Luna profile に限り、TaskBand の Toolbar テーマ字体へ適用する。
コントロール自身の WM_GETFONT（実XPでは -13）とは別の公開テーマ字体で、独自値と欠落・不正時の fallback は ClockFont と同じ。

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

実機 Debian 13 / Ubuntu 24.04 に Xorg、ディスプレイマネージャ、python3-xlib、xterm を導入し、上記と同じビルド依存（64-bit の開発ファイルと i686・x86_64 の MinGW、`scripts/debian.sh` の init の一覧）を入れて Wine をビルドする。ホスト上の prefix はその利用者のホームを `--home` で指定する。chroot の `/work` パスの prefix はホスト用に流用しない。

`~/.config/win4wine/session.json` に、その実機上の絶対パスで設定する。

```json
{
  "wine": "/absolute/path/win4wine/.local/wine-install/bin/wine",
  "prefix": "/absolute/path/win4wine/.local/prefix",
  "debug": "+timestamp,+seh,+loaddll",
  "locale": "ja_JP.UTF-8",
  "input_method": "fcitx5",
  "linux_font": "MS UI Gothic",
  "shutdown_command": ["systemctl", "poweroff"],
  "reboot_command": ["systemctl", "reboot"]
}
```

`input_method` と `linux_font` は段階 6 で加えた（日本語入力の節）。`input_method` を省くと入力メソッドを起動しない。
`linux_font` の既定は `MS UI Gothic`、`null` にすると Linux アプリの字体の設定を変えない。

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

新規 prefix は Wine の `HKCU\Software\Wine\Fonts` に
`AntialiasFakeBoldOrItalic=N` を未設定時だけ保存する。字体自身の GASP 方針を
合成 bold/italic にも適用し、XP の小さい日本語 caption の描画へ近づける。
SPI font smoothing と LOGFONT quality は変更しない。既存 prefix の migration は
この設定を変更しない。既存 prefix には明示的に以下を実行する。

```sh
WINE="$WINE" WINEPREFIX="$WINEPREFIX" ./prefix/display.py --theme luna --dpi 96 --font-rendering xp
# Wine の既定の合成字体 AA 方針へ戻す: --font-rendering wine
```

`xp` は `N`、`wine` は `Y` を保存する。オプションを省略すると既存値を保持する。
これは専用 prefix 全体の Wine 設定なので、Explorer 以外の通常アプリにも影響する。
変更後は専用 Wine セッションを再起動する。

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
native XP タスクバーも起動している場合は `XP_FRAME_TASKBAR_ICONS=1` を加え、非同期 `WM_GETICON`
callback 後の task button 内の実画素と icon 更新後の画素も検査できる。
classic と Luna は表示設定を保存して専用 Wine セッションを再起動し、それぞれ実行する。
これは proxy と native の組の操作試験であり、スタートメニュー全体、Wine アプリ全体のテーマ、
native アプリとの clipboard 相互運用の合格を代用しない。

最大化の段階は作業領域が画面より小さいこと（タスクバーがあること）を前提にする。タスクバーのない試験 display では
Wine が画面全体を覆う最大化枠を全画面として切り詰めるため、`_WIN4WINE_CLASS=Shell_TrayWnd` の dock ウィンドウを
画面下端に置いてから実行する（変更前の bridge でも同じ）。

## WM / proxy のイベント駆動化（旧 0073〜0076、現 0021・0045・0008）

既存の環境は Wine（0073〜0076 で win32u・uxtheme・user32 が変わる）、`bridge-wm/build.sh` の proxy、
`bridge-wm/wm.py` をまとめて更新し、セッションを再起動する。新しい WM は proxy の生存を `proxy.lock` の
ファイルロックで確かめるため、古い proxy と組み合わせると Alt+Tab が XP の切り替えウィンドウを出さずに順に切り替える。
古い WM と新しい proxy の組み合わせでは、`frames.tsv` が変化のないとき書かれず、2秒で proxy が止まったと見なされる。
registry profile は変更なし（version 22）。prefix の再生成や `prefix/stage.py` の再実行は不要。

```sh
./wine/build.sh > .local/wine-build.log 2>&1
./bridge-wm/build.sh
./tests/build.sh
```

遅れと待機中の起床は、WM と proxy が動いている display で測る。

```sh
DISPLAY=:93 XP_BRIDGE_DIR=/absolute/private/ipc WINEPREFIX=/same/prefix/as/proxy \
  python3 tests/wm_latency.py
```

`XP_LATENCY_ROUNDS`（既定 20）と `XP_IDLE_SECONDS`（既定 10）で回数と待機時間を変えられる。
native ウィンドウを作って動かすので、作業用の display では実行しない。待機中の値は直前の試験の後処理が
終わってから測る。`tests/bridge_integration.py` は Win32 ウィンドウの題名変更の通知も検査するため、
題名の WinEvent（現 0021、旧 0073）のない Wine では失敗する。


## パッチの整理（段階 2）後の更新

段階 2 で `wine/patches/` を 55 本に組み直した（`docs/patches.md`）。整理前のパッチを当てたソースツリーには
`wine/patch-state.py` で新しい series を重ねられない（opt-in 判定の共通化と 0008 の変更で内容が変わったため、
「適用済み」と判定できない）。新しいソースとビルドディレクトリを別名で作り、全体をビルドする。

```sh
WINE_SOURCE=/work/.local/wine-upstream-stage2 WINE_BUILD=/work/.local/wine-build-stage2 \
  WINE_INSTALL=/work/.local/wine-install-stage2 ./wine/build.sh > .local/wine-build-stage2.log 2>&1
```

ビルドディレクトリは使い回さない。古いビルドディレクトリにソースを入れ替えて `make` すると、更新時刻の古いファイルが
再コンパイルされず、前のソースのオブジェクトが残ることがある（段階 2 で、検証用ビルドの `user32/text.o`・`win.o` と、
`x11drv.h` の古い構造体のまま `winex11.drv/bitblt.o` が残っていたのを見つけた。`docs/status.md` を参照）。
試験 VM など、モジュールを個別に差し替えてきた環境は、`lib/wine` 全体を新しい install のものに入れ替える。

使う側の設定は変わらない。registry profile は変更なし（version 22）、prefix の再生成や `prefix/stage.py` の
再実行は不要。bridge（WM・proxy）も変更なし。server protocol の変更（0023）は整理前と同じなので、wineserver と
ntdll と win32u は同じ install のものをそろえる（既存の prefix の wineserver は止めてから切り替える）。

0008 は通常の Wine アプリでもテーマ画像の描画を排他にする。ほかの変更は opt-in（`WIN4WINE_EXTERNAL_SHELL`）の
プロセスだけに効くのは整理前と同じ。


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

タスクバーの起動後レイアウト同期の寿命試験は、専用 shell supervisor を停止した X display 上で実行する。同じディレクトリの `proxy.exe` を試験自身が起動する。

```sh
WINEPREFIX=/path/to/dedicated-prefix WIN4WINE_EXTERNAL_SHELL=1 \
  /path/to/wine .local/bridge/shell-layout-sync-test.exe
```

準備前の抑止、一度だけの同期、一時非表示、Tray の破棄と再作成を公開 Win32 API の自作 fixture で検査する。実 shell の寸法・描画は別途確認する。

作業領域の動的変更は次のrunnerで検査する。Xvfbと一時prefixを自動作成するため、
現在のデスクトップやprefixの作業領域は変更しない。Microsoft資産は不要。

```sh
python3 tests/workarea.py --wine "$WINE"
```

実タスク文字と時計の診断は、96 DPI / Lunaの専用XP shell session上で実行する。
profile version 7の移行と起動完了を待ち、対象sessionのDISPLAY/WINEPREFIXを指定する。
taskbar-textは自作windowを開き、既存の同試験fixtureだけを閉じる。

```sh
WIN4WINE_EXTERNAL_SHELL=1 "$WINE" .local/bridge/taskbar-text-test.exe \
  'Z:\path\to\taskbar-text.log' --isolated-session
WIN4WINE_EXTERNAL_SHELL=1 "$WINE" .local/bridge/clock-pixels-test.exe --isolated-session
```

taskbar-textの出力は公開theme字体と実文字の位置・画素数を記録する診断資料で、
自動で実XPとの一致を判定しない。clock-pixelsは取得した現在文字列の白画素を
公開themeと明示MS UI Gothic -12の描画と比較する。測定中の分切替は再試験する。

所有threadの最大化とNOREDRAW表示は、最終Wineとbridgeを起動した専用sessionで検査する。
引数は現在の公開SPI_GETWORKAREAの幅・高さに合わせる（Luna / 1280×800 / 96 DPIの例）。

```sh
WIN4WINE_EXTERNAL_SHELL=1 "$WINE" .local/bridge/wine-maximize-test.exe 1280 766
WIN4WINE_EXTERNAL_SHELL=1 "$WINE" .local/bridge/shell-activation-test.exe
WIN4WINE_EXTERNAL_SHELL=1 "$WINE" .local/bridge/no-redraw-test.exe
```

Wine最大化試験は所有process内の通常位置も検査し、独自WM_GETMINMAXINFOの上限を確認する。
NOREDRAW試験は下地保持・明示owner描画・通常paint復帰・画面端・layered除外を検査する。
Wine10の即時GdiFlush画面反映stubは別件として扱う。bridgeのgeometry.tsv / frames.tsvは
0041対応と同時に形式を更新したため、Wine・WM・proxy・inspectをまとめて更新して再起動する。


## デスクトップの右クリックメニューの区切り線（段階 3）後の更新

registry profile を version 23 にした。既存の prefix はセッションの開始時に stage が profile の版の違いを見つけて
更新するので、`prefix/registry.py` を更新してセッションを起動し直せばよい（手で行う場合は
`./prefix/stage.py --wine "$WINE" "$WINEPREFIX"`）。prefix の再生成は不要。bridge（WM・proxy）は変更なし。

Wine は 0035（user32 の DrawText、アクセスキーの下線の長さ）が変わった。段階 4 で 0015 も変わったので、Wine の更新は
下の「パッチの再整理（段階 4）後の更新」の手順で行う。

比較用のプローブは `./tests/build.sh` で `.local/bridge/desktop-menu-test.exe` と `.local/bridge/text-underline-test.exe` になる。実XPでは参照用 VM のフロッピーから
`A:\DM.EXE COM1 --trace` のように実行し、出力は仮想シリアルへ出る。Wine では XP Explorer と同じく XP の shell 一式を
native で読み込ませる（読み込ませないと Wine の shell32 がメニューを作る）。

```sh
WINEDLLOVERRIDES='shell32,shdocvw,browseui,shlwapi,comctl32,linkinfo,browselc,shdoclc=n' \
  "$WINE" .local/bridge/desktop-menu-test.exe 'Z:\tmp\desktop-menu.txt' --trace
```


## パッチの再整理（段階 4）後の更新

段階 4 で 0015（`include/Makefile.in` に `wine/win4wine.h` を加える）と、段階 3 で 0035 が変わった。前の series を当てたソースツリーには
`wine/patch-state.py` で重ねられない（0015 と 0035 が「適用済み」と判定されない）。段階 2 と同じく、新しいソースとビルドディレクトリを
別名で作り、configure から全体をビルドする。ビルドディレクトリは使い回さない。

```sh
WINE_SOURCE=/work/.local/wine-upstream-stage4 WINE_BUILD=/work/.local/wine-build-stage4 \
  WINE_INSTALL=/work/.local/wine-install-stage4 ./wine/build.sh > .local/wine-build-stage4.log 2>&1
```

`WINE_SOURCE` が無ければ `wine/build.sh` が upstream から取得する。手元の未改変の Wine のリポジトリを `git clone` して
渡してもよい（作業ツリーの変更は複製されない）。新しい install に切り替えるときは、既存の prefix の wineserver を止めてから `lib/wine` と `bin` をまとめて入れ替える。
registry profile は version 23（段階 3）。bridge（WM・proxy）は変更なし。

## Wine 11 への移行（段階 5）後の更新

段階 5 で基にする Wine を 11.0 にした（`wine/upstream.env`）。パッチは 53 本に変わり、番号も振り直した（`docs/patches.md`）。
10.0 のソースツリーには重ねられないので、新しいソースとビルドディレクトリを別名で作り、configure から全体をビルドする。

```sh
WINE_SOURCE=/work/.local/wine-upstream-11 WINE_BUILD=/work/.local/wine-build-11 \
  WINE_INSTALL=/work/.local/wine-install-11 ./wine/build.sh > .local/wine-build-11.log 2>&1
```

`win4wine-controls.dll` は 11.0 では `comctl32_v6.dll` から作られる（`wine/build.sh`）。ビルドの依存は 10.0 と同じ
（chroot の `scripts/debian.sh init` の一覧のまま）。

既存の prefix は 11.0 の Wine で更新できる。

1. セッションを止め、wineserver が終わったことを確かめる。戻せるように prefix を写しておく（11.0 で更新した prefix は
   10.0 では使わない）。
2. install を丸ごと入れ替える（`lib/wine`、`bin` に加えて `share` も。11.0 の `wine.inf` で prefix が更新される）。
3. `./prefix/stage.py --wine "$WINE" "$WINEPREFIX"` を実行する。`wine.inf` が新しいことを見つけて `wineboot -u` を行い、
   その後で registry profile（version 23 のまま）を入れ直す。セッションの開始時にも同じ処理が走るが、段階 5 の入れ替えでは
   一度、stage より先に prefix が更新されて profile が入れ直されず、デスクトップの名前が英語（My Computer）になった
   （原因は未特定）。そうなったら `wine.inf` の時刻を新しくして（`touch`）stage をもう一度実行する。
4. セッションを起動する。

11.0 の `wineboot` は KnownDLLs を `\KnownDlls` に登録するようになった（prefix の更新で
`HKLM\System\CurrentControlSet\Control\Session Manager\KnownDLLs` が増える）。値は変えない。XP Explorer が `C:\xp` の
comctl32 を使えるのは 0016 による。新規 prefix は `prefix/create.py` で 10.0 と同じ手順で作れる（段階 5 で確かめた）。
bridge（WM・proxy）と registry profile は変更なし。

## 日本語入力（段階 6）

段階 6 でパッチは 55 本になり、0026 以降の番号が変わった（`docs/patches.md` の「段階 6」）。前の series を当てたソースツリーには
重ねられないので、新しいソースとビルドディレクトリで configure からビルドする。

```sh
git clone --branch wine-11.0 <未改変の Wine 11.0 のリポジトリ> .local/wine-stage6-src
./scripts/debian.sh run bash -c 'cd /work && WINE_SOURCE=/work/.local/wine-stage6-src \
  WINE_BUILD=/work/.local/wine-stage6-build WINE_INSTALL=/work/.local/wine-stage6-install ./wine/build.sh'
./scripts/debian.sh run bash -c 'cd /work && bridge-wm/build.sh && tests/build.sh'
```

実機（Debian 13）に入れるパッケージ:

```sh
sudo apt-get install --no-install-recommends fcitx5 fcitx5-mozc fcitx5-frontend-gtk3 fcitx5-frontend-gtk4 \
  fcitx5-config-qt locales python3-gi gir1.2-glib-2.0 gir1.2-gdkpixbuf-2.0 librsvg2-common
# 任意: Linux のブラウザ、代わりの日本語の字体
sudo apt-get install --no-install-recommends chromium fonts-ipafont-gothic
# Qt のアプリで日本語を入力するとき（入っている Qt の版の分。無いと Qt のアプリは何も言わずに直接入力だけになり、
# supervisor が fcitx5.log に「fcitx5-frontend-qt6 is not installed」などと書く）
sudo apt-get install --no-install-recommends fcitx5-frontend-qt5 fcitx5-frontend-qt6
```

`ja_JP.UTF-8` のロケールを作る（`/etc/locale.gen` の該当行を有効にして `locale-gen`）。session.json の `locale` に指定した
ロケールが無いと、Wine の XIM が C ロケールで開き、確定した文字が化ける。im-config は入れない（入っていても supervisor は
その設定に従う）。session.json に `"input_method": "fcitx5"` を加え、必要なら `linux_font` を変える（ログイン用 Xsession の節）。
試験 VM はネットワークが無いので、VM の中で `apt-get install --print-uris -qq --no-install-recommends パッケージ…` で
必要な .deb の一覧を出し、ホストで取得して VM の `/var/cache/apt/archives/` に写し（epoch のある版はファイル名の `:` を `%3a` に
する）、`apt-get install --no-download` で入れた。

既存の環境の更新:

1. セッションを止め、wineserver が終わったことを確かめる。
2. install を丸ごと入れ替える（`wine.inf` は段階 5 と同じ。prefix の更新は stage が行う）。試験 VM では
   `.local/vm-stage6-install.sh`（ext4 のイメージを QMP で差し込み、段階 5 の install を `wine-final-install.stage5` として残す）。
3. `./prefix/stage.py --wine "$WINE" "$WINEPREFIX"` を実行する。registry profile は version 23 のまま。
4. bridge（`bridge-wm/tray.py` を新しく使う、`proxy.exe` を作り直す）と `session/supervisor.py` を入れ替え、セッションを起動する。

Ctrl+Space（または全角/半角）で直接入力と Mozc を切り替える。通知領域のアイコンの左クリックでも切り替わり、右クリックで
fcitx5 のメニューが出る。fcitx5 の設定は `~/.config/fcitx5/`。

試験:

```sh
./scripts/debian.sh run bash -c 'cd /work && python3 tests/tray_host.py'   # 専用のセッションバスで tray.py を試す
# XP シェルのセッションで（実XPでは A:\NI.EXE COM1）
WIN4WINE_EXTERNAL_SHELL=1 "$WINE" .local/bridge/notify-icon-test.exe
```

試験用の prefix を別の install で使うと、prefix の更新が走り、その途中の「Wine Mono Installer」の窓が bridge の試験の X 画面に出て
フォーカスを奪う（段階 6 で `wm_integration.py` などが失敗した）。install を替えたら先に
`WINEDLLOVERRIDES='mscoree,mshtml=' wine wineboot -u` で更新しておく。

## 64bit（WoW64）対応（段階 7）

段階 7 で Wine を新しい WoW64 方式（`--enable-archs=i386,x86_64`）でビルドするようにした。Unix 側は 64-bit だけなので、
実機に i386 の multiarch（32-bit のライブラリ）は要らない。32-bit の XP シェルと 32-bit のアプリは WoW64 で動き、64-bit の
Windows アプリも使える。パッチは 56 本になり、0017 以降の番号が変わった（`docs/patches.md` の「段階 7」）。前の series を
当てたソースツリーには重ねられないので、新しいソースとビルドディレクトリで configure からビルドする。

```sh
# chroot の依存（作り直す場合は scripts/debian.sh init。既存の chroot には足す）
sudo chroot .local/debian apt-get install --no-install-recommends libx11-dev libxext-dev libxrender-dev \
  libxrandr-dev libxi-dev libxcursor-dev libxinerama-dev libfreetype-dev libfontconfig-dev libgnutls28-dev \
  libgl-dev libasound2-dev libdbus-1-dev libudev-dev gcc-mingw-w64-x86-64
git clone --branch wine-11.0 <未改変の Wine 11.0 のリポジトリ> .local/wine-stage7-src
./scripts/debian.sh run bash -c 'cd /work && WINE_SOURCE=/work/.local/wine-stage7-src \
  WINE_BUILD=/work/.local/wine-stage7-build WINE_INSTALL=/work/.local/wine-stage7-install ./wine/build.sh'
./scripts/debian.sh run bash -c 'cd /work && bridge-wm/build.sh && tests/build.sh'
```

install には `lib/wine/x86_64-unix`、`x86_64-windows`、`i386-windows` ができ、`i386-unix` は無い。`win4wine-controls.dll` は
両方の PE の `comctl32_v6.dll` から作る。bridge（proxy・inspect・補助ツール）とプローブは 32-bit のまま。プローブを 64-bit でも
作るときは `MINGW_CC=x86_64-w64-mingw32-gcc MINGW_WINDRES=x86_64-w64-mingw32-windres WIN4WINE_PROBES=/work/.local/bridge64 tests/build.sh`。

prefix の構成（`prefix/create.py` が作る 64-bit の prefix）:

- XP の DLL（shell32、shdocvw、browseui、shlwapi、comctl32 の v5 など）は `C:\windows\syswow64` に置く。32-bit の XP シェルからは
  `system32` に見える。`system32` は 64-bit の Wine の DLL のまま（64-bit のアプリは Wine の shell32 などを使う）。
- `win4wine-controls.dll` は `system32`（x86_64）と `syswow64`（i386）の両方。
- registry profile の COM の登録、名前空間の名前、`LogonType` などは 32-bit の view（`HKCR\CLSID` と `HKLM\Software` の
  `Wow6432Node`）に書く（`prefix/registry.py` の `shell_view` が `wine reg` に `/reg:32` を付ける）。HKCU は両方の view で共通。
  registry profile の版は 23 のまま。
- `C:\xp`、`winsxs` の XP の v6、字体、テーマの置き場所は変わらない。

既存の 32-bit prefix はこのビルドでは動かない（`is a 32-bit installation, it cannot support 64-bit applications`。
`WINEARCH=win32` も WoW64 では使えない）。新しい 64-bit の prefix へ移す。

1. セッションを止め、古い prefix の wineserver が終わったことを確かめる。
2. 移す。古い prefix は読むだけで変えない（32-bit のビルドに戻せば、そのまま使える）。

   ```sh
   ./prefix/migrate.py /path/to/old-prefix /path/to/new-prefix --wine /path/to/wine-stage7-install/bin/wine
   ```

   `migrate.py` は、古い prefix の `xp-shell.json` の抽出物（`--assets` で変えられる）と H: のホームで `create.py` を実行し、
   古い prefix の HKCU（`user.reg`。テーマ、DPI、スタートメニュー、タスクバー、フォルダの表示などの設定）と、`drive_c/users`
   （デスクトップ、スタートメニュー、お気に入り、AppData、壁紙の変換結果）、`drive_c/ProgramData`（共通のスタートメニュー、
   ユーザー画像）を写す。`windows/Fonts` と `C:\xp\native` に手で足したファイルも写す（新しい側にあるものは上書きしない）。
   ホームへのリンク（Downloads など）はリンクのまま写し、リンクの先には書き込まない。最後に `stage.py` と同じ配置を行う。
   古い prefix に入れた Windows のアプリ（`Program Files` と HKLM の登録）は移さない。新しい prefix に入れ直す。
3. `~/.config/win4wine/session.json` の `wine` を WoW64 の install、`prefix` を新しい prefix にする。
4. セッションを起動する。

`WINEDLLOVERRIDES` で XP の DLL を `=n` にして 32-bit のプログラムを起動すると、WoW64 では Wine が 64-bit の `start.exe` を
経由して起動し直し、その `start.exe` が 64-bit の native の shell32 を探して失敗する。XP の DLL を指定するときは `=n,b` にする
（32-bit のプロセスは XP の DLL、64-bit の `start.exe` は Wine の DLL を読む）。

```sh
WINEDLLOVERRIDES='shell32,shdocvw,browseui,shlwapi,comctl32,linkinfo,browselc,shdoclc=n,b' \
  "$WINE" .local/bridge/desktop-menu-test.exe 'Z:\tmp\desktop-menu.txt' --trace
```

試験:

```sh
# XP シェルのセッションで。64-bit のアプリからの通知アイコン（0027）
WIN4WINE_EXTERNAL_SHELL=1 "$WINE" .local/bridge64/notify-icon-test.exe
# OpenGL の転送の速さ（引数は1項目の秒数）
"$WINE" .local/bridge/gl-perf-test.exe 2
```

試験 VM では `.local/vm-stage7-install.sh` が install と更新したファイルを ext4 のイメージで写す（段階 6 の install は
`wine-final-install`、移す前の prefix は `prefix-final` のまま残る）。

## Linux アプリのスタートメニュー（2026-10-11）

セッションは XP Explorer を起動する前に、Linux のアプリ（`/usr/share/applications` などの desktop entry）を XP のスタートメニューの
「すべてのプログラム」に入れ、その後もアプリの導入・削除に追従する（`session/linux_apps.py`、仕組みは `docs/architecture.md`）。
名前は「Mousepad (Linux)」のように ` (Linux)` が付き、「アクセサリ」「インターネット」などの分類のフォルダに入る。

既存の環境の更新:

1. Wine を作り直す（0022 が変わった。wineserver と win32u が変わるので、新しいソースとビルドディレクトリで configure から
   `wine/build.sh` を実行し、install をまとめて入れ替える。prefix はそのまま使える）。
2. `./scripts/debian.sh run bash -c 'cd /work && bridge-wm/build.sh'` で `.local/bridge/linux-launch.exe` と `linux-links.exe` を作る。
3. セッションを起動し直す。最初の起動でショートカットとアイコンができる（`C:\xp\linux-apps`、記録は `state.json`、ログは
   `~/.local/state/win4wine/linux-apps.log`）。

`session.json` の `"linux_menu": false` で無効になる（作ったショートカットは残る）。片付けるときはセッションを止めてから:

```sh
python3 session/linux_apps.py /path/to/prefix --wine /path/to/wine --remove
```

作ったショートカットと、作るために足したフォルダ（空になったもの）だけを消す。利用者のショートカットや Windows のアプリのものは残る。

セッションの `PATH` の先頭にはセッションの Wine の `bin` が入る。LXTerminal などの端末で `wine setup.exe` と打つと、XP シェルと
同じ Wine・prefix・環境（`WIN4WINE_EXTERNAL_SHELL`、入力メソッド）で動く。Windows のアプリのインストーラーが作るショートカットは
XP のスタートメニューに直接入る（Wine が `~/.local/share/applications/wine` に作る Linux 側の項目は読まない）。

試験:

```sh
python3 tests/linux_apps.py                                   # Wine なし（GdkPixbuf があればアイコンも）
./scripts/debian.sh run bash -c 'cd /work && python3 tests/linux_apps.py --wine .local/wine-stage8-install/bin/wine'
```

`tests/foreign-icons.c` は `--owner EXE` で owner を別の架構のビルドにできる。WoW64 の 4 通り（32/64-bit の読む側と owner）を試す:

```sh
WIN4WINE_EXTERNAL_SHELL=1 "$WINE" .local/bridge/foreign-icons-test.exe --owner 'Z:\path\to\bridge64\foreign-icons-test.exe'
```

## Windows キー、デスクトップのフォーカス、日本語のコンソール（2026-10-11）

パッチは 58 本になり、0023 以降の番号が変わった（`docs/patches.md` の「Windows キーと日本語のコンソール」）。前の series を
当てたソースツリーには重ねられないので、新しいソースとビルドディレクトリで configure からビルドする（上の節と同じ）。
`bridge-wm/build.sh` で proxy も作り直し、セッションを起動し直す（`bridge-wm/wm.py` と `session/supervisor.py` も変わった）。

- Windows キー単独と Ctrl+Esc でスタートメニューが開き、Win+R、Win+E などの XP のホットキーは Linux のアプリにフォーカスが
  あっても効く（そのキーは Linux のアプリには届かない）。
- デスクトップとタスクバーをクリックすると、キーボードがそこへ移る。
- `input_method` が `fcitx5` のとき、supervisor は `~/.config/fcitx5/config` の `[Behavior]` に
  `ShowInputMethodInformation=False` と `showInputMethodInformationWhenFocusIn=False` を足す（無いときだけ。値を決めて
  あればそのまま）。入力モードは通知領域のアイコンで見る。
- 日本語のロケールの cmd（Wine の conhost）は半角の升目になり、Mozc で日本語を入力できる。

試験:

```sh
./scripts/debian.sh run bash -c 'cd /work && bridge-wm/build.sh'
# wm_integration は Windows キー、Win+R、Ctrl+Esc の proxy への受け渡しと、デスクトップのクリックのフォーカスも検査する
```

## cmd の字体とセッションのログ（2026-10-11）

registry profile は 24 になった。次にセッションを起動すると `stage.py` が適用し、`HKCU\Console` を MS Gothic の 8×18 にする
（一度だけ。その後に変えた字体は残る）。0041 が変わったので Wine を作り直す（新しいソースとビルドディレクトリで）。
セッションのログ（`~/.local/state/win4wine/*.log`）は起動のたびに前回の分が `*.log.1` になる。

## Linux のトレイアイコン（XEmbed）と Qt の日本語入力（2026-10-11、段階 8）

パッチは 59 本になり、0041 以降の番号が変わった（`docs/patches.md` の「段階 8」）。新しいソースとビルドディレクトリで configure
から Wine をビルドし、install を入れ替える。`bridge-wm/tray.py`、`bridge-wm/wm.py`、`session/supervisor.py`、`prefix/stage.py`
を入れ替え、セッションを起動し直す。registry profile は 24 のまま。`.xp-registry-version` は `版 更新の印` の形になり、古い形の
prefix では次の起動で stage.py が profile を一度当て直す。tray.py は python3-xlib（WM と同じ）も使い、X サーバーの Composite、DAMAGE、SHAPE、XTEST の拡張が要る
（Debian の Xorg と Xvfb は既定で持つ。無ければ XEmbed だけを扱わず、tray.log に書く）。

- ほかに X のトレイが無いとき、XEmbed のトレイだけを使うアプリ（GTK の `GtkStatusIcon` を使う yad、古いトレイアプリなど）の
  アイコンも XP の通知領域に出る。左・中・右クリックはアプリに届き、メニューはアプリ自身のものが出る。
- Qt のアプリの日本語入力には、上のパッケージの一覧の `fcitx5-frontend-qt5` / `fcitx5-frontend-qt6` を入れる。session は
  `input_method` が `fcitx5` のとき `QT_IM_MODULE=fcitx` を設定する（段階 6 から）。

試験:

```sh
./scripts/debian.sh run bash -c 'cd /work && python3 tests/tray_host.py'   # SNI に加え、Xvfb で XEmbed のアイコンも試す
```

## ログイン画面（2026-10-11、段階 9）

LightDM の greeter を XP のようこそ画面にする（`greeter/xp-greeter.py`）。ログインは LightDM と PAM が行う。Wine とパッチ、
registry profile は変わらない。

1. 抽出し直す。`extract/extract.py` は logonui.exe・winlogon.exe・arial.ttf と XP の既定のユーザー画像 23 枚も取り出す
   （既存の出力先には書かないので、新しい出力先に抽出する。prefix はそのまま使える）。
2. パッケージ（Debian 13）: `lightdm`、`gir1.2-lightdm-1`、`gir1.2-gtk-3.0`、`python3-gi`、`python3-gi-cairo`、`python3-xlib`。
   使えないときに切り替える `lightdm-gtk-greeter` も入れておく。
3. greeter を入れる（コードと必要な XP のファイルを `/usr/local/share/win4wine/greeter` に写し、
   `/etc/lightdm/lightdm.conf.d/95-xp-greeter.conf` で LightDM の greeter にする）。LightDM を再起動する。

```sh
./greeter/install.sh /absolute/path/to/extracted-assets
sudo systemctl restart lightdm
```

- 元の greeter に戻す: `./greeter/install.sh --remove` の後に LightDM を再起動する（写したファイルは
  `/usr/local/share/win4wine/greeter`、`/usr/share/xgreeters/xp-greeter.desktop`、`/etc/win4wine/greeter.json`。消してもよい）。
- `/etc/lightdm/lightdm.conf` 自体に `greeter-session` を書いているとそちらが優先される（conf.d より後に読まれる）。
- 設定は `/etc/win4wine/greeter.json`: `assets`（写したファイルの場所）、`fallback`（使えないときの greeter、既定
  `lightdm-gtk-greeter`）、`picture`（自分の画像が無い利用者の画像、既定 `beach.bmp`）。利用者の画像は AccountsService の
  ものか `~/.face`。
- 必要なファイルや GTK・LightDM のバインディングが無いと、greeter は `fallback` に切り替える（理由は
  `/var/log/lightdm/seat0-greeter.log`）。リポジトリを更新したら `greeter/install.sh` を再び実行する。
- ログイン後とログオフ中の XP の画面は、XP シェルのセッション（`session/supervisor.py`）が同じ設定を読んで出す。
  `session.json` に `"welcome_screen": false` と書くと出さない。設定が無ければ出さずに進む（`welcome.log` に理由）。

試験:

```sh
./scripts/debian.sh run python3 /work/tests/greeter_layout.py /work/.local/ASSETS   # XP で測った配置と比べる
```

試験 VM での手順は `docs/vm.md` の「ログイン画面」。
