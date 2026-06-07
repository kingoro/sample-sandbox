# Common Utility Development

このrepositoryでは、共通Utilityをドメイン機能から分離し、実装、テスト、設計資料を
同じ単位で管理する。

- [Memory Buffer Library](memory-buffer/README.md)
- [C Event Utility](Utility/event/README.md)
- [C経験者向け はじめてのEvent Utility](Utility/event/README_BEGINNER.md)
- [C Log Utility](Utility/log/README.md)
- [はじめてのLog Utility](Utility/log/README_BEGINNER.md)

## Memory Buffer Library

呼出側が用意した固定メモリ領域を、Cから安全に割り当て・読み書き・解放するための
ライブラリ。

[`memory-buffer`](memory-buffer/README.md)はメモリ操作の内部実装をRustへ隠蔽し、
C APIの利用者にはopaque handleだけを公開する。ヒープを使用せず、境界検査、
解放済みhandleの拒否、ポインタ貸出中の競合防止をライブラリ側で行う。

## C基盤Utility

`Utility/event`は固定長Event Queueと同期Dispatcherを提供する。
`Utility/log`はApplication Logのlevel制御、Console出力、固定長RAM Ringへの
蓄積と実行中dumpを提供する。どちらも製品domain、heap、通信へ依存せず、
別の組み込みLinux製品へsource単位で移植できる構成とする。

## 主な特徴

- 呼出側が提供したarenaから、重複しない領域を割り当てる
- raw pointerの代わりに世代付きopaque handleで生存期間を管理する
- read、write、論理長の境界をAPI内で検査する
- map中の変更や解放を拒否し、一時ポインタの競合を防ぐ
- C ABIの`staticlib`として既存のC applicationから利用できる
- `no_std`でbuildでき、OSやglobal allocatorを必要としない

## はじめに読む資料

1. [環境構築・開発HowTo](docs/development-guide.md)
2. [C経験者向け はじめてのMemory Buffer](memory-buffer/README_BEGINNER.md)
3. [リポジトリ構成と各ファイルの責務](docs/project-structure.md)
4. [memory-bufferの使い方](memory-buffer/docs/usage.md)
5. [外部提供用C API仕様](memory-buffer/docs/api.md)

## Quick start

Dockerを利用できる場合は、ホストへRustや品質toolを個別導入せずに開始できる。

```sh
# Docker Desktop/Engineとの接続確認
make docker-ready

# 初回またはtool更新後
make docker-build

# 日常のRust/Cテスト
make docker-test

# 通常品質ゲート
make docker-check
```

対話shellを開く場合:

```sh
make docker-shell
```

Dockerを使わずLinuxまたはWSL2へ直接構築する方法、merge前検査、Docker Desktopの
WSL連携は[環境構築手順](docs/development-guide.md)を参照する。
利用可能なMake targetは次で確認できる。

```sh
make help
```

## テスト・品質方針

一般的なtest pyramidを土台にしつつ、FFIとunsafe codeを持つmemory library向けに、
契約テスト、動的解析、fuzz、ABI検証、cross buildを重ねる。テスト件数を増やす
こと自体ではなく、公開APIの契約と壊れ方を複数の手段で検証する方針である。

### 検証レイヤ

| レイヤ | このrepositoryでの役割 | 主な対象 |
| --- | --- | --- |
| Rust単体テスト | 小さく高速な土台 | allocator、境界値、世代、状態遷移 |
| C ABI結合テスト | 利用者視点の契約 | 公開関数、result code、型layout |
| CMake/CTest | 言語境界の実build | C header、static library、link、実行 |
| 静的解析 | 実行前の欠陥検出 | Clippy、GCC `-fanalyzer`、Cppcheck |
| Miri | Rust memory modelの検証 | provenance、alias、未初期化memory、UB |
| fuzz | 人手で列挙しにくい操作列 | 無効handle、境界値、map状態、reset |
| ABI/header検査 | C/Rust定義のdrift防止 | size、alignment、field offset、宣言 |
| cross build | host依存の混入検出 | `no_std`、32 bit型幅、target依存ABI |

