# 検証状況

2026-10-09。Ubuntu 26.04 ホスト上の Debian 13 chroot、Xvfb 1280×800、
ローカルの日本語 XP SP3 x86 ISO、ソース版 Wine 10.0 で試験。
ホスト自体を対象 OS の検証結果に含めない。

| 段階 | 状態 | 証拠 / 残件 |
|---|---|---|
| M0 | 基本経路を実測 | Debian 13 のソースビルド成功、ISO から20ファイル抽出、新規 prefix 生成成功。ログ `.local/prefix-m0-create.log` |
| M1 | 新規 prefix で実測 | XP デスクトップ、クラシック Start / Programs、メニューから起動した Wine メモ帳のタスクボタン。画像 `.local/final-m1.png` |
| M2 | 新規 prefix で実測 | マイドキュメントから H: の Linux ホームを表示し、サブフォルダへ移動。追加修正でメニュー・ツールバー・アドレス欄を表示。新規 prefix の `.local/display-clean-folder.png`、VM の `.local/vm-test/display-fixed-folder.png` |
| M3 | 実 XP と Linux xterm で実測 | Start メニューから起動、Alt+Tab、タスクバー右クリックの最小化・閉じる、左クリック最小化・復元を確認。ログ `.local/final-m3-native-operations.log` / `final-m3-alt-tab.log`、左クリックは `.local/vm-test/task-leftclick-test.log` |
| M4 | 独立 Debian 13 VM で実測 | kernel 6.12.107、Xorg、LightDM の xp-shell セッションでログイン。強制終了 2 回から再起動、3 回目で独立 builtin prefix + xterm へ復旧。画像 `.local/vm-test/login.png` / `recovery.png` |
| M5 | Luna の主要部を実測、残件あり | ローカル msstyles の Start ボタン・タスクバー・通常 Start・ウィンドウ枠を描画。96/144/192 DPI のキャプションボタン寸法・クリック領域を検証。時計・ReBar・無効矢印を追加修正。端の区切りなど細部は残る |

## 自動試験

- `tests/extraction_integration.py`: 利用者の抽出済みファイルをローカル fixture として使い、抽出・hash・上書き拒否を確認。
- `tests/wm_integration.py`: 作業領域、Alt+Tab / 逆順、最小化、最後の1枚の最小化、復元、終了要求。
- `tests/bridge_integration.py`: 子ウィンドウでの shell-hook 購読、X→代理 HWND、最小化・復元・フォーカス・名前変更・終了・破棄の往復。
- `tests/recovery_integration.py`: XP の強制終了・再起動・独立 builtin ブラウザと端末の実表示。Debian VM で成功。
- `tests/display.c`: system DPI / ThemeActive / ローカル Luna の公開 API を確認。
- `tests/system-menu.c`: 別プロセスの標準システムメニュー、opt-in の有無、スタイル変更、キャッシュ・revert・破棄。
- `tests/rebar.c`: 利用者の native v6 ReBar の初期位置・バンド配置、明示 CCS_NOMOVEY の維持。
- `tests/xterm_redraw.py`: 端末の文字バッファを残して画面だけ消し、入力なしでフォーカス・サイズ変更後の復帰を検査。
- `tests/button-size.c`: 標準 Button の preferred size が有効、NULL 出力を拒否、選択フォントを反映。

試験用 PE は `tests/build.sh` で `.local/bridge/` に生成する。
スクリーンショットや生ログは Microsoft のリソースを含み得るため `.local/` にのみ保存する。

## 既知の制限

- 通常 XP Start の黒い領域は解消。新規ユーザーの左側の利用履歴は空。利用者による固定・起動・再起動後の保持・解除は確認済みで、利用頻度による自動一覧の蓄積は未検証。既定メニューは引き続き classic、`--menu xp` で通常形式を選択できる。
- Luna の時計・ReBar のグリップ・スクロールバー矢印を追加修正。Start 脇と通知領域脇の細い区切りには classic の表示が残る。完全一致とはしない。
- 外部プロセスのシステムメニュー shim は標準メニューのコピー。アプリ独自の項目や無効化は反映しない。
- Linux ウィンドウには Wine が描く本物の非クライアント枠を付ける。単一 X11 screen のみ検証。
- 実 SOFTWARE/SYSTEM ハイブの変換、Ubuntu 24.04、Wayland/Xwayland は未検証。

## 独立 VM の検証

`.local/vm-test/login-environment.log` に LightDM の `DESKTOP_SESSION=xp-shell`、
`XDG_SESSION_TYPE=x11` と kernel 6.12.107 を記録。
`.local/vm-test/recovery-test.log` は実 XP を taskkill した各 attempt と fallback の状態。
ホストのディスプレイマネージャには導入していない。VM のネットワークは無効。

HiDPI 画像: `.local/vm-test/classic-144-menu.png`、`classic-192-menu.png`、
`classic-192-programs-click.png`、`classic-192-linux.png`。
フォルダのメニューバー・アドレス欄は今回 96 DPI で修正を実測。
修正後の 144/192 DPI の画面は未再試験。
XP は DPI 非対応として Wine が拡大するため、文字やアイコンにぼけがある。
Linux アプリのフォントサイズはアプリ側の設定に従う。
Luna: `.local/vm-test/theme-probe-96.log`、`luna-144.png`。

Wine 更新の再試験: timestamp 不一致を作り、更新・再登録後に XP Shell / Start を確認。
`.local/vm-test/wine-update-result.log` と `wine-update-final.png`。

公開 API 検査も VM で成功: `display-144-test.log` (144 / classic)、
`recovery-integration.log` (192 / classic + 復旧)、`display-luna-144-test.log` (144 / Luna API)。
パッチ9本は固定 upstream commit の別 worktree への順次適用も成功 (`.local/patch-series-check.log`)。

## 修正前の稼働状態

VM を通常の classic / 96 DPI に戻して再起動し、LightDM から attempt 1 で起動。
メニューから Wine メモ帳と Linux xterm、デスクトップから Linux ホームを開いた。
最新画像 `.local/vm-test/final-desktop.png`、再起動ログ `reboot-result.log`。
この作業環境では VM の VNC を `127.0.0.1:5905` で利用できる。ゲストの NIC は引き続きない。
再起動用の実測コマンドは `.local/boot-guest.sh`。再作成は [VM 手順](vm.md) を参照。

追加の制限: VM の std VGA で、サイズ変更・フォーカス変更後に xterm の表示内容が
再描画まで黒く見えることを観測した。フォーカスを戻して入力すると再表示される。
今回の再描画要求で同種の残像から入力なしで復帰する試験は成功。
VGA 経路でイベントが不足する元の原因までは特定していない。

