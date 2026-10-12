# Wayland と自作 compositor の検討（段階 10b）

2026-10-12。段階 10 の 10b として、bridge-wm（X11 のウィンドウマネージャー）に代わる Wayland compositor を比べた。
実装はしていない。この文書で方式を選んでもらい、試作に進むかどうかを決める。

## 1. 何のために Wayland にするか

10a で X11 のまま DPI を変えられるようにした（LogPixels と `Xft.dpi` を揃える）。X11 ではできないことは次の 2 つ。

1. **モニターごとの倍率**: X11 の DPI は画面全体で 1 つ。100% のモニターと 150% のモニターを同時に使えない。
2. **分数の倍率での Linux アプリ**: GTK 3 は整数倍しかできず、10a では文字だけを拡大した。Wayland では
   `wp_fractional_scale_v1` で GTK 4、Qt 6、Chromium、Firefox が 125% や 150% をそのまま描ける。

1 台の画面で使い、倍率が 1 つで足りるなら、10a で目的は果たせている。Wayland に移る理由は、上の 2 つが必要かどうかで決まる。

## 2. 前提として変わらないこと

- **XP のシェルと Wine のアプリは Xwayland で動かす。** XP の Explorer は、デスクトップ、タスクバー、ポップアップメニュー、
  スタートメニューを自分で決めた画面座標に置く。Wayland のクライアントは自分の位置を決められないので、Wine の Wayland ドライバー
  （winewayland.drv）では XP のシェルの配置を再現できない。X11 のクライアントとして Xwayland に置けば、位置は compositor の
  XWM（X11 のウィンドウマネージャーの役）が受け入れる。
- **XP 自体にはモニターごとの DPI が無い。** LogPixels は 1 つで、XP のアプリはその DPI で描く（10a）。倍率の違うモニターでは、
  XP のシェルは同じ DPI のまま実画素で表示する（ぼかして拡大しない）か、compositor が拡大する（ぼける）かのどちらかになる。
  前者を推奨する。
- **Xwayland のウィンドウを拡大させない。** wlroots などの多くの compositor は、倍率 1.5 の出力で Xwayland のウィンドウを
  1.5 倍に引き伸ばす（ぼける）。XP のシェルと Wine には、Xwayland に実画素の大きさを渡して拡大させず、DPI は LogPixels と
  `Xft.dpi` で決める方式（Hyprland の `xwayland:force_zero_scaling` と同じ考え方）が要る。自作するならこの処理を入れる。
- **ログイン画面は今のまま X11 で動かす。** LightDM は X11 の greeter のまま Wayland のセッションを起動できる。

## 3. bridge-wm から移す機能

compositor に移すもの（今は `bridge-wm/wm.py` と `proxy.c`、IPC ディレクトリのファイル）:

| 機能 | 今の実装 | Wayland での形 |
|---|---|---|
| Linux アプリの XP 枠 | wm.py が X の枠を作り、proxy.exe が XP の窓を合わせる | compositor が Wayland ウィンドウの位置と大きさを proxy に伝え、XP 枠（Xwayland の窓）を合わせる |
| タスクバーの一覧と切り替え | `windows.tsv`、`*.cmd` | 同じ IPC を compositor が書く。Wayland ネイティブの窓は foreign-toplevel 相当の情報から |
| Alt+Tab の切り替え窓 | `switch.tsv`、XP の見た目 | compositor がキーを受け、XP の切り替え窓（Xwayland）に一覧を渡す |
| 前面・フォーカスの規則 | wm.py | compositor のフォーカス規則に組み込む |
| 作業領域（タスクバーの分） | `_NET_WM_STRUT` | Xwayland のドックを compositor が除外する |
| トレイアイコン | tray.py（SNI と XEmbed） | SNI はそのまま。XEmbed は Xwayland 上で今のまま |
| 日本語入力 | fcitx5（XIM、Qt と GTK のモジュール） | Wine は Xwayland の XIM のまま。Wayland ネイティブは `text-input-v3` と `input-method-v2` を compositor が持つ必要がある |
| 起動と復旧 | supervisor.py | supervisor が compositor を起動する形に変える。待ち受け・復旧の考え方は同じ |

Wine のパッチ（60 本）は X11 ドライバーと win32u が中心で、Xwayland で動かす限りそのまま使える見込み。
ただし Xwayland の XWM の振る舞い（`_NET_WM_STATE`、ConfigureRequest の扱い）の差で、0024〜0027（winex11 の作業領域、
最大化、ポップアップの下敷き、影）は試作で確かめ直す。

## 4. 方式の比較

