# 検証状況

2026-10-09。Ubuntu 26.04 ホスト上の Debian 13 chroot、Xvfb 1280×800、
ローカルの日本語 XP SP3 x86 ISO、ソース版 Wine 10.0 で試験。
ホスト自体を対象 OS の検証結果に含めない。

| 段階 | 状態 | 証拠 / 残件 |
|---|---|---|
| M0 | 基本経路を実測 | Debian 13 のソースビルド成功、ISO から20ファイル抽出、新規 prefix 生成成功。ログ `.local/prefix-m0-create.log` |
| M1 | 新規 prefix で実測 | XP デスクトップ、クラシック Start / Programs、メニューから起動した Wine メモ帳のタスクボタン。画像 `.local/final-m1.png` |
| M2 | 新規 prefix で実測 | マイドキュメントから H: の Linux ホームを表示し、サブフォルダへ移動。画像 `.local/final-m2-home.png` / `final-m2-navigate.png`。メニュー・アドレス欄の描画に欠落あり |
| M3 | 実 XP と Linux xterm で実測 | Start メニューから起動、Alt+Tab、タスクバー右クリックの最小化・閉じる、左クリック復元を確認。ログ `.local/final-m3-native-operations.log` / `final-m3-alt-tab.log` |
| M4 | 独立 Debian 13 VM で実測 | kernel 6.12.107、Xorg、LightDM の xp-shell セッションでログイン。強制終了 2 回から再起動、3 回目で独立 builtin prefix + xterm へ復旧。画像 `.local/vm-test/login.png` / `recovery.png` |
| M5 | クラシック HiDPI 実測、Luna 部分対応 | 144/192 DPI の Start メニュー・Programs・200% の Linux 起動を確認。Luna は API 有効・ローカル msstyles 読込を確認するがタスクバー背景・Start ボタンが不完全。完成扱いにはしない |

## 自動試験

- `tests/extraction_integration.py`: 利用者の抽出済みファイルをローカル fixture として使い、抽出・hash・上書き拒否を確認。
- `tests/wm_integration.py`: 作業領域、Alt+Tab / 逆順、最小化、最後の1枚の最小化、復元、終了要求。
- `tests/bridge_integration.py`: 子ウィンドウでの shell-hook 購読、X→代理 HWND、最小化・復元・フォーカス・名前変更・終了・破棄の往復。
- `tests/recovery_integration.py`: XP の強制終了・再起動・独立 builtin ブラウザと端末の実表示。Debian VM で成功。
- `tests/display.c`: system DPI / ThemeActive / ローカル Luna の公開 API を確認。
- `tests/button-size.c`: 標準 Button の preferred size が有効、NULL 出力を拒否、選択フォントを反映。

試験用 PE は `tests/build.sh` で `.local/bridge/` に生成する。
スクリーンショットや生ログは Microsoft のリソースを含み得るため `.local/` にのみ保存する。

## 既知の制限

- アクティブなタスクボタンの左クリックでの最小化は未達。右クリックメニューの最小化と左クリック復元は動く。
- Linux ウィンドウの装飾は未実装。Alt+左ドラッグで移動、Alt+右ドラッグでサイズ変更。単一 X11 screen のみ検証。
- 実 SOFTWARE/SYSTEM ハイブの変換、Ubuntu 24.04、Wayland/Xwayland は未検証。

## 独立 VM の検証

`.local/vm-test/login-environment.log` に LightDM の `DESKTOP_SESSION=xp-shell`、
`XDG_SESSION_TYPE=x11` と kernel 6.12.107 を記録。
`.local/vm-test/recovery-test.log` は実 XP を taskkill した各 attempt と fallback の状態。
ホストのディスプレイマネージャには導入していない。VM のネットワークは無効。

HiDPI 画像: `.local/vm-test/classic-144-menu.png`、`classic-192-menu.png`、
`classic-192-programs-click.png`、`classic-192-linux.png`。
フォルダのメニューバー・アドレス欄の不足は DPI に関わらず残る。
XP は DPI 非対応として Wine が拡大するため、文字やアイコンにぼけがある。
Linux アプリのフォントサイズはアプリ側の設定に従う。
Luna: `.local/vm-test/theme-probe-96.log`、`luna-144.png`。

Wine 更新の再試験: timestamp 不一致を作り、更新・再登録後に XP Shell / Start を確認。
`.local/vm-test/wine-update-result.log` と `wine-update-final.png`。

公開 API 検査も VM で成功: `display-144-test.log` (144 / classic)、
`recovery-integration.log` (192 / classic + 復旧)、`display-luna-144-test.log` (144 / Luna API)。
パッチ9本は固定 upstream commit の別 worktree への順次適用も成功 (`.local/patch-series-check.log`)。

## 最後の稼働状態

VM を通常の classic / 96 DPI に戻して再起動し、LightDM から attempt 1 で起動。
メニューから Wine メモ帳と Linux xterm、デスクトップから Linux ホームを開いた。
最新画像 `.local/vm-test/final-desktop.png`、再起動ログ `reboot-result.log`。
この作業環境では VM の VNC を `127.0.0.1:5905` で利用できる。ゲストの NIC は引き続きない。
再起動用の実測コマンドは `.local/boot-guest.sh`。再作成は [VM 手順](vm.md) を参照。

追加の制限: VM の std VGA で、サイズ変更・フォーカス変更後に xterm の表示内容が
再描画まで黒く見えることを観測した。フォーカスを戻して入力すると再表示される。
Xvfb と VM の描画経路の差は未解決で、日常利用の完成扱いにはしない。
