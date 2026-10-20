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
| `lautus`、`lunar`、`mizu` の `.msstyles` | 任意のテーマ（既定はクラシック） |

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
  Common Controls 6 は `C:\ros\comctl32.dll` に置き、ros-explorer.exe だけがそれを使います（Wine のパッチ 0131。Windows 7 の
  0115 と同じ仕組み）。`winsxs` の Wine のアセンブリはそのままで、ほかのプログラムは Wine の Common Controls 6 を使います
  （以前は Wine のファイルを置き換えていたため、ほかの 32bit のアプリでボタンやエディットが作られませんでした）。
- ReactOS の explorer は、`HKLM\...\Winlogon` の `Shell` に自分のファイル名があるときだけシェルになります。`ros-explorer.exe` にします。
- COM の登録などは、ReactOS の shell32、browseui、shdocvw 自身の `DllRegisterServer` に書かせます（32bit の view）。
- フォルダの「開く」と「エクスプローラ」は ros-explorer.exe で開きます（ReactOS の hivecls.inf と同じ形）。
- 日本語では、Tahoma などの UI のフォントに DroidSansFallback を FontLink でつなぎます。Wine はロケールの違う起動で
  FontLink を作り直すので、prefix はそのロケールで作り、セッションの前の `stage` で入れ直します。
  FontLink の届かない所（フォルダの窓のアドレス欄など）と Linux のアプリのために、`setup.sh` は日本語のとき `fonts-noto-cjk` も入れます。
- 日本語では、スタートメニューの「スタートアップ」と「管理ツール」のフォルダを ReactOS と同じ日本語の名前にします（Wine は英語の名前で作ります）。
  Wine は User Shell Folders の場所を使うので、名前を変えたフォルダをそのまま使います。
- 見た目（作るときだけ）: テーマを切り（クラシック）、ReactOS の既定の配色（`win32ss/gdi/ntgdi/stockobj.c`）と寸法と
  フォント（`boot/bootdata/hivedef.inf`）、壁紙 Nebula（塗りつぶし）、スタートメニューの「ログオフ」。
  Wine の名前空間の「Internet」と「/」はデスクトップに出しません。「ヘルプとサポート」は出しません（Wine にヘルプが無いため）。
- `reactos.py stage` は毎回のセッションの前に動き、Wine の更新で上書きされた DLL と登録を入れ直します。

## セッション

session.json（`~/.config/win4wine/reactos-session.json`）の `"shell": "reactos"` で、supervisor は次のように動きます。

- `C:\ros\ros-explorer.exe` を起動し、`prefix/reactos.py stage` を使います。XP のようこそ画面は出しません。
- `WIN4WINE_FOLDER_SHELL` を設定します。ReactOS のシェルはコントロール パネル、検索、Win+E、ショートカットの「リンク先を開く」を
  `explorer.exe` の起動で開きますが、この prefix の explorer.exe は Wine のデスクトップの管理です。パッチ 0079 で、Wine の explorer は
  そうした起動を ros-explorer.exe に渡します。
- `WIN4WINE_SWITCHER=reactos` を設定し、proxy は Alt+Tab の窓を ReactOS の形（`appswitch.c`）で描きます。

bridge と XP 向けのパッチは、そのまま ReactOS のシェルにも効きます（タスクバーのボタン、Linux の窓の枠、Alt+Tab、
Windows キーと Ctrl+Esc、通知領域、ログオフとシャットダウン）。

クイック起動は、タスクバーの右クリック →「Toolbars」→「Quick Launch」で出ます。prefix がクイック起動のフォルダ
（`%APPDATA%\Microsoft\Internet Explorer\Quick Launch`）を作ります（無いと「Cannot create toolbar.」）。ただし ReactOS 0.4.16 の
explorer はツールバーの配置を保存しないので（ReactOS 自身の制限。ツールバーのフォルダも保存しません）、セッションごとに出し直します。

## テーマ（任意）

既定はクラシックですが、ReactOS の 3 つのテーマも選べます。`scripts/setup.sh --shell reactos --theme lautus`（`lunar`、`mizu`）、
または `python3 prefix/reactos.py theme PREFIX lautus`。テーマは Wine の uxtheme が描きます（`prefix/theme-settings.c` の
`msstyles` で当てます）。ReactOS のテーマのスタートボタンは負の余白を持つので、Wine のパッチ 0080 で余白の負の値を読めるようにしました。

## ログイン画面（任意）

`scripts/setup.sh --shell reactos --greeter` で、LightDM のログイン画面を ReactOS のもの（`greeter/reactos-greeter.py`）にします。
ReactOS の msgina.dll のダイアログ（「Ctrl-Alt-Delete キーを押してください」→「ログオン」、「シャットダウン」）を、テンプレートの
配置と文言（日本語と英語）、バナーの画像のまま、ReactOS の壁紙の上にクラシックの見た目で描きます（`greeter/rosdialog.py`）。
認証は LightDM と PAM が行います。

- 「ようこそ」では Ctrl+Alt+Delete で「ログオン」に進みます。`/etc/win4wine/reactos-greeter.json` の `"ctrl_alt_del": false` で、
  最初から「ログオン」を出します。
- 「ログオン」の利用者名は LightDM が選ぶ利用者（前回の利用者）です。ログオン先はこのマシンの名前です。
- 「シャットダウン」では、シャットダウンと再起動（できるならスリープ）を選べます。
- 配置は 1024×768 の ReactOS 0.4.16 で測った位置と同じです（`tests/reactos_greeter.py`）。

## XP のシェルとの違い・制限

- 見た目は ReactOS に近づけましたが、XP のシェルのように画素単位では合わせていません。とくに字は、ReactOS が太字を
  標準の Tahoma から合成するのに対し、Wine は `tahomabd.ttf` を使うので、太字の幅が違います。
- 「ヘルプとサポート」、ネットワーク、プリンターの設定は Wine にその部品が無いので使えません。
- フォルダの窓の「フォルダ」の一覧には、Wine の Unix のルート（`/`）も出ます（デスクトップには出しません）。
- Linux のアプリは、`.desktop` に作業ディレクトリ（`Path`）が無いと prefix の `C:\windows` から始まります（XP のシェルと同じ）。
- Wayland のセッション（`--wayland`、「ReactOS Shell (Wine, Wayland)」）は XP のシェルの Wayland のセッションと同じ labwc で動きます。
  XP のシェルと同じく、X11 のセッションより実験的です（[wayland.md](wayland.md)）。

試験は `tests/reactos_session.py`（[development.md](development.md)）です。