### テストケースの基準

各公開操作について、次の観点を揃える。

- 正常系: allocからread/write、info取得、freeまでの代表的な一連動作
- 境界値: 0、上限、上限直前、上限超過、整数overflow
- 資源不足: arena不足、64 slot上限、断片化した空き領域の再利用
- 生存期間: free後、reset後、slot再利用後の古いhandle拒否
- 状態遷移: map/unmap、二重map、map中のread/write/free/reset拒否
- 不正入力: null、未初期化context、無効handle、範囲外access
- 言語境界: C/Rust間の型幅、alignment、field offset、全公開symbolのlink

不具合修正時は、問題を再現する最も低いレイヤへregression testを追加する。
API契約の変更時は正常系だけでなく、拒否すべき操作とresult codeも更新する。
fuzzで見つけた入力は最小化し、再発防止できる通常テストへ移植する。

### 品質ゲート

| 指標 | 基準 |
| --- | ---: |
| 全言語の単体テスト C1 branch coverage | 80%以上 |
| 全言語の結合テスト C1 branch coverage | 50%以上 |
| production関数の循環的複雑度 | 15以下 |
| production関数のVisual Studio式MI | 35以上 |

基準値の唯一の定義元は`tools/quality-policy.json`とし、言語やmoduleごとの独自閾値は
設けない。

localとCIは同じMake targetを使用する。`make check`を通常の完了条件とし、
unsafe、pointer貸出、状態遷移へ影響する変更では`make extended-check`で
Miriとfuzzも実行する。詳細な対象、tool、レポートの読み方は
[品質ゲート](memory-buffer/docs/quality.md)と
[高度検証](memory-buffer/docs/advanced-verification.md)を参照する。

## 今回整備した開発者向け情報

このrepositoryでは、実装だけでなく新規参加者が自力で検証できることも
保守性の一部として扱う。今回、次を明文化した。

- libraryの用途を特定systemから切り離し、隠蔽されたメモリ操作APIとして説明
- repository全体のdirectory構成と各fileの責務
- OS package、固定Rust toolchain、Cargo toolを含む環境構築
- Dockerによる再現可能な開発環境と品質ゲートの実行方法
- 日常開発、テスト、品質ゲート、CIとの対応
- 使用するtest framework・解析toolの役割と公式資料へのリンク
- fuzz targetの入力形式、検査内容、再現、最小化、生成物

## ドキュメント

- [環境構築・開発HowTo](docs/development-guide.md)
- [リポジトリ構成](docs/project-structure.md)
- [C経験者向け はじめてのMemory Buffer](memory-buffer/README_BEGINNER.md)
- [memory-buffer概要](memory-buffer/README.md)
- [構成・責務・状態遷移](memory-buffer/docs/architecture.md)
- [Cからの利用方法・代表シーケンス](memory-buffer/docs/usage.md)
- [外部提供用C API仕様](memory-buffer/docs/api.md)
- [品質ゲートとHTMLレポート](memory-buffer/docs/quality.md)
- [cbindgen・Miri・fuzz・cross buildによる高度検証](memory-buffer/docs/advanced-verification.md)
- [fuzz testの使い方と検査内容](fuzz/README.md)
- [Log Utility概要](Utility/log/README.md)
- [Log Utility初心者向け導入](Utility/log/README_BEGINNER.md)

全C API、production source、単体テスト、fuzz harnessのDoxygen仕様書は
`make c-docs`で`build/docs/c-api/html/index.html`へ生成する。

## ライブラリの範囲

host上の単体・C ABI結合テスト、静的解析、coverage、Miri、fuzz、
32 bit `no_std` targetへのcross compileを自動化している。

このライブラリが扱うのは、arena内の領域割り当て、handle、生存期間、境界検査、
一時ポインタ貸出までである。thread同期、I/O、永続化、転送処理、cache制御などは
利用側の責務であり、特定のapplication構成や実行環境には依存しない。
