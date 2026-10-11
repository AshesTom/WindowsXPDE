# win4wine 今後の作業計画（/goal 用）

この文書は `/goal` に渡すための作業指示です。上から順に進め、各段階の完了条件を満たしてから次へ進みます。

## 報告と進め方

- **報告・説明・質問はすべて日本語で行う。** コミットメッセージとパッチの説明は既存の慣習どおり英語。
- 区切りのよいところで適切にコミットする。Wine パッチは `wine/patches/` の series として管理し、
  未改変の Wine（`wine/upstream.env`。段階 5 から 11.0）に `python3 wine/patch-state.py --apply` で全パッチが当たることを確認してからコミットする。
- 変更したら `docs/status.md`（何を直したか）、`docs/dll-policy.md`（判断と理由）、`docs/reproduce.md`
  （既存環境の更新手順、registry profile の版）を更新する。
- 見た目や挙動は実XP（参照用 XP VM）と比べて確かめる。測るのは寸法・色・規則などの数値だけ。
- 段階の途中で利用者の判断が必要な分岐があれば、日本語で選択肢と推奨を示す。

## 守ること（全段階共通）

- Windows 由来のバイナリ（explorer.exe、shell32.dll など）には手を加えない。Wine とその周辺で対応する。
- MS のバイナリを逆アセンブルしない。挙動は公開 API と実XP上の自作プローブで調べる。
- MS のファイル、スクリーンショット、ログは `.local/` と `localres/` にだけ置き、Git に入れない。
  リポジトリには XP の画素やグリフを入れない（規則と数値だけ）。
- 利用者が拒否した、ダウンロードした第三者の実行ファイルは再実行しない。回避策も取らない。
- ホストでの試験セッションがホストを停止・再起動しないこと（試験用 session 設定の
  shutdown_command / reboot_command は `["true"]`）。
- Wine パッチは環境変数 `WIN4WINE_EXTERNAL_SHELL` による opt-in を保ち、通常の Wine アプリの挙動を変えない。

## 段階 1: WM の最適化

状態: 2026-10-10 完了。結果と数値は `docs/status.md` の「WM の最適化（2026-10-10、段階 1）」。

目的: Linux アプリ（Linux 版 Chrome など）を XP シェル上で使うときの反応の遅れをなくす。描画は WM を通らないので、
対象はウィンドウ管理と Win32 側との同期。

現状:
- `bridge-wm/wm.py`（python-xlib）は 0.1 秒ごとにループし、`bridge-wm/proxy.c` は 200ms ごとに状態を同期する。
- 両者は IPC ディレクトリのファイル（`windows.tsv`、`frames.tsv`、`geometry.tsv`、`*.cmd`）でやりとりしている。
- そのため、タスクバーへの反映や、Linux のウィンドウを動かしたときの XP 枠の追従が 0.1〜0.2 秒遅れる。
- 待機中も1秒に10回起きて、X サーバーへの問い合わせを繰り返す。
- Alt+Tab の切り替えウィンドウ（`switch.tsv`）では、proxy 側だけ変更通知で起きるようにしてある。

やること:
- 定期的な確認をやめ、イベントをきっかけに処理する。
  - X 側は ConfigureNotify や PropertyNotify。
  - IPC は inotify・変更通知、またはソケット。
- 必要なら WM を Rust か C で書き直してよい。言語を変えるなら、既存の統合テストを新しい WM でも通す。
- 変更の前後で遅れを測る（枠の追従、タスクバーへの反映、待機中の起床回数と CPU 使用率）。

完了条件:
- 枠の追従とタスクバーへの反映が目に見えて遅れない（数値で示す）。
- 待機中に定期的に起きない。
- `tests/wm_integration.py` と `tests/native_frames.py` が通る。
- Alt+Tab 切り替えウィンドウの見た目（実XPと0画素差）と挙動を保つ。

## 段階 2: パッチの整理

状態: 2026-10-10 完了。構成と対応表は `docs/patches.md`、結果は `docs/status.md` の「パッチの整理（2026-10-10、段階 2）」。

目的: 積み重なった Wine パッチ（`wine/patches/` の 0001〜）を、保守しやすく、後で 64bit にも当てやすくする。

