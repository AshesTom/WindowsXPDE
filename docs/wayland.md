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
