# Fuzz test

このディレクトリは、`memory-buffer`の公開APIをランダムな順序と引数で呼び出し、
通常の単体テストでは想定しにくい状態遷移やメモリ安全性の問題を探すための
fuzz testを管理する。

現在のtargetは
[`fuzz_targets/operation_sequence.rs`](fuzz_targets/operation_sequence.rs)の
`operation_sequence`のみで、libFuzzerを`cargo-fuzz`経由で使用する。

## 準備

リポジトリルートで、固定nightly toolchainと`cargo-fuzz`を導入する。

```sh
rustup toolchain install nightly-2026-06-06 --profile minimal
cargo install cargo-fuzz --version 0.13.1 --locked
```

## 実行方法

短時間の回帰確認は、リポジトリルートで次を実行する。

```sh
make fuzz-smoke
```

これは最大入力長4096 byteで2000回実行する。CIでは`make extended-check`の一部
として同じ検査を行う。

時間を指定して探索を続ける場合:

```sh
ASAN_OPTIONS=detect_leaks=0 \
  cargo +nightly-2026-06-06 fuzz run operation_sequence \
  --fuzz-dir fuzz -- -max_total_time=3600 -max_len=4096
```

停止時間を指定しない場合は、`Ctrl-C`で終了するまで実行される。

特定の入力を再実行する場合:

```sh
cargo +nightly-2026-06-06 fuzz run operation_sequence \
  --fuzz-dir fuzz fuzz/artifacts/operation_sequence/<artifact>
```

crash入力を最小化する場合:

```sh
cargo +nightly-2026-06-06 fuzz tmin operation_sequence \
  --fuzz-dir fuzz fuzz/artifacts/operation_sequence/<artifact>
```

## 現在検査している内容

各fuzz入力を4 byte単位の命令として解釈し、最大1024命令を順番に実行する。
命令のbyte配置は次の通り。

| Byte | 用途 |
| --- | --- |
| 0 | opcode。`0..8`へ丸める |
| 1 | 操作対象。8個のhandle slotから選ぶ |
| 2 | capacity、offset、lengthなどの値 |
| 3 | read/write長、map後に書く値など |

opcodeごとの操作:

| Opcode | API | 動作 |
| --- | --- | --- |
| 0 | `mb_alloc` | 未使用slotへ1から64 byteを確保する |
| 1 | `mb_free` | handleを解放し、成功時にslotを空にする |
| 2 | `mb_write` | offset 0から63、長さ0から31 byteで書き込む |
| 3 | `mb_read` | offset 0から63、長さ0から31 byteで読み込む |
| 4 | `mb_set_length` | lengthを0から63へ変更する |
| 5 | `mb_map` | bufferをmapし、capacityがあれば先頭1 byteへ書く |
| 6 | `mb_unmap` | bufferのmapを解除する |
| 7 | `mb_get_info` | length、capacity、mapped状態を取得する |
| 8 | `mb_reset` | map中のbufferがない場合だけ全体をresetする |

テストごとに256 byteのarenaと8個のhandle slotを新しく作る。未確保handle、
解放済みhandle、範囲外offsetなども意図的にAPIへ渡し、不正な操作を安全に
エラーとして処理できるかを含めて検査する。

明示的に確認している条件は次の通り。

- `mb_init`が成功する
- `mb_alloc`の結果が`Ok`または`OutOfMemory`である
- map中でない場合の`mb_reset`が成功する
- panic、assertion failure、AddressSanitizerの異常が発生しない

`read`、`write`などの戻り値は限定していない。ランダムな操作列では正常系と
エラー系の両方が期待されるためである。

## 生成ファイル

実行により次のディレクトリが生成される。いずれも`.gitignore`対象である。

- `fuzz/corpus/operation_sequence/`: 到達経路を増やした入力の学習データ
- `fuzz/artifacts/operation_sequence/`: crashやassertion failureを再現する入力
- `fuzz/target/`: fuzz targetのbuild成果物

不具合を検出した場合はartifactで再現し、`tmin`で最小化して原因を修正する。
修正後は、同じ不具合を将来も検出できる通常のregression testへ移植する。

## 現時点の範囲

このtargetはAPIの操作順、境界値、無効handle、map状態遷移を広く探索する。
一方で、戻り値とbuffer内容を参照実装と比較するmodel-based test、複数threadから
の同時操作、C caller側との非同期連携は対象外である。

`ASAN_OPTIONS=detect_leaks=0`は、`ptrace`配下でLeakSanitizer自体が終了時に
失敗する環境を避けるために指定している。AddressSanitizerによる範囲外accessや
use-after-freeなどの検査は引き続き有効である。
