# 構成

`extract/extract.py SOURCE OUTPUT` はローカル ISO または XP Windows ディレクトリから選択したファイルのみを抽出する。ネットワーク処理はない。出力と出典・ハッシュは `.local/` に置く。フォント・アイコンの資源を再配布せず、DLL 内のアイコンもそのままローカルで利用する。

`prefix/create.py ASSETS PREFIX` は Wine が生成するクリーンな win32 レジストリに、winxp、ホームの H: マッピング、Personal フォルダ、クラシック表示を設定する。XP explorer は `C:\xp` に置き、builtin explorer を保存する。

`wine/build.sh` は upstream mirror の固定 commit を取得し、`wine/patches/series` を順に適用する。適用済みの判定を行う。Wine の修正には目的、対象 XP 動作、実装根拠・制約を書く。`.local/wine-upstream` の変更を直接の成果物とせず、必ずパッチに反映する。

`bridge-wm/wm.py` は単一 X11 screen の非 reparenting WM。`_WIN4WINE_CLASS` と `_WIN4WINE_HWND` を Wine X11 ドライバから読み、デスクトップ、トレイ、代理ウィンドウを識別する。通常の Wine ウィンドウを二重に代理化しない。desktop を下げ、dock を上げる。dock の strut または位置から `_NET_WORKAREA` を生成する。Alt+Tab、click to focus、EWMH activate/close/maximize、ICCCM iconic state を処理する。

`bridge-wm/proxy.c` は Wine 上の Win32 プロセス。Linux の通常ウィンドウに `Win4WineProxy` HWND を割り当てる。Win32 のシェルが GetWindowText、IsWindowVisible、IsIconic 等を参照できる。代理 X ウィンドウは WM が画面に出さない。shell hook の登録は user32 パッチで HWND の property に保存し、仲介プロセスが SHELLHOOK メッセージを配信する。これは 200ms ポーリングの実装で、完全な Win32 hook 実装ではない。

## IPC v1

セッションごとの所有者のみアクセスできる 0700 ディレクトリを使う。共有の固定 `/tmp` ファイルや外部待受ソケットは使わない。

WM → proxy: `windows.tsv` を同一ディレクトリで atomic rename。行は `XID(hex) TAB minimized(0/1) TAB active(0/1) TAB UTF-8 title LF`。改行・tab はタイトルから除去する。最小化されたウィンドウは一覧に残す。破棄時に除く。Wine HWND と transient は含めない。

proxy → WM: `PID-SEQUENCE.cmd` を atomic rename。内容は `activate|minimize|close XID(hex)`。WM は現在管理する Linux の通常ウィンドウだけを操作し、処理後削除する。proxy は最大 2048 ウィンドウまで。

タイトル変更、破棄、生成、アクティブ変更は SHELLHOOK に変換する。短命ウィンドウはポーリング間で消えることがある。`WH_SHELL` DLL injection は実装しない。購読先の子ウィンドウも検索し、実 XP の MSTaskSwWClass への通知を確認した。

## セッション

ディスプレイマネージャが Xorg と認証を起動し、`session/supervisor.py` が WM → proxy → XP explorer を起動する。WM 所有権取得に失敗したらセッションを中断する。トレイの存在を監視し、起動失敗・消失・proxy 終了時に専用 wineserver を終了して再試行する。120 秒以内に 3 回失敗すると native override と外部 shell フラグを解除し、独立した recovery prefix の Wine builtin ホームブラウザと復旧用 xterm を起動する。セッション終了時に子プロセス・専用 wineserver・IPC を掃除する。

Win7 対応時も WM と IPC を維持し、DLL profile、shell readiness class、必要な Wine shim を変更する方針。
