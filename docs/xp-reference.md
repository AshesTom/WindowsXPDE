# 実 XP の表示比較

利用者のローカル ISO をそのまま使う、Wine とは別の検証用 VM。
Microsoft バイナリのダウンロード・改変・解析・再配布は行わない。
ディスク、測定用 PE、画面、生ログは `.local/xp-reference/` 以下に置き、Git に含めない。
ホストのディスプレイマネージャも既存 Debian 検証 VM も置き換えない。

この環境では QEMU / KVM、Pentium III、768 MiB、IDE の新規8 GiB qcow2、標準 VGA、
NIC なしで起動する。VNC は `127.0.0.1:5906`。Debian VM の5905とは分離する。
再起動用の実測コマンドはローカルの `.local/xp-reference/boot.sh`。
ISO は `localres/WindowsXPSP3.iso` を読み取り専用 CD として使う。

この ISO のセットアップは対応するボリュームライセンス用キーを要求した。
利用者が提供し入力を許可したキーで次の画面へ進めた。
キー自体は本資料・ソース・検証ログには記載しない。

## 公開 API の比較用プログラム

`tests/build.sh` は自作 `tests/reference-metrics.c` を
`.local/bridge/reference-metrics-test.exe` にビルドする。
XP で利用できる公開 API だけで、次を測る。

- system DPI、caption・frame の寸法、SPI の6種類のフォント。
- user/system UI language、現在のテーマの path・color・size、
  NULL / Window / TaskBand HTHEME の caption system font と各 HRESULT。
- Window / Start::Button のテーマフォント、close button の画像寸法・状態数。
- Button / Start::Button の OpenThemeData handle と失敗時の error、
  NULL owner と自作 Button owner、NULL DC と実 DC の GetThemeFont HRESULT・結果。
  SetWindowTheme の Start subapp 指定も別に検査する。実タスクバーのテーマは変更しない。
- 自作ウィンドウの WM_NCHITTEST による3ボタンの実際の範囲。
- ウィンドウ領域と左上の角の包含、taskbar Button の WM_GETFONT。
- 同じ500×320のアクティブ／非アクティブウィンドウの実表示。

既定の出力先は `C:\win4wine-reference.txt`。
第1引数で出力ファイルを指定できる。Wine 側では例えば
`Z:\work\.local\reference-wine.txt` とし、専用 prefix 内で起動する。
比較用プログラムはウィンドウを閉じるまで終了しない。

検証 VM の A: は自作プログラムだけを入れた FAT フロッピー。
`REF.EXE` に上記 PE を置く。XP デスクトップ到達後に
`A:\REF.EXE COM1` を実行すると、測定値だけを仮想シリアルへ出力できる。
COM1 出力はこの独立 VM 用で、QEMU の `.local/xp-reference/serial.log` に保存される。
DLL・テーマ・フォントのファイルやキーを出力するプログラムではない。
フロッピーをホストで更新するときは先に QMP で媒体を取り外す。

比較画面を広げる場合だけ `A:\REF.EXE COM1 --display-1024x768` を指定する。
この第2引数がある場合に限り、測定前に公開 ChangeDisplaySettingsW で1024×768・32bppを要求し、
戻り値と実際の画面寸法を出力する。registry 保存 flag は使わない。
通常の実行は画面設定を変更しない。指定した mode が利用できなくても診断結果を残して測定を続ける。

API がテーマ指定と一致することと、実 XP の見た目が一致することは区別する。
同じ DPI・ウィンドウサイズ・状態で比較してから、寸法や書体を確定する。

初回の日本語 XP / 96 DPI 測定では本文・メニュー・アイコンのフォントは
MS UI Gothic -12、caption は -13 bold、taskbar Button は -13 bold だった。
close button のテーマ画像は21×21・8状態。
Wine でのテーマ適用結果と日本語 XP の実表示には書体・本文サイズの差があり、調整中。

実 XP では `OpenThemeData(..., L"Start::Button")` は失敗した。
自作 Button に `SetWindowTheme(button, L"Start", NULL)` を設定してから
`OpenThemeData(button, L"Button")` を使うと、Start のフォントを取得できた。
NULL / 実 DC の両方で MS UI Gothic -16・bold・italic が返った。

任意の `--probe-spi-font` は caption の SPI 設定だけを一時変更し、
既存 / 新規 / NULL HTHEME で GetThemeSysFont を比較してから元の設定を復元する。
永続保存・変更通知は行わない。実 XP と Wine の両方で、既存 / 新規 HTHEME は
テーマのフォントを保持し、NULL HTHEME は変更された SPI を返した。
両環境とも復元成功と LOGFONT の完全一致を確認した。

`--theme-hash` は現在のテーマファイルを読み取り、公開 CryptoAPI で SHA256 のみを出力する。
ファイルの内容を解析・出力しない。実 XP のインストール済み Luna と、同じ ISO から
抽出して Wine に配置した Luna の SHA256 は一致した。
XP ではプロセスのテーマ初期化前の GetCurrentThemeName が失敗したため、
この測定は自作ウィンドウのテーマを開いた後で行う。
