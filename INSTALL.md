# 導入手順

**日本語** · [English](.github/readme/INSTALL.en.md)

> [!WARNING]
> - Windows XP の ISO はご自身で入手してください。利用は自己責任でお願いします（[README の注意](README.md#注意)）。
> - 実験的なプロジェクトです。確かめているのは、**新しくインストールした Debian 13** に `scripts/setup.sh` で入れた場合だけです。
>   すでに使っている環境への導入は試していません。導入スクリプトはパッケージを入れ、LightDM の設定を変えることがあります
>   （下の「2. スクリプトがすること」）。仮想マシンや試験用のマシンで試すことをおすすめします。

## 必要なもの

- **Debian 13（trixie）amd64** のマシン（実機または仮想マシン）。デスクトップ環境は要りません。
- **日本語版 Windows XP Professional SP3（x86）の ISO**（または XP の Windows ディレクトリ）。
- ネットワーク（パッケージと Wine のソースの取得に使います）と、**40 GB ほどの空き**（Wine のソースとビルド）。
- sudo を使える利用者。

## 1. まとめて入れる

XP のシェルを使う利用者で（root ではなく）実行してください。パッケージとシステムのファイルには sudo を使います。

```sh
git clone https://github.com/mikuta0407/win4wine.git ~/win4wine
cd ~/win4wine
scripts/setup.sh --media /path/to/WindowsXPSP3.iso --greeter
```

| オプション | 意味 |
|---|---|
| `--media PATH` | XP の ISO または Windows ディレクトリ（必須） |
| `--greeter` | ログイン画面を XP のようこそ画面にします（LightDM をディスプレイマネージャにします） |
| `--no-greeter` | 今のログイン画面のままにします（既定）。XP のカーソルだけ入れます |
| `--theme luna\|classic` | 見た目です。`luna`（既定、XP 形式のスタートメニュー）か `classic`（クラシック形式） |
| `--dpi N` | 画面の DPI（XP の「DPI 設定」）です。96（通常）、120（大きいサイズ）、または 19〜480 の任意の値。XP のシェル、セッションの Linux のアプリ、ようこそ画面に効きます |
| `--wayland` | Wayland のセッション「XP Shell (Wine, Wayland)」も入れます（labwc と wlroots をビルドします）。モニターごとの倍率、125%・150% などの倍率が使え、Wayland のアプリにも XP の枠が付きます。`--greeter` と一緒なら、ようこそ画面の既定のセッションになります。**X11 のセッション以上に実験的です**（確かめた範囲が狭く、既知の制限があります。[docs/wayland.md](docs/wayland.md)） |
| `--wine-install DIR` | 別の Debian 13 マシンでビルドした Wine（その `.local/wine-install` を写したもの）を使い、ここではビルドしません |
| `--jobs N` | Wine のビルドの並列数です（既定は CPU の数） |

途中で失敗しても、そのまま実行し直せば、済んだ段階は飛ばします。ビルドしたものはすべて、このチェックアウトの `.local/` に入ります。

## 2. スクリプトがすること

1. パッケージを入れます（Wine のビルドの依存、Xorg、python3-xlib、fcitx5 + Mozc など）。ディスプレイマネージャが無ければ LightDM も入れます。
2. `ja_JP.UTF-8` のロケールを作ります。
3. Wine 11.0 を取得し、パッチを当ててビルドします（`.local/wine-install`。4 コアで 1 時間半ほど。ログは `.local/wine-build.log`）。
4. bridge（ウィンドウマネージャーと代理ウィンドウ）をビルドします。
5. ISO から必要なファイルだけを取り出します（`.local/assets`）。
6. XP 用の Wine の prefix を作ります（`.local/prefix`）。
7. `~/.config/win4wine/session.json` を書き、セッション「XP Shell (Wine, X11)」を登録します。
8. `--greeter` のときは XP のようこそ画面を入れます。
9. `--wayland` のときは labwc と wlroots にパッチを当ててビルドし（`.local/labwc-install`。ログは `.local/labwc-build.log`）、
   セッション「XP Shell (Wine, Wayland)」も登録します。

## 3. ログインする

ディスプレイマネージャを再起動するか、マシンを再起動してください。

- `--greeter` のとき: XP のようこそ画面で利用者を選び、パスワードを入れます。
- そうでないとき: ログイン画面でセッション「**XP Shell (Wine, X11)**」（`--wayland` のときは「**XP Shell (Wine, Wayland)**」も選べます）を選びます。

## 使い方

| やりたいこと | 方法 |
|---|---|
| 日本語入力の切り替え | Ctrl+Space または 全角/半角。通知領域のアイコンのクリックでも切り替えられます |
| Linux のアプリを起動 | スタート → すべてのプログラム（名前に「(Linux)」が付いています） |
| Windows のアプリを入れる | 端末で `wine setup.exe`。セッションの `PATH` の先頭にはこの Wine が入っています |
| スタートメニュー | Windows キー、Ctrl+Esc |
| ファイル名を指定して実行 / エクスプローラ | Win+R / Win+E |
| ログオフ・終了 | スタート → ログオフ / 終了オプション |

## 設定（`~/.config/win4wine/session.json`）

| キー | 既定 | 意味 |
|---|---|---|
| `wine`, `prefix` | （setup.sh が書きます） | 使う Wine と prefix の絶対パス |
| `locale` | `ja_JP.UTF-8` | セッションのロケール |
| `input_method` | `fcitx5` | 入力メソッド。省くと起動しません |
| `linux_font` | `MS UI Gothic` | Linux のアプリの字体。`null` で変えません |
| `linux_menu` | `true` | Linux のアプリをスタートメニューに入れます |
| `welcome_screen` | `true` | ログイン後とログオフ中に XP の画面を出します |
| `cursor_theme` | `win4wine-xp` | カーソルのテーマ。`null` で使いません |
| `shutdown_command`, `reboot_command` | `systemctl poweroff` / `reboot` | 終了オプションで実行するコマンド |
| `outputs` | （無し） | Wayland のセッションの、出力ごとの設定。例: `{"HDMI-A-1": {"scale": 1.5}, "DP-1": {"scale": 1, "position": "2560,0"}}`（`mode` も書けます。出力の名前は `wlr-randr` で見られます） |
| `primary` | 一番左の出力 | Wayland のセッションで Win32 の主モニターにする出力の名前。XP のデスクトップとタスクバーはここに出ます |

見た目を後から変えるときは、セッションを止めてから `scripts/setup.sh --media ISO --theme classic`（または `luna`）を実行してください。
DPI（文字や画面の部品の大きさ）は `scripts/setup.sh --media ISO --dpi 120` で変え、ログインし直してください。XP と同じく、文字と寸法が
大きくなり、アイコンは 32×32 のままです。Linux のアプリも同じ DPI で文字が大きくなります（GTK は 192 以上で部品も 2 倍になります）。

Wayland のセッションでは、Linux のアプリは出力ごとの倍率（`outputs` の `scale`）で描かれ、XP のシェルは DPI で描かれて、
どの出力でもぼけません。DPI は主に使う出力の倍率に合わせてください（倍率 1.5 なら 144）。詳しくは [docs/wayland.md](docs/wayland.md) にあります。

## うまく動かないとき

- セッションのログは `~/.local/state/win4wine/` にあります（前回の分は `*.log.1`）。
- XP のシェルが続けて落ちると、Wine 標準の explorer と xterm に切り替わります。
- ようこそ画面が出ないときは `/var/log/lightdm/seat0-greeter.log` を見てください。必要なものが足りないと、通常の LightDM の画面に切り替わります。

## 元に戻す

```sh
greeter/install.sh --remove                     # ようこそ画面をやめる（LightDM の元の画面に戻ります）
sudo rm /usr/share/xsessions/xp-shell.desktop /usr/local/bin/xp-shell-session   # セッションの登録を消す
sudo rm /usr/share/wayland-sessions/xp-shell-wayland.desktop /usr/local/bin/xp-shell-wayland-session   # Wayland のセッションも
python3 session/linux_apps.py .local/prefix --wine .local/wine-install/bin/wine --remove   # 作ったスタートメニューの項目を消す
```

ビルドしたもの、取り出したファイル、prefix は、このチェックアウトの `.local/` を消せば消えます。
スクリプトが入れたパッケージ（LightDM を含みます）と、ディスプレイマネージャの切り替えは元に戻しません。
