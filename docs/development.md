# 開発

ふつうに使うだけなら [INSTALL.md](../INSTALL.md) の `scripts/setup.sh` で足りる。この文書は、パッチや bridge を変えて試すときの手順。

## リポジトリの構成

| 場所 | 中身 |
|---|---|
| `wine/` | 基にする Wine（`upstream.env`）、パッチ列（`patches/series`）、ビルド（`build.sh`）、適用判定（`patch-state.py`） |
| `bridge-wm/` | X11 の WM（`wm.py`）、Win32 の代理（`proxy.c`）、トレイの仲介（`tray.py`）、調査用（`inspect.c`） |
| `session/` | セッションの監視（`supervisor.py`）、Linux アプリのスタートメニュー（`linux_apps.py`）、Xsession の登録 |
| `prefix/` | prefix の作成（`create.py`）、更新（`stage.py`）、見た目と DPI（`display.py`）、registry profile |
| `extract/` | ISO から必要なファイルだけを取り出す（`extract.py`） |
| `greeter/` | LightDM の XP のようこそ画面、ログオン・ログオフの画面、XP のカーソル |
| `scripts/` | まとめて入れる（`setup.sh`）、開発用の chroot（`debian.sh`）、pre-commit の検査 |
| `tests/` | 統合試験と、Wine・実 XP の両方で動かす公開 API のプローブ（C） |
| `localres/`, `.local/` | ISO、抽出物、prefix、ビルド、ログ、画面。**Git 管理外** |

開発の記録（段階ごとの検証の記録や作業計画）は Git 管理外に置いている。文書に出てくる「段階 N」はその作業の区切り。

## Git の設定

```sh
git config core.hooksPath .githooks
```

pre-commit（`scripts/check-staged.py`）は `localres/`・`.local/` のファイル、PE、レジストリのハイブ、字体、NUL を含むバイナリを
拒否する（`git add -f` でも）。例外は `docs/images/` の PNG（README のスクリーンショット）だけ。

## chroot での開発環境

ホストで git、sudo、debootstrap が使え、15 GB 以上の空きがあること。chroot は `.local/debian` に作られ、`/work` がこのリポジトリ。

```sh
scripts/debian.sh init
scripts/debian.sh run /bin/bash
```

chroot の中で:

```sh
cd /work
./wine/build.sh > .local/wine-build.log 2>&1    # Wine の取得、パッチ、ビルド
./bridge-wm/build.sh                            # .local/bridge（proxy.exe、linux-launch.exe など）
./tests/build.sh                                # プローブ
./extract/extract.py localres/WindowsXPSP3.iso .local/assets
Xvfb :95 -screen 0 1280x800x24 -nolisten tcp > .local/xvfb.log 2>&1 &
export DISPLAY=:95 WINE=/work/.local/wine-install/bin/wine
./prefix/create.py .local/assets .local/prefix-dev --home /home/builder --wine "$WINE"
```

- `wine/build.sh` は `WINE_SOURCE`、`WINE_BUILD`、`WINE_INSTALL`、`JOBS` で場所と並列数を変えられる。
  Wine は新しい WoW64 方式（`--enable-archs=i386,x86_64`）でビルドする。i386 のライブラリは要らない。
- パッチ列を変えたら、**新しいソースとビルドディレクトリで configure から**ビルドして確かめる。使い回したビルドディレクトリでは
  古いオブジェクトが残り、漏れ（ヘッダーの登録など）に気づけない。
- `extract.py` と `create.py` は既存の出力先を拒否する。やり直すときは別の名前で作る。

## パッチ

```sh
python3 wine/patch-state.py SOURCE wine/patches --apply   # 未改変の Wine 11.0 に全部当たるか
```

並び、書式、opt-in の判定、番号の対応は [patches.md](patches.md)。パッチには目的、合わせる XP の挙動、根拠と制約を書く。
`.local/wine-upstream` を直接変えたものを成果物にせず、必ずパッチにする。

## 試験

| 試験 | 内容 |
|---|---|
| `tests/wm_integration.py` | WM 単体（Alt+Tab、フォーカス、最小化、Windows キーの受け渡し） |
| `tests/bridge_integration.py` | WM と proxy と XP シェル |
| `tests/native_frames.py` | Linux のウィンドウの XP の枠（Luna とクラシック） |
| `tests/xterm_redraw.py`, `tests/wm_latency.py` | 再描画と遅延 |
| `tests/tray_host.py` | トレイの仲介（StatusNotifierItem と XEmbed） |
| `tests/linux_apps.py` | Linux アプリのスタートメニュー |
| `tests/recovery_integration.py CONFIG.json` | シェルが落ちたときの切り替え |
| `tests/greeter_layout.py ASSETS`, `tests/cursor_theme.py ASSETS` | ようこそ画面の配置（96・120・144 DPI）、カーソル（32 と 64） |
| `tests/patch_state.py`, `tests/font_*.py`, `tests/workarea.py`, `tests/dpi_settings.py` | 単体の試験 |
| `tests/*.c` | 公開 API のプローブ。同じものを Wine と実 XP で動かして数値を比べる（DPI は `dpi-metrics.c`、`char-widths-dpi.c`、`caption-glyph-dpi.c`、`text-bits-dpi.c`） |

## 手作業での導入

`setup.sh` を使わずに入れるときは、Wine、bridge、抽出、prefix を上と同じに作り、`~/.config/win4wine/session.json` を書く
（キーは [INSTALL.md](../INSTALL.md#設定configwin4winesessionjson)）。

```sh
./session/install.sh                         # /usr/local/bin/xp-shell-session と xsessions の登録
./greeter/install.sh /absolute/path/assets   # 任意: ようこそ画面
```

`session/install.sh` が入れる wrapper はこのチェックアウトを指すので、移動したら入れ直す。

## 既存の環境を更新する

1. セッションを止め、wineserver が終わったことを確かめる。
2. Wine の install を入れ替える。
3. `./prefix/stage.py --wine "$WINE" "$WINEPREFIX"`。wine.inf の更新を見て wineboot、XP の COM の再登録、registry profile の適用、
   XP の DLL の配置を行う。セッションの起動時にも自動で走る。
4. `bridge-wm/build.sh` で bridge を作り直し、セッションを起動し直す。

見た目と DPI は `prefix/display.py --theme luna|classic --menu xp|classic --dpi N`（セッションを止めてから）。N は XP と同じく
96（通常）、120（大きいサイズ）か、カスタムの 19〜480。セッションは同じ DPI を Linux のアプリにも渡す（`Xft.dpi`、fontconfig、
カーソルの大きさ、192 以上で GTK の `GDK_SCALE`。`session/dpi.py`）。ようこそ画面の DPI は `/etc/win4wine/greeter.json` の
`"dpi"` で決める（XP でもマシン全体の設定）。

## 既存の XP のハイブから取り込む

`python3-hivex` を入れ、読み取り専用で変換する。名前空間の CLSID と NLS の限られた値だけを扱い、サービス、ドライバ、ログオン、
製品情報は移さない。

```sh
./prefix/hive-to-reg.py --software .../SOFTWARE --system .../SYSTEM --output .local/shell-hive.reg
```
