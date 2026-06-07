# Memory Buffer

`memory-buffer`は、バッファの割り当てと生存期間をRust側で管理し、
小さなC ABIとしてファームウェアへ提供するヒープ非使用ライブラリである。

## ドキュメント

- [構成・責務・状態遷移](docs/architecture.md)
- [Cへの組み込み方・copy経路・DMAシーケンス](docs/usage.md)
- [外部提供用C API仕様](docs/api.md)
- [coverage・静的解析・CC・MIの品質ゲート](docs/quality.md)
- [cbindgen・Miri・fuzz・MCU向け高度検証](docs/advanced-verification.md)
- [2026年6月7日時点の全体レビュー](docs/review-2026-06-07.md)
- [公開C API](include/memory_buffer.h)

## 責務

このライブラリが担当する範囲は次のとおり。

- 呼出側が提供したRAMから、重複しない領域を割り当てる
- opaque handleを使ってバッファの生存期間を管理する
- read、write、論理長の境界を検証する
- ポインタ貸出中の解放や変更を防止する
- 解放またはreset後の古いhandleを拒否する

Job、Workflow、Event配送、retry、protocol DTO、DMA設定、cache maintenance、
driver状態は担当しない。

## 設計判断

- 制御領域とデータarenaは呼出側が提供する
- `mb_init`は未初期化領域にも使用でき、利用者が停止済みなら再初期化にも
  使用できる。再初期化前のhandleはすべて破棄する
- 1 contextにつき最大64バッファを管理する
- handleに世代番号を含め、解放後アクセスを拒否する
- 通常アクセスには境界検査付きcopy APIを使う
- DMAやzero-copyが必要な場合だけ`mb_map`でポインタを排他的に一時貸出する
- map中のバッファは二重map、read、write、length変更、free、resetを拒否する
- 内部lockは持たない。1 taskがcontextを所有するか、呼出側が同期する
- first-fitで割り当て、arenaのcompactionは行わない。長期稼働する製品では
  size classを固定するか、用途ごとにcontextを分離する

## 並行実行の契約

ABIは意図的にlock-freeとしている。contextは1 taskが所有するか、すべての
操作を同じ外部lockで保護する。taskと割り込みハンドラから同じcontextへ
同時アクセスしてはならない。

## ポインタ貸出の契約

`mb_map`はDMAとzero-copyのためのAPIであり、所有権移譲ではなく一時貸出である。
同じバッファへ同時に複数のpointerは貸し出さない。
`mb_unmap`より前にDMAを停止し、taskや割り込みがポインタを参照しないことを
呼出側が保証する。cache clean/invalidateはplatform側の責務とする。

## 異常時の方針

想定内の異常はすべて`mb_result_t`で返す。無効なhandleや範囲外アクセスで
panicしない。`no_std` static library内部でpanicした場合は、C ABIをまたぐ
unwindを防ぐためspinする。製品組み込み時にplatformのfault方針へ置き換えてよい。

## C APIの最小例

```c
static mb_context_t context;
static uint8_t arena[64 * 1024];

mb_handle_t handle;
mb_init(&context, sizeof(context), arena, sizeof(arena));
mb_alloc(&context, 4096, &handle);
mb_write(&context, handle, 0, source, source_length);
mb_free(&context, handle);
```

DMAなどで直接アクセスする場合は明示的に貸出・返却する。

```c
uint8_t *data;
size_t capacity;

mb_map(&context, handle, &data, &capacity);
/* DMAまたは直接I/Oでdataを使用する。 */
mb_unmap(&context, handle);
/* data[0..bytes_written]が初期化済みの場合だけ論理長を確定する。 */
mb_set_length(&context, handle, bytes_written);
```

`mb_unmap`を呼ぶ前に、`data`を使用する処理がすべて停止していなければならない。

## BuildとTest

```sh
make check
```

`make check`はformat、Clippy、GCC静的解析、Rust/C結合test、Rustdoc生成、
C1 coverage、CC、MI、header drift、MCU cross buildの品質ゲートを実行する。

Miriとfuzz smokeまで含める場合:

```sh
make extended-check
```

閲覧用HTMLは次のコマンドで生成する。

```sh
make quality-report
```

入口は`build/reports/index.html`。

host testでは標準Rust test harnessを使うため`std` featureを有効にする。
CMake結合buildではdefault featureを無効にし、製品で使う`no_std` static
libraryを実際に検証する。

`.github/workflows/ci.yml`により、pushとpull requestごとに同じ品質ゲートを
実行する。いずれかが失敗した変更は完了扱いにしない。

## ドキュメント生成

公開Rust APIにはRustdoc、C headerにはDoxygen互換コメントを記述する。
Rust API referenceは次のコマンドで生成する。

```sh
make docs
```

生成先は`target/doc/memory_buffer/index.html`。設計判断とcross-languageの
利用規約はMarkdownへ、APIの事前条件は宣言の隣へ記述し、コード変更時に
生成ドキュメントも追従できる状態を保つ。

## テスト方針

単体テストでは、割り当てmetadata、古いhandle、境界検査、mapping、空き領域の
再利用を検証する。C結合テストでは、公開headerとstatic libraryを実際にlinkして
実行する。不具合修正時は、問題を再現する最下層のregression testを追加する。

firmware target向けにはCargoへtarget tripleを渡し、生成された
`libmemory_buffer.a`を既存C buildへlinkする。
