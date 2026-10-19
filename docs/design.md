# 設計の要点

win4wine の主な判断とその理由を、話題ごとにまとめたものです。調べた内容や試験の詳細は、行った順の記録
[history/investigations.md](history/investigations.md) にあります（節の名前を括弧で示します）。

## 守っていること

- **Windows 由来のバイナリは変えません。** XP の explorer.exe や DLL は ISO から取り出したまま使い、違いは Wine（パッチ）と、
  その周りの自作の部品（WM、proxy、セッション）で吸収します。
- **Microsoft のバイナリを逆アセンブルしません。** 挙動は、公開 API の呼び出しの記録（Wine の `+relay` など）、公開文書、
  実際の XP の上で動かす自作のプローブ（`tests/` の C のプログラム）で調べます。
- **Microsoft のファイルはリポジトリに入れません。** XP の画素や文字列も入れず、測った規則と数値だけを置きます。
  文字列や画像は、利用者の ISO から実行時に読みます。
- **普通の Wine のアプリの挙動は変えません。** XP のシェルのための変更は opt-in（`WIN4WINE_EXTERNAL_SHELL`）に限ります
  （[patches.md](patches.md)）。

## どの部品を XP のものにするか

| 部品 | 使うもの | 理由 |
|---|---|---|
| explorer.exe | XP | タスクバー、スタートメニュー、デスクトップの本体です |
| shell32、browseui、shdocvw、shlwapi | XP | Wine の browseui は XP の explorer が使う機能（序数 107 など）が未実装で、同じ世代の組で使う必要があります（調査 1） |
| browselc、shdoclc、linkinfo、msgina、mydocs | XP（資源・一部の機能） | 日本語のメニューと文字、ショートカット、ログオフ・終了のダイアログ、アイコン（調査 8・14・36・39） |
| comctl32 | XP の v6（XP のシェルのプロセスだけ） | Wine の comctl32 では XP の explorer の toolbar が動きません。ボタンなどは Wine のテーマ付きの実装を別名で使います（調査 7、パッチ 0056・0057） |
| uxtheme | Wine | 利用者の Luna のテーマファイルを Wine の uxtheme で描きます。XP のテーマのサービスは入れません |
| user32.dll（データとして） | XP（アイコンだけ） | アプリの既定のアイコンやメッセージ ボックスのアイコンを XP のものにします。`C:\xp\res\user32-xp.dll` に置いて資源だけを読みます（パッチ 0126。カーソルもここから取り出しています） |
| kernel32、ntdll、user32、gdi32 など | Wine | Linux の上で動かす基盤です |
| mshtml、mscoree | 無効 | IE と .NET は対象外です |

XP の DLL を使うのは **xp-explorer.exe のプロセスだけ**です（AppDefaults）。セッション全体を XP の DLL にすると、
Wine 自身の管理のプロセスや復旧の経路まで XP の DLL に頼ることになるためです（「Override の隔離」）。

## Wine の prefix

- XP のシェルのための専用の prefix を作ります。COM の登録やレジストリの値は、必要と確かめたものだけを自分で書き
  （`prefix/registry.py` の registry profile）、実際の XP のレジストリを丸ごと移すことはしません（調査 6）。
- Wine は自分の版が変わると prefix を更新して登録を書き戻すので、`prefix/stage.py` が更新の印を見て、XP の DLL と登録を
  入れ直します（調査 13）。
- prefix は 64-bit（WoW64）です。XP の DLL は `syswow64` に置き、XP のシェルのための登録は 32-bit の側にだけ書きます
  （「64bit（WoW64）対応」）。

## ほかのシェル（ReactOS、Windows 7）

XP のシェルと同じ Wine、bridge、supervisor で、ほかの Windows 系のシェルも動かせるようにしています。シェルごとに専用の
prefix を作り、そのシェルの DLL はそのシェルのプロセスにだけ使わせます（XP と同じ「Override の隔離」）。

- **ReactOS**（[reactos.md](reactos.md)）: 自由なソフトウェアなので公式のリリースから取得します。登録は ReactOS の DLL 自身の
  `DllRegisterServer` に書かせます。
- **Windows 7**（[windows7.md](windows7.md)、実験的）: 64bit のシェルを使います（prefix が WoW64 なので、64bit のまま動かせます）。
  登録は、XP のように 1 つずつ確かめて手で書くには多すぎる（DirectUI、プロパティ システム、既知のフォルダなど）ので、
  **使うファイルの部品のマニフェスト**（WinSxS）が登録するものを入れます。インストール済みの Windows のレジストリは移しません。
  Windows 7 のためだけの Wine の挙動は `WIN4WINE_SHELL=win7` のときだけ効かせ、XP と ReactOS のシェルには影響させません。
  例外として、アプリのファイルのダイアログを Windows 7 の形にするため、ダイアログが読み込む DLL（shell32 など 10 個）は
  アプリにも使わせます。Wine 自身のプログラムは Wine の DLL のままで、うまく動かないアプリも Wine の DLL に戻せます
  （[windows7.md](windows7.md) の「どのプログラムが Windows 7 の DLL を使うか」）。

## Linux のアプリとの統合（bridge）