やること:
- 後のパッチで置き換えられた変更や、打ち消し合う変更をまとめる（例: 0060 は 0068 に統合済み、
  0061 の COLORONCOLOR は 0063 で削除済み）。
- DLL・機能ごとに論理的なまとまりへ並べ直し、各パッチの説明（目的、XP の挙動、根拠、制約）を最新の内容に揃える。
- 外部シェルの opt-in 判定など、重複している処理を共通化できるか検討する。
- 整理の前後で、ビルド結果の挙動が変わらないことを確かめる（同じソースツリーになるか、既存の試験と画面比較）。

完了条件:
- 未改変の Wine 10.0 に全パッチが当たり、整理前と同じ見た目・挙動になる。
- パッチ数と内容の対応表を `docs/` に残す。

## 段階 3: デスクトップの右クリックメニューの区切り線

状態: 2026-10-10 完了。原因は New ハンドラーの CLSID の `flags` 値で、registry profile 23 で XP と同じにした。
アクセスキーの下線が1px短かったのも 0035 で直し、デスクトップの右クリックメニューは実XPと0画素差。
結果は `docs/status.md` の「デスクトップの右クリックメニューの区切り線（2026-10-10、段階 3）」。

目的: XP にある「ショートカットの貼り付け」と「新規作成」の間の区切り線を出す。

現状:
- user32 のメニュー挿入の基本規則は XP と同じ（自作プローブで確認済み）。
- XP の shell32 がメニューを組み立てる途中の判断が違う。shell32 は変更できないので、その判断に使われる入力を
  Wine 側で XP と同じにする。

やること（逆アセンブルなし、公開 API だけで調べる）:
- 実XPと Wine の両方で、自作プローブからデスクトップ背景の IContextMenu を取得し、自分の HMENU に
  QueryContextMenu させて、項目・区切り・ID・状態を全部書き出して比べる。
- 「新規作成」など `Directory\Background\shellex\ContextMenuHandlers` の拡張を1つずつ単独で呼び、
  区切り線を出しているのがどれかを切り分ける。
- 原因に応じて直す。
  - レジストリの登録の違いなら、registry profile に加える。
  - user32 の細かい振る舞い（区切りの ID や状態の返し方など）の違いなら、Wine のパッチで XP に合わせる。
  - shell32 が見ている条件（フォルダの属性、貼り付けや元に戻すの可否など）の違いなら、その条件を XP と同じにする。

完了条件:
- デスクトップの右クリックメニューが、区切り線を含めて実XPと一致する（アクセスキーの下線の有無を除く）。
- 比較に使ったプローブを `tests/` に残す。

## 段階 4: パッチの再整理

状態: 2026-10-10 完了。段階 3 の下線の変更は 0035 に含めた。未改変のソースから configure すると失敗する漏れ（0015 の
`include/Makefile.in`）を見つけて直した。クリーンビルドで見た目と挙動が変わらないことを確かめた。結果は `docs/status.md` の
「パッチの再整理（2026-10-10、段階 4）」、対応表は `docs/patches.md`。

目的: 段階 3 で加えた変更を含めて、もう一度パッチを整理する。

やること:
- 段階 3 のパッチやレジストリの変更を、段階 2 で決めたまとまりと書式に合わせて組み込む。
- 段階 2 と同じ方法で、整理の前後で見た目と挙動が変わらないことを確かめる。

完了条件:
- 未改変の Wine 10.0 に全パッチが当たり、見た目と挙動が整理前と同じ。
- 段階 2 で作ったパッチの対応表を更新する。

## 段階 5: Wine 11 への移行

状態: 2026-10-10 完了。11.0 に移り、パッチは 53 本（10.0 版の3本を外し、ntdll の 0016 を加えて振り直した）。
結果は `docs/status.md` の「Wine 11 への移行（2026-10-10、段階 5）」、対応表は `docs/patches.md`、既存環境の更新は
`docs/reproduce.md`。

目的: 基にする Wine を 10.0 から 11.0（2026-01-13 公開の安定版）にする。11.0 では新しい WoW64 が完成し
（16bit アプリにも対応）、段階 7 の 64bit 対応を 11.0 の上で行う。入力（段階 6 の Mozc）や描画の経路も変わったため、
その前に移る。

