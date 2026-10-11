<p align="center">
  <img src="docs/images/screenshot.png" alt="win4wine: Debian 13 の上で動く Windows XP のシェル" width="800" />
</p>
<p align="center">
  <strong>Linux のデスクトップを、Windows XP のシェルで。</strong>
</p>
<p align="center">
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-LGPL--2.1--or--later-blue?style=flat" alt="License: LGPL-2.1-or-later"></a>
  <img src="https://img.shields.io/badge/Debian-13-A81D33?style=flat&logo=debian&logoColor=white" alt="Debian 13">
  <img src="https://img.shields.io/badge/Wine-11.0-8B0000?style=flat" alt="Wine 11.0">
</p>
<p align="center">
  <strong title="日本語" aria-label="日本語">🇯🇵</strong> ·
  <a href=".github/readme/README.en.md" title="English" aria-label="English">🇬🇧</a>
</p>

## 導入

1. 日本語版 Windows XP Professional SP3（x86）の ISO を**自分で**用意する。
2. 新しく入れた Debian 13 で、ログインする利用者として実行する。

```sh
git clone https://github.com/mikuta0407/win4wine.git ~/win4wine
cd ~/win4wine
scripts/setup.sh --media /path/to/WindowsXPSP3.iso --greeter
```

3. 再起動すると、XP のようこそ画面からそのまま XP シェルに入る。

詳しくは 🔗 [導入手順（INSTALL.md）](INSTALL.md)。Wine のビルドに 4 コアで1時間半ほどかかる。

## これは何

Windows XP の `explorer.exe` を、パッチを当てた Wine の上で **Linux のデスクトップシェル**として動かす。
タスクバー、スタートメニュー、デスクトップ、フォルダウィンドウは XP 本物のもの。Linux のアプリも XP の枠とタスクバーで扱える。

Microsoft のファイルは同梱しない。利用者の ISO から、必要なファイルだけを手元で取り出して使う。

## できること

- **XP のシェル**: Luna（またはクラシック）のタスクバー、スタートメニュー、デスクトップ、マイ コンピュータ、フォルダウィンドウ。
- **Linux アプリの統合**: Linux のウィンドウに XP の枠が付き、タスクバー、Alt+Tab、最小化・最大化が XP と同じに動く。
  Linux のアプリは「すべてのプログラム」に並ぶ。
- **通知領域**: Linux のトレイアイコン（StatusNotifierItem と XEmbed）が XP の通知領域に出る。
- **日本語入力**: fcitx5 + Mozc。Ctrl+Space（または全角/半角）で切り替える。GTK、Qt、Wine のアプリで使える。
- **Windows アプリ**: WoW64 の Wine 11.0 で、32bit と 64bit のアプリが動く。端末で `wine setup.exe` と打てば、
  インストーラーが作るショートカットは XP のスタートメニューに入る。
- **XP のようこそ画面**: LightDM の greeter として、XP のようこそ画面とログオフの画面を出す（任意）。
- **XP のキー操作**: Windows キー、Ctrl+Esc、Win+R、Win+E。
- **落ちても戻れる**: XP のシェルが繰り返し落ちると、Wine 標準の explorer と xterm に切り替える。

## 注意

> [!WARNING]
> - **Windows XP の ISO は自分で入手すること。** このリポジトリにも導入スクリプトにも、Microsoft のファイルを取得する処理はない。
>   使う ISO と取り出したファイルの扱いは、Microsoft の使用許諾に従うこと。
> - **利用は自己責任で。** 個人の実験的なプロジェクトで、日常利用できる完成品ではない。データの損失や不具合について、作者は責任を負わない。
> - Microsoft とも Wine プロジェクトとも関係のない、非公式のプロジェクト。
> - 確かめたのは日本語版 XP SP3（x86）だけ。日本語の字体（MS ゴシック）を前提にしている。

## 仕組み

```text
LightDM ─ XP Welcome greeter (greeter/)
   └─ XP Shell セッション (session/supervisor.py)
        ├─ Wine 11.0 + 60 本のパッチ (wine/patches/)
        │    └─ XP の explorer.exe, shell32.dll, …（ISO から取り出したもの）
        ├─ bridge-wm: X11 の WM + Win32 の代理ウィンドウ (bridge-wm/)
        └─ fcitx5 + Mozc, トレイの仲介 (bridge-wm/tray.py)
```

- [構成（architecture.md）](docs/architecture.md): WM、代理ウィンドウ、IPC、トレイ、ログイン画面
- [Wine パッチ（patches.md）](docs/patches.md): 60 本の一覧と並び、書式
- [判断と理由（dll-policy.md）](docs/dll-policy.md): どの DLL を XP のものにし、Wine のどこを直したか
- [開発（development.md）](docs/development.md): chroot でのビルド、試験、手作業での導入

## 守っていること

- Windows 由来のバイナリには手を加えない。違いは Wine とその周辺で吸収する。
- Microsoft のバイナリを逆アセンブルしない。挙動は公開 API と、実 XP 上の自作プローブで調べる。
- Microsoft のファイル、抽出物、prefix は Git 管理外の `localres/` と `.local/` にだけ置く。

## ライセンス

[LGPL-2.1-or-later](LICENSE)（Wine と同じ）。対象はこのリポジトリのコードとパッチだけ。
Windows XP のファイルと、スクリーンショットに写っている XP の画面の著作権は Microsoft にある。