## 前回の表示修正の追加検証（2026-10-09、d162a5b）

- 同じローカル ISO から `browselc.dll` / `shdoclc.dll` を追加抽出（当時は計22ファイル）。
  `.local/extract-display.log`、`extraction-display-test.log`。Microsoft ファイルは無改変。
- 新規 `.local/prefix-display-clean` でクラシック 96 DPI のフォルダを表示。
  メニュー、ツールバーの文字、アドレス欄の表示を確認。画像 `display-clean-folder.png`。
  既存 prefix の registry profile も version 2 に更新し、再 stage が何もしないことを確認。
- 0010 により XP タスクバーの GetSystemMenu → SC_MINIMIZE の確認が通る。
  VM の xterm / Wine メモ帳で、実ボタンをクリックして Iconic → Normal の往復を確認。
  `.local/vm-test/task-leftclick-test.log`。
- 0011 で native ReBar の暗黙の配置を明示 CCS_TOP にする。
  フォルダ親領域と ReBar の上端が一致し、下のアドレス欄が隠れない。
  VM でサブフォルダへ移動し Alt+Left で戻る。`folder-navigation-test.log`。
- WM は native X11 クライアントのフォーカス・サイズ変更で描画用子ウィンドウへ Expose を送る。
  内容を消去せず、入力も注入しない。通常の ConfigureRequest に synthetic ConfigureNotify も返す。
  Xvfb の WM / bridge 試験成功（`.local/display-wm-tests.log`）。
  xterm の入力なし復帰試験は VM でも成功（`.local/vm-test/xterm-redraw-test.log`）。
- 公開 API の system-menu / button-size / display 試験成功（`.local/display-public-api-tests.log`）。
  native ReBar 試験成功（`.local/rebar-test.log`、VM の `folder-navigation-test.log`）。
  固定 upstream の新しい worktree に11本を順次適用成功（`.local/display-patch-series-check.log`）。
- 新規 prefix の強制終了・再起動上限・独立 builtin ブラウザと端末への復旧も成功。
  初回の registry 保存は画面表示後になるため、試験は保存完了を待つ。
  `.local/display-recovery-test.log`。
- 通常 XP Start は公開 SHGetSetSettings / SSF_STARTPANELON で選択・読戻しを確認。
  `.local/startpanel-cache-registered.png` / `luna-normal-menu.png` に残る黒い領域を記録。
  native v5 はアイコン等が欠落し、native v6 Button の SxS 選択は例外が再発したため採用しない。
  `--menu xp` は部分対応を調べるための選択肢であり、描画の合格ではない。

前回修正終了時の VM はネットワークなし、classic / 96 DPI、クラシック Start で稼働。
VNC は引き続き `127.0.0.1:5905`。MS 由来の表示・生ログは `.local/` のみ。

## 通常 Start・Luna・Linux 枠の追加検証（2026-10-09）

- 同じ ISO から Marlett と Trebuchet MS の4書体を追加抽出。計27ファイル、フォント8ファイル。
  `.local/assets-luna` / `extract-luna.log`。抽出試験で各フォントが入力と同一であることも確認。
- 通常 Start の WM_PRINT キャプチャに子・所有ウィンドウを合成する（0012）。
  translated DC のコントロール clipping を修正（0013/0017）、NULL DC のテーマフォントも修正（0015）。
  新規 prefix の `luna-final-programs-fixed.png`、VM の `luna-normal-programs.png` で
  ユーザー欄・一覧・下部・すべてのプログラムを確認。実メニューから Linux Terminal が起動する。
- native XP common controls は維持し、Start ボタンだけ別名の自作 Wine control で描く（0013）。
  Windows ロゴの image list は native comctl32 の公開 API で描画する。native Button の SxS は採用しない。
- Luna のタスクバー背景を利用者のテーマから描画（0016）。公開試験で clipping と DC の状態保持を確認。
- Window テーマの画像寸法をボタン描画と WM_NCHITTEST で共有（0014）。
  96 DPI は21×21px・間隔2px、144 DPI は32×32px・間隔3px、192 DPI は42×42px・間隔4px。
  最小化・最大化が無効な領域は HTCAPTION。通常・hover・押下の実表示も確認。
  `.local/luna-nonclient-test.log` / `luna-nonclient-144.log` / `luna-nonclient-192.log`、
  `luna-min-pressed-final.png`。タイトル文字色もテーマの色 ID で読む。
- Linux クライアントと Wine の枠は X root の兄弟で、枠を直下に配置する。
  native 側のタスク ID とキーボード入力を維持し、caption の移動・サイズ変更・最大化・終了を bridge する。
  新規 prefix の `luna-final-linux-launched.png` で通常 Start から起動した xterm の枠を確認。
- DV2ControlHost は実フォーカスを受け取る popup とし、アプリの active ID や shell-hook のタスクにしない。
  実メニューの開閉・All Programs・起動を確認し、余計な「スタート メニュー」タスクを除去。
- 新規 prefix の `.local/luna-final-public-tests.log` で WM_PRINT / theme font / NC bounds /
  taskbar background が成功。VM の `.local/vm-test/luna-final-integration.log` でも公開 API と
  入力なしの xterm 復帰は成功。native 枠の試験は `.local/vm-test/frame-unique-command-test.log` で成功。
  最初の失敗は試験が IPC ファイル名を再利用し、未処理の restore を上書きしたためだった。
  試験を実 bridge と同じ一意なファイル名に直し、原因を確認して再検証した。

- テーマ適用のフォントサイズは point の正値から LOGFONT の負の pixel 高さへ変換（0018）。
  6種の system font を classic/Luna の2往復で検査し、96/144/192 DPI で成功。
  `.local/theme-applied-font-before-test.log` は修正前の12件失敗、
  `theme-applied-font-after-test.log` は修正後。DPI 丸めは標準 SPI の96 DPI基準に従う。
- 重なった Wine パッチは一時コピー上で逆順に戻して適用済みを判定する。
  `tests/patch_state.py` は新規・11本適用済み・全18本・17→18本・利用者の編集保持・
  競合時の無変更を確認。Wine ソースの reset は行わない。

今回の VM は Luna / 96 DPI / 通常 XP Start へ変更し、LightDM から attempt 1 で起動。
最終18パッチでの枠操作は `luna-final-18-tests.log`、入力なしの xterm 復帰と公開 API は
`luna-final-18-redraw-public.log`、実 system font は `luna-final-font-metrics.log` に記録。
VNC は引き続き `127.0.0.1:5905`、NIC はない。