- **WM は枠を描きません。** Linux の窓の XP の枠は、proxy.exe が作る Win32 の窓に Wine が描きます。XP の見た目を自分で
  まねて描くのではなく、Wine と利用者のテーマで本物の XP の枠を描くためです（[architecture.md](architecture.md)）。
- **XP のタスクバーは Linux の窓を Win32 の窓として扱います。** そのために、proxy が窓ごとに代理の窓を持ちます。
- **WM と proxy はイベントでだけ動きます。** 以前は周期的に確かめていて、枠の追従が遅れていました。X のイベントと
  inotify、Win32 の WinEvent で起きるようにして、遅れを約 11 ms にしました（「40. WM と proxy のイベント駆動化」）。
- WM は Python のままです。言語を変えても遅れの原因（周期的な確認と、X への問い合わせ）は残るためです。
- **枠と Linux の窓は WM が一緒に動かします。** proxy を通すと、ドラッグの 1 歩ごとに、枠が先に動いて空いた部分が
  黒く見えました。どちらかが動く要求を受けた WM が、同じ処理の中でもう一方も動かします。
- **Wine の窓の中身は X サーバーに覚えさせます（Composite）。** 下の窓が見えたとき、その窓のプロセスが描き直すまで
  黒く見えないようにするためです。Linux の窓の枠は、動いている間だけ同じようにします（大きさを変えるたびに画像を作り直すと、
  大きさの変更が数倍遅くなるため）。合成（影や透明）のための compositing manager は、仮想マシンの CPU の負担を増やすので使いません。
- **仮想マシンでは X がメモリーに描くようにします（ShadowFB）。** GPU の無い仮想マシンでは、窓を動かすたびに X が
  仮想のグラフィックカードのメモリーから読み戻し、これが遅いためです（`scripts/setup.sh`）。

## 日本語入力

- fcitx5 と Mozc を使います。Wine とは XIM、GTK と Qt とはそれぞれの入力モジュールでつながります。
- 起動と環境変数は supervisor が管理し、im-config は使いません。ほかのセッションに影響しないようにするためです
  （「日本語入力（段階 6）」）。
- 入力モードは、fcitx5 のアイコンを XP の通知領域に出して示します。
- 変換中の文字は、XP の IME と同じく入力欄の中に出します（XIM の on-the-spot）。fcitx5 は XIM の利用者に on-the-spot を
  `~/.config/fcitx5/conf/xim.conf` の `UseOnTheSpot=True` のときだけ出すので、supervisor が（利用者が決めていなければ）書きます。
  Wine はキャレットの行の下端を fcitx5 に渡し、候補の窓はその下に出ます（パッチ 0030）。xterm などは今までどおり over-the-spot です。

## ログイン画面

- greeter は自作です（Python、GTK 3、cairo、Pango）。XP の logonui.exe と msgina.dll は動かさず、資源（画像と文字列）だけを読みます。
  パスワードを扱う場所で Microsoft のコードを動かさないためです。
- 文字は GDI の置き方をまねて描き、配置は実際の XP の画面で測った数値に合わせています（「ログイン画面（段階 9）」）。

## DPI

- XP と同じく `LogPixels` 1 つで決めます。XP には DPI の仮想化が無く、どのアプリも自分で大きく描くので、Wine の画像の拡大
  （ぼける）を止め、XP のシェルのプロセスを system DPI aware にしています（パッチ 0052）。
- アイコンやカーソルの大きさなど、XP が独自の規則を持つものは、実際の XP を 96〜216 DPI にして測りました（「DPI の拡大」）。
- Linux のアプリには同じ DPI を `Xft.dpi` などで渡します。

## Wayland

- X11 ではモニターごとの倍率が使えないため、2 つ目のセッションとして、labwc を改変した Wayland の compositor を用意しました
  （試験中）。比べた方式と試作の結果は [history/wayland-study.md](history/wayland-study.md) にあります。
- 自作せず labwc を改変したのは、Xwayland の扱い（XWM）、出力、入力、Wayland のプロトコルを wlroots と labwc に任せ、
  bridge-wm の役（XP の枠、重なり順、IPC）だけを足せば済むからです。
- XP のシェルと Wine は Xwayland の X11 のクライアントのままにします。XP の Explorer は自分で決めた画面座標に窓を置くので、
  位置を決められない Wayland のクライアント（winewayland.drv）では再現できません。
- XP のシェルは出力の 1 画素ずつに表示し、compositor に拡大させません（拡大するとぼけます）。DPI は X11 のセッションと同じく
  `LogPixels` 1 つで決めます。
- X11 のセッションは変えません。Wine のパッチで Wayland のために足したものは、X の root の窓を読めないときだけ別の道を通ります。
- 仕組みと残っていることは [wayland.md](wayland.md) にあります。

## 実際の XP との比べ方

- 参照用に本物の XP（日本語版 SP3 と英語版 SP3）の VM を使い、公開 API の値と画面の画素を比べます。
- 測るのは寸法、色、規則などの数値だけで、XP の画素そのものはリポジトリに入れません。
- 試験は `tests/` にあり、同じプローブを実際の XP と Wine の両方で動かして結果を比べます（[development.md](development.md)）。
- コントロールやメッセージ ボックスは、同じ窓を実際の XP と XP のセッションの両方に出して画素で比べ、Wine の側を XP に合わせています
  （パッチ 0054、0123〜0127。Windows 7 は 0115〜0121）。
