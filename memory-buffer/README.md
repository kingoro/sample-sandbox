# Memory Buffer

`memory-buffer`は、バッファの割り当てと生存期間をRust側で管理し、
小さなC ABIとして公開するヒープ非使用のメモリ操作ライブラリである。

利用者は内部のpointerやallocation metadataを直接操作せず、opaque handleを介して
バッファを扱う。範囲外access、解放後access、貸出中pointerとの競合をAPI境界で
検査し、メモリ管理の詳細を呼出側から隠蔽する。

初めて読む人、C経験が中心の人は、先に
[C経験者向け はじめてのMemory Buffer](README_BEGINNER.md)を参照する。

## ドキュメント

- [C経験者向け はじめてのMemory Buffer](README_BEGINNER.md)
- [構成・責務・状態遷移](docs/architecture.md)
- [Cからの利用方法・copy経路・直接access](docs/usage.md)
- [外部提供用C API仕様](docs/api.md)
- [coverage・静的解析・CC・MIの品質ゲート](docs/quality.md)
- [cbindgen・Miri・fuzz・cross buildによる高度検証](docs/advanced-verification.md)
- [公開C API](include/memory_buffer.h)

## 責務

このライブラリが担当する範囲は次のとおり。

- 呼出側が提供したRAMから、重複しない領域を割り当てる
- opaque handleを使ってバッファの生存期間を管理する
- read、write、論理長の境界を検証する
- ポインタ貸出中の解放や変更を防止する
- 解放またはreset後の古いhandleを拒否する

thread同期、I/O、retry、protocol、永続化、転送制御、cache maintenanceは
担当しない。このライブラリはbyte列の格納領域とその所有状態だけを扱う。

## 設計判断

- 制御領域とデータarenaは呼出側が提供する
- `mb_init`は未初期化領域にも使用でき、利用者が停止済みなら再初期化にも
  使用できる。再初期化前のhandleはすべて破棄する
- 1 contextにつき最大64バッファを管理する
- handleに世代番号を含め、解放後アクセスを拒否する
- 通常アクセスには境界検査付きcopy APIを使う
- copyを避ける必要がある場合だけ`mb_map`でポインタを排他的に一時貸出する
- map中のバッファは二重map、read、write、length変更、free、resetを拒否する
- 内部lockは持たない。1 taskがcontextを所有するか、呼出側が同期する
- first-fitで割り当て、arenaのcompactionは行わない。長期稼働する製品では
  size classを固定するか、用途ごとにcontextを分離する

## 並行実行の契約

ABIは内部lockを持たない。contextは1つの実行主体が所有するか、すべての操作を
同じ外部lockで保護する。同じcontextへ複数の実行主体から同時アクセスしては
ならない。

## ポインタ貸出の契約

`mb_map`はcopyを介さずに内容へ直接accessするためのAPIであり、所有権移譲ではなく
一時貸出である。
同じバッファへ同時に複数のpointerは貸し出さない。
`mb_unmap`より前に、貸し出したpointerを参照する処理がすべて終了したことを
呼出側が保証する。

## 異常時の方針

想定内の異常はすべて`mb_result_t`で返す。無効なhandleや範囲外アクセスで
panicしない。`no_std` static library内部でpanicした場合は、C ABIをまたぐ
unwindを防ぐためspinする。利用環境に応じたfault方針へ置き換えてよい。

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

内容へ直接アクセスする場合は明示的に貸出・返却する。

```c
uint8_t *data;
size_t capacity;

mb_map(&context, handle, &data, &capacity);
/* dataを直接読み書きする。 */
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
C1 coverage、CC、MI、header drift、32 bit `no_std` cross buildの品質ゲートを
実行する。

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
CMake結合buildではdefault featureを無効にし、`no_std` static libraryを
実際に検証する。

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

別target向けにはCargoへtarget tripleを渡し、生成された`libmemory_buffer.a`を
既存C buildへlinkする。
