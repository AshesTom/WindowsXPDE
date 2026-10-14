# Windows 7 のシェル（実験的）

Windows 7 SP1（x64）の `explorer.exe` とその周りの DLL を、XP のシェルの代わりに動かすセッションです。見た目は
「Windows 7 ベーシック」（Aero のテーマを、デスクトップの合成なしで描いたもの）です。半透明のガラスなどの Aero の効果はまだありません。
XP のシェルと同じ Wine、同じ bridge（ウィンドウマネージャー、代理ウィンドウ、トレイ）、同じ supervisor で動きます。

```sh
scripts/setup.sh --shell win7 --media /path/to/Windows7SP1x64.iso
```

ログイン画面でセッション「**Windows 7 Shell (Wine, X11)**」を選びます。XP や ReactOS のシェルも入れてあれば、セッションが並びます。

> [!WARNING]
> XP のシェルより新しく、確かめた範囲も狭い試作です。下の「制限」を読んでから試してください。
> Windows 7 の ISO はご自身で用意してください。Microsoft のファイルの扱いは、Microsoft の使用許諾に従ってください。

## 使う Windows 7 のファイル

`extract/win7.py` が、ISO の `sources/install.wim` から次のものだけを `.local/assets-win7` に取り出します。
エディションごとのイメージのうち Professional を使います（無ければ最後のもの）。シェルのファイルはどのエディションでも同じです。
言語はイメージの既定の言語で決まり、その言語の MUI ファイル（`ja-JP\shell32.dll.mui` など）も取り出します。

| ファイル | 使い方 |
|---|---|
| `explorer.exe` | `C:\w7\w7-explorer.exe` として置くシェル（x64） |
| `explorerframe`、`shell32`、`shlwapi`、`propsys`、`shdocvw`、`dui70`、`duser`、`profapi`、`sndvolsso`、`winbrand`、`shacct`、`thumbcache`、`authui`、`linkinfo` | w7-explorer.exe（と、サムネイルを作る dllhost.exe）だけが使う DLL |
| `actxprxy` | Windows 7 のインターフェースの proxy。`w7actxprxy.dll` の名前で置きます（Wine の actxprxy.dll は Wine の DLL の proxy に残します） |
| `imageres`、`sud`、`DeviceCenter` | 文字列とアイコンだけを読む DLL（override なし） |
| `SysWOW64\imageres.dll` | 32bit のプログラムが読むファイルの種類のアイコン |
| Common Controls 6（amd64） | w7-explorer.exe が使う comctl32。`C:\w7\comctl32.dll` として、シェルの横に置きます |
| Segoe UI、メイリオ（日本語のとき） | Windows 7 の UI のフォント |
| `Resources\Themes\Aero` | Aero のテーマ（形式 4 の msstyles。Wine の 0079 で読みます） |
| `img0.jpg`、User Account Pictures、スタートメニューの `desktop.ini` | 壁紙、利用者の画像、スタートメニューのフォルダの日本語の名前 |

取り出したファイルには手を加えません。prefix には、Windows 7 と同じく DLL の隣の言語のフォルダに MUI ファイルを置き
（`system32\ja-JP\shell32.dll.mui`）、Wine がそれを読みます（Wine のパッチ 0091。言語に依存しない DLL で見つからない
リソースを、その DLL の MUI ファイルから探します）。名前を変えて置くもの（`w7-explorer.exe`、`w7actxprxy.dll`）は、MUI ファイルも
その名前にします。

### レジストリ

Windows 7 は、部品（component）ごとのマニフェスト（`WinSxS\Manifests`）にファイルとレジストリの値を書いています。
`extract/manifests.py` が、上のファイルの部品のマニフェストから、レジストリのキーと値、設定の既定値（configuration の schema）を
`.reg` にします。インストール済みの Windows のレジストリを丸ごと移すことはしません。使うファイルの部品が登録するものだけを入れます。
アイコンを番号（`shell32.dll,3`）で指す値は、リソースの ID（`shell32.dll,-4`）に直します。32bit のプログラムが読む Wine の shell32 は、
アイコンの並びが Windows 7 のものと違うためです（ID は同じです）。32bit のプログラムのために、ファイルの種類のアイコンを持つ
32bit の `imageres.dll` も `SysWOW64` に置きます。

Windows 7 の comdlg32 の部品が登録する `FileOpenDialog`（シェルの「参照」が開くダイアログ）は、prefix/win7.py が Wine の comdlg32 の
ダイアログに向けます（部品のファイルは Wine のものなので、取り出しません）。

