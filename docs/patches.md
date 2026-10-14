# Wine のパッチ

`wine/patches/` には、未改変の Wine 11.0（`wine/upstream.env` の commit）に上から順に当てる 88 本のパッチがあります。
当てるのは `wine/build.sh` で、実際の適用は `wine/patch-state.py` が行います（途中まで当たったソースにも、足りない分だけを当てます）。

番号の付け直しや、以前の番号との対応表は [history/patch-history.md](history/patch-history.md) にあります。
古い記録に出てくるパッチ番号は、書いた時点の番号なので、そちらで読み替えてください。

## opt-in（`WIN4WINE_EXTERNAL_SHELL`）

XP のシェルのための変更は、環境変数 `WIN4WINE_EXTERNAL_SHELL` があるプロセスでだけ効きます。セッション（`session/supervisor.py`）と
prefix の補助ツールがこの変数を設定します。同じ Wine で普通の Windows のアプリを動かしても、挙動は変わりません。

- 判定は 0013 が加える `include/wine/win4wine.h` の `win4wine_external_shell()` に 1 つにまとめています。
  ntdll は Rtl の関数で、wow64.dll（0017）は ntdll の関数で同じ変数を読みます。
- opt-in なしで効くのは、文書化された Win32 の挙動の修正（0001〜0012、0064、0076）と、普通のアプリの見た目と動作を変えない 0019・0020、
  Wine に無かった関数を足す 0078、0080、0084 です。
- 0018 は `WIN4WINE_SESSION_REQUEST` があるときだけ効きます。
- 0063 は、さらに `WIN4WINE_FOLDER_SHELL` があるときだけ効きます（ReactOS のシェルのセッションだけが設定します）。
- ReactOS のシェルのセッションは `WIN4WINE_SHELL=reactos` も設定します。0047 はそのとき、XP のための部品の差し替えをしません。
- 0081、0083、0085 は `WIN4WINE_SHELL=win7` のときだけ効きます（Windows 7 のシェルの試作。0081 は wineserver が起動したときの環境で判断します）。
  0082 はそのとき 0053 と 0054 の XP のための置き換えをしません。

## 一覧

下の表の「opt-in」が「あり」のものは、上の環境変数があるときだけ効きます。

### Wine 一般の修正（0001〜0008）

文書化された挙動に合わせる修正で、すべての Wine のアプリに効きます。

| 番号 | 内容 | opt-in |
|---|---|---|
| 0001 | uxtheme: GetThemeSysColor の色番号をテーマの色に対応させる | なし |
| 0002 | uxtheme: GetThemeFont で DC を省略できるようにする | なし |
| 0003 | uxtheme: テーマの字体をシステムに文字の高さ（画素）で適用する | なし |
| 0004 | uxtheme: 空白で区切ったテーマの字体の属性を読む | なし |
| 0005 | uxtheme: 片側だけ無効にしたスクロールバーの矢印を無効の形で描く | なし |
| 0006 | uxtheme: SetWindowTheme で空の文字列を受け付ける | なし |
| 0007 | uxtheme: 共有のテーマ画像の読み込みと転送を 1 スレッドずつにする | なし |
| 0008 | win32u: caption の右クリックを離した位置を WM_NCHITTEST で判定する | なし |

### XP の部品が呼ぶ API（0009〜0012）

| 番号 | 内容 | opt-in |
|---|---|---|
| 0009 | services: XP の explorer が待つ SCM の準備完了のイベントを出す | なし |
| 0010 | setupapi: ドッキングステーションが無いと答える | なし |
| 0011 | wuaueng: 終了時の更新の確認に「更新なし」と答える | なし |
| 0012 | user32: DestroyReasons を加える | なし |

### 外部シェルの基盤（0013〜0018）

| 番号 | 内容 | opt-in |
|---|---|---|
| 0013 | include: opt-in の判定を加える | — |
| 0014 | explorer: シェルのウィンドウと endpoint を外部のシェルに任せる | あり |
| 0015 | kernelbase: explorer.exe の名前を、名前を変えた XP のシェルに解決する | あり |
| 0016 | ntdll: comctl32 を実行ファイルの隣から読ませる | あり |
| 0017 | wow64: XP のシェルに WOW64 で動いていることを知らせない | あり（XP の explorer だけ） |
| 0018 | user32: ログオフ・終了の要求をセッションに知らせる | `WIN4WINE_SESSION_REQUEST` |

