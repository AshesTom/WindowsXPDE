# 構成

`extract/extract.py SOURCE OUTPUT` はローカル ISO または XP Windows ディレクトリから選択したファイルのみを抽出する。ネットワーク処理はない。出力と出典・ハッシュは `.local/` に置く。フォント・アイコンの資源を再配布せず、DLL 内のアイコンもそのままローカルで利用する。

`prefix/create.py ASSETS PREFIX` は Wine が生成するクリーンな 64-bit（WoW64）の prefix（段階 7 より前は win32）のレジストリに、winxp、ホームの H: マッピング、Personal フォルダ、クラシック表示を設定する。XP explorer は `C:\xp` に置き、builtin explorer を保存する。

`wine/build.sh` は upstream mirror の固定 commit を取得し、`wine/patches/series` を順に適用する。適用済みの判定を行う。Wine の修正には目的、対象 XP 動作、実装根拠・制約を書く。`.local/wine-upstream` の変更を直接の成果物とせず、必ずパッチに反映する。パッチの並び、書式、opt-in 判定、旧番号との対応は `docs/patches.md`。

`bridge-wm/wm.py` は単一 X11 screen の非 reparenting WM。`_WIN4WINE_CLASS` と `_WIN4WINE_HWND` を Wine X11 ドライバから読み、デスクトップ、トレイ、代理ウィンドウを識別する。通常の Wine ウィンドウを二重に代理化しない。desktop を下げ、dock を上げる。dock の strut または位置から `_NET_WORKAREA` を生成する。Alt+Tab、click to focus、EWMH activate/close/maximize、ICCCM iconic state を処理する。
定期的には起きない。X のイベント（MapRequest、ConfigureNotify、PropertyNotify、FocusIn など）、IPC ディレクトリの
inotify（proxy の `*.cmd` と `frames.tsv`）、シグナルだけで起き、そのたびに状態を計算し直して内容が変わったファイルだけを書く。
クライアントの位置・大きさ・題名・transient・`_WIN4WINE_HWND` は ConfigureNotify と PropertyNotify から記憶し、
`geometry.tsv` を X への問い合わせなしで最初に書く。native ウィンドウが動いた直後の X サーバーは、見えるようになった
デスクトップの再描画で混んでおり、問い合わせのたびにその後ろで待たされるため。

`bridge-wm/proxy.c` は Wine 上の Win32 プロセス。Linux の通常ウィンドウに `Win4WineProxy` HWND を割り当てる。Win32 のシェルが GetWindowText、IsWindowVisible、IsIconic 等を参照できる。代理 HWND は `WS_OVERLAPPEDWINDOW` を持ち、その X ウィンドウを native クライアントの直下に表示して Win32 のタイトルバーと枠を使う。対応するクライアントがない代理は隠す。shell hook の登録は user32 パッチで HWND の property に保存し、仲介プロセスが SHELLHOOK メッセージを配信する。完全な Win32 hook 実装ではない。
proxy も定期的には起きない。IPC ディレクトリの変更通知、out-of-context の WinEvent（前面化、最小化の開始・終了、
最上位ウィンドウの生成・破棄・表示・非表示、題名の変更）、自分のウィンドウへのメッセージで起き、Win32 の一覧と前回との差を
SHELLHOOK で配信する。題名の変更は、Wine 11.0 の DefWindowProc の WM_SETTEXT が EVENT_OBJECT_NAMECHANGE で知らせる
（Wine 10 では opt-in のパッチで出していた。段階 5 で外した）。ファイルの同期はメッセージ処理より先に行う
（先に動いた native が枠の一部を見えるようにし、その再描画を先にすると枠の移動が遅れるため）。

仲介プロセスは、表示済みの `Shell_TrayWnd` と Start/ReBar 子ウィンドウの準備が 200ms 間隔の二回の確認で安定した後、現在の親 client サイズを使った公開 `WM_SIZE` を一度だけ送る。起動直後に保持された子の ideal 高さを、シェル自身の通常レイアウトで親に合わせ直すためで、子の寸法や字体は指定しない。応答待ちは最大 500ms。Tray HWND/PID の置換・消失で状態をリセットし、一時的な非表示では再送しない。この確認の timer は、表示中の tray がこの同期を待っている間だけ動く。Luna と Classic に適用し、`BCM_GETIDEALSIZE` の測定値は変更しない。

