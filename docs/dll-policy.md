# DLL 選択と調査

| DLL / EXE | 初期選択 | 理由 |
|---|---|---|
| explorer.exe | native XP | 目的のタスクバー・スタート・デスクトップの実体 |
| browseui | native XP | builtin の ordinal 107 が起動直後に未実装中断。単純な成功応答ではフォルダ構築を代替できない |
| shdocvw | native XP | browseui と同世代のフォルダホスト・内部インターフェースを使う |
| shell32 | native XP | browseui/shdocvw と同じ XP の名前空間と内部 ordinal の組を保つ |
| shlwapi | native XP | shell32/browseui の XP 固有 ordinal 依存を同世代で解決する |
| comctl32 | XP shell のみ native v6 | SP3 の 6.0.2600.5512 を C:\xp に配置。Button は Wine 標準。通常アプリは builtin |
| uxtheme | builtin | クラシックで開始。native のテーマサービス依存を初期経路に入れない |
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
**判断: 今回の基準はクラシック表示とし、Luna の完全な描画を保留する。**
native uxtheme や XP の Themes サービスを追加して動作保証した扱いにはしない。
クラシックの 144/192 DPI では Start / Programs の文字と配置、200% の Linux 起動を実測。
DPI 非対応 XP アプリの拡大なので画質の改善は別の課題とする。

## 調査 13: Wine 更新後の native profile

Wine の prefix 自動更新は DLL と COM 登録を入れ替えるため、先に native DLL を配置するだけでは
次の Wine ビルドで壊れる。**判断: 更新と native profile の再配置を順序付ける。**
wine.inf と `.update-timestamp` を照合し、不一致時のみ builtin wineboot を完了させ、
自前 COM 登録を再適用し、専用 wineserver を終了してから native DLL を atomic 配置する。
VM で timestamp 不一致を作り、同じセッションから XP Shell / Start が再度動くことを確認した。
