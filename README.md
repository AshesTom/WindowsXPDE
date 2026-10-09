# win4wine

Debian 13 上で、利用者の XP SP3 x86 メディアから取り出した Explorer を
Wine のデスクトップシェルとして動かす実験環境です。Microsoft のファイルは同梱しません。

- [再現手順](docs/reproduce.md)
- [実装計画](docs/plan.md)
- [現在の検証状況](docs/status.md)
- [DLL 選択と障害調査](docs/dll-policy.md)
- [bridge / session の構成](docs/architecture.md)

`localres/` は入力メディア、`.local/` は抽出物・prefix・Wine ビルド・ログ専用です。
どちらも Git 対象外です。ソース、スクリプト、Wine パッチ列のみ管理します。
現時点では日常利用可能な完成 OS ではありません。検証状況を確認してから専用 X セッションで試してください。
