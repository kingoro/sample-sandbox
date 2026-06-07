# 品質ゲート

## 目的

テスト件数だけでなく、実行経路、複雑度、保守容易性を継続的に計測する。
localとCIは同じMake targetを使用し、CIではHTMLレポートをartifactとして保存する。

## 基準

| 対象 | 指標 | 基準 |
|---|---:|---:|
| Rust単体テスト | C1 branch coverage | 80%以上 |
| C ABI結合テスト | C1 branch coverage | 50%以上 |
| production関数 | 循環的複雑度 CC | 15以下 |
| production関数 | Visual Studio式 MI | 35以上 |

C1は`cargo-llvm-cov`のbranch coverageで計測する。branch instrumentationには
固定したnightly toolchainが必要だが、製品buildと通常testは固定したstable
Rustを使用する。

## テスト観点

| 分類 | 主な観点 |
|---|---|
| 正常系 | alloc、write、read、info取得、freeの一連動作 |
| 正常系 | map、DMA相当の直接access、unmap、論理長設定 |
| 準正常系 | arena不足、slot上限、fragment再利用、map中busy |
| 異常系 | null、未初期化context、無効handle、範囲外、整数overflow |
| 異常系 | 二重free、過剰unmap、resetによる古いhandle無効化 |

単体テストはprivate helperの境界条件を検証するため、実装と同じ
`src/buffer.rs`内に置く。結合テストは
`memory-buffer/tests/c_abi.rs`から公開C ABI相当の関数だけを使用し、利用者視点の
契約を検証する。実C compilerとのlink確認は`examples/c_usage.c`をCTestで実行する。

## 静的解析

- Rust: `cargo clippy -- -D warnings`
- C: GCC `-fanalyzer`、警告をerror扱い
- CI追加検査: Cppcheckのwarning、style、performance、portability
- Rust/C共通metrics: `rust-code-analysis-cli`によるCC、認知的複雑度、MI
- FFI header: `cbindgen --verify`によるRust定義とのdrift検査
- Undefined behavior: Miriによる単体test実行
- 操作列: libFuzzerによるAPI sequence fuzzing
- MCU移植性: `thumbv7em-none-eabi`向け`no_std` cross build

RadonはPython向けなので使用しない。LizardはRust/CのCC確認には利用できるが、
MIを同じ基準で取得できないため、現在は`rust-code-analysis`へ統一している。

## 実行方法

全品質ゲート:

```sh
make check
```

Miriとfuzz smokeを含むmerge前検査:

```sh
make extended-check
```

HTMLレポート生成:

```sh
make quality-report
```

入口は`build/reports/index.html`。単体・結合coverageの行単位詳細と、
関数単位のCC・MI一覧を参照できる。

必要tool:

```sh
cargo install cargo-llvm-cov --version 0.8.7 --locked
cargo install rust-code-analysis-cli --version 0.0.25 --locked
cargo install cbindgen --version 0.29.3 --locked
cargo install cargo-fuzz --version 0.13.1 --locked
rustup toolchain install nightly-2026-06-06 --profile minimal \
  --component llvm-tools-preview,miri,rust-src
rustup target add thumbv7em-none-eabi --toolchain 1.96.0
```

CppcheckはCIでは自動導入する。localで`make cppcheck`も実行する場合はOSの
package managerで`cppcheck`を導入する。

## MCU CI

既定targetはCortex-M4/M7系の代表として`thumbv7em-none-eabi`を使用する。
採用MCU確定後は次のように差し替える。

```sh
make mcu-check MCU_TARGET=<target-triple>
```

現段階で保証するのはcross compileまでであり、実機起動、linker script、RTOS、
DMA cache動作はboard確定後にrunnerを追加して検証する。

cbindgen、Miri、fuzz、MCU buildの詳細、検出実績、保証範囲は
[高度検証](advanced-verification.md)を参照する。