### bridge（WM と proxy）との連携と X11 の表示（0019〜0029）

| 番号 | 内容 | opt-in |
|---|---|---|
| 0019 | winex11: X の窓に Win32 のクラスと HWND を付ける（WM が読む） | なし |
| 0020 | user32: shell hook の登録を proxy のために記録する | なし |
| 0021 | win32u: 別のプロセスの標準のシステムメニューをタスクバーに返す | あり |
| 0022 | server, win32u: 別のプロセスのウィンドウのアイコンをタスクバーが読めるようにする | あり |
| 0023 | win32u: Ctrl+Esc と Windows キーでスタートメニューを開く | あり |
| 0024 | winex11: 画面モードの変更なしに作業領域を更新する | あり |
| 0025 | winex11: 最大化の大きさを窓の持ち主が決める | あり |
| 0026 | winex11: 自分で描かないポップアップの下の画面を残す | あり |
| 0027 | winex11: CS_DROPSHADOW の窓の下に XP の影を描く（画面を読めない Xwayland では、透明度つきの黒い窓で描く） | あり |
| 0028 | shell32: 通知領域のアイコンを XP の形式で送る | あり |
| 0029 | winex11: XIC を作る前に指定された変換位置を覚える | あり |

### ウィンドウの配置、UI 状態、メニュー、WM_PRINT、壁紙（0030〜0035）

| 番号 | 内容 | opt-in |
|---|---|---|
| 0030 | win32u: 位置を指定しない窓を XP と同じく重ねて置く | あり |
| 0031 | win32u: キーボードの手がかり（下線やフォーカスの枠）の UI 状態を持つ | あり |
| 0032 | win32u: ポップアップメニューを XP と同じく並べて描く | あり |
| 0033 | user32: WM_PRINT の画像に子孫の窓も描く | あり |
| 0034 | user32: ReBar の既定の配置を明示する | あり |
| 0035 | user32: 拡大の壁紙を描く（ReactOS のシェルの「合わせる」「塗りつぶす」も） | あり |

### 文字と GDI の画素（0036〜0044）

| 番号 | 内容 | opt-in |
|---|---|---|
| 0036 | win32u: 白黒の文字の送り幅を測った値にする | あり |
| 0037 | user32: DrawText の中央揃え、折り返し、下線を XP に合わせる | あり |
| 0038 | win32u: クリップ中の HALFTONE の座標の対応を保つ | あり |
| 0039 | win32u: TransparentBlt の拡大縮小を XP に合わせる | あり |
| 0040 | user32: クラシックの caption の記号の大きさと位置を XP に合わせる | あり |
| 0041 | user32: アイコンのアルファの乗算を XP に合わせる | あり |
| 0042 | conhost: 2 バイトの文字に 2 升を使う | あり |
| 0043 | win32u: まだ描かれていない最上位の窓から下の画面を見せる | あり |
| 0044 | win32u: system DPI で描き、アイコンの大きさを XP に合わせる | あり |

### コントロール（0045〜0048）

| 番号 | 内容 | opt-in |
|---|---|---|
| 0045 | user32: クラシックの押しボタンの理想の大きさを測る | あり |
| 0046 | user32, comctl32: スタートボタンを XP と同じく描き、大きさを決める | あり |
| 0047 | user32, comctl32: XP のシェルのコントロールを Wine のテーマ付きの実装で作る | あり |
| 0048 | comctl32: テーマ付きのチェックボックスとグループボックスの文字の位置を XP に合わせる | あり |

### uxtheme（0049〜0061）

| 番号 | 内容 | opt-in |
|---|---|---|
| 0049 | uxtheme, user32: テーマ付きのウィンドウの枠を描く | あり |
| 0050 | uxtheme: caption の形で窓の形を決める | あり |
| 0051 | uxtheme, user32: Luna の枠を必要な部分だけ描き直す | あり |
| 0052 | user32, uxtheme: caption のヘルプボタンを出して追う | あり |
| 0053 | uxtheme: テーマ画像の拡大縮小、配置、選び方を XP に合わせる | あり |
| 0054 | uxtheme: テーマ画像のアルファの乗算を XP に合わせる | あり |
| 0055 | uxtheme: SetWindowTheme で窓ごとのテーマの変更番号を送る | あり |
| 0056 | uxtheme: 日本語の XP の Luna の字体の設定（prefix にあるもの）を使う | あり |
| 0057 | uxtheme: 地域版の msstyles の文字列表にある値を使う | あり |
| 0058 | uxtheme: スタートメニューの利用者名の連続した影を描く | あり |
| 0059 | uxtheme: タスクバーの子のテーマ付きの背景を描く | あり |
| 0060 | uxtheme: 通知領域の時計をテーマで描く | あり |
| 0061 | uxtheme: タスクバーの ReBar のつまみと末尾の余白を描く | あり |

