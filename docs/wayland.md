# Wayland のセッション

ログイン画面で選べる 2 つ目のセッション「XP Shell (Wine, Wayland)」の説明です。X11 のウィンドウマネージャー（bridge-wm）の
代わりに、パッチを当てた labwc（wlroots の compositor）が同じ役を持ちます。X11 のセッションはそのまま残り、動きも変わりません。

**X11 のセッション以上に実験的な機能です。** 作ったばかりで、確かめた範囲は X11 のセッションより狭く、下の「X11 のセッションとの違い・残っていること」の制限があります。どの compositor を選んだか、その比較と最初の試作の結果は [history/wayland-study.md](history/wayland-study.md) にあります。

## 何ができるようになるか

- **モニターごとの倍率**: 100% のモニターと 150% のモニターを同時に使えます。X11 では DPI が画面全体で 1 つでした。
- **分数の倍率での Linux のアプリ**: GTK、Qt、Chromium、Firefox などが 125% や 150% をそのまま描けます。
- **XP のシェルはぼけません**: XP のシェルと Wine のアプリは、Xwayland の X11 のクライアントのまま、DPI（LogPixels）で
  描いたものを出力の 1 画素ずつに表示します。compositor が拡大することはありません。

XP にはモニターごとの DPI が無いので、XP のシェルの DPI は 1 つです。主に使う出力の倍率に合わせてください（倍率 1.5 なら 144）。

## 使い方

- `scripts/setup.sh --media ISO --wayland` で、labwc と wlroots をビルドし（`wayland/build.sh`、`.local/labwc-install`）、
  セッションを登録します（`session/install.sh --wayland`）。`--greeter` と一緒なら、XP のようこそ画面の既定のセッションも
  Wayland になります。
- 出力の倍率と位置は `session.json` の `"outputs"`、Win32 の主モニターは `"primary"`（出力の名前）で決めます。
  例: `"outputs": {"HDMI-A-1": {"scale": 1.5}, "DP-1": {"scale": 1, "position": "2560,0"}}, "primary": "HDMI-A-1"`。
  `mode`（`wlr-randr --mode` の値）も書けます。出力の名前は `wlr-randr` で見られます。

## 部品

| 場所 | 中身 |
|---|---|
| `wayland/upstream.env`, `wayland/build.sh` | labwc 0.8.3 に wlroots 0.18.2 と libsfdo を subproject として静的に組み込んでビルドします（システムの wlroots は使いません） |
| `wayland/patches/wlroots/0001` | scene の surface を 1/倍率 の大きさで、また一部だけを切り取って表示します。その倍率の出力では 1 画素ずつ描きます |
| `wayland/patches/wlroots/0002` | XWM が `_NET_WM_STATE_ABOVE`（Win32 の最前面の窓）を落とさずに保ち、変化を知らせます |
| `wayland/patches/labwc/0001` | `xwayland-scale.c`（Xwayland を出力の画素で動かす）と `win4wine.c`（bridge-wm の役） |
| `wayland/labwc/` | labwc の設定（Alt+F4 だけのキー、空のメニュー、XP の Luna に近い色の枠） |
| `wayland/session.sh` | IPC の場所（`WIN4WINE_IPC`）を決めて labwc を起動し、その中で supervisor を動かします。supervisor が終わるとセッションも終わります |
| `session/supervisor.py` | labwc の中では bridge-wm を起動せず、`wlr-randr` で出力を設定し、主モニターの名前を IPC の `primary` に書きます |
| Wine 0027, 0071 | Xwayland ではルートの窓から画面を読めないので、XP の影は透明度つきの黒い窓で描き（0027）、ログオフの灰色の画面は各窓の中身から作ります（0071） |

## Xwayland を出力の画素で動かす（`xwayland-scale.c`）

- Xwayland には自前の xdg-output を見せ、各出力を画素の大きさで横に並べます（「X の空間」）。主モニターを先頭（X の 0,0）に
  置き、Xwayland の RandR の primary にもします。Win32 の 0,0 が主モニターの左上になり、XP のデスクトップのアイコンと壁紙は
  主モニターに出ます。
- X の窓は、載っている出力の倍率で 1/倍率 に縮めて置きます（X の 1 画素が出力の 1 画素）。倍率の違う出力にまたがる窓は、
  出力ごとに切り取った部分を並べます。
- 窓の位置と大きさの要求、size hints、作業領域（`_NET_WORKAREA` と出力ごとの `_GTK_WORKAREAS_D0`）、押したまま窓の外に
  出たポインターの位置を、X の空間とレイアウトの座標の間で変換します。

## bridge-wm の役（`win4wine.c`）

`WIN4WINE_IPC` があるときだけ動きます。

- proxy.exe との IPC は bridge-wm と同じです（`windows.tsv`、`geometry.tsv`、`frames.tsv`、`*.cmd`、`switch.tsv`、
  `shellkeys.tsv`、`icon-*.argb`。[architecture.md](architecture.md) の「IPC」）。IPC のファイルは inotify で読み、窓の変化は
  labwc の中から受けて、まとめて次の idle で書きます。
- Linux の窓（Wayland の窓と、Wine 以外の X11 の窓）に XP の枠を付けます。枠は窓のすぐ下に重ね、移動・大きさの変更・最大化・
  最小化・閉じる・ドラッグを窓に結び付けます。最大化の大きさは、枠の分を除いた作業領域です。アイコンは、Wayland の窓は
  アプリの desktop エントリーから、X11 の窓は `_NET_WM_ICON` から取ります。
