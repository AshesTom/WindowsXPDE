# 導入手順

🇯🇵 日本語 · [🇬🇧 English](.github/readme/INSTALL.en.md)

> [!WARNING]
> Windows XP の ISO は自分で入手すること。利用は自己責任で（[README の注意](README.md#注意)）。

## 必要なもの

- **Debian 13（trixie）amd64** を新しく入れたマシン（実機または VM）。デスクトップ環境は要らない。
- **日本語版 Windows XP Professional SP3（x86）の ISO**（または XP の Windows ディレクトリ）。
- ネットワーク（パッケージと Wine のソースの取得）と、**40 GB ほどの空き**（Wine のソースとビルド）。
- sudo を使える利用者。

## 1. まとめて入れる

XP シェルを使う利用者で（root ではなく）実行する。パッケージとシステムのファイルには sudo を使う。

```sh
git clone https://github.com/mikuta0407/win4wine.git ~/win4wine
cd ~/win4wine
scripts/setup.sh --media /path/to/WindowsXPSP3.iso --greeter
```

| オプション | 意味 |
|---|---|
| `--media PATH` | XP の ISO または Windows ディレクトリ（必須） |
| `--greeter` | ログイン画面を XP のようこそ画面にする（LightDM をディスプレイマネージャにする） |
| `--no-greeter` | 今のログイン画面のまま（既定）。XP のカーソルだけ入れる |
| `--theme luna\|classic` | 見た目。`luna`（既定、XP 形式のスタートメニュー）か `classic`（クラシック形式） |
| `--wine-install DIR` | 別の Debian 13 マシンでビルドした Wine（その `.local/wine-install` を写したもの）を使い、ここではビルドしない |
| `--jobs N` | Wine のビルドの並列数（既定は CPU の数） |

途中で失敗しても、そのまま再実行すれば済んだ段階は飛ばす。ビルドしたものはすべて、このチェックアウトの `.local/` に入る。

## 2. スクリプトがすること

1. パッケージを入れる（Wine のビルドの依存、Xorg、python3-xlib、fcitx5 + Mozc など）。ディスプレイマネージャが無ければ LightDM。
2. `ja_JP.UTF-8` のロケールを作る。
3. Wine 11.0 を取得し、パッチを当ててビルドする（`.local/wine-install`。4 コアで1時間半ほど。ログは `.local/wine-build.log`）。
4. bridge（WM と代理ウィンドウ）をビルドする。
5. ISO から必要なファイルだけを取り出す（`.local/assets`）。
6. XP の prefix を作る（`.local/prefix`）。
7. `~/.config/win4wine/session.json` を書き、セッション「XP Shell (Wine, X11)」を登録する。
8. `--greeter` なら XP のようこそ画面を入れる。

## 3. ログインする

ディスプレイマネージャを再起動する（または再起動する）。

- `--greeter` のとき: XP のようこそ画面で利用者を選び、パスワードを入れる。
- そうでないとき: ログイン画面でセッション「**XP Shell (Wine, X11)**」を選ぶ。

## 使い方

| やりたいこと | 方法 |
|---|---|
| 日本語入力の切り替え | Ctrl+Space または 全角/半角。通知領域のアイコンのクリックでも |
| Linux のアプリを起動 | スタート → すべてのプログラム（名前に「(Linux)」が付く） |
| Windows のアプリを入れる | 端末で `wine setup.exe`。セッションの `PATH` にはこの Wine が先頭に入っている |
| スタートメニュー | Windows キー、Ctrl+Esc |
| ファイル名を指定して実行 / エクスプローラ | Win+R / Win+E |
| ログオフ・終了 | スタート → ログオフ / 終了オプション |

## 設定（`~/.config/win4wine/session.json`）

| キー | 既定 | 意味 |
|---|---|---|
| `wine`, `prefix` | （setup.sh が書く） | 使う Wine と prefix の絶対パス |
| `locale` | `ja_JP.UTF-8` | セッションのロケール |
| `input_method` | `fcitx5` | 入力メソッド。省くと起動しない |
| `linux_font` | `MS UI Gothic` | Linux アプリの字体。`null` で変えない |
| `linux_menu` | `true` | Linux アプリをスタートメニューに入れる |
| `welcome_screen` | `true` | ログイン後・ログオフ中に XP の画面を出す |
| `cursor_theme` | `win4wine-xp` | カーソルのテーマ。`null` で使わない |
| `shutdown_command`, `reboot_command` | `systemctl poweroff` / `reboot` | 終了オプションで実行するコマンド |

見た目を後から変えるときは、セッションを止めてから `scripts/setup.sh --media ISO --theme classic`（または `luna`）。

## うまく動かないとき

- セッションのログ: `~/.local/state/win4wine/`（前回の分は `*.log.1`）。
- XP のシェルが続けて落ちると、Wine 標準の explorer と xterm に切り替わる。
- ようこそ画面が出ないとき: `/var/log/lightdm/seat0-greeter.log`。必要なものが足りないと、通常の LightDM の画面に切り替わる。

## 元に戻す

```sh
greeter/install.sh --remove                     # ようこそ画面をやめる（LightDM の元の画面に戻る）
sudo rm /usr/share/xsessions/xp-shell.desktop /usr/local/bin/xp-shell-session   # セッションの登録を消す
python3 session/linux_apps.py .local/prefix --wine .local/wine-install/bin/wine --remove   # 作ったスタートメニューの項目を消す
```

ビルドしたもの、取り出したファイル、prefix は、このチェックアウトの `.local/` を消せば消える。