WM は XSETTINGS の selection が空いているときだけその所有者になり、`Gtk/DecorationLayout` を `:` にする。
GTK のヘッダーバーを使うアプリ（Remmina など）は自前の最小化・最大化・閉じるボタンを出さず、
Win32 の枠のボタンだけが残る。既存の settings daemon がある場合は何もしない。他の設定は GTK の既定値。

## IPC v1 と枠の拡張

セッションごとの所有者のみアクセスできる 0700 ディレクトリを使う。共有の固定 `/tmp` ファイルや外部待受ソケットは使わない。

WM → proxy: `windows.tsv` を同一ディレクトリで atomic rename。行は `XID(hex) TAB minimized(0/1) TAB active(0/1) TAB UTF-8 title LF`。改行・tab はタイトルから除去する。最小化されたウィンドウは一覧に残す。破棄時に除く。Wine HWND と transient は含めない。

proxy → WM: `PID-SEQUENCE.cmd` を atomic rename。内容は `activate|minimize|close|maximize|restore XID(hex)` または `geometry XID(hex) x y width height`。WM は現在管理する Linux の通常ウィンドウだけを操作し、処理後削除する。proxy は最大 2048 ウィンドウまで。

Alt+Tab は XP の切り替えウィンドウを proxy が描く。WM は Alt を押している間キーボードを grab し、
`switch.tsv`（`session steps state`、state は o=表示中・c=確定・x=取り消し）を atomic rename で更新する。
steps は Tab の回数（Shift+Tab は負）。proxy は IPC ディレクトリの変更通知で直ちに起き、新しい session で
Win32 の Z 順（アクティブなウィンドウが先頭）から候補を集め、選択を描く。確定時は `switch XID(hex)` を送り、
WM は Wine のウィンドウも含めクリックと同じく活性化する（Wine は管理対象の窓の X フォーカスを WM に任せるため）。
最小化中の Wine ウィンドウは先に元に戻す。proxy がなければ WM は従来どおり順に切り替える。
proxy が動いているかは、proxy が `proxy.lock` の先頭1バイトにかけている LockFile で判断する。wineserver はこれを POSIX lock に
し、proxy の終了で外すので、WM は F_GETLK で確かめられる（以前は `frames.tsv` の更新時刻が2秒以内かで見ていたが、
変化がないと書かなくなったため）。

枠の同期には次の 2 ファイルを追加する。どちらも同じ IPC ディレクトリで atomic rename し、数値を空白で区切る。

- WM → proxy の `geometry.tsv`: `XID(hex) x y width height maximized(0/1) workX workY workWidth workHeight`。座標とサイズは root 上の native クライアント領域。末尾は現在の作業領域。
- proxy → WM の `frames.tsv`: `HWND(hex) XID(hex) left right top bottom maximized(0/1) iconic(0/1) borderX borderY`。余白は公開 `GetWindowRect` / `GetClientRect` / `ClientToScreen`、borderは`GetWindowInfo`の測定値。

どちらも内容が変わったときだけ書く（相手を無駄に起こさないため）。proxy は枠の位置が既に合っていれば SetWindowPos しない。

