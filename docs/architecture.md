# 構成

`extract/extract.py SOURCE OUTPUT` はローカル ISO または XP Windows ディレクトリから選択したファイルのみを抽出する。ネットワーク処理はない。出力と出典・ハッシュは `.local/` に置く。フォント・アイコンの資源を再配布せず、DLL 内のアイコンもそのままローカルで利用する。

`prefix/create.py ASSETS PREFIX` は Wine が生成するクリーンな win32 レジストリに、winxp、ホームの H: マッピング、Personal フォルダ、クラシック表示を設定する。XP explorer は `C:\xp` に置き、builtin explorer を保存する。

`wine/build.sh` は upstream mirror の固定 commit を取得し、`wine/patches/series` を順に適用する。適用済みの判定を行う。Wine の修正には目的、対象 XP 動作、実装根拠・制約を書く。`.local/wine-upstream` の変更を直接の成果物とせず、必ずパッチに反映する。

`bridge-wm/wm.py` は単一 X11 screen の非 reparenting WM。`_WIN4WINE_CLASS` と `_WIN4WINE_HWND` を Wine X11 ドライバから読み、デスクトップ、トレイ、代理ウィンドウを識別する。通常の Wine ウィンドウを二重に代理化しない。desktop を下げ、dock を上げる。dock の strut または位置から `_NET_WORKAREA` を生成する。Alt+Tab、click to focus、EWMH activate/close/maximize、ICCCM iconic state を処理する。

`bridge-wm/proxy.c` は Wine 上の Win32 プロセス。Linux の通常ウィンドウに `Win4WineProxy` HWND を割り当てる。Win32 のシェルが GetWindowText、IsWindowVisible、IsIconic 等を参照できる。代理 HWND は `WS_OVERLAPPEDWINDOW` を持ち、その X ウィンドウを native クライアントの直下に表示して Win32 のタイトルバーと枠を使う。対応するクライアントがない代理は隠す。shell hook の登録は user32 パッチで HWND の property に保存し、仲介プロセスが SHELLHOOK メッセージを配信する。これは 200ms ポーリングの実装で、完全な Win32 hook 実装ではない。

仲介プロセスは、表示済みの `Shell_TrayWnd` と Start/ReBar 子ウィンドウの準備が二回の巡回で安定した後、現在の親 client サイズを使った公開 `WM_SIZE` を一度だけ送る。起動直後に保持された子の ideal 高さを、シェル自身の通常レイアウトで親に合わせ直すためで、子の寸法や字体は指定しない。応答待ちは最大 500ms。Tray HWND/PID の置換・消失で状態をリセットし、一時的な非表示では再送しない。Luna と Classic に適用し、`BCM_GETIDEALSIZE` の測定値は変更しない。

## IPC v1 と枠の拡張

セッションごとの所有者のみアクセスできる 0700 ディレクトリを使う。共有の固定 `/tmp` ファイルや外部待受ソケットは使わない。

WM → proxy: `windows.tsv` を同一ディレクトリで atomic rename。行は `XID(hex) TAB minimized(0/1) TAB active(0/1) TAB UTF-8 title LF`。改行・tab はタイトルから除去する。最小化されたウィンドウは一覧に残す。破棄時に除く。Wine HWND と transient は含めない。

proxy → WM: `PID-SEQUENCE.cmd` を atomic rename。内容は `activate|minimize|close|maximize|restore XID(hex)` または `geometry XID(hex) x y width height`。WM は現在管理する Linux の通常ウィンドウだけを操作し、処理後削除する。proxy は最大 2048 ウィンドウまで。

枠の同期には次の 2 ファイルを追加する。どちらも同じ IPC ディレクトリで atomic rename し、数値を空白で区切る。

- WM → proxy の `geometry.tsv`: `XID(hex) x y width height maximized(0/1) workX workY workWidth workHeight`。座標とサイズは root 上の native クライアント領域。末尾は現在の作業領域。
- proxy → WM の `frames.tsv`: `HWND(hex) XID(hex) left right top bottom maximized(0/1) iconic(0/1) borderX borderY`。余白は公開 `GetWindowRect` / `GetClientRect` / `ClientToScreen`、borderは`GetWindowInfo`の測定値。