| | A. labwc を改変 | B. wlroots で自作（C） | C. Smithay で自作（Rust） | D. KWin を使う |
|---|---|---|---|---|
| 土台 | wlroots の重ね合わせ型 compositor（Openbox に近い設定） | wlroots と tinywl 相当の最小構成 | Smithay（cosmic-comp、niri が使う）と anvil 相当の最小構成 | KDE Plasma の compositor。スクリプトと効果で拡張 |
| Xwayland の XWM | wlroots のものを使う | 同左 | Smithay の xwm モジュール | KWin のもの（成熟） |
| Xwayland を拡大させない処理 | 自分で足す（wlroots 側の変更が要る可能性） | 自分で書く | 自分で書く | 設定で選べる（「アプリに拡大させる」） |
| 分数倍率・モニター別倍率 | wlroots が持つ | 同左 | Smithay が持つ | 持っている |
| bridge-wm の機能を入れる場所 | labwc のソースに追加（C） | 自分のコード（C） | 自分のコード（Rust） | KWin スクリプト（JavaScript）と外部プロセス。できないことがある |
| 依存の重さ | 軽い（wlroots、labwc） | 軽い | 軽い（Rust の依存はビルド時） | 重い（KDE Frameworks、Plasma の一部） |
| 上流への追従 | labwc と wlroots の更新に合わせて改変を当て直す | wlroots の API 変更に追従 | Smithay の API 変更に追従（変化が大きい） | KWin の API 変更に追従 |
| 試作の見積もり | 2〜4 週間 | 4〜8 週間 | 6〜10 週間 | 3〜6 週間（ただし制約の確認次第） |

見積もりは、最初の段階を「XP のシェルが Xwayland で起動し、Linux アプリ（Wayland ネイティブ）が XP 枠つきで動き、タスクバーと
Alt+Tab が今と同じに使える」までとしたもの。モニター別の倍率と日本語入力（input-method-v2）は含む。
見た目の画素合わせ（今の試験 `tests/wm_integration.py`、`tests/native_frames.py` を Wayland 用に直すこと）も含む。

### A. labwc を改変（推奨）

- 重ね合わせ型で、ウィンドウの扱いが今の wm.py（Openbox に近い）と近い。
- 枠を描かない設定（サーバー側の装飾を切る）があり、XP 枠は今と同じく proxy.exe が描ける。
- 改変は「bridge-wm の IPC を書く・読む部分」「Xwayland を拡大させない部分」「XP 枠と Wayland 窓の対応」に集まる。
- 欠点: labwc の更新に合わせて改変を当て直す手間がある（Wine のパッチと同じ管理になる）。

### B. wlroots で自作

- 必要な機能だけを持つので、振る舞いを全部把握できる。
- 欠点: 出力の設定、入力機器、クリップボード、ドラッグ＆ドロップ、画面ロックなど、labwc が既に持つものを書き直す量が多い。

### C. Smithay で自作

- Rust で書け、メモリ安全。段階 1 で「Rust で書き直してよい」とした方針に合う。
- 欠点: Smithay の API は今も大きく変わる。Xwayland の xwm は wlroots より新しく、XP のシェルのように X11 の振る舞いに
  強く頼るクライアントでの実績が少ない。最も時間がかかる。

### D. KWin を使う

- 分数倍率と Xwayland の扱いは最も成熟している。
- 欠点: XP 枠と Wayland 窓の対応、タスクバーへの一覧、Alt+Tab の差し替えを KWin スクリプトだけでできるかは不確か。
  KDE の多くの部品が入り、「XP のシェルだけの軽いセッション」という今の形から離れる。

## 5. 推奨と進め方

1. **まず決めてほしいこと: モニターごとの倍率、または分数倍率の Linux アプリが必要か。**
   必要でなければ、10a の X11 のまま進め、Wayland には移らないことを勧める。
2. 必要なら **A（labwc の改変）** で試作する。最初の 1 週間は次の確認だけにし、結果を見て続けるかを決める。
   - XP のシェル（Explorer、タスクバー、スタートメニュー、ポップアップ）が Xwayland で今と同じ位置に出るか。
   - Xwayland を拡大させずに、倍率 1.5 の出力で Wine を 144 DPI でくっきり描けるか。
   - Wayland ネイティブの GTK 4 か Qt 6 のアプリに XP 枠を付けて、動かす・大きさを変えるに追従できるか。
3. 確認で XP のシェルの配置が保てない場合は、Wayland 化を止める（winewayland.drv に XP のシェルを載せるには、Wine と
   compositor の両方に独自のプロトコルが要り、見積もりが大きく増える）。

## 6. 決めてほしいこと（まとめ）

- モニターごとの倍率・分数倍率の Linux アプリが要るか（要らなければ Wayland 化はしない）。
- 要る場合、A〜D のどれで試作するか（推奨は A）。
- 試作の最初の 1 週間の確認（5 の 2）から始めてよいか。

## 7. 試作の結果（2026-10-12、方式 A の最初の確認）

labwc 0.8.3 と wlroots 0.18.2（Debian 13 の版）を改変し、入れ子の labwc（X11 バックエンド、1280×800 の出力 2 つ、
左は倍率 1、右は倍率 1.5）の上で、XP のシェル（Wine 11、61 本のパッチ）を Xwayland で動かした。Wine の DPI は 144。

### 改変の内容

- **Xwayland に実際の画素を見せる。** Xwayland にだけ自前の xdg-output を見せ、各出力を実際の画素の大きさで、左から
  横に並べた座標（X の空間）として伝える。Xwayland のルートは 2560×800 になり、X の窓は出力の画素で描かれる。