スクリーンショット・テーマ・DLL・フォントはすべて `.local/` にのみ保存する。
Microsoft バイナリを変更・解析・配布する修正はない。

## キャプション寸法と Start 再表示の追補

- 新規 prefix は registry の ThemeActive だけを変更せず、Wine の classic 適用を呼ぶ。
  旧手順では Luna の caption height 25 が残り、classic の枠ボタンまで縦長になっていた。
  新規作成と Luna → classic の往復で caption が18×18、Tahoma -11 に戻ることを公開 SPI で確認。
  Wine の classic 描画の内側は16×14。XP 実機での寸法測定とは区別する。
  `.local/classic-fresh-caption-test.log`。既存 prefix の修復は再現手順を参照。
- Start を閉じた際の ICCCM withdrawal 応答がなかったため、Wine は次の XMapWindow を保留した。
  WM は真の withdraw で WM_STATE を削除し、最小化や代理枠の非表示と区別する。
  `.local/frame-withdraw-ack-interaction.log` と VM の `luna-start-repeat-final.log` で
  実 Start の反復開閉を確認（VM は5往復）。フォルダの起動・Alpha への移動・Alt+Left も成功。
  native 枠の試験にも property 削除の検査を追加した。

## 実 XP との比較が必要な表示差（利用者指摘）

2026-10-09 の追加指摘。以下は最終的な調整対象で、テーマ API と一致したことだけでは
実 XP の表示一致としない。利用者はローカル ISO から別の XP 検証 VM を作り比較することを許可した。
ネットワークなし・ホストの表示環境を変更しない条件と、Microsoft 資源を Git に含めない方針を維持する。

- 閉じる等のキャプションボタンが大きく見える。実 XP の外寸・余白と比較する。
- タイトルバーの書体、およびウィンドウ内・タスクバーの文字サイズ。
- Start ラベルの斜体。
- ウィンドウ左上・右上の白い部分。
- 非アクティブ時にキャプションボタンが濃いまま残る。
- 左上のアイコンがないウィンドウの見せ方。Linux アプリのアイコン取得・適切な既定値も検討する。

実 XP 比較と修正の進捗は以下の追補を参照。配置・余白などの完全一致は未完了。

## 時計・グリップ・アイコンの追加検証

- registry profile version 4 で My Computer のアイコンと公開 Pin handler を登録。
  既存 profile の移行・再 stage の no-op、および固定・起動・再起動後の保持・解除を確認。
- 0019 は元の時計の WM_PAINT による時刻整形・更新を保ち、実テーマ背景と白文字を補描する。
  Wine 上の native XP Explorer で 12:37 → 12:39 の自然更新を確認。
  `.local/clock-real-paint-before.log` / `clock-real-paint-after.log`。
- 0020 はスクロールバーの独立した無効矢印をテーマに反映する。20種類の実表示比較が一致。
- 0021 は native ReBar の9px header のグリップをテーマで描く。元の入力処理・戻り値を維持。
  print DC・描画中の破棄・別プロセスからの SetWindowTheme も確認。
- 最終21パッチの source 整合性と patch-state の6試験が成功。
  `.local/luna-details-patch-state-tests.log`。
- 独立 Debian VM は registry version 4 / Luna / 96 DPI / 通常 XP Start で attempt 1 起動。
  WM_PRINT・theme font・caption bounds・taskbar background・clock・scrollbar・ReBar の公開 API 試験は
  `.local/vm-test/luna-details-public-tests.log` で全成功。実 Start 5往復も成功
  (`luna-details-start-repeat.log`)。枠操作と入力なしの xterm 復帰は
  `luna-details-frame-redraw.log` で成功。

ここでの native XP Explorer の確認は Wine 上の試験であり、実 XP OS との比較ではない。
この時点では利用者指摘の6点と、タスクバー両端の細い classic 区切りが残件だった。

## 非アクティブ枠・角・フォント・Linux アイコンの追加検証

- 0022 は公開テーマ API の8状態を使い、非アクティブ時のキャプションボタンを切り替える。
  4状態のテーマには従来の状態を維持する。
- 0023 は公開 GetThemeBackgroundRegion に基づく丸い上角を適用する。
  アプリが指定した region を保持し、自前で管理する region だけを更新・解除する。
  非アクティブ描画・resize・最大化・classic 切替・破棄を96/144/192 DPIで検証。
  `.local/nc-region-after.log`、`nc-region-144.log`、`nc-region-192.log`。
- 0024 は Wine のフォント属性の空白処理と複数属性の認識を修正する。
  caption の Bold、Start の Italic が欠落していた。自作7入力と境界外参照の検査が成功。
  Luna 再適用後は caption weight 700、Start italic 1 を公開 API で確認。
  `.local/font-parser-regression.log`、`shell-font-probe-after0024.log`。
- Linux アプリの EWMH / ICCCM アイコンを代理 HWND の本物の HICON に反映する。
  画像のサイズを制限し、更新・削除・不正な画像・既定アイコン・半透明を検証。
  classic / Luna の実キャプション画像でも確認。この時点ではタスクバーの別プロセス HICON 参照は未対応だった（0025で対応）。
  `.local/native-icons-final-test.log`、`native-icons-classic-final-test.log`。
- 独立 Debian VM の24パッチで、8種類の公開 API 試験と Linux 枠の操作・アイコン更新、
  入力なしの xterm 再描画試験が成功。
  `.local/vm-test/luna-fidelity-public-tests.log`、`luna-fidelity-frames-redraw.log`。

## 日本語 XP OS での初回比較

利用者の ISO からネットワークなしの別 VM を新規作成し、デスクトップまで到達。
利用者が入力を許可したキーはセットアップで受理された。キー値は保存資料に記載しない。
VNC は `127.0.0.1:5906`。手順は [実 XP の比較](xp-reference.md)。

自作 `tests/reference-metrics.c` を実 XP で実行し、96 DPI の公開 API を測定した。
実 XP の本文・メニュー・アイコンは MS UI Gothic -12、caption は同 -13 bold、
Shell_TrayWnd 子 Button は同 -13 bold（Start の可能性があり、アプリのタスク字体とは断定しない）。一方、現在の Wine では本文 Tahoma -11、caption Trebuchet MS -13 bold。
この初回比較時点の日本語字体差は、下記0026の明示 profile で改善した。
close のテーマ画像は両環境とも21×21・8状態。実際の配置とクリック領域には差があるため、
画像寸法だけでボタンの見た目が一致したとは判断しない。
生ログは `.local/xp-reference/serial.log`、Wine 側は `.local/reference-wine-24.txt`。
上角・非アクティブ・Start 斜体・Linux の左上アイコンは改善したが、実 XP との最終比較は継続する。