- Wine の窓の種類（`_WIN4WINE_CLASS`、`_WIN4WINE_HWND`）は labwc 自身の X の接続で読みます。XP のデスクトップは一番下、
  タスクバー・スタートメニュー・最前面の窓（とその所有する窓）は上に置き、X の重なり順も合わせます（Xwayland はポインターの
  下の窓を X の重なり順で決めるためです）。
- Alt+Tab は XP の切り替え窓に、Windows キーと Ctrl+Esc は Linux の窓の上でも XP のシェルに送ります。タスクバーの分は
  作業領域から除きます。
- 活性化は Wine に合わせます。Wine の窓が消えたときに次に活性化する窓は Wine が選びます。Wine が活性化しないと印を付けた
  窓（`_NET_WM_USER_TIME` 0）と、最前面の全画面の窓（ログオフの灰色の画面）は、表示しても活性化しません。
  Wine の窓がキーボードを持つときは、X のフォーカスも直接渡します（Wine は前面でない窓への WM_TAKE_FOCUS を無視します）。
  wlroots が自分の X の接続で X のフォーカスを外したのが後に届いて、X のフォーカスがどの窓にも無くなったときは、渡し直します。
  Wine の窓がキーボードを持つ間に、同じ Wine のプロセスが管理外の窓（override-redirect）を出しても、キーボードは移しません。
  その中の X のフォーカスは Wine が自分で動かします（ReactOS のスタートメニューは、所有者のタスクバーのキーで動きます）。
  Linux の窓が活性のときは、その枠を `_NET_ACTIVE_WINDOW` に書いて XP の枠を活性の色にします。X のフォーカスは移しません
  （移すと fcitx5 が XIM で枠を入力先と取り違えます）。
- Xwayland には relative pointer を見せません。Xwayland はボタンをその装置から送り、その位置は絶対座標の移動（タブレット、
  仮想マシンのポインター）では動かないので、最初のクリックが別の場所に届いてしまいます。
- 窓が大きくなるときは、X の窓に frame callback を送ります。最大化で Linux の窓の下に隠れた枠には callback が届かず、
  Xwayland が新しい大きさを描かないままになるためです。

## 確かめたこと

入れ子の labwc（chroot、出力 2 つ、倍率 1 と 1.5、144 DPI）と、試験用の仮想マシンの実際の出力（DRM、1280×800、
倍率 1 の 96 DPI と倍率 1.5 の 144 DPI）で確かめています。

- XP のシェル: デスクトップ、タスクバー、スタートメニュー、メニューの影、ファイル名を指定して実行、マイ コンピュータ、
  ログオフのダイアログと灰色の画面。Wine の窓の最大化（作業領域に合います）、元に戻す、ドラッグ。倍率 1.5 でもぼけません。
- Linux の窓: foot（Wayland）、mousepad（GTK3、Wayland）、xterm（X11）に XP の枠とタスクバーのボタン、アイコンが付きます。
  枠のドラッグ（最初の 1 回から）、大きさの変更、最大化（枠ごと作業領域に収まります）、元に戻す、最小化、タスクバーからの
  復元、閉じる。枠の活性の色が窓の活性に従います。
- キー: Alt+Tab（XP の切り替え窓）、Win+R、Win+E、Ctrl+Esc を Linux の窓の上で。
- 日本語入力: foot（text-input-v3、fcitx5 + Mozc）と、Wine の「ファイル名を指定して実行」（XIM）で変換して確定できます。
- 出力 2 つ: XP のデスクトップは主モニターに出て、Wine の窓は両方の出力に出せます。主モニターは `"primary"` で選べます。
- セッションの終わり: XP のログオフで labwc が終わり、ログイン画面に戻ります。
- X11 のセッション: 同じ仮想マシンで X11 のセッションに戻し、今までどおり動くことを確かめています。
- 自動の試験: `tests/wayland_compositor.py`（Wine を使わず、Wine の窓の印を付けた X の窓で compositor の規則を確かめます）。
  XP のシェルと proxy を使う確かめ（X11 の `tests/native_frames.py` などに当たるもの）は、まだ手で行っています。

## X11 のセッションとの違い・残っていること

- ログオフ・終了の灰色の画面: Wayland のアプリの窓は X の窓ではないので写りません（その下にある Wine の窓やデスクトップが
  写ります）。
- 主モニター以外の出力: XP のデスクトップの窓は広がりますが、壁紙は主モニターにだけ描かれ、ほかは背景色になります。
- X の空間では主モニターが先頭なので、主モニターの左にある出力へ Wine の窓をドラッグする途中は、隣の出力にはみ出した部分が
  元の出力の倍率で（小さく）描かれます。移し終えれば、その出力の倍率で描かれます。
- 倍率が整数でない出力では、X の窓どうしの位置が出力の 1 画素ずれることがあります（レイアウトの座標が整数のためです）。
  窓の中身は引き伸ばさずに 1 画素ずつ描きます。
- Linux のアプリのダイアログ（親のある窓）には XP の枠が付かず、labwc の枠（XP の Luna に近い色。
  `wayland/labwc/themerc-override`）になります（X11 のセッションでは枠が付きません）。
- GTK4 と libadwaita のアプリはクライアント側の装飾を描くので、XP の枠と二重になるはずです（まだ確かめていません）。
- labwc のパッチは 1 つの大きなパッチ（`wayland/patches/labwc/0001`）です。Wine のパッチのように目的ごとには分けていません。