## prefix（`prefix/win7.py`）

`.local/prefix-win7` は WoW64 の prefix で、XP の prefix と同じ考え方で作ります。

- Windows 7 の DLL は `C:\w7\native` に置き、`system32` に写します（64bit のシェルなので system32 です）。prefix 全体では
  Wine の DLL のまま（DllOverrides の `builtin`）にし、`w7-explorer.exe` と `dllhost.exe` だけに AppDefaults で `native` を使わせます。
- Windows 7 の Common Controls 6 は、シェルの横（`C:\w7\comctl32.dll`）に置きます。シェルのマニフェストの SxS の振り分けは
  Wine の Common Controls 6 を指しますが、Windows 7 のシェルのセッションでは、実行ファイルの横に Wine のものではない
  comctl32.dll があれば、そちらを使います（Wine のパッチ 0092）。Wine の Common Controls 6 は、ほかのすべてのプログラムのものです。
- `HKLM\...\Winlogon` の `Shell` を `w7-explorer.exe` にします。
- 日本語では、Segoe UI や Tahoma などの UI のフォントにメイリオを FontLink でつなぎます。
- 見た目: ClearType（Windows 7 の既定）、Aero のテーマ（`prefix/theme-settings.c`）。テーマのフォント（メイリオ 9pt）を利用者の
  フォントとして保存します（`prefix/theme-fonts.c`。Windows でテーマを当てたときと同じ。日本語のフォント名はテーマの MUI にあるので、
  prefix の言語で動かします）。壁紙は Windows 7 と同じく `TranscodedWallpaper.bmp` に変換して使います。
- 「ドキュメント」は Linux のホーム（`H:`）です（XP のシェルと同じ）。
- `win7.py stage` は毎回のセッションの前に動き、Wine の更新で上書きされた DLL と登録（「コンピューター」「コントロール パネル」
  「ネットワーク」の名前など）を入れ直します。

## セッション

session.json（`~/.config/win4wine/win7-session.json`）の `"shell": "win7"` で、supervisor は次のように動きます。

- `C:\w7\w7-explorer.exe` を起動し、`prefix/win7.py stage` を使います。XP のようこそ画面は出しません。
- `WIN4WINE_SHELL=win7` を設定します。Windows 7 のためだけの Wine のパッチ（0081、0083、0085、0086 の一部）は、これがあるときだけ効きます。
- Linux の窓は、`WM_CLASS` ごとにタスクバーのボタンにまとまります（proxy が窓に AppUserModelID を付けます）。
- Linux のトレイアイコン（StatusNotifierItem と XEmbed）は、Windows 7 の通知領域に出ます（Wine の 0085）。
- 日本語入力は XP のシェルと同じ fcitx5 + Mozc です。入力モード（「あ」など）は通知領域に出て、Wine のアプリでは窓の中で
  変換します。
- Windows のアプリがスタートメニューに作るショートカットは、「すべてのプログラム」に並びます。
- ファイルを開く・保存するダイアログ: Windows のアプリ（64bit・32bit）は Wine のダイアログ、Linux のアプリ（GTK 3・GTK 4・Qt）は
  それぞれのダイアログです。どちらも「ドキュメント」（`H:`）は Linux のホームで、日本語の名前のファイルも開け、保存できます。
  シェルの「ファイル名を指定して実行」の「参照」は、Windows 7 の shell32 で Wine のダイアログを開きます（Wine の 0095）。
- Windows キー、Ctrl+Esc（Linux の窓からも）、Win+R、Win+E、Alt+Tab（XP の形）が使えます。
- スタートメニューの「シャットダウン」と、その横のメニューの「ログオフ」「再起動」で、セッションを終えます（XP のシェルと同じく、
  終了と再起動では session.json の `shutdown_command`・`reboot_command` を実行します）。

Windows 7 のシェルのために足した Wine のパッチは 0078〜0095 です（[patches.md](patches.md)）。

## 制限

- Aero の効果（ガラス、ライブ サムネイル、Aero Peek）はありません。デスクトップの合成（DWM）が無いためです。
- ログイン画面とカーソルは Windows 7 のものではありません（ディスプレイマネージャーと X のもの）。
- Wayland のセッションはまだありません。
- 確かめたのは日本語版の Windows 7 Professional SP1（x64）だけです。英語版も同じ手順で作りますが、試していません。
- ネットワーク、ヘルプとサポート、コントロール パネルの中身は、Wine にその部品が無いので使えません。
- スタートメニューの「ロック」「スリープ」「休止状態」は何もしません。