## 外部プロセスのアイコン API と実 XP 比較の追補

- 0025 は外部シェル opt-in の別プロセス WM_GETICON / HICON 読み取りを実装。
  元の handle の所有権を保ち、server にサイズ・個数を制限した不変画素を保存する。
  自作画素による2プロセス試験で半透明・白黒・CopyIcon・foreign 破棄拒否・80個の cache・
  handle 再利用・owner 終了時の解放・通常 Wine の opt-out が成功。
  `.local/foreign-icons-enabled.log`、`foreign-icons-disabled.log`、`foreign-icons-owner-cleanup.log`。
- 別ディレクトリへの完全 build/install と native 枠の操作試験は成功。
  server protocol を変更するため、wineserver / ntdll / win32u を一組で更新する。
  24パッチの既存 Debian VM はこの追補時点ではまだ切り替えていない。
- Wine 上の XP タスクバーでも、非同期 callback 後の実画素が固有アイコンと一致した。
  初期の赤16×16と更新後の緑16×16を bounded wait で確認。Luna / classic とも成功。
  `.local/native-icons-taskbar-0025-pixels.log` と `native-icons-taskbar-0025-classic.png`。
- 実 XP のインストール済み Luna と抽出済み Luna の SHA256 は一致した。
  自作多言語 fixture の言語選択、SPI 変更時のテーマ字体保持、公開 property origin も両環境で一致。
  真の日本語 locale と、Wine のテーマ読込みを日本語優先にする限定実験でも字体差は解消しなかった。
  根拠を得られなかった言語選択の変更は採用しない。
- 実 XP の Start テーマ字体は MS UI Gothic -16・bold・italic。
  取得には自作 Button の SetWindowTheme(Start) → OpenThemeData(Button) を使った。
  上記の Shell_TrayWnd 子 Button の WM_GETFONT は、アプリのタスクボタンの字体と断定しない。
- 0025 の追加試験で、初回の foreign GetIconInfo が GDI 初期化を USER lock 内で行う不具合を発見。
  初期化・画像 import を lock 外へ移し、再取得時の owner/generation を検証する。
  callback-first・SendMessageTimeout/Callback・並行読み取りと native XP ImageList の公開 API 試験が成功。
  `.local/foreign-icons-native-comctl.log`。この修正前の25版は Debian VM へ配置していない。


## 日本語フォント profile と最終26パッチの検証

- 0026 は利用者の日本語 XP の公開 API 実測値を prefix の明示設定として使用する。
  Luna の caption は MS UI Gothic -13 bold、本文・メニュー・アイコンは同 -12、
  Start は同 -16 bold italic（96 DPI）。Microsoft 資源やコードの改変はない。
- registry version 5 の移行は既存の profile 値を上書きせず、Classic baseline も変更しない。
  新規 prefix だけ Classic の6字体を実 XP の MS UI Gothic -12 に初期化する。
  新規・既存独自字体の別プロセスでの読み取りと Luna 往復、移行 no-op は成功。
  `.local/fresh-japanese-classic-public-test.log`、`existing-classic-custom-preservation-test.log`、
  `registry-v5-preservation-test.log`。profile の96/144/192 DPI 試験も成功。
- 独立 Debian VM は完全な26パッチの runtime に切り替えた。
  テーマ字体・キャプション・タスクバー背景・時計・矢印・ReBar・WM_PRINT・display・字体往復は成功。
  Luna 再適用・LightDM 再起動後の公開測定でも上記7字体が一致した。
  VM 内の `/work/.local/vm-reference-26-live.txt` に記録。
- VM の実 Linux 枠操作・タスクバーのアイコン更新、入力なしの xterm 再描画、
  通常 Start の実クリック5往復が成功。フォルダのメニュー・ツールバー・アドレス欄も実表示。
  `.local/vm-test/final-26-start.png`、`final-26-folder.png`。
- 検証 VM の ext4 整合性エラーは停止状態で e2fsck により修復し、再検査も正常だった。
  ホストと実 XP のディスクは対象にしていない。`.local/vm-test/fsck-26.log`。

残件: Luna ボタンの配置・余白・クリック領域には実 XP との差がある。
Start/通知領域脇の細い区切りも完全一致ではない。実 XP の144/192 DPI は未測定で、
高 DPI の profile 試験は96 DPI 実測値からの拡大規則の検査である。


## 実 XP のキャプション画像と入力余白（0027）

公開 `GetWindowDC` の自作ウィンドウ画像と `DrawThemeBackground` の出力を比較し、
ボタン画像の位置を測定した。テーマ内部の読み取り・資源出力はしていない。
500px幅、96 DPI の通常枠で、実 XP / 修正後 Wine はいずれも次の値になった。

| ボタン | 画像の矩形 | WM_NCHITTEST の矩形 |
|---|---|---|
| 最小化 | 427,6..448,27 | 426,4..448,29 |
| 最大化 | 450,6..471,27 | 450,4..472,29 |
| 閉じる | 473,6..494,27 | 472,4..496,29 |

従来の画像は1px右・1px上で、入力は21px角だけだった。0027 は画像寸法を維持し、
画像の配置と入力余白を分ける。固定サイズ枠も実 XP と一致した（画像y5..26、入力y3..28）。
`SM_CYCAPTION` は caption の生メトリックより1px大きいので、境界を別途扱う。
実測ログは `.local/xp-reference/serial.log`、VM 内の
`/work/.local/button-pixels-wine27-final.txt` / `button-pixels-wine27-fixed.txt`。
公開描画の半透明な角を重ねて比較するため、最適位置の不一致16画素は両環境で共通だった。
生画像の完全一致という意味ではない。

96 DPI の VM で、独立した400pxの実 XP fixture、active/inactive/disabled画素、
角の形・所有権・resize・最大化・Classic切替・破棄を検証して成功。
144/192 DPIも同じ回帰試験に成功（`.local/nc27-144.log` / `nc27-192.log`）。
高 DPI の位置は実 XP 測定ではなく、拡大規則と実 Wine metrics の整合性検査である。

自作 `shell-layout` probe では、実 XP の左アイコンが6,8..22,24、Wine が6,9..22,25。
タイトル文字の実画素にも小差がある。通知側 ReBar の子の後方4pxは実 XP で青、
Wine で灰色と確認できたため、これらは次の調整対象。


