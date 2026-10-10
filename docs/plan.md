# 実装計画と判断記録

## 制約
Microsoft のファイルは利用者指定のローカル入力からのみ抽出する。`localres/` と `.local/` は Git 対象外。コードは公開 API、upstream Wine のソース、実行ログを根拠に作成し、Microsoft バイナリを逆アセンブルしない。Wine の取得 URL は Wine の upstream mirror に限定する。Wineboot の Mono/Gecko インストーラは無効にする。

## 順序
M0: Debian 13 のクリーン chroot、Wine 10.0 x86 ソースビルド、抽出、prefix 作成。
M1: XP classic shell の実測、Win32 アプリの起動とタスクバー。
M2: フォルダと H: のホーム参照。
M3: X11 WM、Win32 代理ウィンドウ、shell hook、最小化・復元・Alt+Tab。
M4: Xsession、監視、クラッシュループから builtin explorer への退避。
M5: Luna と 144/192 DPI の実測。

## 初期判断
- ホストは Ubuntu 26.04。指定された Debian 13 を `.local/debian` に作成し、その中で検証する。ホスト GUI は変更しない。
- XP ISO は日本語版 GRTMPVOL_JA の x86。32-bit Wine を使う。Wine 10.0 の tag/commit を固定する。（段階 5 で Wine 11.0、段階 7 で WoW64 のビルドと 64-bit の prefix に移った。XP のシェルは 32-bit のまま。）
- Xorg を基準にする。Xwayland は compositor の window 管理との調停が必要なので保留。
- 名前空間と関連付けは Wine が生成するレジストリを土台にする。ユーザーの SOFTWARE/SYSTEM は丸ごと移植しない。サービス、ドライバ、認証、マシン固有設定を混入させるため。
- XP explorer は `C:\xp\explorer.exe` に隔離し、Wine builtin explorer をフォールバック用に保存する。
- 通常の comctl32 と SxS v6 を分離。v6 の配置・manifest を保持し、system32 を v6 で上書きしない。
- Linux 統合は Win32 proxy と X11 WM をプロトコルで分離し、将来の shell 世代への変更点を抑える。

## 未解決
実行ログで必要な native DLL と Wine の未実装 API を決める。M1 が成立するまで M2 以後を到達済みとはしない。SxS、ハイブ変換、Luna はクラシック動作後に評価する。
