# 環境構築・開発HowTo

## 推奨: Dockerで構築する

Dockerを使うと、固定Rust toolchain、C compiler、CMake、Cppcheck、Miri、fuzz、
coverage、metrics toolをimage内へまとめて導入できる。ホストに必要なのは
Git、Docker Engine、Docker Compose v2、GNU Makeだけである。

- Linux: [Docker Engineの導入](https://docs.docker.com/engine/install/)
- Windows/WSL2:
  [Docker Desktop WSL 2 backend](https://docs.docker.com/desktop/features/wsl/)
- macOS: [Docker Desktopの導入](https://docs.docker.com/desktop/setup/install/mac-install/)

WindowsではrepositoryをWSL2のLinux filesystem内へcloneして実行する。
Docker DesktopのSettingsから、使用するWSL distributionとの連携を有効にする。
WSL2でGNU Makeが未導入の場合は`sudo apt-get install make`で導入する。

### 1. Imageをbuildする

repositoryルートで実行する。

```sh
make docker-build
```

imageにはCIと同じ固定versionのtoolを導入する。初回はCargo toolのcompileを含む
ため時間がかかる。`Dockerfile`、Rust version、品質tool versionを変更した場合は
再buildする。

GNU Makeを使わず直接実行する場合:

```sh
LOCAL_UID=$(id -u) LOCAL_GID=$(id -g) docker compose build dev
```

### 2. Testを実行する

```sh
# Rust testとCMake/CTest結合テスト
make docker-test

# 通常品質ゲート
make docker-check

# Miri、fuzz、Cppcheckを含むmerge前相当の検査
make docker-extended-check
```

### 3. 対話shellで開発する

```sh
make docker-shell
```

shell内では通常のMake targetをそのまま使える。

```sh
make test
make check
cargo test <test-name>
```

### Docker内のfile配置

repository全体を`/workspace`へbind mountする。コンテナはホストと同じUID/GIDで
動くため、編集ファイルや`build/reports`がroot所有になることを避ける。

Cargoがdownloadしたcrate indexとsourceはDocker named volumeへcacheする。
`target/`、`build/`、fuzz corpus/artifactはrepository側へ生成されるため、
コンテナを削除しても残る。

```sh
# 停止済みcontainerを含めてCompose環境を削除
docker compose down

# Cargo download cacheも削除
docker compose down --volumes
```

### Docker構成ファイル

| File | 役割 |
| --- | --- |
| `Dockerfile` | OS package、Rust toolchain、Cargo toolを固定して導入 |
| `compose.yaml` | source mount、UID/GID、Cargo cache、実行serviceを定義 |
| `.dockerignore` | build contextから生成物とGit metadataを除外 |

## 代替: Hostへ直接構築する

Dockerを使わない場合は、CIと同じLinuxを基準とする。WindowsではWSL2を推奨する。

通常のCargo buildは他のOSでも動作し得るが、全品質ゲートには次の制約がある。

- C静的解析でGCCの`-fanalyzer`を使う
- `cargo-fuzz`はnightly Rust、LLVM sanitizer、Unix系OSを必要とする
- CMake/CTestでLinux用static libraryをC executableへlinkする
- CIの再現環境はGitHub ActionsのUbuntu runnerである

### 1. OS package

UbuntuまたはWSL2 Ubuntu:

```sh
sudo apt-get update
sudo apt-get install -y \
  build-essential \
  ca-certificates \
  cmake \
  cppcheck \
  curl \
  doxygen \
  git \
  python3
```

必要条件はCMake 3.20以上、GCCの`-fanalyzer`を利用できるC compiler、
Python 3、GNU Makeである。別distributionでは同等のpackageを導入する。

### 2. Rust

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

### 3. Cargo tool

品質ゲートで使うversionをCIと揃えて導入する。

```sh
cargo install cbindgen --version 0.29.3 --locked
cargo install cargo-fuzz --version 0.13.1 --locked
cargo install cargo-llvm-cov --version 0.8.7 --locked
cargo install rust-code-analysis-cli --version 0.0.25 --locked
```

tool更新時は[高度検証](../memory-buffer/docs/advanced-verification.md)にある
固定version表、CI設定、本文書を同じ変更で更新する。

### 4. 導入確認

```sh
rustc --version
cargo --version
cargo clippy --version
cmake --version
cc --version
python3 --version
doxygen --version
cbindgen --version
cargo fuzz --version
cargo llvm-cov --version
rust-code-analysis-cli --version
```

`make c-docs`と`make check`は`tools/run_doxygen.sh`を経由する。system PATH上の
`doxygen`を優先し、存在しない場合は`$HOME/.local/share/doxygen`へ展開された
ユーザーローカル版を使用する。どちらもない場合は導入方法を表示して失敗する。

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
| `make test` | Rust test、C結合、全C Foundation単体テスト | Rust、CMake、C compiler |
| `make foundation-test` | 全C Foundationの単体テスト | CMake、C compiler |
| `make foundation-log-test` | Log FoundationのC単体テスト | CMake、C compiler |
| `make foundation-time-test` | Time FoundationのC単体テスト | CMake、C compiler |
| `make platform-linux-test` | Linux Time adapterのC単体テスト | CMake、C compiler |
| `make service-time-test` | Time ServiceのC単体テスト | CMake、C compiler |
| `make static-analysis` | rustfmt、Clippy、GCC `-fanalyzer` | Clippy、GCC |
| `make c-docs-check` | 全C headerのDoxygenコメント契約検査 | Python |
| `make c-docs` | 全C API、source、test、fuzz仕様書生成 | Doxygen |
| `make coverage` | Rust/Cのunit・integration branch coverage検査 | llvm-cov、gcov |
| `make header-check` | Rust定義と生成headerの差分検査 | cbindgen |
| `make coverage` | 単体/結合branch coverageと閾値検査 | nightly、cargo-llvm-cov、Python |
| `make metrics` | CC、認知的複雑度、MIのHTML生成 | Python、rust-code-analysis-cli |
| `make quality-report` | coverageとmetricsのHTML index生成 | 上記coverage/metrics tool |
| `make portable-check` | 32 bit target向け`no_std` build | stable Rust、target追加 |
| `make miri` | Rust memory modelに基づく単体テスト | nightly、Miri |
| `make fuzz-smoke` | libFuzzerを2000回実行 | nightly、cargo-fuzz、C++ compiler |
| `make cppcheck` | C利用例の追加静的解析 | Cppcheck |
| `make check` | 通常の品質ゲート一式 | 上記の通常検査tool、Doxygen |
| `make extended-check` | `check`にMiriとfuzzを追加 | 全tool |
| `make docker-build` | 固定tool入りDocker imageをbuild | Docker、Compose v2 |
| `make docker-shell` | 開発containerのshellを開く | Docker image |
| `make docker-test` | container内で`make test` | Docker image |
| `make docker-check` | container内で`make check` | Docker image |
| `make docker-extended-check` | container内でmerge前検査 | Docker image |

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
3. static libraryをC applicationのlink設定へ追加する。
4. 呼出側が`mb_context_t`とpayload arenaを静的に確保する。
5. 起動時に`mb_init`し、`mb_alloc`、`mb_write`/`mb_read`、`mb_free`を使う。

具体的なCコード、直接access、error処理は
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

### WSL2で`docker: command not found`

1. WindowsでDocker Desktopを起動する。
2. `Settings > Resources > WSL Integration`を開く。
3. `Enable integration with my default WSL distro`または使用中の`Ubuntu`を有効にする。
4. `Apply & restart`を押す。
5. WSL shellを開き直す。

```sh
make docker-ready
```

まだ失敗する場合はWindows PowerShellで`wsl --shutdown`を実行し、Docker Desktopを
再起動してからUbuntuを開き直す。

### Docker生成物がroot所有になる

`make docker-*`は`id -u`と`id -g`をimage buildへ渡す。過去にrootで生成したfileが
残っている場合は所有者を修正した後、`make docker-build`でimageを作り直す。

### Docker image内のtoolを更新したい

`Dockerfile`のversionと、CI・品質資料の固定versionを同時に更新してからbuildする。

```sh
docker compose build --no-cache dev
```

### cross buildで`can't find crate for core`

検証用の32 bit `no_std` targetが未導入である。

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