現状:
- `wine/upstream.env` は `wine-10.0`（b0738596…）。series は 55 本（`docs/patches.md`）。
- 変更が集中するのは win32u、user32、uxtheme、winex11、comctl32、explorer、kernelbase。11.0 ではとくに winex11
  （EGL の描画経路、Wayland 対応）と win32u が大きく変わっている。
- 段階 4 で、新しいソースとビルドディレクトリで configure からビルドする手順と、試験 VM へ install を移す手順
  （ext4 のイメージを QMP で差し込む）、入れ替えの前後の比較（`.local/vm-cleanup-scenes.sh` の場面、非クライアント領域の試験、
  chroot の bridge の試験）を整えた（`docs/status.md` の段階 4、`docs/reproduce.md`）。

やること:
- 11.0 の安定版のタグ（11.x の保守版があれば最新の 11.0.x を検討し、選んだ理由を記録）に `wine/upstream.env` を更新する。
- 55 本を 11.0 に移植する。段階 2 のまとまりと書式（`docs/patches.md`）を保ち、各パッチの説明（Wine before など）を 11.0 の
  コードに合わせて直す。11.0 で upstream が直した・不要になった変更は、根拠を確かめてから外し、対応表に残す。
- 未改変の 11.0 に `wine/patch-state.py --apply` で全部当たることと、新しいソースとビルドディレクトリで configure から
  ビルドできることを確かめる。
- 11.0 の Wine で既存の prefix が更新されること（wineboot、registry profile、XP の DLL の配置）を確かめる。新規 prefix の
  作成（`prefix/create.py`）も試す。必要なら `docs/reproduce.md` に既存環境の移行手順を書く。
- ビルドは引き続き 32bit（`gcc -m32`、`--without-wayland`）のまま。WoW64 への切り替えは段階 7 で行う。

完了条件:
- 未改変の Wine 11.0 に全パッチが当たり、新しいビルドディレクトリで警告なくビルドできる（upstream 由来の警告は除く）。
- 試験 VM で XP シェルが起動し、段階 4 と同じ場面が 10.0 版と同じ見た目になる（差があれば原因と判断を記録）。
  デスクトップの右クリックメニューと Alt+Tab は実XPと0画素差を保つ。
- `tests/wm_integration.py`、`tests/native_frames.py`（Luna と Classic）、`tests/xterm_redraw.py`、
  `tests/bridge_integration.py` と非クライアント領域の試験が 10.0 版と同じ結果になる。
- `docs/patches.md` に 10.0 → 11.0 の対応（移植・統合・削除）を残し、`docs/status.md`、`docs/dll-policy.md`、
  `docs/reproduce.md` を更新する。

## 段階 6: Mozc（日本語入力）

状態: 2026-10-10 完了。fcitx5 と Mozc で Linux アプリ（Chromium）と Wine のアプリ（メモ帳、「ファイル名を指定して実行」、
アドレスバー）の変換・確定ができ、入力モードを XP の通知領域に出した。パッチは 55 本。結果は `docs/status.md` の
「日本語入力（2026-10-10、段階 6）」、判断は `docs/dll-policy.md`、手順は `docs/reproduce.md`。

目的: XP シェル上の Linux アプリと Wine のアプリの両方で、Mozc で日本語を入力できるようにする。

現状:
- session は fcitx5 や ibus を起動せず、`XMODIFIERS` や `GTK_IM_MODULE` も設定していない（未検証）。
- タスクバーのトレイは Win32 のアイコンにしか対応しておらず、Linux のトレイアイコン（XEmbed や StatusNotifierItem）、
  つまり入力モードの表示が出ない。

やること:
- VM に fcitx5-mozc を入れる。Linux 版 Chrome と、Wine 側（「ファイル名を指定して実行」、アドレスバー、
  メモ帳など）で入力を確かめる。
- session で入力メソッドを起動し、環境変数を設定する（LightDM の Xsession や im-config との関係を確認する）。
- 入力モードの表示方法を決める（トレイに Linux アイコン対応を加える、fcitx5 の浮動パネルを使う、など）。
  可能なら XP の見た目に合う形にする。

