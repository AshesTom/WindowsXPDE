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
| M4 | 骨格・復旧試験あり | セッション定義と supervisor 実装済み。ソース Wine と実 DM でのログイン・復旧検証は未完 |
| M5 | 設定経路のみ | Luna 抽出・設定スクリプトあり。実表示と 150% / 200% は未検証 |

## 自動試験

- `tests/extraction_integration.py`: 利用者の抽出済みファイルをローカル fixture として使い、抽出・hash・上書き拒否を確認。
- `tests/wm_integration.py`: 作業領域、Alt+Tab / 逆順、最小化、最後の1枚の最小化、復元、終了要求。
- `tests/bridge_integration.py`: 子ウィンドウでの shell-hook 購読、X→代理 HWND、最小化・復元・フォーカス・名前変更・終了・破棄の往復。
- `tests/button-size.c`: 標準 Button の preferred size が有効、NULL 出力を拒否、選択フォントを反映。

試験用 PE は `tests/build.sh` で `.local/bridge/` に生成する。
スクリーンショットや生ログは Microsoft のリソースを含み得るため `.local/` にのみ保存する。

## 既知の制限

- アクティブなタスクボタンの左クリックでの最小化は未達。右クリックメニューの最小化と左クリック復元は動く。
- Linux ウィンドウの装飾は未実装。Alt+左ドラッグで移動、Alt+右ドラッグでサイズ変更。単一 X11 screen のみ検証。
- 実 SOFTWARE/SYSTEM ハイブの変換、Ubuntu 24.04、Wayland/Xwayland は未検証。
