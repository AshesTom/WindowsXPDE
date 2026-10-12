# ReactOS のシェル

XP の ISO が無くても試せるように、[ReactOS](https://reactos.org/) の `explorer.exe` とその周りの DLL を、XP のシェルの代わりに
動かすセッションです。見た目は ReactOS の「クラシック」（Windows 2000 の「Windows スタンダード」の配色）です。
XP のシェルと同じ Wine、同じ bridge（ウィンドウマネージャー、代理ウィンドウ、トレイ）、同じ supervisor で動きます。

```sh
scripts/setup.sh --shell reactos --lang ja    # 英語なら --lang en
```

ログイン画面でセッション「**ReactOS Shell (Wine, X11)**」を選びます。XP のシェルも入れてあれば、両方のセッションが並びます。

## 使う ReactOS のファイル

`extract/reactos.py` が、ReactOS の公式のリリース（GitHub の `0.4.16-release` の `ReactOS-0.4.16-i386.zip`）を取得し、
SHA-256 を確かめて、ISO から次のものだけを `.local/assets-reactos` に取り出します。取得した zip は `.local/dl` に残ります。
`--zip` で手元の zip も使えます。

| ファイル | 使い方 |
|---|---|
| `explorer.exe` | `C:\ros\ros-explorer.exe` として置くシェル |
| `shell32`、`browseui`、`shdocvw`、`shlwapi`、`comctl32`（5 と 6）、`zipfldr` | ros-explorer.exe だけが使う DLL |
| `msgina` | 「シャットダウン」と「ログオフ」のダイアログ |
| `tahoma`、`tahomabd`、`Marlett`、`DroidSansFallback` | ReactOS の自由なフォント（日本語の字は DroidSansFallback） |
| `user32.dll` | カーソルだけを読み、X のカーソルテーマ `win4wine-reactos` を作ります |
| `Nebula.bmp` | ReactOS の既定の壁紙 |

32bit 版（i386）を使います。XP のシェルと同じ WoW64 の 32bit の経路で動き、64bit の ReactOS は毎晩のビルドしかないためです。

### ライセンス

ReactOS は全体として GPL-2.0-or-later です。使う DLL のソースは大半が LGPL-2.1-or-later（Wine に由来するものを含みます）で、
一部のファイルが GPL-2.0 です。フォントはそれぞれのライセンス（Bitstream Vera 系、GPL とフォントの例外、Apache-2.0）です。
win4wine は ReactOS のファイルを同梱せず、公式のリリースから取得して使うだけです。取り出したファイルを配るときは、
GPL と LGPL に従い、対応するソース（`manifest.json` の `source`、ReactOS の `0.4.16-release` のタグ）を示してください。

## prefix（`prefix/reactos.py`）

`.local/prefix-reactos` は WoW64 の prefix で、XP の prefix と同じ考え方で作ります。

- ReactOS の DLL は `syswow64` に置きます（shell32 などは KnownDlls なので、exe の隣は探されません）。prefix 全体では
  Wine の DLL のまま（DllOverrides の `builtin`）にし、`ros-explorer.exe` だけに AppDefaults で `native` を使わせます。
  Common Controls 6 は Wine と同じ版番号なので、`winsxs` の Wine のファイルを置き換えます（ほかのプログラムは `builtin` のまま）。
- ReactOS の explorer は、`HKLM\...\Winlogon` の `Shell` に自分のファイル名があるときだけシェルになります。`ros-explorer.exe` にします。
- COM の登録などは、ReactOS の shell32、browseui、shdocvw 自身の `DllRegisterServer` に書かせます（32bit の view）。
- フォルダの「開く」と「エクスプローラ」は ros-explorer.exe で開きます（ReactOS の hivecls.inf と同じ形）。
- 日本語では、Tahoma などの UI のフォントに DroidSansFallback を FontLink でつなぎます。Wine はロケールの違う起動で
  FontLink を作り直すので、prefix はそのロケールで作り、セッションの前の `stage` で入れ直します。
- 見た目（作るときだけ）: テーマを切り（クラシック）、ReactOS の既定の配色（`win32ss/gdi/ntgdi/stockobj.c`）と寸法と
  フォント（`boot/bootdata/hivedef.inf`）、壁紙 Nebula（塗りつぶし）、スタートメニューの「ログオフ」。
  Wine の名前空間の「Internet」と「/」はデスクトップに出しません。「ヘルプとサポート」は出しません（Wine にヘルプが無いため）。
- `reactos.py stage` は毎回のセッションの前に動き、Wine の更新で上書きされた DLL と登録を入れ直します。

## セッション

session.json（`~/.config/win4wine/reactos-session.json`）の `"shell": "reactos"` で、supervisor は次のように動きます。

- `C:\ros\ros-explorer.exe` を起動し、`prefix/reactos.py stage` を使います。XP のようこそ画面は出しません。
- `WIN4WINE_FOLDER_SHELL` を設定します。ReactOS のシェルはコントロール パネル、検索、Win+E、ショートカットの「リンク先を開く」を
  `explorer.exe` の起動で開きますが、この prefix の explorer.exe は Wine のデスクトップの管理です。パッチ 0063 で、Wine の explorer は
  そうした起動を ros-explorer.exe に渡します。
- `WIN4WINE_SWITCHER=reactos` を設定し、proxy は Alt+Tab の窓を ReactOS の形（`appswitch.c`）で描きます。

bridge と XP 向けのパッチは、そのまま ReactOS のシェルにも効きます（タスクバーのボタン、Linux の窓の枠、Alt+Tab、
Windows キーと Ctrl+Esc、通知領域、ログオフとシャットダウン）。

## XP のシェルとの違い・制限

- 見た目は ReactOS に近づけましたが、XP のシェルのように画素単位では合わせていません。とくに字は、ReactOS が太字を
  標準の Tahoma から合成するのに対し、Wine は `tahomabd.ttf` を使うので、太字の幅が違います。
- 「ヘルプとサポート」、ネットワーク、プリンターの設定は Wine にその部品が無いので使えません。
- ログイン画面は今のところディスプレイマネージャのものです（XP のようこそ画面は XP の画像を使うため）。
- Wayland のセッションはまだありません。

試験は `tests/reactos_session.py`（[development.md](development.md)）です。