完了条件:
- Linux アプリと Wine アプリの両方で、変換・確定ができる。
- 入力モードが画面上で分かる。

## 段階 7: 64bit（WoW64）対応

状態: 2026-10-10 完了。WoW64 のビルドと 64bit の prefix に移り、パッチは 56 本（0017 を加え、0026・0027 を直した）。
結果は `docs/status.md` の「64bit（WoW64）対応（2026-10-10、段階 7）」、判断は `docs/dll-policy.md`、既存の 32bit prefix の
移行（`prefix/migrate.py`）は `docs/reproduce.md`。

目的: 実機に i386 の multiarch（32bit のライブラリ）を入れずに動かせるようにし、64bit の Windows アプリも使えるようにする。

現状:
- Wine は純粋な 32bit ビルド（i386-unix と i386-windows、`wine/build.sh` は `gcc -m32 --without-wayland`）。
- XP の explorer と DLL は 32bit。
- 段階 6 の 0026 は、XP のタスクバーへ 32bit の handle を並べた形式で通知アイコンを送る。64bit のアプリからの
  呼び出しでも同じ形式（handle を 32bit に詰める）になることを確かめる。

やること:
- Wine の新しい WoW64 方式でビルドする（i386 と x86_64 の PE）。
- パッチを 64bit の PE 用にもビルドして確かめる。
- prefix の構成を見直す。
  - XP の DLL は syswow64 に置く。
  - レジストリの書き込みは Wow6432Node に振り分けられる。
  - `prefix/create.py`、`prefix/stage.py`、`prefix/registry.py` が対象。
- 32bit アプリの OpenGL の性能を確かめる。

完了条件:
- ホストに i386 のライブラリがなくても、XP シェルが今と同じ見た目で動く。
- 既存の試験と、実XPとの画面比較を通る。
- 既存の 32bit prefix の更新手順を `docs/reproduce.md` に書く。

## 段階 8: 後回しの候補、Linux のトレイアイコン（XEmbed）、Qt の日本語入力

目的: 段階 1〜7 と 2026-10-11 の作業で見つけて残した差と不足を片付ける。

前提（2026-10-11 の状態）:
- パッチは 58 本（`docs/patches.md`）。今のクリーンビルドは `.local/wine-stage10-install`。試験 VM は段階 7 の install に
  このビルドの wineserver、win32u.so、conhost.exe を入れた `/work/.local/wine-stage7-install` と、`prefix-final64` で動く
  （VM のディスクは空きが約 8GB で、install 全体を入れ替えてよい）。chroot の試験用の prefix は `prefix-wmopt-w64` と
  `prefix-wmopt-classic-w64` だけ。番号の古い試験の補助（`.local/s7-recovery.sh` など）は消した install を指しているので、
  使うときは今の install に直す。
- 2026-10-11 の内容は `docs/status.md` の最後の4節（Linux アプリのスタートメニュー、Windows キー、cmd の字体など）。

やること（1つずつ、原因を調べてから直す。直さないと決めたものは理由を残す）:

1. Linux のトレイアイコン（XEmbed）
   - 今は StatusNotifierItem だけを XP の通知領域に出す（`bridge-wm/tray.py`、`docs/architecture.md`）。
     `_NET_SYSTEM_TRAY_S0` の XEmbed のトレイだけを使う古いアプリのアイコンは出ない。
   - tray.py（または WM）が、ほかにトレイが無いときだけ `_NET_SYSTEM_TRAY_S0` を所有し、XEmbed のアイコンの窓を
     受け入れ、その画像を SNI と同じ経路（`tray-ID.argb`）で XP の通知領域に出す。クリック（左・右・中）はアイコンの窓に
     送り返す。アイコンの窓は画面に出さない。
   - 試験 VM に XEmbed だけを使うアプリを入れて確かめる（例: `stalonetray` の自前のアイコンや、Python で書いた XEmbed の
     最小のアイコン。外部のバイナリを入れる場合は .deb をホストで取得して入れる）。`tests/tray_host.py` に XEmbed の場合を足す。