`_WIN4WINE_HWND` と `frames.tsv` の HWND を照合して枠と native クライアントを組にする。
WM は枠をクライアントのすぐ下へ積み、枠の未使用の Win32 client 領域を native の画面で覆う。
native ウィンドウの X border は 0 にし、測定した余白を `_NET_FRAME_EXTENTS` に公開する。初めて枠を付けるとき、USPosition/PPosition のないウィンドウは XP の重ね配置の次の位置へ置く。
位置指定のあるウィンドウは workarea の上端と左端を基準に補正し、0,0 で起動した xterm のタイトルバーも画面内に置く。
EWMH の一覧、アクティブ ID、キーボードフォーカスは native XID のまま。
アイコンは native の `_NET_WM_ICON` を優先し、なければ ICCCM `WM_HINTS` の pixmap / mask を読む。
property は最大1 MiB、画像は256×256までとし、manage と icon / hints property 変更時だけ取得する。
`icon-XID.argb` は `W4I1`、little-endian の32bit width / height、行順の straight ARGB32画素からなる。
所有者専用 IPC へ atomic rename で公開し、proxy は更新されたファイルだけ読み、公開 Win32 API で HICON を作る。
small / big の WM_SETICON に同じ HICON を設定し、交換・ウィンドウ破棄時に所有した HICON を一度破棄する。
アイコンがない・壊れている場合は Wine builtin の共有 `IDI_APPLICATION` に戻す。共有アイコンは破棄しない。
Wine の CreateIconIndirect は alpha 用 bitmap を内部で premultiply するため、bridge では二重変換しない。
Wine パッチ 0022 は外部シェル opt-in 時だけ、別プロセス間の公開 `WM_GETICON` と、別プロセスのクラスのアイコンの `GetClassLongPtr` を許可する。
返す値は所有者の元の borrowed HICON で、受信側で `GetIconInfo` / `DrawIconEx` / `CopyIcon` を使える。
所有者が初期化した static icon の画素・resource metadata を wineserver に不変 snapshot として登録する。
画像は256×256、metadata は各2048 bytes、所有プロセスは16 MiB、server 全体は64 MiBまで。
読み手は毎回 handle generation と生存を照合し、最大64個のローカル bitmap コピーを保持する。
GDI driver の初回初期化と bitmap import は USER lock の外で行い、cache 登録前に owner / generation を再確認する。
同時に登録されたコピーを重複保持せず、参照中のコピーは pin し、所有者の破棄・終了後は次の読み取りを拒否する。`CopyIcon` の結果は読み手が所有する。
foreign HICON の破棄・変更、別プロセスの `WM_SETICON`、他の GDI handle message は許可しない。
opt-in がない通常 Wine の挙動は維持する。上限を超える画像と animated icon は foreign snapshot の対象外。
検証用の読み取り専用 WM_APP query は引き続き proxy 内で寸法・画素 checksum を返し、実 caption は X image でも検査する。
XP タスクバーは非同期 `WM_GETICON` の callback 後に固有 icon を描く。初期の既定画像からの切り替えと icon 更新後の表示は、taskbar の実 X 画素で検査する。
0023 は server protocol を変更するため、wineserver / ntdll / win32u を揃えて配備し、既存 prefix server を停止して切り替える。

代理 XID への EWMH activate も native のフォーカスに変換するので、タスクが二重にならない。
タイトルバーと角の操作は Win32 の通常の非クライアント処理を通し、移動・リサイズを
`geometry` コマンドで native へ戻す。最小化では組を隠し、復元では proxy の非 iconic 応答を待って枠を戻す。
withdraw / destroy 時は組を直ちに解除して残った枠を隠す。真の withdraw では WM_STATE property を削除して WithdrawnState を通知する。Wine X11 driver はこの応答を待って次の XMapWindow を発行するため、応答がないと Start の Win32 HWND は visible でも X のメニューが再表示されない。最小化された client と WM が隠した代理枠にはこの削除を行わない。次の snapshot で不要な代理 HWND も破棄される。

最大化はnativeの復元位置をWMに保存する。proxyは公開WM_GETMINMAXINFOで作業領域と
border幅を返し、ShowWindow(SW_MAXIMIZE/RESTORE)で本物のWS_MAXIMIZEを切り替える。
外側borderは画面外へ出し、Luna/96 DPIの可視余白は左右下0、上26pxになる。
Win4WineMaximized propertyも既存の非クライアント描画との互換用に維持する。

Wineの通常ウィンドウは代理化しない。Wine パッチ 0025 は外部shellに限り、managed最大化windowの
位置・サイズを所有threadからX11へ送る。WMはEWMH状態を確認し、幾何を上書きしない。
これによりWM_GETMINMAXINFOのアプリ独自制限と復元位置を尊重する。
WMが非同期に最大化サイズを強制する方式は、Wineのpending矩形と実Xサイズを食い違わせ、
高速復元で位置だけ・サイズだけが混ざるため採用しない。
wm.py、proxy.exe、Wine0041を同時に更新する。

