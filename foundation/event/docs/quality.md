# テスト方針

CTestからC11の単体テストを実行する。

```sh
make foundation-test
```

検証対象:

- FIFO順序とリング終端でのwraparound
- 空Queue、満杯Queue、clear
- Event記述子とpayload参照のcopy
- Event ID一致と`UT_EVENT_ID_ANY`への配送
- 重複登録、登録容量不足、解除
- 配送先なし
- handlerからの再帰dispatch拒否
- State Machineの通常遷移、内部遷移、ANY遷移
- State Machineのguard、action、entry、exit、trace、再入拒否
- Timerのone-shot、periodic、restart、cancel、最短deadline
- periodic遅延時のcoalesce、Queue満杯時の再試行、uint64_t境界
- Buffer Envelopeのcreate、move、read、release、release-all
- Queue満杯・free失敗時の所有権維持と二重所有防止
- Contract table重複、不正size、未登録Event、payload違反
- Executorのbudget、未購読、Contract拒否、再入拒否、Timer満杯再試行
- Metricsのhigh-water mark、snapshot、reset、counter飽和
- NULL、容量0、未初期化context

結合テスト`utility_event_integration`では、次の製品利用に近い経路を一つの
シナリオとして検証する。

```text
Timer Scheduler
    -> Event Queue
    -> Contract Registry
    -> Event Executor
    -> Dispatcher
    -> State Machine
    -> Event・状態遷移Trace
    -> Log Foundation RAM Ring
    -> Metrics snapshot
```

シナリオは`IDLE -> RUNNING -> Timer timeout -> FAULT -> IDLE`を通り、途中に
遷移対象外Eventを含める。最終状態、entry/action呼出回数、Timer消費、Event履歴、
状態遷移履歴を検証する。

`memory_buffer_event_integration`では実際のRust/C Memory Buffer ABIと
Buffer Envelopeを接続し、alloc/write、Queue move、Dispatcher handler read、
release、stale handle拒否までを検証する。

通常の完了条件:

```sh
make foundation-static-analysis
make foundation-test
make foundation-fuzz-smoke
```

## API・Test仕様書

全C header、production source、単体テスト、fuzz harnessをDoxygen入力に含める。
header内のmacro、型、enum値、field、関数宣言にはDoxygenコメントを必須とし、
未文書化要素、引数説明不足、Doxygen構文errorは品質ゲートを失敗させる。

```sh
make c-docs-check
make c-docs
```

生成入口は`build/docs/c-api/html/index.html`。単体テスト関数とfuzz入口も仕様書から
参照できる。

`make coverage`または`make quality-report`では、GCC/gcovを使ってC単体テストを
coverage instrumentation付きで再実行する。結果は次へ出力する。

- `build/reports/coverage/foundation-c/unit/summary.json`
- `build/reports/coverage/foundation-c/unit/html/index.html`
- `build/reports/coverage/foundation-c/integration/summary.json`
- `build/reports/coverage/foundation-c/integration/html/index.html`
- `build/reports/index.html`のC Foundationカード

品質基準はrepository共通の`tools/quality-policy.json`を使用する。Event Foundationの
C単体テストには他言語の単体テストと同じ`unit_branch_min`、Event結合テストには
`integration_branch_min`をmodule単位で適用する。line coverageは未実行箇所を探す
参考値として表示するが、独自の閾値は設けない。

不具合修正時は、Queueなら`tests/test_utility_event_queue.c`、Dispatcherなら
`tests/test_utility_event_dispatcher.c`、State Machineなら
`tests/test_utility_event_state_machine.c`へ再現ケースを追加してから修正する。
thread safetyやISR safetyはこのUtility単体の保証範囲外であり、利用環境のadapterと
結合した試験を別途用意する。

## Fuzz test

`fuzz/fuzz_event_operations.c`は4 byte単位の命令列を解釈し、QueueとDispatcherの
APIに加えてTimer APIをランダムな順序で実行する。次の不変条件を照合する。

- Queue件数がpush、pop、clearの結果と一致する
- Queue件数がcapacityを超えない
- subscription件数がsubscribe、unsubscribeの結果と一致する
- subscription件数がcapacityを超えない
- dispatchの戻り値と実行handler数が矛盾しない
- Timer active件数がslot状態と一致しcapacityを超えない
- Buffer handle数がproducer、Queue、consumerの所有数合計と一致する
- Executor Queue件数とhigh-water markがcapacityを超えない
- Contract違反EventをExecutorがhandlerへ配送しない
- ASan／UBSanが範囲外access、use-after-free、整数UBを報告しない

短時間検査はGCCでも実行できる。

```sh
make foundation-fuzz-smoke
```

Clangがある環境ではcoverage-guidedなlibFuzzerを継続実行できる。

```sh
make foundation-fuzz
```

検出した入力は最小化し、原因修正後に通常の単体テストへ移植する。

C branch coverageはC Foundation全体の合算ではなく、`foundation/event`単位で
repository共通の単体テスト基準を満たさなければならない。