2. Qt のアプリの日本語入力
   - 試験 VM に `fcitx5-frontend-qt5` と `fcitx5-frontend-qt6` を入れ（.deb はホストで取得）、Qt のアプリ（入っている
     `fcitx5-config-qt` の入力欄、Qt の小さなエディタなど）で Mozc の変換・確定ができることを確かめる。
   - session は `QT_IM_MODULE=fcitx` を設定する。フロントエンドが無いときの挙動（入力できない、警告）を確かめ、
     `docs/reproduce.md` の導入の一覧と、必要なら supervisor の扱い（入っていなければログに書く）を直す。
3. ロードマップの「後回しの候補」（下の節。番号は 10.0 版なので `docs/patches.md` の対応表で今の番号に読み替える）
   - Alt+Tab の切り替えウィンドウで、フォルダのアイコン（32×32）の半透明部分が実XPと各色1違う（146画素）。
   - Alt と Shift を先に押してから Tab を押した Alt+Shift+Tab が、直前のウィンドウを選ぶ。実XPと比べる。
   - 非クライアント領域の試験の既知の失敗（nonclient-caption の非アクティブ時の6件、nonclient-theme の
     「caption button pixels differ: active=1」）。
   - 待機中に wineserver が毎秒60〜85回起きる。どのプロセスかを特定する（WM の最適化の段階の「待機中に定期的に起きない」に
     合わせて、直せるものは直す）。
   - MENUEX のリソースの区切りの状態（実XPは MFS_GRAYED、Wine は0）。
   - DrawText が末尾の `&` を描く（XP は描かない）。DT_EXPANDTABS のタブの後の下線の位置がずれる。
   - 主ツールバーの TB_GETBUTTONSIZE（実XP 76×38、Wine 84×38。ボタンの位置は同じ）。
   - 試験 VM の install を入れ替えた2回目に registry profile が入れ直されず、デスクトップの名前が英語になった（段階 5）。
     再現の手順を探し、`prefix/stage.py` の判定を確かめる。
   - 対象外のまま残すもの（理由は `docs/status.md`）: WinHelp のポップアップの中身、フォルダ オプションの「オフライン ファイル」
     タブ（cscui.dll）、「新規作成」のサブメニューのファイルの種類（利用者の判断）、cmd のボタンのロゴ（利用者の判断）。
     それぞれ直す方法があるかだけ、日本語で短くまとめる。

完了条件:
- XEmbed だけを使うアプリのアイコンが XP の通知領域に出て、左・右クリックが届く。
- Qt のアプリで Mozc の日本語入力ができる（必要なパッケージは文書に書く）。
- 「後回しの候補」の各項目が、直したか、直さない理由と測った数値が `docs/status.md` に残っている。
- 既存の試験（chroot の WM・枠・ブリッジ・トレイの試験、VM の非クライアント領域の試験）と、段階 7 と同じ9場面の比較が通る
  （差があれば理由を書く）。パッチを変えたら、新しいソースとビルドディレクトリで configure からビルドする。

## 段階 9: ログイン画面

目的: Linux のログイン画面（LightDM）を XP のようこそ画面の見た目にする。

方針:
- XP の logonui.exe や msgina は動かさない（単体ではログイン処理にならず、パスワードを扱う場所で Wine と MS の
  コードを動かすのも避ける）。
- ログイン処理は通常の LightDM と PAM に任せ、画面だけを自作の greeter で再現する（web-greeter のテーマ、
  または GTK の自作 greeter）。
- 背景、ロゴ、ユーザー画像の枠、矢印ボタンなどの画像は、壁紙やユーザー画像と同じく利用者の XP メディアから
  抽出して実行時に読み込む。リポジトリには入れない。
- 寸法、配置、文字は実XPのようこそ画面で測って合わせる。

やること:
- 実XPのようこそ画面（ユーザー選択、パスワード入力、誤入力時、シャットダウンの選択）を測る。
- greeter を作り、テスト VM の LightDM に組み込む（既存の greeter に戻す手順も用意する）。
- ログオン・ログオフ時の「ようこそ」「ログオフしています…」の画面を、セッション開始・終了時の表示として再現する。

完了条件:
- テスト VM で、XP のようこそ画面に似た画面からログインでき、XP シェルが起動する。
- 主要な画面の寸法と配置が実XPと揃っている（差があれば記録する）。
- 画像が抽出されていない環境では、通常の greeter のまま使える。