`DV2ControlHost` は上に積む popup として扱い、
Alt+Tab、native snapshot、shell hook の通常タスク一覧から除く。popup には Wine の activation に必要な X フォーカスと WM_TAKE_FOCUS を与えるが、アクティブなアプリの EWMH ID は上書きしない。shell hook の WINDOWACTIVATED は現在のtask一覧に含まれるidentityだけを通知する。owned dialogは公開GW_OWNERを順に辿り、最も近いtask ownerを選ぶ。Start popupや未所属toolをactivation経由でtaskに追加しない。
FocusIn は通常の通知だけを採用する。keyboard grab の NotifyGrab / NotifyUngrab と
pointer 由来の通知で Alt+Tab の選択を上書きしない。

この方式は Win32 / X11 間の reparent や native クライアントの Win32 化を行わない。
Win32 HWND と X11 の内容は別ウィンドウで、枠と内容の移動には時間差がある（Debian VM で中央値約 11 ms）。
native transient は snapshot の対象外なので、この経路で新たな代理枠を作らない。
アプリの独自システムメニューや追加 caption button を native 側へ移植する処理もない。
クリップボードの変換は Wine X11 driver の既存機能に任せ、proxy は独自の clipboard IPC を持たない。
Microsoft の画像を Python で模写・再配布する方式ではなく、ローカルのテーマを Wine の公開描画 API で使用する。

タイトル変更、破棄、生成、アクティブ変更は SHELLHOOK に変換する。
通常の Wine HWND も前回のタイトルを比較し、生成後に付いたフォルダ名や移動後の名前を通知する。比較は WinEvent を受けた時に行うので、
WinEvent の処理より先に生成から破棄まで終わる短命ウィンドウは通知しないことがある。`WH_SHELL` DLL injection は実装しない。購読先の子ウィンドウも検索し、実 XP の MSTaskSwWClass への通知を確認した。

## Linux のトレイアイコン（段階 6）

`bridge-wm/tray.py`（python3-gi の Gio と GdkPixbuf）は、セッションバスで `org.kde.StatusNotifierWatcher` を所有し、自身を
host として登録する。登録された StatusNotifierItem ごとに番号を振り、アイコン・ツールチップ・メニューを IPC ディレクトリへ書く。
すでに別の watcher（デスクトップ環境のトレイ）がいれば何もせずに終わる。D-Bus と IPC ディレクトリの変更（Gio の
FileMonitor）でだけ起きる。XEmbed のトレイ（`_NET_SYSTEM_TRAY_S0`）は扱わない。

- tray.py → proxy の `tray.tsv`: `ID TAB 状態 TAB ツールチップ`。状態は A（Active）か N（NeedsAttention）。Passive は載せない。
- tray.py → proxy の `tray-ID.argb`: アイコン。`W4I1` 形式（WM のウィンドウアイコンと同じ）。大きさは proxy が `tray-size` に書く
  `SM_CXSMICON`。IconPixmap があればそれを、なければ IconName をアイコンテーマ（IconThemePath、`~/.local/share/icons`、
  `/usr/share/icons` 以下のすべてのテーマと pixmaps）から探して PNG / SVG を描く。NeedsAttention のときは Attention の画像を先に使う。
- proxy → tray.py の `*.tray`: `activate ID X Y`（左クリック、ItemIsMenu ならメニュー）、`secondary ID X Y`（中クリック）、
  `context ID X Y`（右クリック）、`event ID SEQ ITEM`（メニューの選択）。WM の `*.cmd` とは拡張子で分ける。
- tray.py → proxy の `menu-ID.tsv`: 右クリックのたびに `com.canonical.dbusmenu` の AboutToShow と GetLayout を呼び、先頭の行に
  連番、続けて `深さ TAB 項目 TAB 印 TAB ラベル`（印は s 区切り、d 無効、c チェック、r ラジオ）を書く。ラベルの `_` は `&` に直す。
  proxy は自分が求めた連番より新しいものだけを TrackPopupMenu で出し、選んだ項目を `event` で返す。古い連番の選択は捨てる。

proxy は `Win4WineTray` の隠しウィンドウで公開の `Shell_NotifyIconW` を呼び、XP の通知領域にアイコンを追加・更新・削除する。
XP の Explorer が受け取れる形式で届くのは Wine パッチ 0028 による（Wine の builtin の形式は XP の Explorer が受け付けない）。
`TaskbarCreated` を受けたら追加し直す。

## セッション