0028 は左アイコンの描画高さから `SM_CYCAPTION` の1px境界を除き、
同じ自作単色アイコンが実 XP と Wine の両方で6,8..22,24の256画素になった。
システムメニュー入力も実 XP の4,4..23,29へ合わせ、右側をタイトル移動に戻す。
VMの `shell-layout-28-test --verify-xp96-icon` は独立fixtureで成功。
タイトル文字の配置・影の比較と、タスクバー隙間の調整は継続中。


0029 は task ReBar の子ウィンドウ後方に残る未使用領域を、公開 parent background で描く。
実 XP では同じ4pxが青いグラデーション、従来 Wine では灰色だった。
固定4pxや色を埋め込まず、公開 child/band 矩形から末尾を算出する。
locked/unlocked、横/縦、chevron/child-edge/hidden除外、translated/clipped DC、
子画素・native入力の保持が独立環境と Debian VM で成功。
`.local/rebar-tail-test.log`、`rebar-tail-regression.log`、
VM内 `/work/.local/rebar-tail-29.log`。実 shell の境界3行も実 XP と一致した
（`.local/shell-layout-tail-fixed.txt`）。Start脇の親隙間と外枠は引き続き調整対象。


## 日本語 caption の文字幅・縦位置・影と Start 理想寸法（0030–0033）

- 実 XP の SPI font smoothing は TRUE だった。FALSE を初期値にする案は採用しない。
  新規専用 prefix の未設定時だけ Wine の `AntialiasFakeBoldOrItalic=N` を設定し、
  フォント自身の小サイズ GASP 方針を使う。既存 prefix の移行では変更しない。
  明示切替は `display.py --font-rendering xp|wine`。省略時と独自値の保持を検証した。
- 0030 は外部 shell の monochrome 描画で、outline metrics と異なる bitmap の
  embedded advance をそのまま使わず、既存の明示 advance 計算経路を使う。
  DIB/screen、明示 dx、PDY、character extra の回帰は成功。
- 0031 は外部 shell の単一行 DT_VCENTER を、残った高さを一度だけ半分にする計算へ修正。
  0032 は公開テーマの single shadow を描き、caption も DrawThemeText を使用する。
  active/inactive、translated clip、DC/RECT/viewport 保持と CALCRECT/MODIFYSTRING 除外を検証。
  通常 Wine の opt-out 出力は独立環境で従来と同一だった。
- 独立 Debian VM の再起動後、実ウィンドウ caption の自作比較文字
  `Agjp WIN4WINE title` は実 XP と同じ27,12..154,24・764画素になった。
  左アイコン6,8..22,24・256画素と HTSYSMENU 4,4..23,29 も同時に一致。
  `shell-layout --verify-xp96-caption` の独立した96 DPI実測 fixture で成功。
  `.local/vm-test/caption-layout32.log`。全画面・全字体の完全一致を意味しない。
- 0033 は独立 Start alias の理想寸法をテーマ文字・opaque image list・余白から算出する。
  生背景画像99×33を下限にする処理を除き、日本語 Start の公開 BCM_GETIDEALSIZE は
  実 XP と同じ112×26になった。入力幅4通り、明示 Courier -40、画像余白/上下配置、
  Classic 往復と通常 Button の明示字体保持が成功。
  `.local/vm-test/font-start-33-public.log`。通常 Start の実クリック5往復も成功
  (`.local/vm-test/start-repeat33.log`)。
- 33までの独立 VM で Linux 枠の移動・resize・最大化・最小化・close、単一タスク、
  taskbar の固有アイコン更新、入力なし xterm 再描画の回帰も成功。
  `.local/vm-test/native-frames33.log`、`xterm-redraw33.log`。
  実フォルダのdouble-click→Alt+Left、タスク左クリックの最小化→復元も成功
  (`.local/vm-test/folder-operations33.log`)。

残件: 実 Start HWND は112×26で、親の30px高さの下4pxが空いている。
実 XP の実 HWND は112×30だった。公開 API の理想寸法とは分けて配置経路を調査する。
Luna を実行中に Classic 経由で再適用すると native tray の Classic 枠が残るため、
描画確認は専用 session 再起動後に行う。再起動後は余分な左右下枠と Start脇の親灰色隙間はない。
実 XP の高 DPI 比較は引き続き未測定。


### Caption の統合回帰と taskbar 字体の確認

`tests/nonclient-caption.c` は長い日本語・literal `&`・ellipsis、ボタンへの非重複、
active/inactive、通常400/240pxと実最大化1288pxを検査する。空titleの同条件NC背景に
公開 DrawThemeText を重ねた期待描画と全画素一致し、translated/clipped WM_PRINT の
clip外画素とfont/text/bkcolor/bkmode/viewport/clip保持も成功。
独立環境と Debian VM の `.local/vm-test/nonclient-caption33.log` に記録。
最大化背景自体のclip時のsampling差はこの文字試験の対象外で、公開背景API単体で別途比較する。

実 XP の task Toolbar/ReBar の公開 WM_GETFONT は MS UI Gothic -13 regular、
通知 toolbar は -12 regular、Start は -13 boldだった。Wine の別プロセス WM_GETFONT は
HFONTをmarshalしないためNULLになるが、自作の公開thread hookで同一process内から読むと
同じ4字体を確認できた（`.local/task-font-sameprocess.log`）。NULLから既定字体を推定しない。
追加の task font override は採用しない。stock DEFAULT_GUI_FONT はsystem localeに従い、
英語Wineでは-11、日本語ACP932のWineでは実XP同じ-12になる。本文SPIとは別の設定である。

検証VMの既存prefixにはJapaneseXPClassic初期値keyがなかったため、試験用に明示生成して
classic-initializeを実行した。再起動後もClassic6字体 MS UI Gothic -12とcaption18×18を確認。
既存prefixのmigrationが字体を自動変更しない方針は維持している。


## 利用者からの追加の実画面指摘（継続確認）

2026-10-09の追加指摘。API値の一致だけで解決扱いにせず、同じ解像度・DPI・window位置の
実XP画面と比較する。既存修正で改善している点も、現行画面で再確認する。

- Classic: 最小化と最大化ボタンが接して見える、Start文字が右にはみ出す。
- Luna: 左右上角が丸くない、タスクボタンのプログラム名字体が小さい。
- Luna: タスクバー上端が二重に見える、Start右のグリップの点が少ない。
- Luna: 右上3ボタンが大きい、Start下の青い余白が太い、時計の字体とサイズが違う。

Start下の4px隙間は既知の未解決点。字体はowner側のLOGFONTも取得できたが、
実描画の見た目を改めて比較する。丸い角はnormal/maximized/画面端配置を区別して検査する。