## 段階 10: スケーリング（まず X、その後 compositor を検討）

### 9a. X11 での DPI 拡大
- XP 本来の方法（LogPixels）で 120（125%）、144（150%）などに対応する。今の `prefix/display.py` は 96・144・192 だけ。
- 参照用 XP を同じ DPI にして見た目を比べ、差を詰める。Luna の画像は 96 DPI 用なので、XP と同じ引き伸ばし方になればよい。
- Linux アプリの倍率（`Xft.dpi` など）を同じ値に揃える。

### 9b. Wayland と自作 compositor の検討
- モニターごとの倍率や分数スケーリングのために、bridge-wm に代わる Wayland compositor（wlroots 系など）を検討する。
  - Wine と XP のシェルは Xwayland 上で動かし、compositor がその位置指定を受け入れる。
  - Linux アプリは Wayland ネイティブで動かす。
- まず方式の比較と試作の見積もりを日本語でまとめ、利用者の判断を仰いでから実装する。

## 後回しの候補

（2026-10-11、段階 8 で扱う。）

（この節のパッチ番号は 10.0 版のもの。今の番号は `docs/patches.md` の対応表で読み替える。）

- 既知の未解決: WinHelp のポップアップ、「オフライン ファイル」タブ、主ツールバーの TB_GETBUTTONSIZE の差。
- 段階 1 で見つけた、変更前からある差（詳細は `docs/status.md` の「WM の最適化」）:
  - Alt+Tab の切り替えウィンドウで、XP Explorer のフォルダのアイコン（32×32）の半透明部分が実XPと各色1だけ違う
    （2つのアイコンで146画素）。DrawIconEx の合成の丸めと見られる。以前の「実XPと0画素差」は別のウィンドウでの比較。
  - Alt と Shift を押してから Tab を押す Alt+Shift+Tab が、最下位ではなく直前のウィンドウを選ぶ。実XPとは未比較。
    Alt を押したまま後から Shift+Tab を押す場合の逆順は動く。
  - 非クライアント領域の試験で、nonclient-caption の非アクティブ時6件と nonclient-theme の
    「caption button pixels differ: active=1」が失敗する（変更前の bridge・Wine でも同じ結果）。
  - 待機中も wineserver が毎秒60〜85回起きる。proxy がなくても同程度で、Wine の常駐プロセス（services、
    plugplay、winedevice など）によるもの。どのプロセスかは未特定。
  - （段階 2 で対応済み）uxtheme のテーマ画像の競合は、0008（旧 0076）で opt-in をやめ、読み込みの一覧にも排他を
    加えた（`docs/dll-policy.md` の「パッチの整理」）。
- 段階 3 で見つけた、変更前からある差（詳細は `docs/status.md` の段階 3）:
  - （段階 3 で対応済み）メニューのアクセスキーの下線が1px短かった。0035 で XP と同じにした。
  - （利用者の判断で対応しない）「新規作成」のサブメニューに実XPのファイルの種類（7種）が無く、末尾に区切りだけが残る。
  - MENUEX のリソースから読んだ区切りの状態が、実XPは MFS_GRAYED、Wine は0（描画には影響しない）。
  - DrawText が末尾の `&` を描く（XP は描かない）。DT_EXPANDTABS のタブの後の下線の位置がずれる。
- 段階 5 で見つけたこと（詳細は `docs/status.md` の段階 5）:
  - 試験 VM の install を入れ替えた2回目に、registry profile が入れ直されずデスクトップの名前が英語になった。stage より先に
    prefix が更新されたと見られるが、原因は未特定。
- 段階 6 で見つけたこと・見送ったこと（詳細は `docs/status.md` の段階 6）:
  - Linux のトレイは StatusNotifierItem だけに対応した。XEmbed のトレイ（`_NET_SYSTEM_TRAY_S0`）だけを使う古いアプリの
    アイコンは出ない。
  - 入力モードの表示は fcitx5（と fcitx5-mozc）のテーマのアイコンを通知領域に出す。実XPの MS-IME の言語バー（浮動または
    タスクバーに格納）の見た目にはしていない。
  - Qt アプリの日本語入力には fcitx5-frontend-qt5 / qt6 が要る（試験 VM には入れていない）。
