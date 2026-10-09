# 構成

`extract/extract.py SOURCE OUTPUT` はローカル ISO または XP Windows ディレクトリから選択したファイルのみを抽出する。ネットワーク処理はない。出力と出典・ハッシュは `.local/` に置く。フォント・アイコンの資源を再配布せず、DLL 内のアイコンもそのままローカルで利用する。

`prefix/create.py ASSETS PREFIX` は Wine が生成するクリーンな win32 レジストリに、winxp、ホームの H: マッピング、Personal フォルダ、クラシック表示を設定する。XP explorer は `C:\xp` に置き、builtin explorer を保存する。

`wine/build.sh` は upstream mirror の固定 commit を取得し、`wine/patches/series` を順に適用する。適用済みの判定を行う。Wine の修正には目的、対象 XP 動作、実装根拠・制約を書く。`.local/wine-upstream` の変更を直接の成果物とせず、必ずパッチに反映する。

`bridge-wm/wm.py` は単一 X11 screen の非 reparenting WM。`_WIN4WINE_CLASS` と `_WIN4WINE_HWND` を Wine X11 ドライバから読み、デスクトップ、トレイ、代理ウィンドウを識別する。通常の Wine ウィンドウを二重に代理化しない。desktop を下げ、dock を上げる。dock の strut または位置から `_NET_WORKAREA` を生成する。Alt+Tab、click to focus、EWMH activate/close/maximize、ICCCM iconic state を処理する。

`bridge-wm/proxy.c` は Wine 上の Win32 プロセス。Linux の通常ウィンドウに `Win4WineProxy` HWND を割り当てる。Win32 のシェルが GetWindowText、IsWindowVisible、IsIconic 等を参照できる。代理 HWND は `WS_OVERLAPPEDWINDOW` を持ち、その X ウィンドウを native クライアントの直下に表示して Win32 のタイトルバーと枠を使う。対応するクライアントがない代理は隠す。shell hook の登録は user32 パッチで HWND の property に保存し、仲介プロセスが SHELLHOOK メッセージを配信する。これは 200ms ポーリングの実装で、完全な Win32 hook 実装ではない。

## IPC v1 と枠の拡張

セッションごとの所有者のみアクセスできる 0700 ディレクトリを使う。共有の固定 `/tmp` ファイルや外部待受ソケットは使わない。

WM → proxy: `windows.tsv` を同一ディレクトリで atomic rename。行は `XID(hex) TAB minimized(0/1) TAB active(0/1) TAB UTF-8 title LF`。改行・tab はタイトルから除去する。最小化されたウィンドウは一覧に残す。破棄時に除く。Wine HWND と transient は含めない。

proxy → WM: `PID-SEQUENCE.cmd` を atomic rename。内容は `activate|minimize|close|maximize|restore XID(hex)` または `geometry XID(hex) x y width height`。WM は現在管理する Linux の通常ウィンドウだけを操作し、処理後削除する。proxy は最大 2048 ウィンドウまで。

枠の同期には次の 2 ファイルを追加する。どちらも同じ IPC ディレクトリで atomic rename し、数値を空白で区切る。

- WM → proxy の `geometry.tsv`: `XID(hex) x y width height maximized(0/1)`。座標とサイズは root 上の native クライアント領域。
- proxy → WM の `frames.tsv`: `HWND(hex) XID(hex) left right top bottom maximized(0/1) iconic(0/1)`。余白は公開 `GetWindowRect` / `GetClientRect` / `ClientToScreen` の測定値。

`_WIN4WINE_HWND` と `frames.tsv` の HWND を照合して枠と native クライアントを組にする。
WM は枠をクライアントのすぐ下へ積み、枠の未使用の Win32 client 領域を native の画面で覆う。
native ウィンドウの X border は 0 にし、測定した余白を `_NET_FRAME_EXTENTS` に公開する。初めて枠を付けるときは workarea の上端と左端を基準に位置を補正し、既定位置 0,0 で起動した xterm のタイトルバーも画面内に置く。
EWMH の一覧、アクティブ ID、キーボードフォーカスは native XID のまま。
代理 XID への EWMH activate も native のフォーカスに変換するので、タスクが二重にならない。
タイトルバーと角の操作は Win32 の通常の非クライアント処理を通し、移動・リサイズを
`geometry` コマンドで native へ戻す。最小化では組を隠し、復元では proxy の非 iconic 応答を待って枠を戻す。
withdraw / destroy 時は組を直ちに解除して残った枠を隠す。次の snapshot で代理 HWND も破棄される。

最大化は native の復元位置を WM に保存し、枠を含めて workarea に合わせる。
代理の `WS_MAXIMIZE` は Wine X11 driver の managed-window サイズ抑制を招くため変更せず、
公開 HWND property `Win4WineMaximized` で状態を伝える。非クライアント描画はこの property も参照して復元ボタンを選ぶ。
Wine の通常ウィンドウは代理化しない。`DV2ControlHost` は上に積む popup として扱い、
Alt+Tab、native snapshot、shell hook の通常タスク一覧から除く。popup には Wine の activation に必要な X フォーカスと WM_TAKE_FOCUS を与えるが、アクティブなアプリの EWMH ID は上書きしない。shell hook の WINDOWACTIVATED でも DV2ControlHost を通常タスクに通知しない。
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
