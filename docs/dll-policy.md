# DLL 選択と調査

| DLL / EXE | 初期選択 | 理由 |
|---|---|---|
| explorer.exe | native XP | 目的のタスクバー・スタート・デスクトップの実体 |
| browseui | native XP | builtin の ordinal 107 が起動直後に未実装中断。単純な成功応答ではフォルダ構築を代替できない |
| shdocvw | native XP | browseui と同世代のフォルダホスト・内部インターフェースを使う |
| shell32 | native XP | browseui/shdocvw と同じ XP の名前空間と内部 ordinal の組を保つ |
| browselc, shdoclc | native XP の資源 | 同じ ISO の日本語メニュー・文字・アイコン。コードの追加取得や IE renderer 導入はしない |
| shlwapi | native XP | shell32/browseui の XP 固有 ordinal 依存を同世代で解決する |
| comctl32 | XP shell のみ native v6 | SP3 の 6.0.2600.5512 を C:\xp に配置。Start の Button は自作 Wine controls の別名モジュールで分離。通常アプリは builtin |
| uxtheme | builtin | ローカル Luna の msstyles を読む。Wine 側の描画・フォント処理を修正し、native のテーマサービスは導入しない |
| kernel32, ntdll, user32, gdi32, ole32, advapi32 など | builtin | Linux/Wine の基盤。XP のカーネルやサービスを取り込まない |
| mshtml, mscoree | disabled | IE/CLR は対象外。Mono/Gecko の自動取得を止める |

## 調査 1: builtin shell family

Debian 13 chroot、パッケージ Wine 10.0、XP 日本語 SP3 x86、Xvfb 1280×800。
`WINEDEBUG=+seh,+loaddll` で BROWSEUI.dll ordinal 107 の `EXCEPTION_WINE_STUB` を確認。
**判断: native に差し替える。** 同じ ISO の shell family を使用する。
生ログ: `.local/explorer-builtin.log`（ローカルのみ）。

## 調査 2: native shell family の無表示終了

`+relay,+seh` で native explorer 起動中の `GetShellWindow` が既に非 NULL であることを確認。
Wine の `programs/explorer/desktop.c` が desktop manager に SetShellWindow する実装と照合。
**判断: Wine に opt-in shim を足す。** builtin desktop manager 自体は保持し、
`WIN4WINE_EXTERNAL_SHELL=1` の場合だけ shell HWND 登録を譲る。
この観測だけで native explorer の終了原因が完全に解消したとは判断しない。パッチ後の再試験が必要。
生ログ: `.local/explorer-relay.log`。パッチ: `0003-explorer-external-shell.patch`。

## 調査 3: shell hook

upstream Wine 10.0 の RegisterShellHookWindow / DeregisterShellHookWindow は stub。
**判断: Wine に限定的な登録 shim、独立 bridge に配信を足す。** 実装制約は architecture.md に記載。

未試験の DLL 選択は動作保証ではない。Luna/native uxtheme、COM 登録、フォルダ内部インターフェースは実測で追記する。

## 調査 4: Debian 13 toolchain と Wine 10.0