### 未指定 caption icon の補完（0034）

Lunaの描画がwindow-small/class-smallだけを取得し、window-bigと既定iconを取り落としていた。
Classicと同じpublic small/big/class small/class bigの順を使い、未指定ならWine自身の
共有stock IDI_WINLOGOを描く。modal no-iconは補完しない。Microsoft資源の追加・改変はない。
no-icons/class-big/class-small/window-big/window-small優先/modal no-iconの6ケースは
独立環境とDebianVMの画素比較で成功。日本語・省略・最大化・clipped WM_PRINTも再成功。
`.local/vm-test/caption34.log`。

クリーンsourceへの再現確認で0026の追加ファイルheaderに`new file mode`が欠けていたため、
patch metadataを修正した。既存適用済みsourceの整合性とpatch-state6試験が成功し、
別の完全clone/source/build/installで33までのfresh buildが成功し、続けて34/35の追加build/installも成功。
稼働中runtimeとは分離している。`.local/wine-verified33-build.log`。

公開背景比較probe `tests/gradient-clip.c` では、実XPは12条件すべてclip前後の差0。
Wineは最大化captionのclip時だけsample画素に差がある。この差は確認済みの調査対象で、
caption文字の統合試験が通ることと、背景samplingの完全一致は区別する。


### 起動時のタスクバー配置と時計の字体（0035）

公開APIの実XP測定では、Startのidealは112×26だが実配置は112×30。
Wineでも通常のWM_SIZEでshell自身が112×30へ配置するため、proxyは表示済みTrayと
Start/ReBarの識別子・矩形が連続2回安定してから、現在のclient sizeを一度だけ通知する。
子の寸法は固定しない。独立fixtureの準備待ち・移動中・一時非表示・再作成など6項目、
LunaとClassicの物理クリック5往復、Classicの利用者指定6字体の保持が成功。
`.local/shell-layout-sync-lifetime.log`、`.local/start-layout-classic-custom-check.log`。

時計は実XPでMS UI Gothic -12、旧WineはTahoma -11だった。
0035は既存の外部shell限定・active theme一致・LOGFONT検証を共有し、
TrayNotify::ClockのCLP_TIME/CLS_NORMAL/TMT_FONTだけClockFontを参照する。
registry profile v6は未設定値だけ追加し、独自字体・無効設定を上書きしない。
96/144/192 DPI、独自値、欠落・不正値・profile無効・非対象partの独立試験が成功。
Debian VMの公開probeもMS UI Gothic -12、GDI height12/ascent10/descent2を確認。
`.local/vm-test/clock35.log`。時刻形式・言語設定は変更していない。

新規sourceから全35patchの適用・全build/installが成功。
`.local/wine-verified33-build.log`、`.local/wine-verified35-build.log`。
Microsoftファイルの変更・追加取得はない。

VMの配置同期後、Start物理クリック5往復、xterm入力なし再描画、native枠の移動・resize・
最大化/復元・最小化・close・taskbar icon更新も成功。
`.local/vm-test/layout35-operations.log`。v6への既存prefix移行後の8字体profile試験も成功。
`.local/vm-test/font-profile35.log`。


### 部分再描画時のタイトルバー背景（0036）

実XPの公開テーマ描画12条件では、clip/viewportを変えても背景画素は変化しない。
WineはHALFTONE拡大の比率をclip後の矩形で計算し直し、補間用の隣接source画素も
切り落としていた。0036は外部shell限定で元のstretch座標と補間位相を保持し、
StretchBlt/StretchDIBits双方の必要なsource範囲を残す。整数演算は64bitを使う。

独立環境で自作画素1408条件（8形式、拡大/縮小/等倍、反転、上下DIB、複合clip、viewport）
が成功。実画面のWM_PAINT部分更新8条件も変色0となり、Luna公開12条件も差0。
元の全面描画hashは維持。opt-outの既存1024条件とテーマ12条件は旧runtimeと同じhash。
`.local/halftone-final-intmin-safe.log`、`.local/gradient-final-intmin-safe.txt`、
`.local/gradient-onscreen-fixed.txt`、`.local/halftone-final-comparison.log`。

別source/build/installでも全36patchの適用・全build/installが成功。
`.local/wine-verified36-build.log`。Debian VMへのatomic反映後も公開12条件すべて差0、自作GDI1408条件すべて成功。
`.local/vm-test/gradient36.log`。最終64bit版のopt-out比較も全1408条件同一。
`.local/halftone-intmin-baseline-optout.log` / `halftone-intmin-fixed-optout.log`。

36反映後の日本語caption文字/影764画素・左icon256画素とhit領域、通常/最大化・
非アクティブ・省略・clipped WM_PRINTも再成功。Startは実112×30/ideal112×26を維持。
`.local/vm-test/caption-layout36.log` / `clock-caption36.log`。
フォルダのicon本体をダブルクリックし、閲覧完了後にAlt+Left、task左クリック最小化/復元も成功。
`.local/vm-test/folder36-operations.log`。
以前のラベル座標へ連続入力する短いscriptは一部失敗しており、即時連続入力の完全保証とはしない。


### フォルダ上角の追加調査（0036時点）

VMのExplorerは画面1280×800に対し800×600、座標-4,-4で表示される。
public IsZoomed=TRUE / GetWindowPlacement.showCmd=SW_SHOWMAXIMIZED、X11の
MAXIMIZED_VERT/HORZも有効だった。通常ウィンドウの丸角欠落と見なしていたが、
実際は小さいままの最大化状態である。`.local/vm-test/folder-state36.log`、
`folder-placement36-xprops.log`。通常ウィンドウを画面内に置いた自作probeでは丸角は存在する。

Cabinet初回配置clamp候補の自作X11試験10項目は成功したが、この実VMの最大化は
その対象外で、改善できていない。候補は未commit、最大化状態を受け取るWM経路を調査中。
`.local/vm-test/window-placement36.log` / `folder-rounded36.png`。


### Classic Startの文字幅（0037）

実XP ClassicはStart実62×22/ideal62×21、MS UI Gothic -12 bold、text margins各1px。
旧Wineのaliasは幅57pxで、classic時だけtext margin初期化も欠けていた。
0037は外部shellのStart aliasかつtheme無効時だけ公開測定に基づく外側余白を加える。
大きな画像はその高さを維持し、文字の最低高さと区別する。通常Button/Lunaは維持。

