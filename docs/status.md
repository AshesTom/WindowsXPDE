# 検証状況

2026-10-09。Ubuntu 26.04 ホスト上の Debian 13 chroot、Xvfb 1280×800、
ローカルの日本語 XP SP3 x86 ISO、ソース版 Wine 10.0 で試験。
ホスト自体を対象 OS の検証結果に含めない。

| 段階 | 状態 | 証拠 / 残件 |
|---|---|---|
| M0 | 基本経路を実測 | Debian 13 のソースビルド成功、ISO から20ファイル抽出、新規 prefix 生成成功。ログ `.local/prefix-m0-create.log` |
| M1 | 調査用 prefix で表示・通知が動作 | XP デスクトップ、クラシック Start メニュー、Programs のショートカット、Wine メモ帳のタスクボタンを確認。最新登録一式での新規 prefix 再試験中 |
| M2 | 調査中 | H: / Personal の設定済み。native フォルダを開く経路に不足 COM とハングが残る |
| M3 | bridge 単体の往復試験成功 | 実 XP タスクバーにも xterm のボタンを確認。タスクバークリックを含む実 XP での全操作再試験が必要 |
| M4 | 骨格・復旧試験あり | セッション定義と supervisor 実装済み。ソース Wine と実 DM でのログイン・復旧検証は未完 |
| M5 | 設定経路のみ | Luna 抽出・設定スクリプトあり。実表示と 150% / 200% は未検証 |

## 自動試験

- `tests/extraction_integration.py`: 利用者の抽出済みファイルをローカル fixture として使い、抽出・hash・上書き拒否を確認。
- `tests/wm_integration.py`: 作業領域、Alt+Tab / 逆順、最小化、最後の1枚の最小化、復元、終了要求。
- `tests/bridge_integration.py`: 子ウィンドウでの shell-hook 購読、X→代理 HWND、最小化・復元・フォーカス・名前変更・終了・破棄の往復。
- `tests/button-size.c`: 標準 Button の preferred size が有効、NULL 出力を拒否、選択フォントを反映。

試験用 PE は `tests/build.sh` で `.local/bridge/` に生成する。
スクリーンショットや生ログは Microsoft のリソースを含み得るため `.local/` にのみ保存する。