ディスプレイマネージャが Xorg と認証を起動し、`session/supervisor.py` が WM → tray.py → 入力メソッド → proxy → XP explorer を起動する。WM 所有権取得に失敗したらセッションを中断する。トレイの存在を監視し、起動失敗・消失・proxy 終了時に専用 wineserver を終了して再試行する。120 秒以内に 3 回失敗すると native override と外部 shell フラグを解除し、独立した recovery prefix の Wine builtin ホームブラウザと復旧用 xterm を起動する。セッション終了時に子プロセス・専用 wineserver・IPC を掃除する。

`session.json` の `input_method` が `fcitx5` なら、`XMODIFIERS=@im=fcitx`、`GTK_IM_MODULE`・`QT_IM_MODULE`・`SDL_IM_MODULE=fcitx` を
Wine と Linux アプリの環境に入れ、fcitx5 を起動して見張る（120 秒に 3 回まで起動し直す）。ログイン側（im-config など）がすでに
これらの変数を設定していればそれに従い、XIM サーバー `@server=fcitx` がすでにあれば起動しない。`linux_font`（既定 `MS UI Gothic`、
`null` で無効）は、prefix の `windows/Fonts` を加え、sans-serif・Sans・system-ui にその字体を優先する fontconfig の設定を
状態ディレクトリに作り、`FONTCONFIG_FILE` で渡す。システムと利用者の fontconfig の設定は include で引き続き効く。

`session.json` の `linux_menu`（既定 `true`）が有効なら、XP Explorer を起動する前に `session/linux_apps.py` で Linux の
アプリをスタートメニューへ入れ、その後は `--watch` で見張る（下の「Linux アプリのスタートメニュー」）。Wine と Linux アプリの
`PATH` の先頭にはセッションの Wine の `bin` を置き、端末で打つ `wine` がセッションと同じ Wine と prefix を使うようにする。

Win7 対応時も WM と IPC を維持し、DLL profile、shell readiness class、必要な Wine shim を変更する方針。

WM は native X クライアントのフォーカス・サイズ変更に合わせて、表示中の子孫へ Expose を送る。
描画内容は消去しない。Wine HWND のあるクライアントは Wine の surface 管理に任せる。
周期的な再描画ではない。ConfigureRequest には root 座標の
synthetic ConfigureNotify も返す。

XP のタスクボタンが他プロセスの最小化可否を調べるため、Wine パッチ 0021 は外部シェル opt-in 時に
標準システムメニューのプロセス内コピーを返す。アプリ独自の変更は同期しない限定 shim。
0034 は名前付き child ReBar の未指定 alignment を CCS_TOP に正規化し、フォルダの初期ずれを防ぐ。

## Linux アプリのスタートメニュー（2026-10-11）

`session/linux_apps.py PREFIX --wine WINE [--watch]` は、XDG のデータディレクトリ（`$XDG_DATA_HOME`、`$XDG_DATA_DIRS`）の
`applications` にある desktop entry（freedesktop.org の Desktop Entry Specification）を読み、表示するアプリごとに prefix の共通の
Programs（`C:\ProgramData\Microsoft\Windows\Start Menu\Programs`）にショートカットを1つ置く。

- 表示するもの: `Type=Application` で、`NoDisplay`・`Hidden` が真でなく、`OnlyShowIn`・`NotShowIn` が今のデスクトップ
  （`XDG_CURRENT_DESKTOP`。XP シェルのセッションでは空）に合い、`TryExec` と Exec のプログラムが見つかるもの。同じ desktop file ID
  では先のディレクトリ（利用者の側）が勝つ。Wine 自身が Windows のアプリのために作る `applications/wine` 以下は読まない。
- 名前は `Name` のロケールの値（`LC_ALL`・`LC_MESSAGES`・`LANG`）に ` (Linux)` を付ける（利用者の判断）。同じフォルダで重なると
  ` (Linux 2)` …。説明（スタートメニューのツールチップ）は `Comment`、なければ `GenericName`。
- フォルダは `Categories` の主な分類から、XP にあるものはその名前にする（日本語のロケールでは日本語）。Windows のアプリの
  フォルダと同じ名前なら混ざる（利用者の判断）。

  | 分類 | フォルダ |
  |---|---|
  | TerminalEmulator、Utility | アクセサリ |
  | AudioVideo、Audio、Video | アクセサリ\エンターテイメント |
  | System | アクセサリ\システム ツール |
  | Network | インターネット |
  | Game | ゲーム |
  | Graphics、Office、Development、Education、Science、Settings | グラフィックス、オフィス、開発、教育、科学、設定 |
  | なし | すべてのプログラムの直下 |