### Wayland のセッション（0062）

| 番号 | 内容 | opt-in |
|---|---|---|
| 0062 | winex11: rootless の X サーバー（Xwayland）では、画面の読み取りを各窓の中身から作る（ログオフの灰色の画面） | あり |

labwc と wlroots のパッチは `wayland/patches/` にあります（[wayland.md](wayland.md)）。書式は Wine のパッチと同じです。

### ReactOS のシェル（0063〜0066）

| 番号 | 内容 | opt-in |
|---|---|---|
| 0063 | explorer: `explorer.exe` で開くフォルダの窓を、外部のシェルの explorer に渡す（`WIN4WINE_FOLDER_SHELL` があるときだけ） | あり |
| 0064 | uxtheme: テーマの余白（MARGINS）の負の値を正しく読む（ReactOS のテーマのスタートボタン。XP の Luna に負の余白は無い） | なし |
| 0065 | user32: SwitchToThisWindow で窓を前面にもする（ReactOS のタスクバー。Wayland のセッションで戻した窓が有効にならなかった） | あり |
| 0066 | user32: ダイアログの部品のリソースの番号を、ReactOS と同じ「#番号」の名前で渡す（ReactOS の Static がバナーとアイコンを読めるように。`WIN4WINE_SHELL=reactos` のときだけ） | あり |

### その後の修正（0067〜0077）

| 番号 | 内容 | opt-in |
|---|---|---|
| 0067 | win32u: X が見えるようにした窓の部分を、すぐ画面に出す（ドラッグした窓の下の窓が、次のメッセージまで黒いままだった） | あり |
| 0068 | win32u: 「クリック後にメニュー項目をフェードアウトする」を無効と答える（窓の不透明度を出せないので、選んだ項目がログオフの灰色の画面に残った） | あり |
| 0069 | winex11: on-the-spot の入力でも、キャレットの行の下端を入力メソッドに渡す（候補の窓を入力欄の下に出す） | あり |
| 0070 | imm32: 消えた IME の UI の窓を作り直す（XP の「ファイル名を指定して実行」の 2 回目から、変換中の文字が出なかった） | あり |
| 0071 | win32u: Windows キーだけか、ほかのキーと一緒かを、サーバーのキーの反転ビットで見分ける（0023 の判定が、Win+R の R の離しを別のスレッドより先に Windows キーの離しを処理して誤り、開いたダイアログが有効にならなかった） | あり |
| 0072 | uxtheme: テーマを当てたときのメニューバーの高さを XP と同じ 19（96 DPI）から始める（Luna はクラシックより 1 高い。英語版の Tahoma では 18 のままだった） | あり |
| 0073 | win32u: 窓のドラッグを EVENT_SYSTEM_MOVESIZESTART/END と `Win4WineSizeMove` で知らせ、寄せた窓を引き剥がすと元の大きさに戻す。`WIN4WINE_WINDOW_SNAP=1` では最大化した窓もドラッグで元の大きさに戻す（proxy の窓の寄せ。`window_snap` のときだけ使われます） | あり |
| 0074 | winex11: メモリー DC を作るたびに X に送り出さない（GC を作る要求は、それを使う要求と一緒に出ます。テーマの描画はメモリー DC をいくつも作るので、そのたびに X サーバーを起こしていた） | あり |
| 0075 | uxtheme, user32: Luna の枠とテーマの部品を、少ない転送で描く。8・24 bpp のテーマ画像を 32 bpp で持つ、0053 の XP の拡大を列ごとの転送なしで行う、窓の DC への色キー付きの部品を AlphaBlend 1 回で描く、大きさだけ変わったときに枠を二度描かない、Luna の caption の下にクラシックの caption を描かない（窓の大きさを変えるときに、枠を描く回数がおよそ 2 倍に。画素は変わりません） | あり |
| 0076 | uxtheme: DrawThemeParentBackground で、親の背景を子の窓の位置に描く（範囲は切り抜きにだけ使う。XP と同じ。フォルダの窓のツールバーで、状態の変わったボタンだけを描き直すと、無効になった「移動」「コピー」の背景が黒くなった） | なし |
| 0077 | win32u: XP の ClearType（`FontSmoothingType` が 2）を、XP と同じく描いて測る。fontconfig と X の設定より prefix の設定を優先する、埋め込みビットマップのある大きさ（MS UI Gothic の 22 ピクセルまで）は標準と同じに描く、ほかは横を 6 倍にして白黒で描き、サブピクセルごとに 6 つのサンプルを数える（7 段階。ガンマ 2.2）、字幅は標準と同じ白黒の字幅（ClearType にしても行の長さが変わらない） | あり |