自作7fixture（文字だけ、画像サイズ/余白、大小字体）が実XPと一致し、
VMでも7項目、6種類の日本語Classic字体、Start実62×22とphysical開閉5往復が成功。
`.local/vm-test/classic37-fixture.log`、`classic37.log` / `classic37.png`。
Lunaへ復元・cold restart後もStart実112×30/ideal112×26と開閉5往復を維持。
`.local/vm-test/luna37.log`。別sourceで全37patchの適用・全build/installも成功。
`.local/wine-verified37-build.log`。144/192 DPIのこのClassic余白を実XPと比較した扱いにはしない。

Classicのmin/max描画枠は実XPも接している。500px幅の視覚範囲はmin444..460、
max460..476、close478..494で、既存Wine規則と一致するためgapを追加しない。
`.local/xp-reference/classic-buttons.png` / `serial.log`（公開probe）。


### Start右のgripper（0038）

実XPの公開probeは、30px高のband header内でRP_GRIPPERを原点から6×30に描くと
実画面300画素すべて一致した。従来のWine helperはTS_TRUEの6×16を中央配置していた。
0038はheader原点と全長を使い、太さ・間隔・繰り返しはlocal themeに任せる。
固定の点数や画像寸法を埋め込まない。横/縦、locked/fixed/headerless、clipped/translated DC、
sentinel、native paint/input維持のVM試験が成功し、実画面の点々も改善。
`.local/vm-test/gripper38.log` / `desktop38.png`。
別sourceで全38patchの適用・全build/installも成功。`.local/wine-verified38-build.log`。

タスクバー上端の二重に見える帯は、unlock時の4px resize帯と30px clientに対応する。
実XPとWineのStart直後の背景34行はRGBが一致した。`.local/taskbar-top-comparison.txt`。
この局所比較は画面幅が異なるため全配置の一致を保証しないが、帯を削除する根拠にはしない。
実XP publicgripper結果は`.local/xp-reference/serial.log`のGRIP6、補助記録は
`.local/taskbar-gripper-findings.md`。Microsoftコード・資源は未変更。

### 動的な作業領域（0039）

WMの公開_NET_WORKAREAは1280×766でも、WineのSPI_GETWORKAREAとMonitor.rcWorkは
1280×800のままだった。0039は外部shellのdesktop所有threadで公開EWMH/GTK propertyの
有効な変更を観測し、monitor cacheを更新してWM_SETTINGCHANGEを通知する。
同一値・不正値による通知を抑え、WM_DISPLAYCHANGEは送らない。
明示SPI_SETWORKAREAを呼んだprocessの上書き値と、通常Wineの挙動は保持する。

`tests/workarea.py`は独立Xvfbと一時prefixで、30/34px予約の切替、GTK優先・削除、
同じHWNDの復元・最大化、40回の同値通知、不正property、明示SPIの保持を検査する。
Microsoft資産なしの新規prefixでも全9段階が成功。opt-outは旧実装と出力が一致。
`.local/workarea-final-fresh.log` / `workarea-final-fixed-optout.log`。
別sourceで全40patchの適用・全build/installと、このrunnerの再試験も成功。
`.local/wine-verified40-build.log` / `workarea-verified40.log`。

### 実タスク文字の字体（0040）と時計の実適用

実XPのTaskBand::Toolbar公開テーマ字体はMS UI Gothic -12 regularだった。
controlのWM_GETFONTは-13であり、それだけの一致では描画を検証できない。
Wineは実タスクのpart=0/state=1,5でTahoma -11を選び、文字幅も小さかった。
0040は日本語Luna profileの有効なTaskFontをこの公開テーマ経路へ適用する。
registry version 7は未設定時だけ追加し、独自設定を上書きしない。

実タスクの自作文字列「W4W Task 1234」は、修正後の通常/active両状態で実XPと
文字画素数199・位置・幅が一致した。`.local/taskbar-text-final.txt`。
9種類のprofile字体は96/144/192 DPIと無効化・欠落・不正値・独自設定を検査済み。
`tests/taskbar-text.c`は実タスク画面と公開theme queryを記録する診断用probe。

時計も`tests/clock-pixels.c`で実TrayClockWClassの現在文字列を取得し、同じ文字列の
公開DrawThemeTextおよび明示MS UI Gothic -12描画と比較した。2種類の時刻で
全白文字画素の差0。0035の字体が実時計に適用されており、追加patchは不要だった。
`.local/clock-pixels-actual.log`。英語形式の独立prefixでの適用検査であり、
実XPの24時間形式との幅の比較とは区別する。

Debian VMにも0039/0040とprofile version 7をatomic反映し、cold restartのattempt 1で起動。
実SPI/Monitor作業領域はともに1280×766へ改善した。時計の白文字97画素は両比較で差0、
タスク文字も通常/activeの位置と199画素を維持した。
`.local/vm-test/fonts-workarea40.log` / `taskbar-text40.log`。
folder-state probeはこの時点でフォルダを開いていないためexit 1だが、作業領域のAPI取得は成功。

### 所有threadによる最大化（0041）とtask activation

WMがWineの最大化サイズを非同期に強制する候補は、公開IsZoomedのACKと世代番号を
追加しても、高速復元で通常位置の座標や幅を壊した。Wine自身のtraceとWM要求maskで、
実Xサイズが最大化のままなのにpending矩形が復元済みとなり、位置だけの要求を送ることを確認。
`.local/vm-test/wine-maximize-lifetime40-repeat.log` / `max-rapid-mask-wm.log`。
この候補と専用IPCは採用しない。

0041は外部shellに限りWineのmanaged最大化サイズ抑制を解除し、所有threadが計算した
位置・サイズを反映する。WMはWineのEWMH状態を確認するだけで幾何を上書きしない。
通常/owned APPWINDOWの初回最大化、復元、高速往復、独自WM_GETMINMAXINFO上限の
10項目が成功。Lunaは5回50/50成功、Classicも10項目成功。
`.local/wine-maximize-owner41-luna-repeat.log` / `frame-owner41-classic-regression.log`。
通常Wineの非opt-in経路とminimizeの従来抑制は変更しない。

Linux代理枠も公開WM_GETMINMAXINFOとShowWindowで実WS_MAXIMIZEを使う。
Lunaは外寸-4,-4..1284,770、client0,26..1280,766、可視余白0/0/26/0。
Classicの可視余白は0/0/20/0。caption操作、復元後のdrag、resize、icon更新、
最小化・終了・withdraw/remapが両テーマで成功。
`.local/native-owner41-luna-final.log` / `native-owner41-classic-drag.log`。

