# 環境構築・開発HowTo

## 対応環境

CIと同じLinuxを基準とし、WindowsではWSL2の利用を推奨する。
通常のCargo buildは他のOSでも動作し得るが、全品質ゲートには次の制約がある。

- C静的解析でGCCの`-fanalyzer`を使う
- `cargo-fuzz`はnightly Rust、LLVM sanitizer、Unix系OSを必要とする
- CMake/CTestでLinux用static libraryをC executableへlinkする
- CIの再現環境はGitHub ActionsのUbuntu runnerである

## 1. OS package

UbuntuまたはWSL2 Ubuntu:

```sh
sudo apt-get update
sudo apt-get install -y \
  build-essential \
  ca-certificates \
  cmake \
  cppcheck \
  curl \
  git \
  python3
```

必要条件はCMake 3.20以上、GCCの`-fanalyzer`を利用できるC compiler、
Python 3、GNU Makeである。別distributionでは同等のpackageを導入する。

## 2. Rust

[Rust公式の導入手順](https://doc.rust-lang.org/stable/cargo/getting-started/installation.html)
に従って`rustup`を導入し、新しいshellを開く。

通常build用の固定stable:

```sh
rustup toolchain install 1.96.0 --profile minimal \
  --component clippy,rustfmt
rustup target add thumbv7em-none-eabi --toolchain 1.96.0
```

coverage、Miri、fuzz用の固定nightly:

```sh
rustup toolchain install nightly-2026-06-06 --profile minimal \
  --component llvm-tools-preview,miri,rust-src
```

repository直下では`rust-toolchain.toml`によりRust 1.96.0が自動選択される。
nightlyを使うtargetだけ、Makefileが明示的に
`+nightly-2026-06-06`を指定する。

## 3. Cargo tool

品質ゲートで使うversionをCIと揃えて導入する。

```sh
cargo install cbindgen --version 0.29.3 --locked
cargo install cargo-fuzz --version 0.13.1 --locked
cargo install cargo-llvm-cov --version 0.8.7 --locked
cargo install rust-code-analysis-cli --version 0.0.25 --locked
```

tool更新時は[高度検証](../memory-buffer/docs/advanced-verification.md)にある
固定version表、CI設定、本文書を同じ変更で更新する。

## 4. 導入確認

```sh
rustc --version
cargo --version
cargo clippy --version
cmake --version
cc --version
python3 --version
cbindgen --version
cargo fuzz --version
cargo llvm-cov --version
rust-code-analysis-cli --version
```

最初は短い検査から順に実行する。

```sh
cargo test --workspace
make test
make check
make extended-check
```

`make check`以降で不足toolが見つかった場合は、失敗したtargetと
後述の対応表から必要toolを確認する。

## 日常の開発手順

1. 変更範囲に近い単体または結合テストを追加・更新する。
2. 実装を変更する。
3. `cargo fmt --all`でformatする。
4. 開発中は対象test、完了前は`make check`を実行する。
5. unsafe、pointer、状態遷移へ影響する変更は`make extended-check`も実行する。
6. C公開型・関数を変えた場合は`make header`を実行し、header差分をreviewする。
7. API契約や利用手順が変わった場合は関連Markdownも同じ変更で更新する。

## よく使うコマンド

| Command | 内容 | 主な依存 |
| --- | --- | --- |
| `cargo test --workspace` | Rust単体・結合テスト | stable Rust |
| `make test` | Rust testとCMake/CTest結合テスト | Rust、CMake、C compiler |
| `make static-analysis` | rustfmt、Clippy、GCC `-fanalyzer` | Clippy、GCC |
| `make header-check` | Rust定義と生成headerの差分検査 | cbindgen |
| `make coverage` | 単体/結合branch coverageと閾値検査 | nightly、cargo-llvm-cov、Python |
| `make metrics` | CC、認知的複雑度、MIのHTML生成 | Python、rust-code-analysis-cli |
| `make quality-report` | coverageとmetricsのHTML index生成 | 上記coverage/metrics tool |
| `make mcu-check` | `thumbv7em-none-eabi`向け`no_std` build | stable Rust、target追加 |
| `make miri` | Rust memory modelに基づく単体テスト | nightly、Miri |
| `make fuzz-smoke` | libFuzzerを2000回実行 | nightly、cargo-fuzz、C++ compiler |
| `make cppcheck` | C利用例の追加静的解析 | Cppcheck |
| `make check` | 通常の品質ゲート一式 | 上記の通常検査tool |
| `make extended-check` | `check`にMiriとfuzzを追加 | 全tool |

全targetの短い説明は`make help`で表示できる。

## テストの使い分け

### Rust test

実装に近い単体テストと、公開ABIを使う結合テストを
[Cargo標準test harness](https://doc.rust-lang.org/cargo/guide/tests.html)で実行する。

```sh
cargo test --workspace
cargo test <test-name>
cargo test --test c_abi
```

### C結合テスト

[CMake](https://cmake.org/cmake/help/latest/)が`no_std`の
`libmemory_buffer.a`と`examples/c_usage.c`をbuildし、
[CTest](https://cmake.org/cmake/help/latest/manual/ctest.1.html)が実行する。

```sh
make test
```

これはRust内からC ABI相当関数を呼ぶだけでなく、実際のC compiler、公開header、
static libraryのlinkが成立することを確認する。

### Coverageとmetrics

[cargo-llvm-cov](https://github.com/taiki-e/cargo-llvm-cov)でC1 branch coverageを
計測し、`tools/coverage.sh`で閾値を判定する。
[rust-code-analysis](https://github.com/mozilla/rust-code-analysis)で関数ごとの
循環的複雑度と保守容易性指数を計測する。

```sh
make quality-report
```

結果は`build/reports/index.html`から参照する。基準値と対象範囲は
[品質ゲート](../memory-buffer/docs/quality.md)を参照する。

### Miri、fuzz、cbindgen

- [Miri](https://github.com/rust-lang/miri): unsafe Rustの未定義動作を検査する
- [cargo-fuzz](https://github.com/rust-fuzz/cargo-fuzz): libFuzzerでAPI操作列を探索する
- [cbindgen](https://github.com/mozilla/cbindgen): Rust公開ABIからC headerを生成する

このrepository固有の設定と保証範囲は
[高度検証](../memory-buffer/docs/advanced-verification.md)を参照する。
fuzz入力の形式、再現、最小化は[fuzz README](../fuzz/README.md)に記載する。

## Cから利用する

1. `memory-buffer/include/memory_buffer.h`をC sourceからincludeする。
2. `cargo build -p memory-buffer --release --no-default-features`で
   `target/release/libmemory_buffer.a`を生成する。
3. static libraryをfirmwareのlink設定へ追加する。
4. 呼出側が`mb_context_t`とpayload arenaを静的に確保する。
5. 起動時に`mb_init`し、`mb_alloc`、`mb_write`/`mb_read`、`mb_free`を使う。

具体的なCコード、DMA/zero-copy、error処理は
[memory-buffer利用手順](../memory-buffer/docs/usage.md)を参照する。
関数ごとの引数、戻り値、事前条件は
[C API仕様](../memory-buffer/docs/api.md)に記載する。

## CIとの対応

`.github/workflows/ci.yml`はpushとpull requestで
`make extended-check cppcheck`を実行する。localで同じ範囲を確認する場合:

```sh
make extended-check cppcheck
```

CIは品質レポートを`quality-report` artifactとして保存する。localの生成先は
`build/reports/`である。

## Troubleshooting

### `can't find crate for core`

MCU targetが未導入である。

```sh
rustup target add thumbv7em-none-eabi --toolchain 1.96.0
```

### `no such command: fuzz`または`llvm-cov`

対応するCargo toolを固定versionで再導入する。

```sh
cargo install cargo-fuzz --version 0.13.1 --locked
cargo install cargo-llvm-cov --version 0.8.7 --locked
```

### `cc: error: unrecognized command-line option '-fanalyzer'`

使用中のC compilerがGCC `-fanalyzer`に対応していない。Linux/WSL2上の対応GCCを
導入し、必要なら`make static-analysis CC=gcc`としてcompilerを指定する。

### fuzz終了時にLeakSanitizerが失敗する

`ptrace`配下ではLeakSanitizer自体が失敗する場合がある。`make fuzz-smoke`は
`ASAN_OPTIONS=detect_leaks=0`を設定済みである。範囲外accessやuse-after-freeを
検出するAddressSanitizerは有効なままである。
