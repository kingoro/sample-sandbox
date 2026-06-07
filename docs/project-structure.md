# リポジトリ構成

## 全体像

このリポジトリはCargo workspaceをルートに置き、Rustライブラリ、C利用例、
品質ツール、fuzz projectを同じrepositoryで管理する。

```text
.
├── Cargo.toml                 # Rust workspaceとrelease profile
├── Cargo.lock                 # workspace依存versionの固定
├── rust-toolchain.toml        # 通常開発用Rust 1.96.0
├── Makefile                   # local/CI共通の操作入口
├── Dockerfile                 # 固定tool入り開発image
├── compose.yaml               # 開発containerとmount/cache設定
├── .dockerignore              # Docker build対象外の生成物
├── README.md                  # repository全体の入口
├── docs/
│   ├── development-guide.md   # 環境構築と開発HowTo
│   ├── c-api-main.md          # Doxygen C仕様書のトップページ
│   └── project-structure.md   # 本文書
├── memory-buffer/
│   ├── Cargo.toml             # library crate設定
│   ├── CMakeLists.txt         # no_std staticlibとC利用例のhost結合
│   ├── README.md              # component概要
│   ├── cbindgen.toml          # C header生成設定
│   ├── src/                   # Rust実装と単体テスト
│   ├── tests/                 # Rustから公開C ABIを呼ぶ結合テスト
│   ├── include/               # 利用者向けheaderと生成ABI manifest
│   ├── examples/              # C callerの実行例
│   └── docs/                  # 設計、API、品質、利用手順
├── Utility/
│   ├── event/
│       ├── README_BEGINNER.md # Event駆動に不慣れなC開発者向け導入
│       ├── include/           # 集約headerと責務別の公開header
│       ├── src/               # QueueとDispatcherのC11実装
│       ├── tests/             # C単体テスト
│       ├── fuzz/              # C操作列fuzz harness
│       └── docs/              # 設計、API、品質、利用手順
│   └── log/
│       ├── README_BEGINNER.md # Application Logに不慣れな開発者向け導入
│       ├── include/           # 集約headerと責務別の公開header
│       ├── src/               # LoggerとConsole adapterのC11実装
│       ├── tests/             # C単体テスト
│       ├── fuzz/              # C操作列fuzz harness
│       └── docs/              # 設計、API、品質、利用手順
├── fuzz/
│   ├── Cargo.toml             # cargo-fuzz専用の独立workspace
│   ├── README.md              # fuzzの実行・解析手順
│   └── fuzz_targets/          # libFuzzer harness
├── tools/
│   ├── coverage.sh            # branch coverage生成と閾値検査
│   ├── c_coverage.sh          # C単体テストのgcovレポート生成
│   ├── check_c_docs.py        # C headerのDoxygenコメント契約検査
│   ├── quality-policy.json    # 全言語共通の品質閾値
│   └── quality.py             # CC/MI集計とHTML index生成
├── Doxyfile                   # 全C API、source、test、fuzzの仕様書生成
└── .github/workflows/ci.yml   # push/PR時のGitHub Actions
```

## Workspaceの分け方

ルートのCargo workspaceには`memory-buffer`だけを含める。`fuzz`は
`fuzz/Cargo.toml`内の`[workspace]`で独立させている。通常の
`cargo test --workspace`へlibFuzzer用binaryと依存関係を混ぜず、
`cargo fuzz --fuzz-dir fuzz`からだけbuildするためである。

## memory-buffer

`memory-buffer`は`rlib`と`staticlib`を生成する。

- `rlib`: Rustの単体・結合テストで使用する
- `staticlib`: C applicationからlinkする`libmemory_buffer.a`
- defaultの`std` feature: host test用
- `--no-default-features`: OS非依存の`no_std` build用

主な責務と境界は
[アーキテクチャ](../memory-buffer/docs/architecture.md)を参照する。
C applicationからの利用方法は
[利用手順](../memory-buffer/docs/usage.md)、公開関数とerror codeは
[C API仕様](../memory-buffer/docs/api.md)に記載する。

## C header

`memory-buffer/include/memory_buffer.h`は利用者がincludeする正式headerである。
opaque context、Doxygen互換コメント、利用者向けの宣言を管理する。

`memory_buffer_generated.h`はcbindgenでRust定義から生成するABI manifestであり、
直接編集しない。`make header-check`でRust側とのずれを検出し、更新が必要な場合は
`make header`で再生成する。

## Event Utility

`Utility/event`は、ドメイン非依存の固定長Event Queueと同期DispatcherをC11で
実装する。独立配布libraryではなく、利用側buildへsourceを組み込むUtilityである。
heap、RTOS、I/Oへ依存せず、storageは呼出側が提供する。

`make utility-test`でC単体テスト、`make utility-static-analysis`でGCC警告と
`-fanalyzer`を実行する。通常の`make test`と`make check`にも含まれる。

## Log Utility

`Utility/log`は、Application LogをConsoleと固定長RAM Ringへ配送するC11基盤で
ある。実行時level切替、Recordの所有、上書き件数、read/dump、任意の排他callbackを
提供する。EEPROM製品Log、USB/UART、通信、永続化は責務に含めない。

`make utility-log-test`で単体テスト、`make utility-log-static-analysis`で
GCC警告と`-fanalyzer`を実行する。全C Utilityは`make utility-test`と
`make utility-static-analysis`でまとめて検証できる。

## テストの配置

| 場所 | 種類 | 目的 |
| --- | --- | --- |
| `memory-buffer/src/buffer.rs` | Rust単体テスト | private helper、境界値、状態遷移 |
| `memory-buffer/tests/c_abi.rs` | Rust結合テスト | 公開C ABI相当の契約 |
| `memory-buffer/examples/c_usage.c` | CTest | C compiler、header、staticlibの実linkと実行 |
| `Utility/event/tests/` | C単体テスト | Event QueueとDispatcherの契約 |
| `Utility/log/tests/` | C単体テスト | Logger、RAM Ring、Consoleの契約 |
| `fuzz/fuzz_targets/` | cargo-fuzz | 任意のAPI操作列とsanitizer検査 |

テスト戦略と品質基準は
[品質ゲート](../memory-buffer/docs/quality.md)、Miriやfuzzの役割は
[高度検証](../memory-buffer/docs/advanced-verification.md)を参照する。

## 生成物

次はコマンド実行時に生成され、Git管理しない。

| Path | 内容 |
| --- | --- |
| `target/` | 通常のCargo build、test、Rustdoc |
| `build/memory-buffer/` | CMake buildとC結合実行ファイル |
| `build/utility-event/` | Event Utility CMake build |
| `build/utility-log/` | Log Utility CMake build |
| `build/reports/` | coverage、CC、MIのHTMLレポート |
| `build/docs/c-api/` | Doxygenで生成する全C API・test仕様書 |
| `fuzz/target/` | fuzz targetのbuild成果物 |
| `fuzz/corpus/` | libFuzzerが学習した入力 |
| `fuzz/artifacts/` | crashを再現する入力 |

`make clean`は通常Cargo成果物とCMake buildを削除する。fuzzのcorpusとartifactは
調査に必要な場合があるため、自動では削除しない。