実Cabinetも最大化で閉じた後、次回から正しい作業領域へ最大化され、
復元で通常800×600と丸角に戻る。`.local/cabinet-owner41-reopen.log`。
Wineの別process向けGetWindowPlacementは未完全対応なので、そのnormal値だけでは
復元位置を判定しない。所有process内のfixtureと実復元操作で検査する。

shell activationは現在のtask一覧に含まれるHWNDだけを通知する。owned dialogは公開
GW_OWNERを辿って最も近いtask ownerを選び、未所属tool/Startから余分なtaskを作らない。
2段owner chainの途中がAPPWINDOWの場合を含め4項目が成功。
`.local/shell-activation-owner41-final.log`。

### Start表示直後の黒いframe（0042）

安定後の描画とは別に、Startが開く約0.1〜0.3秒は全面黒だった。
公開API traceではSWP_SHOWWINDOW|SWP_NOREDRAWで先に表示し、WM_PRINTでchildrenを
捕捉した後に完成bitmapを転送していた。実XPの自作probeはNOREDRAW時に元の画面を
保つが、Wineは新規surfaceの黒いbufferを先行転送する。AnimateWindowは使われていない。

0042は外部shellの明示NOREDRAW表示・非layered popupに限り、X背景をNoneに維持し、
新規bufferを同depthのroot画素から初期化する。画面外は64bit演算でclipし、root変更による
BadMatchをtrapする。通常showで状態を解除し、既存のbuffered描画は維持する。
取得失敗・depth不一致などでは既存fallbackとなり、その条件で下地保持は保証しない。

公開15項目（初回/再show、owner DC、通常paintへの復帰、画面端、layered除外）が成功。
`.local/no-redraw-final-test.log`。opt-outの出力は旧40と同一。
実Start5回すべて全面黒なし、黒画素は最大1818（旧181616）。安定後画像は全5回とも
同一で、旧40の画像ともbyte一致。All Programs submenuも描画した。
`.local/start-flash-final42-match.log` / `start-flash-candidate-programs.png`。
直接X描画へ切り替える試作は安定後に枠の差が出たため採用しない。

独立source/build/installで全41/42patchの適用・全build/installが成功。
`.local/wine-verified41-build.log` / `wine-verified42-build.log`。
即時GdiFlushの画面反映はWine10の既存stub問題として残る。今回の公開試験はowner DCへの
描画と明示paintを分けて確認し、その既存問題まで解決した扱いにはしない。

### 最終42patch構成の独立VM検証

pin済みのWine baseへ全42patchを新規適用し、変更・追加54sourceが独立full build側と
byte一致した。`.local/patch-series42-fresh.log` / `patch-series42-source-match.log`。
patch状態試験6件と、最終buildでの動的作業領域9段階も成功。
`.local/patch-state42-tests.log` / `workarea-verified42.log`。

Debian VMへ最終Wine・proxy・inspect・WMを反映し、cold restartのattempt 1で起動。
公開NOREDRAW 15項目、所有thread最大化10項目、task activation 4項目が成功。
`.local/vm-test/owner-noredraw42-public.log`。
Linux枠の実IsZoomed・最大化外寸・復元後dragを含む操作試験もLuna/Classic双方で成功。
Classic最終96 DPIでは外寸-4,-4..1284,776、可視caption 19px、作業領域高さ772px。
以前の20px測定とは設定を区別する。`.local/vm-test/classic42-public-start.log`。

通常XP Startを5回開閉し、全面黒のframeはなく、安定後の同一性も全5回確認。
旧画像とのRGB差24画素は上部丸角の外側の背景だけで、公開X bounding shape内の差は0。
`.local/vm-test/frames-start42.log` / `start42-rgb-comparison.log` /
`menu-shape-folder42.log`。画面外の背景差までメニュー本体の差として扱わない。

実Explorerのフォルダ移動・Alt+Left・タスク左クリック最小化と復元が成功。
実xtermも文字の再描画・resize・focusを入力なしで確認した。
`.local/vm-test/folder42-operations.log` / `redraw-classic42-switch.log`。
後者のcommand全体はテーマ切替後の停止済みwineserverの終了値でexit 1になったが、
xterm試験自体はPASS。停止処理を補正して再起動した。
Classicの独立Start寸法7項目、通常XP Start開閉5回も成功。

最後にLuna / 96 DPI / 通常XPメニューへ戻してcold restartし、Explorer操作を再確認。
`.local/vm-test/final42-folder.log`、通常角に復元したExplorerと実xtermの画面
`final42-desktop.png` / `final42-start.png`を保存した。
Microsoftバイナリ・テーマ・フォントの変更や追加取得、追跡対象への格納は行っていない。

## 外観の追加調整（2026-10-09 夕、0043〜）

利用者の指摘（Start パネル下部の黒い文字、Start ボタン右上・右下の白、タスクバー上端の
二重線など）を、実XP参照VMの画面・公開API測定と比較して順に修正する。

- Start パネル下部の「ログオフ」「シャットダウン」は 0043 で実XPと同じ白い文字になった。
  `tests/window-theme-empty.c` は修正前の Wine で `8007007b` を返して失敗し、修正後は成功する。
- Start ボタンの丸角は 0044 でタスクバーの背景になった。
- Start 画像（99×33を30px高に縮小）中段の不正な画素は 0045 で消えた。縮小時の行の対応は
  実XPと同じ13/15/17行目になり、文字を除く背景は実XPの画面と一致する。
  `tests/transparent-shrink.c` は修正前に21画素の不正値で失敗し、修正後は全 mode で成功する。
- `tests/theme-part-pixels.c` は任意のテーマ part を公開APIで描き、property と画素を標準出力へ出す。
  画面との比較用で、ファイルやテーマ資源を書き出さない。
- タスクバー上端の線が二重に見える場合と見えない場合の差は、タスクバーの固定状態による。
  実XP参照画像でも固定時は30px高、固定解除時は上端に4pxのサイズ変更帯（TaskBar part 5）が加わり34px高。
  Wine 上も両状態で背景・高さ・Start ボタンが実XPと一致した。新規ユーザーの XP は固定状態なので、
  registry profile version 8 で未設定時だけ `TaskbarSizeMove=0` を補う。
- Luna の右上3ボタンが大きく見えたのは、ボタン外形ではなく glyph 画像の選択による（0046）。
  21×21の button で19px glyph を選んでいたため、実XPと同じ13px glyph に修正した。
  アクティブな閉じるボタンは実XPの画面と画素単位で一致する。nonclient-caption /
  caption-icons / nonclient-theme の既存試験も成功。
- 「スタート」の太字斜体の字形は実XPと一部異なる（未修正）。