### Windows 7 のシェル（0078〜）

| 番号 | 内容 | opt-in |
|---|---|---|
| 0078 | dlls: Windows 7 の explorer と shell32 などが import する関数を足す（ntdll の WinSqm*・ShipAssert・RtlSetEnvironmentVar、user32 の CalculatePopupWindowPosition など、kernel32 の CheckElevation など、kernelbase の LoadStringByReference、winsta の WinStationIsSessionRemoteable、64 ビットの oleaut32 の NDR64 の名前、rpcrt4 の NdrStubCall3、gdi32 の CreateBitmapFromDxSurface）。足すだけで、Wine にあった関数は変わらない | なし |
| 0079 | uxtheme: Windows Vista と 7 のテーマ（msstyles の形式 4。VMAP、CMAP、VARIANT のバイナリのプロパティ、MUI の文字列表、PNG の画像）を読む。値は形式 3 の ini と同じテキストにして、今のクラス・パーツ・ステートに入れる（Windows 7 Basic の aero.msstyles）。クラスは BCMAP の基底のクラスから足りない値を取る（Tooltip は TooltipStyle から。無いと DirectUI がフォルダの窓を作るのを止めた）。形式 3（XP、ReactOS）の読み方は変わらない | なし |
| 0080 | user32, samlib: Windows 7 のスタートメニューが開くようにする。user32 に RegisterMessagePumpHook・UnregisterMessagePumpHook を足す（フックに user32 の関数の表を渡して登録だけを数える。ReactOS と同じく、メッセージの関数はフックを通さない。無いと検索欄の部分の作成が失敗し、スタートメニュー全体が壊される）。samlib の SamConnect は失敗を返し（SAM は無い）、SamCloseHandle と SamFreeMemory は成功を返す | なし |
| 0081 | server: ファイルとフォルダに付けた DACL を拡張属性 `user.wine.dacl` に残し、セキュリティを読むときにそれを返す（所有者とグループは今までどおり Unix の所有者から）。Windows 7 の shell32 はごみ箱のフォルダの DACL を自分の付けたものと比べ、違うと「ごみ箱が壊れています」と出す | あり（`WIN4WINE_SHELL=win7`） |
| 0082 | uxtheme: Windows 7 のタスクバーが自分で通知領域と時計を描く。`WIN4WINE_SHELL=win7` では 0053（DrawThemeParentBackground で通知領域の背景だけを描く）と 0054（時計の差し替え）をしない（Windows 7 の通知領域の背景は半透明で、黒くなり、時刻が二重になった）。BeginBufferedPaint の BP_PAINTPARAMS（BPPF_ERASE、prcExclude、pBlendFunction での AlphaBlend）、BufferedPaintClear、BufferedPaintSetAlpha を文書どおりに | 一部あり（`WIN4WINE_SHELL=win7`） |
| 0083 | win32u, winex11, comctl32: Windows 7 のスタートボタン（タスクバーが持ち主のレイヤードウィンドウ）とスタートメニューのユーザーの写真を描く。半透明の画素を持ち主の窓が出している画素（その surface の写し）と合成し、持ち主の外やまだ描かれていない所ではアルファが 128 未満の画素を抜いてほかは乗算を戻す（元の画像を覚えておき、持ち主が新しく描いたら合成し直す）（合成のマネージャーが無いと縁が黒いギザギザになった）。ホバーで窓が伸びるときは、新しい画像を描いてから大きさを変え、その後で送り出す。窓は WM の管理にせず、大きさを変えたら画像の前に X と同期する（伸びた所が一瞬黒くなった）。0040 のスタートボタンのクラスは、レイヤードの窓には描かない | あり（`WIN4WINE_SHELL=win7`） |
| 0084 | apphelp: AllowPermLayer（互換モードの層を使ってよいか。ネットワークのパスとリモートのドライブでは FALSE。ReactOS と同じ）。Windows 7 のスタートメニューからプログラムを起動すると呼ばれ、stub ではシェルが落ちた | なし |
| 0085 | shell32: 通知領域のアイコンを Windows 7 のタスクバーの形式で送る（1484 バイト: 0056 の XP の形、32bit のバルーンのアイコン、呼び出したプロセスのモジュールのパス。Windows 7 の shell32 とおとりのタスクバーの窓で測った）。XP の形は拒まれていた | あり（`WIN4WINE_SHELL=win7`） |
| 0086 | dwmapi, combase, ole32: Windows キーとフォルダの窓のために。x64 で DwmpActivateLivePreview（ordinal 113）を S_OK に（合成が無いのでプレビューは無い。stub ではシェルが Windows キーで落ちた）。`WIN4WINE_SHELL=win7` で、フリースレッドのマーシャラーが同じプロセス内では QueryInterface で得られないインターフェースもそのまま渡す（「ファイル システムのエラー」になっていた）。x64 の ole32 に HWND などの _User*64（NDR64 の名前）を通常の関数として | 一部あり（`WIN4WINE_SHELL=win7`） |
| 0087 | xmllite: 16 KB を超える UTF-16 の入力を読む。UTF-16 のときだけ、変換した生のバッファを空けておらず、3 回目の読み込みで同じ 8 KB をもう一度写していた（Windows 7 の DirectUI が UTF-16 で渡すフォルダの窓の UIFILE が途中で壊れ、Win+E の窓が出る前に閉じた）。Wine の xmllite の reader のテスト（2145 件）は通る | なし |
| 0088 | ieframe, dsrole, samcli, ntshrui: Windows 7 のフォルダの窓のために。ieframe に ordinal 158・159（SHRestricted2A/W。Windows 7 の shdocvw が転送する）。新しい組み込みの dsrole.dll と samcli.dll（netapi32 への転送）と ntshrui.dll（共有しているフォルダは無い、と答える）。Windows 7 の shell32 と linkinfo が遅延読み込みする | なし |