- **X の空間とレイアウトの座標を境目で変換する。** labwc の窓の管理（配置、最大化、移動）はレイアウトの座標のまま。
  X に送る位置と大きさは倍率を掛け、X から来た値は倍率で割る（最小サイズなどの hints も）。丸めの往復で大きさが
  1 画素ずれないよう、X から求められた値をそのまま返す。
- **X の窓を倍率で縮めて置く（wlroots の scene に 1 項目）。** 倍率 1.5 の出力では X の 1 画素が出力の 1 画素になる。
  ポインタの位置も同じ倍率で X の座標に戻す。
- **bridge-wm の IPC を labwc に持たせる（試作は 50ms ごとの確認）。** Wayland の窓に仮の番号を振って `windows.tsv` と
  `geometry.tsv` を書き、proxy.exe の `frames.tsv` から枠を見つけて Wayland の窓のすぐ下に重ねる。`*.cmd`（移動、
  最大化、最小化、閉じる、活性化）を処理する。座標は Win32 の画面座標（X の空間から主モニターの原点を引く）。
  XP のデスクトップ（Progman）を常に一番下に置く。
- **Wine:** XP のメニューの影（0027）は影の下の画面をルート窓から読むが、rootless の Xwayland では読めず
  （BadMatch）、Explorer が落ちた。読めなければ影を描かないようにした。

パッチは `devnotes/wayland-prototype/`（Git の外）。

### 確認 1: XP のシェルが Xwayland で今と同じ位置に出るか — 出た（条件つき）

- デスクトップ、タスクバー、スタートメニュー（override-redirect の窓）は、倍率 1 と 1.5 のどちらの出力でも、Wine が
  求めた位置と大きさに出た。
- 改変しない labwc では、Xwayland が出力を論理サイズ（853×533）で見て、デスクトップの全画面の窓もその大きさになった。
- **残ること:** XP のデスクトップは主モニターにしか出ない（Wine がその出力だけの全画面にする。XP は全モニターに
  またがる）。倍率の違う出力にまたがる X の窓は、1 つの倍率でしか縮められない（出力ごとに切り分ける必要がある）。
  タスクバーの作業領域（strut）が倍率で変換されていない。Xwayland の主モニターは xrandr で指定した。

### 確認 2: 倍率 1.5 の出力で、Wine がぼけずに描けるか — 描けた

- 倍率 1.5 の出力の実際の画素（入れ子の外の Xvfb から読んだもの）と、Xwayland のデスクトップの窓の画像を比べ、
  マウスポインタの 121 画素以外は一致した（1280×800 のうち）。
- 改変しない labwc では、96 DPI の画面を 1.5 倍に引き伸ばしてぼけていた。

### 確認 3: Wayland のアプリに XP の枠を付けて、移動と大きさの変更に追従できるか — できた（条件つき）

- Wayland ネイティブの端末（foot）に、proxy.exe が描く本物の XP の枠（Luna、144 DPI の余白 5・5・44・5）が付いた。
  タスクバーにもボタンが出た。foot は倍率 1.5 で、枠は 1 画素ずつ描かれ、どちらもくっきりしている。
- 枠の最大化ボタンで foot が最大化し、枠も付いてきた。
- 枠のタイトルバーのドラッグで、枠と foot が一緒に動いた（Wine が labwc に移動を頼み、labwc が枠を動かし、proxy が
  foot の位置を送る）。
- **残ること:** 起動後の最初のドラッグは活性化だけで終わり、2 回目から動く。最大化の大きさがタスクバーの分を
  除いていない（作業領域の変換）。枠の移動・大きさの変更の途中は、試作の 50ms ごとの確認の分だけ遅れる。

### わかったこと

- 方式 A の核になる部分（ぼけずに描くこと、Wine の位置指定、Wayland の窓への XP の枠）は、labwc に約 740 行、
  wlroots に約 30 行の変更で動いた。最大の不確かさだった「Xwayland を拡大させない」は、Xwayland 自体を変えずに
  compositor 側だけでできた。
- 本格的に進める場合の残り:
  - 出力にまたがる X の窓（XP のデスクトップ）を出力ごとに切り分けて描く。
  - 作業領域、主モニター、出力の追加と削除の扱い。
  - bridge-wm の残りの機能（Alt+Tab、フォーカスの規則、最初のクリックの扱い、イベント駆動の同期、アイコン）。
  - Wayland のアプリの日本語入力（text-input-v3 と input-method-v2、labwc 0.8 は持っている）。
  - Wine の画面の読み取り（0026 の下敷き、0027 の影、0043 の背景）を、rootless の Xwayland で読めない前提に直す。
    影は透明度つきの窓で描けば読み取りが要らない。
  - 試験 VM での実機の出力（DRM）と、セッションの起動（supervisor）。
- 見積もりは前の表（2〜4 週間）から大きく外れない。いちばん重いのは bridge-wm の機能の移植。

この後、方式 A で Wayland のセッションを作った（段階 11）。今の説明は [../wayland.md](../wayland.md)。