ソース版の初回 wineboot が `LdrResolveDelayLoadedAPI` の IAT 書き込みで access violation。
Wine 自身の PE のセクションヘッダを確認し、binutils 2.44 が `.idata` を read-only にする変更と照合した。
**判断: Wine に既知のビルド修正をバックポート。** 遅延 import の書換領域を writable data に配置する。
[Debian Wine 修正](https://sources.debian.org/patches/wine/10.0~repack-12/fixes/binutils2.44.patch/) と
[binutils の説明](https://sourceware.org/pipermail/binutils/2025-April/140559.html) を参照。
MS バイナリではなく Wine 自身のソースと PE ヘッダのみを調査した。
パッチは `0004-winebuild-binutils244-delay-imports.patch`。生ログは `.local/source-boot-debug.log`。
この問題があるため、パッケージ Wine に自作 explorer モジュールだけを混ぜた途中試験は到達判定に使用しない。

## Override の隔離

XP explorer をローカルで `xp-explorer.exe` として配置し、AppDefaults でそのプロセスにだけ native shell family を適用する。
Wine 自身の explorer/desktop manager は builtin のまま。セッション全体へ native shell32 を指定すると、
管理プロセスや復旧経路まで XP の DLL に依存するため。元ファイルの内容は変更しない。

## 調査 5: サービス初期化待ちとドッキング API

ソース版で `+relay,+seh` を取得。XP explorer が `Global\ScmCreatedEvent` を
待つが Wine の services はイベントを作らず、起動が約 60 秒止まる。
**判断: Wine に shim を足す。** RPC/サービス DB 初期化後にサービス管理プロセスが
イベントを作成・通知し、終了まで保持する（0005）。
その先で `CM_Is_Dock_Station_Present` の stub exception を確認。
**判断: Wine に shim を足す。** Windows ドックを公開しない環境なので FALSE と成功を返す（0006）。
生ログ: `.local/xp-controls-relay.log`、`.local/wine-dock-build.log`。

## 調査 6: COM 登録の不足

BandSite / ShellBrowser 等の生成が `REGDB_E_CLASSNOTREG` で失敗。
**判断: native を使い、公開 CLSID の最小登録を生成する。**
`prefix/registry.py` に公開 manifest のクラスと、起動ログに現れたクラスを登録。
後者の提供 DLL は利用者の DLL に対する公開 API `DllGetClassObject` で確認した。
診断用 `prefix/class-probe.c` はクラスファクトリを取得・解放するだけで、コードを解析しない。
Wine 10.0 の HKCR 読み出しでは HKCU\Software\Classes に追加した登録が見えなかったため、
専用 prefix 内の HKCR に生成する。任意の実環境ハイブの丸ごと移植はしない。
生ログ: `.local/class-providers.log`、`.local/xp-menu-classes.log`。

## 調査 7: コントロールと実行ファイル名

builtin comctl32 には XP 内部 toolbar メッセージの不足があり、native shell と組み合わせた試験で例外が出た。
native v5 では例外が減ったが表示寸法は崩れた。native v6 を SxS manifest で
Button にも割り当てる試験は native v6 内の access violation で失敗した。
**暫定判断: native v6 を実行ファイル隣に置き、Button は Wine 標準のままクラシック表示する。**
SP3 の 6.0.2600.5512 を優先し、正確な SxS Button 選択用の実験 manifest は自動導入しない。
表の comctl32 の初期選択から変更した。全アプリへの native 指定は行わない。

名前を分離した shell に対する `GetModuleHandleW("EXPLORER.EXE")` の失敗も観測したため、
外部シェル opt-in かつ当該プロセスにだけ元名を主モジュールに解決するパッチ 0007 を追加。
これだけでは寸法不良は直らなかった。

追加の relay で Start の `BCM_GETIDEALSIZE` が FALSE を返し SIZE がゼロのままになることを確認。
**判断: Wine に限定 shim を足す。** 0008 は標準 pushbutton の文字寸法と DPI に応じた余白を返す。
image-list 描画や Windows と同一の寸法を保証する実装ではない。
生ログ: `.local/xp-sizing-relay.log`、`.local/xp-button-size.log`。

## 調査 8: ショートカットと子ウィンドウの購読

Classic メニューを `NoSimpleStartMenu=1` で選択し、必要な MenuBand / MenuDeskBar / MenuBandSite
のクラスファクトリ提供元を公開 API で確認して登録した。
[ポリシーの公開資料](https://learn.microsoft.com/en-us/windows/client-management/mdm/policy-csp-admx-startmenu) を参照。
ショートカットの列挙で `LINKINFO.dll.IsValidLinkInfo` の未実装中断が発生。
**判断: native に差し替える。** ローカル ISO の linkinfo.dll を抽出対象に追加した。
生ログ `.local/xp-classic-menu.log`、`.local/xp-linkinfo.log`。

XP は子ウィンドウ MSTaskSwWClass で RegisterShellHookWindow する。
bridge の EnumWindows のみの配信では届かないため、子孫も購読検索の対象にした。
子ウィンドウ購読に変更した統合試験と、実 XP のメモ帳 / xterm のタスクボタン表示を確認。
生ログ `.local/bridge-child-integration.log`。

## 調査 9: フォルダの shell endpoint と実行関連付け

native フォルダ生成は COM 登録追加だけではハングした。Wine の desktop manager が
ShellDDE、ShellWindows、隠し Shell_TrayWnd を同時に所有していたため、外部シェル時だけ
これらを作らないパッチ 0009 を追加した。**判断: Wine に opt-in shim を足す。**
native XP が名前空間・フォルダの endpoint を所有し、Wine は X11/clipboard を維持する。
新規 prefix で H: とサブフォルダの閲覧を確認した。

native ShellExecuteEx は `.exe` の関連付けがないと ERROR_NO_ASSOCIATION (1155) を返した。
**判断: 最小レジストリを自前生成する。** `.exe/.com` と exefile/open を専用 HKCR に登録。
メモ帳のメニュー起動で確認した。

## 調査 10: Linux の起動とフォーカス

`start /unix /usr/bin/xterm` では Windows 形式の argv[0] が渡され、起動した xterm が
WM_DELETE_WINDOW で終了しないケースを実測した。**判断: Linux 側の起動を仲介する。**
ショートカットを `start /unix /usr/bin/env xterm` とし、終了要求への応答を確認した。
Wine や Microsoft バイナリのコード解析は行わず、Wine と xterm の公開ソースを照合した。
ログ `.local/xterm-wine-close-check.py`、`.local/final-m3-native-operations.log`。

proxy が X11 の active 属性だけでフォーカスを取り返すと XP のメニュー操作を妨げた。
WM は実際の X input focus の所有元も確認し、proxy は遷移時だけ Win32 foreground を更新する。
PROGMAN のクラス名は大小文字を区別せず処理する。

## 調査 11: 復旧の隔離

XP 用 HKCR を持つ prefix では builtin explorer も表示前に停止した。一方、同じ Wine の
クリーン prefix では Linux ホームのフォルダを表示できた。
**判断: 復旧のレジストリも分離する。** 専用 prefix 内の `recovery/` を builtin のみで作成する。
XP のファイルや COM 登録に依存せず、ネイティブ xterm も同時に開く。

## 調査 12: Luna と HiDPI

Wine builtin uxtheme は利用者の Luna msstyles を読み、IsThemeActive / IsAppThemed が TRUE、
GetCurrentThemeName が該当パス、Taskbar のテーマ part size が成功することを確認した。
`+uxtheme` ログにも Blue_TaskbarBackground 等の読込を記録。
ただし native XP comctl と Wine 標準 Button の組では、背景・Start ボタンの描画が揃わない。
**初回の判断: クラシック表示を基準とし、Luna の描画修正を後続調査へ回す。**
後続の描画・フォント調査は調査17以降を参照。
native uxtheme や XP の Themes サービスを追加して動作保証した扱いにはしない。
クラシックの 144/192 DPI では Start / Programs の文字と配置、200% の Linux 起動を実測。
DPI 非対応 XP アプリの拡大なので画質の改善は別の課題とする。

## 調査 13: Wine 更新後の native profile

Wine の prefix 自動更新は DLL と COM 登録を入れ替えるため、先に native DLL を配置するだけでは
次の Wine ビルドで壊れる。**判断: 更新と native profile の再配置を順序付ける。**
wine.inf と `.update-timestamp` を照合し、不一致時のみ builtin wineboot を完了させ、
自前 COM 登録を再適用し、専用 wineserver を終了してから native DLL を atomic 配置する。
VM で timestamp 不一致を作り、同じセッションから XP Shell / Start が再度動くことを確認した。

## 調査 14: フォルダの資源と ReBar 配置

公開 API trace の LoadMenuW / LoadStringW が資源用 module NULL のまま失敗していた。
同じ ISO の `browselc.dll` / `shdoclc.dll` を無改変で加えると、日本語メニュー・
ツールバーの文字・初期ウィンドウ名・アイコンが表示された。
**判断: ローカル資源 DLL を抽出・専用 prefix へ配置する。**
既存 prefix にも AppDefaults の native 選択と全体の builtin 選択を適用する。
IE/CLR の disabled 方針は変えない。

アドレス欄は別問題で、親 WorkerW の上端から ReBar が32pxずれていた。
自作の公開 CreateWindowExW / GetWindowRect / MapWindowPoints probe では、
native SP3 v6 の配置 bits=0 で初期高さ36px分移動し、明示 CCS_TOP では y=0 となる。
その後の native browseui の SWP_NOMOVE がずれを保持する。
**判断: 外部シェルの名前付き子 ReBar にだけ、未指定時の CCS_TOP を明示する（0011）。**
明示した CCS_NOMOVEY 等は変えない。実フォルダと native バンドの試験で確認した。
[公開 common-control styles](https://learn.microsoft.com/en-us/windows/win32/controls/common-control-styles) と
Wine の ReBar ソースを参照。Microsoft の実装コードは解析していない。

## 調査 15: タスクボタンの左クリック

実 XP の WM_COMMAND 処理が、別プロセスへの GetSystemMenu の未対応で止まっていた。
**判断: 外部シェル opt-in の標準システムメニュー snapshot（0010）。**
Wine のメニューはプロセス内のデータなので、本物の他プロセスメニューを共有した扱いにはしない。
最大2048個をロック付きで保持し、消失した HWND を回収する。
WS_MINIMIZEBOX / WS_MAXIMIZEBOX を反映し、revert は呼出側のコピーだけを破棄する。
アプリ独自の項目・無効化は未対応。通常の Wine には適用しない。
VM のメモ帳と xterm の実ボタンで左クリック最小化・復元を実測した。

## 調査 16: 通常 Start と再描画

通常の XP Start のため、shell32 が公開 factory を提供する
`{D912F8CF-0396-4915-884E-FB425D32943B}` を追加登録した。
profile version を Wine の timestamp と別に保存し、既存 prefix の登録も更新する。
`NoSimpleStartMenu=0` だけでは選択されず、公開
[SSF_STARTPANELON](https://learn.microsoft.com/ja-jp/windows/win32/shell/ssf-constants) の設定・読戻しが必要だった。
`prefix/menu-settings.c` が専用 helper の native shell32 だけを使って設定する。
この時点では右側の一覧を表示するが、ユーザー欄・左側・下部が黒かった。
後続調査17で保存画像の WM_PRINT 不足を特定した。
SxS native Button は例外が再発し、v5 controls はアイコン等を欠くため採用しない。

xterm の描画用子ウィンドウに Expose を配信することで、文字バッファを保った画面消失から
入力なしで復帰することを Xvfb / VM で確認した。WM のフォーカス・サイズ変更に限定し、
Wine の HWND があるウィンドウには適用しない。定期的な画面消去やキー入力で隠さない。
VGA/Xorg の元のイベント不足の原因は未特定で、全 Linux アプリの描画保証ではない。


## 調査 17: 通常 Start の保存画像と子ウィンドウ

通常 Start を開く公開 API trace では、XP が `DV2ControlHost` に
`WM_PRINT` / `PRF_CHILDREN | PRF_OWNED | PRF_NONCLIENT | PRF_CLIENT | PRF_ERASEBKGND`
を送り、得た bitmap をメニュー全体へ転送していた。
Wine 10.0 の `dlls/win32u/defwnd.c` は子・所有・非クライアントの flags を未対応として無視する。
実際の trace も親への `WM_ERASEBKGND` / `WM_PRINTCLIENT` だけで、子への配信がなかった。
この保存画像が既に描いた子を覆い、一覧の一部だけが後から再描画されていた。
**判断: 外部シェル opt-in の Wine 側で WM_PRINT の子・所有ウィンドウと枠を描く（0012）。**
子は背面から描き、座標と親の clip を反映し、呼出元 DC の状態を保存・復元する。
Microsoft の window procedure や機械語を解析・変更した結果ではない。
[公開 WM_PRINT 契約](https://learn.microsoft.com/en-us/windows/win32/gdi/wm-print) と
Wine の公開ソースを照合した。

自作 `tests/print-window.c` は MS DLL を使わず、子・孫・所有ウィンドウ、
隠れたウィンドウ、境界からはみ出す子、重なり、枠、DC の viewport / clip の維持を検査する。
修正前の失敗 `.local/start-print-before-test.log`、修正後の成功
`.local/start-print-after-test.log` を記録した。

## 調査 18: Luna のフォントと owner-draw の clip

Luna のユーザー名が途中で切れ、テーマの LOGFONT の高さが0になっていた。
公開 `GetThemeFont` は NULL DC を許すが、Wine の `MSSTYLES_GetPropertyFont` に
そのまま NULL を渡すと `GetDeviceCaps(NULL, LOGPIXELSY)` が0になり、ポイント数も0へ変換される。
**判断: builtin uxtheme の GetThemeFont に画面 DC の既定値を補う（0015）。**
既存 GetThemeSysFont と同じ方法で一時 DC を取得・解放し、明示 DC の挙動は変えない。
[公開 GetThemeFont 契約](https://learn.microsoft.com/en-us/windows/win32/api/uxtheme/nf-uxtheme-getthemefont) を参照。
`tests/theme-font.c` はローカル Luna の StartPanel の2種類のフォントについて、
明示 DC と NULL DC の高さ・名前・weight / italic が一致することを確認する。
修正前は高さ -19 / -11 に対し NULL だけ0、修正後は成功。
ログ `.local/theme-font-before-test.log` / `theme-font-after-test.log`。
実画面でもユーザー名全体の表示を確認した（`.local/luna-font-fixed.png`）。

「すべてのプログラム」は owner-draw の WM_DRAWITEM も DrawTextW も呼ばれていたが、
描画時の clip が空だった。親の保存画像用 DC は viewport が (7,377) / (8,408) に移動している。
Wine の `set_control_clipping` が論理 client rectangle を再度 DPtoLP し、
無関係な負の位置へずらした rectangle と交差させていた。
**判断: 外部シェル opt-in かつ MM_TEXT では論理 rectangle を直接 clip に使う（0017）。**
通常 Wine と別の mapping mode の既存変換は維持する。
自作試験は builtin owner-draw Button / Static の保存画像も検査する。
修正後の実 DrawText trace では clip が (0,0)-(192,30) / (0,0)-(174,24) となり、
文字・font・rectangle が有効なまま描画できる。
診断のための Wine text.c の追加 trace は取り除き、クリーンな再ビルドでも
`.local/start-clean-public-tests.log` の公開 API 試験が成功した。

## 調査 19: All Programs の COM 登録とローカルフォント

通常 Start の All Programs が要求する
`{3F6953F0-5359-47FC-BD99-9F2CB95A62FD}` の factory を、同じローカル ISO の
shell32.dll に対する公開 `DllGetClassObject` で確認した。
**判断: 最小 HKCR 登録を追加し、registry profile を version 3 に更新する。**
既存 prefix の移行は Wine の更新 timestamp と独立して行う。
独立した試験 prefix で該当 CLSID を消し version 2 に戻してから stage すると、
登録と version 3 が復元された。2回目は registry の hash / mtime を変えず、更新処理も行わない。
`.local/registry-v3-migration-test.log` / `registry-v3-check-result.log`。
生ログ `.local/programs-class-providers.log`。内部 COM 実装の解析や実機ハイブの移植は行わない。

Luna で使う Trebuchet の regular / bold / italic / bold italic を同じ ISO から追加抽出した。
`trebuc.ttf` / `trebucbd.ttf` / `trebucit.ttf` / `trebucbi.ttf` を無改変で専用 prefix の Fonts へ配置する。
資源は `.local/assets-luna` の27ファイルに限定し、フォントも Microsoft の原本と同様に Git へ含めない。
ダウンロード、代替元からの取得、フォントや msstyles の編集はしていない。
既存の抽出 integration 試験をこの利用者ローカル fixture でも実行した
（`.local/extraction-luna-test.log`）。


## 調査 20: テーマ適用時の実際の system font

GetThemeSysFont が正しい値を返すことだけでは、適用されたシステム寸法は保証できなかった。
Wine の `parse_handle_nonclient_font` は msstyles のポイント数（10 / 8）を、
LOGFONT の正のピクセル高さとしてそのまま SPI_SETNONCLIENTMETRICS / SPI_SETICONTITLELOGFONT に渡していた。
自作の同一プロセス内の適用・読戻し試験では、実際の system font は +10 / +8、
テーマ API は -13 / -11 となり、6種類すべてが一致しなかった。
**判断: テーマ適用用のフォントを、DPI-unaware で適用する前に96 DPI の負の文字高さへ変換する（0018）。**
caption / small caption / menu / status / message / icon title の全てを扱う。
その後の DPI 拡大は既存の SystemParametersInfo に任せる。

`tests/theme-metrics.c` は builtin Wine のテーマ適用後に実際の SPI の font を読み、
classic → Luna の往復を2回、96 / 144 / 192 DPI で検査する。
caption は96 / 144 DPI で GetThemeSysFont の値と一致する。
高 DPI では96 DPIで丸めた SPI の高さを拡大するため、ポイント数から直接換算する
テーマ API と1px異なる場合があることも確認する。
修正前 `.local/theme-applied-font-before-test.log`、修正後
`.local/theme-applied-font-after-test.log`。Microsoft の font やテーマの内容は変更していない。

## 調査 21: 重なる Wine パッチの再適用判定

0014 は0012で加えた関数の内部も変更するため、完成したソース上で0012だけを
reverse --check すると失敗する。これはパッチ未適用の証拠にはならない。
**判断: touched source のコピーだけで逆順に戻し、順に再構成してから不足分を適用する。**
`wine/patch-state.py` は利用者のソースを reset せず、変更内容を破棄しない。
再構成で本当の衝突があれば、実ソースへ書く前に停止する。
固定 upstream から18本を順次適用した内容が実際のビルド用ソースと一致することを検査した。
未適用、11本適用、18本適用、17→18本の更新、hunk 外の独自変更を持つソースで、
不足分の適用・再実行時の no-op・独自変更の維持を確認した。
`.local/luna-patch-series-check.log` / `patch-state-unit-tests.log`。

## 22. My Computer のアイコン参照と registry profile version 4

新規 prefix の My Computer CLSID には `DefaultIcon` がなく、Wine builtin は
自分の既定アイコンを補う。native XP の公開 `IExtractIconW::GetIconLocation` は
`explorer.exe,0` を返したが、`Extract` は S_FALSE、大小アイコンとも NULL だった。
XP Explorer は Wine の desktop manager を残すため `C:\xp\xp-explorer.exe` に配置している。
同じ公開 API が返した index 0 をこの実ファイルへ明示すると、大小とも S_OK で取得できた。

**判断: version 4 は My Computer の `DefaultIcon` だけを
`C:\xp\xp-explorer.exe,0` に登録する。** Wine 生成の `InprocServer32` は変更しない。
これは Microsoft 公開の [namespace root icon 登録](https://learn.microsoft.com/en-us/windows/win32/shell/nse-implement)
に従う設定変更で、利用者の Explorer を変更・再配布しない。コード解析はしていない。
公開 API の比較ログは `.local/namespace-icon-test.log` と
`.local/namespace-icon-registered-test.log` に保存した。

version 4 は Microsoft 公開の [Pin to Start Menu handler CLSID](https://learn.microsoft.com/en-us/windows/win32/shell/reg-shell-exts)
を `exefile` と `lnkfile` の `shellex\ContextMenuHandlers\StartMenuPin` にも関連付ける。
factory だけの登録では All Programs の shortcut menu に Pin が出なかった。
利用者の操作で Windows Notepad を pin すると左列に実際のアイコンと名前が描かれ、
そこから起動できた。専用 session 再起動後の pin 保持と、Unpin 後の削除も確認した。
既定 pin や MRU を捏造する処理は追加しない。画面は
`.local/start-fidelity-icons-open.png`、`.local/start-fidelity-pinned.png`、
`.local/start-fidelity-pin-persisted.png`、`.local/start-fidelity-pin-removed.png`。
version 3 から4への移行で icon/handler 登録が復元され、2回目の staging は registry の
hash と mtime を変えなかった。My Computer factory も元の Wine 登録を保持した。

## 調査 23: テーマ付きスクロールバーの片側無効化

Wine 自身の `uxtheme/scrollbar.c` はウィンドウ全体の無効化だけを描画に反映し、
公開 `EnableScrollBar` の片側無効化 flags を無視していた。
自作 `tests/scrollbar-theme.c` は上下・左右の実際の非クライアント矢印を取得し、
同じローカルテーマの公開 `DrawThemeBackground` の結果と比較する。
修正前は無効な上・左矢印が各217 pixels異なり、有効な下・右は一致した。
**判断: Wine のテーマ描画で各矢印の disable flags を反映する（0020）。**
修正後は有効・片側無効2種類・両側無効・ウィンドウ全体無効の20比較すべて一致した。
`.local/scrollbar-theme-after-test.log`。テーマ画像や Microsoft コードは変更していない。

## 調査 24: native 時計の Luna 描画と時刻更新

公開 API trace では native TrayClockWClass が通常の Clock theme を開いた後、
TrayNotify app theme を設定して初期 handle を閉じ、以後テーマを開き直さなかった。
親 TrayNotifyWnd の WM_PRINTCLIENT は正しい Luna 背景を返すが、時計自身が白背景と黒文字を重ねる。
**判断: 外部シェル opt-in の当該時計だけ、Wine 側でテーマ描画を補う（0019）。**
元の WM_PAINT は時刻の整形・更新も行うため、先に元の処理を呼び、その WM_GETTEXT を使う。
WM_TIMER・クリック・tooltip・時刻形式は元の処理を維持する。
親の実背景と TrayNotify::Clock の実フォント・文字色を公開テーマ API で描く。
classic / テーマ無効時は元の処理に戻し、独自 theme handle と subclass は破棄時に回収する。
自作 clock-theme 試験は native 相当の WM_PAINT による時刻更新、translated print、
DC 状態・clip、timer / click の委譲、theme 切替を確認する。

## 調査 25: native タスクバー ReBar のグリップ

RB_GETBANDINFOW / RB_GETRECT で取得した9pxの header が classic 表示と一致した。
native ReBar は Taskbar app theme 設定時に初期 theme を閉じ、テーマ part を描かない。
公開 OpenThemeData の Taskbar::Rebar は正しいグリップを返す。
**判断: 外部シェルのタスクバー直下 ReBar の非表示 title header だけを補描する（0021）。**
元のバンド配置・入力・drag 処理を維持し、親のテーマ背景と実 part 寸法を使う。
translated DC・native paint/input 戻り値・無効化・描画中の破棄を自作 rebar-theme 試験で確認。
Start と通知領域の脇にある ReBar 外の細い区切りは残件で、全境界の一致とはしない。

## 26. Wine のフォント属性 tokenizer と実フォントの再適用

Wine の `MSSTYLES_GetNextToken` は末尾空白を除く際に token の直後を参照していた。
length-delimited な値の終端を越えて読む可能性があり、末尾に空白のある名前・属性も残った。
さらに font parser は comma で区切った属性欄全体を `bold` / `italic` 等と比較するため、
複数の属性を空白で並べた欄を処理できなかった。

**判断: 0024 は有効範囲の内側で末尾を trim し、font 属性だけは comma と空白の両方で区切る。**
フォント名内の空白は保持し、テーマが指定する名前と point size は変更しない。
`tests/font_parser.py` は Wine の実 parser 関数を一時 translation unit に取り込み、
自作の `PTHEME_PROPERTY` 文字列 fixture を検査する。Microsoft の theme 資源やコードは読まない。
comma-separated 属性は修正前も成功したが、combined attributes と末尾空白は4件失敗し、
修正後は7件とも成功した。NUL 終端を持たない bounded token と、その直後のアクセス禁止 page
も追加して範囲外参照がないことを確認した。
`.local/msstyles-font-fixture-before.log` / `msstyles-font-fixture-after.log`、
`.local/font-parser-regression.log` に結果がある。

実 Luna の公開 API では caption の weight が400から700、Start::Button の italic が0から1になった。
既存 prefix の保存済み SPI metrics は自動的には変わらないため、Luna の再適用後にも検査した。
再適用後の SPI caption は `Trebuchet MS / -13 / weight 700` で `GetThemeSysFont` と一致し、
menu / message / status は `Tahoma / -11 / weight 400` を維持する（96 DPI）。
Start::Button の font は公開 `GetThemeFont` で `Franklin Gothic Medium / -19 / italic 1` を返す。
`.local/shell-font-probe-after0024.log`、`start-font-parser-fixed.png`、
`caption-font-parser-themed.png` に API と実表示を記録した。
6種の system font の classic/Luna 往復と optional NULL DC の公開試験も成功した。
これは Wine の解釈と実描画の改善であり、実 XP OS との一致は別の reference 試験で確認する。

## 27. 別プロセスの公開 WM_GETICON と borrowed HICON

Wine 10 の自身の `win32u/message.c` は別プロセスの `WM_GETICON` を拒否し、
`cursoricon.c` は foreign USER handle を拒否していた。proxy が正しい icon を所有しても、
native XP タスクバーが公開 API でその画素を取得できないため、既定 icon へ戻っていた。
Microsoft バイナリの内部コードは調べていない。

**判断: 0025 は外部シェル opt-in 時に限り、公開 API の読み取り経路を実装する。**
[WM_GETICON](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-geticon) の戻り値は元の borrowed HICON のままにし、
server に登録した不変画素 snapshot を必要な読み手だけに materialize する。
[GetIconInfo](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-geticoninfo) が返す bitmap と
`CopyIcon` の結果は読み手が所有する。元の foreign icon の mutation / destruction は許可しない。
共有 `LoadImage` icon を破棄しないという
[DestroyIcon](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-destroyicon) の規則も維持する。
`WM_SETICON` や他の GDI handle message のプロセス間制限は解除しない。

server は owner、USER handle の generation、生存、画像寸法、payload 長を検証する。
static icon の上限は256×256、resource metadata は各2048 bytes、snapshot は16 MiB/owner と64 MiB/server。
owner の破棄・終了時に server の画素を解放する。読み手の cache は最大64個で、利用中の bitmap を pin し、
次の読み取りごとに生存を再確認する。animated icon と上限を超える icon は共有対象外で、所有プロセス内の API は維持する。
opt-in がない通常 Wine では従来どおり foreign message / icon 読み取りを拒否する。

`tests/foreign-icons.c` は自作画素だけで2プロセスを起動し、元の HICON identity、`GetIconInfo`、
半透明色・monochrome AND/XOR の `DrawIconEx`、独立した `CopyIcon`、foreign 破棄拒否、
80個の cache pressure、100回の handle 再利用、owner 破棄・明示破棄なしのプロセス終了を検査する。
257px icon は owner 内で使え、foreign snapshot だけ拒否された。
256px alpha icon 40個のうち31個だけが共有され、owner 内では全て利用できた。
`.local/foreign-icons-enabled.log` と `foreign-icons-disabled.log` はいずれも PASS。
同一 wineserver を維持して owner の起動・大量共有・強制終了を5回繰り返しても毎回31個を共有でき、
終了済み owner の snapshot が server の64 MiB上限を消費し続けないことも確認した
（`.local/foreign-icons-owner-cleanup.log`）。
実行は専用 X display と専用 `WINEPREFIX` を指定し、`foreign-icons-test.exe` を
`WIN4WINE_EXTERNAL_SHELL=1` の有無それぞれで起動する。

0025 は Wine server protocol を変更する。wineserver、ntdll、win32u が混在しない完全 build を使い、
利用中の prefix server を停止してから切り替える。検証では独立した source / build / install を使用し、
元の共有 Wine install を上書きしていない。これにより Microsoft DLL の修正・再配布は発生しない。

追加の callback-first fixture は、読み手が GDI を使う前に `GetIconInfo` を呼んだときの
Wine の初回 display driver 初期化を検査する。既存 `copy_bitmap` が USER lock 中に最初の
compatible DC を作って assertion になる経路を確認したため、cache miss 時の初回 DC 作成と
bitmap import を lock 外で行い、その後 owner / generation と cache の競合登録を再確認する。
`SendMessageTimeout` / `SendMessageCallback`、同時2 thread の cache 読み取り・退避も成功した。
専用 XP prefix ではローカルの元の native `comctl32.dll` を明示して公開 `ImageList_ReplaceIcon` /
`ImageList_GetIcon` を実行し、foreign と local の描画一致を確認した
（`.local/foreign-icons-native-comctl.log`）。Microsoft DLL の内部解析はしていない。
実 XP タスクバーは非同期 callback 後に固有 icon を表示した。settled screenshot の task button 内に
16×16の赤色256画素があり、property 更新後は16×16の緑色256画素に変わった
（`.local/native-icons-taskbar-0025-settled.png` / `native-icons-taskbar-update-0025.png`）。
callback 前の瞬間的な既定画像と混同しないよう、`XP_FRAME_TASKBAR_ICONS=1` の native frame 試験は
公開画面の taskbar 内に初期 icon と更新 icon の画素が現れるまで bounded wait で検査する。

## 28. 公開 API で実測した日本語 XP 字体の prefix profile

ユーザーの実 XP と同一 SHA256 のローカル Luna を公開 API で比較したところ、Wine の
一般的な言語 resource 選択試験、UI language、SPI 変更時の挙動を合わせても字体差が残った。
Microsoft 実装の解析やテーマ資産の改変で原因を推定せず、実 XP の公開 `GetThemeSysFont` /
`GetThemeFont` / SPI 測定値を、自作の明示的な prefix 設定として保存する。
Luna は MS UI Gothic、96 DPI で caption -13 bold、small caption -12 bold、
menu/status/message/icon -12 regular、Start の themed font -16 bold italic だった。
`reference-metrics.c` の Shell_TrayWnd 子 Button の `WM_GETFONT` は Start ボタンの可能性があり、
これをアプリケーション task button の字体と断定しない。

registry profile version 5 は `HKCU\Software\Win4Wine\ThemeProfiles\JapaneseXPLuna` に
Enabled/Version、ThemePath/ColorName/SizeName と7個の自作 LOGFONTW データを、値がない場合だけ追加する。
既存の Enabled=0、独自字体、不正値も上書きしない。0026 の Wine builtin uxtheme が
`WIN4WINE_EXTERNAL_SHELL=1`、active theme と path/color/size の一致、正常な既存 API query、
有効な profile データを満たす場合だけ使う。DLL に固定字体は埋め込まない。
NULL theme の `GetThemeSysFont` は従来の SPI のままで、ユーザーが変更した SPI も維持する。
通常アプリ、別テーマ、Classic、無効化した profile は対象外である。
`create.py` / `display.py` は自作 theme-settings helper の呼び出しにだけ opt-in を追加する。

実 XP Classic の公開 SPI は6字体とも MS UI Gothic -12、caption/small のみ bold だった。
新規 prefix 作成時だけ別の `JapaneseXPClassic` key に6字体を生成し、helper の
`classic-initialize` が Apply(NULL) → 公開 SPI で字体設定 → Luna apply → Apply(NULL) を
同一プロセスで実行する。Wine 自身に新しい Classic baseline を保存・復元させ、非公開の
ThemeManager 保存値は直接編集しない。通常の `classic` / `luna` 切替は従来の Apply(NULL) を維持し、
既存 prefix の migration は Classic 字体を初期化しない。新規作成前に必要なローカル
msgothic.ttc と Luna msstyles の存在を検証する。これらの資産は従来どおり `.local/` 内だけに置く。

`tests/font-profile.c` は96/144/192 DPI、opt-in 有無、profile 無効化・独自字体・欠落・不正型・
不正 LOGFONT、path/color/size/version 不一致、普通の Button、NULL/無効 theme handle、
一時的な SPI 上書きを検査し、すべて成功した（`.local/font-profile-final-public-tests.log`）。
実 migration は独自設定を保持し、繰り返し stage は registry の hash/mtime も変更しなかった。
新規 prefix はサーバー停止後の別プロセスでも Classic6字体と caption18×18 を読み取り、
Luna 往復後も維持した（`.local/fresh-japanese-classic-public-test.log`）。
既存の独自 Classic6字体も別プロセス、Luna 往復、v4→v5 migration を通して維持した
（`.local/existing-classic-custom-preservation-test.log`）。`tests/theme-metrics.c` は
テーマ適用字体に加えて Classic6字体の往復前後の意味的な一致も検査する。

この実画素検査と全 frame gesture 試験は Luna / classic の両方で成功した
（`.local/native-icons-taskbar-0025-pixels.log` / `native-icons-taskbar-0025-classic.log`）。
classic の taskbar 外枠は root の外へはみ出すため、画面に見える範囲へ clip して取得する。


## 29. 実 XP の公開キャプション入力・描画位置

自作ウィンドウの公開 `WM_NCHITTEST` と `GetWindowDC`、公開 `DrawThemeBackground` の
出力だけを比較した。Microsoft のテーマ内部や実装は調べていない。
実 XP の画像は Wine より1px左・1px下で、クリック領域は画像より上下に広く、
最大化と閉じるのクリック領域は接していた。固定サイズ枠でも同じ余白関係だった。
**判断: 0027 は外部シェルの標準テーマ枠に限り、描画画像と入力余白を独立に扱う。**
画像寸法・状態画像は引き続き利用者のテーマを公開 API で読む。
子・tool window・Classic・非opt-in は従来の経路を維持する。
96 DPI は実 XP の独立fixtureと一致し、144/192は選んだ拡大規則で回帰検査した。
高 DPI の実 XP 一致や文字のラスタライズの完全一致とはしない。