- 起動: ショートカットの先は `C:\xp\linux-apps\linux-launch.exe`（`session/linux-launch.c`、GUI のサブシステムなのでコンソールが
  開かない）。引数は Exec からファイルと URL のフィールドコードを除き、`%c`・`%k`・`%i` を展開した Windows の形のコマンドライン。
  `Terminal=true` は `x-terminal-emulator -e`（なければ `xterm -e`）で包む。`Path` は作業ディレクトリ（`Z:` の形）。
  linux-launch.exe は `CreateProcessW` で `/usr/bin/env -u WINEDEBUG PROGRAM ...` を起動する（Wine は Unix の実行ファイルを
  そのまま起動し、Windows の形の argv[0] は env が受け流す）。セッションのデバッグの設定は Linux のアプリに渡さない。
- アイコン: `Icon` をアイコンテーマ（`~/.config/gtk-3.0/settings.ini` のテーマとその継承、hicolor、`/usr/share/pixmaps`）から探し、
  GdkPixbuf で 16・32・48 の 32-bit の .ico（AND マスク付き）にして `C:\xp\linux-apps\icons` に置く。
- ショートカットは `session/linux-links.c`（`IShellLinkW`、`IPersistFile`）を Wine で1回起動して書く。そのとき
  `winemenubuilder.exe=d` にする（Wine の shell32 はショートカットを保存すると Linux のメニューの項目を作り、それをまた読むことになる）。
- 作ったショートカットと、作るために足したフォルダは `C:\xp\linux-apps\state.json` に記録する。変わらないものは書き直さず、
  消えたアプリのショートカットと、空になった自分のフォルダだけを消す。利用者や Windows のアプリのファイルには触れない。
- `--watch`: `applications` の各ディレクトリ（まだ無いものはその最も近い親）を inotify で見張り、変更が2秒止んでから同期する。
  パッケージの導入・削除でスタートメニューが追従する。XP Explorer は開いている間もフォルダの変更を読み直す。

## Windows キーとデスクトップのフォーカス（2026-10-11）

- WM は Super（Windows キー）をすべての修飾の組み合わせで、Ctrl+Esc を同期モードで横取りする。Wine のウィンドウにフォーカスが
  あるときはそのまま再送し（`ReplayKeyboard`）、Wine 自身が処理する。Linux のウィンドウにフォーカスがあるときは、Windows キーが
  離されるまでのキーを WM が受け取り（キーの横取りは離されるまで続く）、IPC ディレクトリの `shellkeys.tsv` に
  `番号 windows VK...`（文字・数字・F1〜F24・Tab・Pause・Space・Esc・Enter）または `番号 ctrl-escape` を書く。proxy はそれを
  `SendInput` で Wine に入れる。Wine の前面のスレッドが新しい入力として受けるので、XP のホットキー（Win+R、Win+E など、
  wineserver が照合する）も、スタートメニューのキー（Wine パッチ 0023）も、Win32 のウィンドウのときと同じに動く。Linux の
  アプリにはそのキーは届かない。
- Wine パッチ 0023: キーを受けたプロセスで、ほかのキーを挟まずに押して離した Windows キーと、Ctrl+Esc（Shift、Alt なし。Esc は
  届けない）に対し、`Win4WineShellHook` の属性のあるウィンドウ（XP のタスク切り替えの子ウィンドウ）へ前面を渡してから
  `HSHELL_TASKMAN` を送る。XP の Explorer はそれでスタートメニューを開く。
- WM は左と右のクリックでウィンドウを有効にする（XP と同じ。デスクトップの右クリックメニューもキーボードを受ける）。
  デスクトップ（Progman）とタスクバー（Shell_TrayWnd）をクリックすると、WM はそのウィンドウに X のフォーカスを渡し
  （そのイベントの時刻で WM_TAKE_FOCUS。Wine は前面のウィンドウがフォーカスを得た時刻より古い要求を無視する）、
  `_NET_ACTIVE_WINDOW` を 0 にする（アプリのタスクは無くなる）。タスクボタンでの最小化と復元は今までどおり。