`_WIN4WINE_HWND` と `frames.tsv` の HWND を照合して枠と native クライアントを組にする。
WM は枠をクライアントのすぐ下へ積み、枠の未使用の Win32 client 領域を native の画面で覆う。
native ウィンドウの X border は 0 にし、測定した余白を `_NET_FRAME_EXTENTS` に公開する。初めて枠を付けるときは workarea の上端と左端を基準に位置を補正し、既定位置 0,0 で起動した xterm のタイトルバーも画面内に置く。
EWMH の一覧、アクティブ ID、キーボードフォーカスは native XID のまま。
アイコンは native の `_NET_WM_ICON` を優先し、なければ ICCCM `WM_HINTS` の pixmap / mask を読む。
property は最大1 MiB、画像は256×256までとし、manage と icon / hints property 変更時だけ取得する。
`icon-XID.argb` は `W4I1`、little-endian の32bit width / height、行順の straight ARGB32画素からなる。
所有者専用 IPC へ atomic rename で公開し、proxy は更新されたファイルだけ読み、公開 Win32 API で HICON を作る。
small / big の WM_SETICON に同じ HICON を設定し、交換・ウィンドウ破棄時に所有した HICON を一度破棄する。
アイコンがない・壊れている場合は Wine builtin の共有 `IDI_APPLICATION` に戻す。共有アイコンは破棄しない。
Wine の CreateIconIndirect は alpha 用 bitmap を内部で premultiply するため、bridge では二重変換しない。
Wine 0025 は外部シェル opt-in 時だけ、別プロセス間の公開 `WM_GETICON` を許可する。
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
0025 は server protocol を変更するため、wineserver / ntdll / win32u を揃えて配備し、既存 prefix server を停止して切り替える。

代理 XID への EWMH activate も native のフォーカスに変換するので、タスクが二重にならない。
タイトルバーと角の操作は Win32 の通常の非クライアント処理を通し、移動・リサイズを
`geometry` コマンドで native へ戻す。最小化では組を隠し、復元では proxy の非 iconic 応答を待って枠を戻す。
withdraw / destroy 時は組を直ちに解除して残った枠を隠す。真の withdraw では WM_STATE property を削除して WithdrawnState を通知する。Wine X11 driver はこの応答を待って次の XMapWindow を発行するため、応答がないと Start の Win32 HWND は visible でも X のメニューが再表示されない。最小化された client と WM が隠した代理枠にはこの削除を行わない。次の snapshot で不要な代理 HWND も破棄される。

最大化はnativeの復元位置をWMに保存する。proxyは公開WM_GETMINMAXINFOで作業領域と
border幅を返し、ShowWindow(SW_MAXIMIZE/RESTORE)で本物のWS_MAXIMIZEを切り替える。
外側borderは画面外へ出し、Luna/96 DPIの可視余白は左右下0、上26pxになる。
Win4WineMaximized propertyも既存の非クライアント描画との互換用に維持する。

Wineの通常ウィンドウは代理化しない。0041は外部shellに限り、managed最大化windowの
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
Win32 HWND と X11 の内容は別ウィンドウで、200ms の同期前後には枠と内容の移動に時間差があり得る。
native transient は snapshot の対象外なので、この経路で新たな代理枠を作らない。
アプリの独自システムメニューや追加 caption button を native 側へ移植する処理もない。
クリップボードの変換は Wine X11 driver の既存機能に任せ、proxy は独自の clipboard IPC を持たない。
Microsoft の画像を Python で模写・再配布する方式ではなく、ローカルのテーマを Wine の公開描画 API で使用する。

タイトル変更、破棄、生成、アクティブ変更は SHELLHOOK に変換する。
通常の Wine HWND も前回のタイトルを比較し、生成後に付いたフォルダ名や移動後の名前を通知する。短命ウィンドウはポーリング間で消えることがある。`WH_SHELL` DLL injection は実装しない。購読先の子ウィンドウも検索し、実 XP の MSTaskSwWClass への通知を確認した。

## セッション

ディスプレイマネージャが Xorg と認証を起動し、`session/supervisor.py` が WM → proxy → XP explorer を起動する。WM 所有権取得に失敗したらセッションを中断する。トレイの存在を監視し、起動失敗・消失・proxy 終了時に専用 wineserver を終了して再試行する。120 秒以内に 3 回失敗すると native override と外部 shell フラグを解除し、独立した recovery prefix の Wine builtin ホームブラウザと復旧用 xterm を起動する。セッション終了時に子プロセス・専用 wineserver・IPC を掃除する。

Win7 対応時も WM と IPC を維持し、DLL profile、shell readiness class、必要な Wine shim を変更する方針。

WM は native X クライアントのフォーカス・サイズ変更に合わせて、表示中の子孫へ Expose を送る。
描画内容は消去しない。Wine HWND のあるクライアントは Wine の surface 管理に任せる。
ポーリングによる周期的な再描画ではない。ConfigureRequest には root 座標の
synthetic ConfigureNotify も返す。

XP のタスクボタンが他プロセスの最小化可否を調べるため、Wine 0010 は外部シェル opt-in 時に
標準システムメニューのプロセス内コピーを返す。アプリ独自の変更は同期しない限定 shim。
0011 は名前付き child ReBar の未指定 alignment を CCS_TOP に正規化し、フォルダの初期ずれを防ぐ。