## 並び順の考え方

まとまりは「opt-in の要らない修正 → XP の部品が呼ぶ API → 外部シェルの基盤 → bridge との連携 → ウィンドウ → 文字と画素 →
コントロール → uxtheme → Wayland のセッション → ReactOS のシェル → Windows 7 のシェル」の順で、その後の修正は末尾に足します。同じまとまりの中では、後のパッチが前のパッチのコードを使う順にしてあります
（例: 0050〜0052 は 0049 の枠の描画を前提にしています）。

## 書式

- ファイル名は `NNNN-<DLL>-[external-]<内容>.patch` です。`external` は opt-in のときだけ効くものに付けます。
- 先頭は `From:` と `Subject: [PATCH] <DLL>: <内容>` です。本文は英語で、次の項目のうち必要なものを書きます。
  - `Purpose:` 目的
  - `XP behavior:` 実際の XP（参照用の XP の VM で、公開 API と画面）で測った挙動
  - `Wine before:` 変更前の Wine の挙動と、その原因
  - `Change:` 変更の内容
  - `Evidence:` 根拠になる試験（`tests/`）や測定
  - `Scope:` 効く範囲（opt-in の有無、対象のウィンドウやクラス）
  - `Old patches:` 整理（2026-10-10）より前の番号
- 本文の後に `---` と diffstat、`git diff` の形の差分を置きます。

## パッチを足すとき

- まとまりに合う位置に入れ、番号を振り直します。既存のパッチを直す変更は、そのパッチに含めます。
- XP のシェルのための挙動は `win4wine_external_shell()` で限ります。文書化された Win32 の挙動の修正で、普通の Wine の
  アプリにも正しいものは、opt-in にしません（0001〜0012 と同じ扱い）。
- 未改変の Wine 11.0 に、`python3 wine/patch-state.py <ソース> wine/patches --apply` で全部当たることを確かめます。
- WoW64 のビルドでは PE のモジュールが i386 と x86_64 の両方になります。プロセスの間で受け渡す構造体は、handle の幅と
  大きさを固定してください（0028）。Unix 側の浮動小数点の結果は、32bit と 64bit で同じ丸めになるようにしてください（0027）。
- 新しいファイルを加えたときや、並びを整理した後は、新しいソースとビルドディレクトリで configure からビルドして確かめます。
  使い回したビルドディレクトリでは、Makefile の一覧の漏れや古いオブジェクトに気づけません。
