# テスト方針

CTestからC11の単体テストを実行する。

```sh
make utility-test
```

検証対象:

- FIFO順序とリング終端でのwraparound
- 空Queue、満杯Queue、clear
- Event記述子とpayload参照のcopy
- Event ID一致と`UT_EVENT_ID_ANY`への配送
- 重複登録、登録容量不足、解除
- 配送先なし
- handlerからの再帰dispatch拒否
- NULL、容量0、未初期化context

通常の完了条件:

```sh
make utility-static-analysis
make utility-test
make utility-fuzz-smoke
```

`make coverage`または`make quality-report`では、GCC/gcovを使ってC単体テストを
coverage instrumentation付きで再実行する。結果は次へ出力する。

- `build/reports/coverage/event-c/summary.json`
- `build/reports/coverage/event-c/html/index.html`
- `build/reports/index.html`のEvent Utilityカード

品質基準はline coverage 80%以上、branch coverage 70%以上とする。

不具合修正時は、Queueなら`tests/test_utility_event_queue.c`、Dispatcherなら
`tests/test_utility_event_dispatcher.c`へ再現ケースを追加してから修正する。
thread safetyやISR safetyはこのUtility単体の保証範囲外であり、利用環境のadapterと
結合した試験を別途用意する。

## Fuzz test

`fuzz/fuzz_event_operations.c`は4 byte単位の命令列を解釈し、QueueとDispatcherの
APIをランダムな順序で実行する。次の不変条件を参照modelと照合する。

- Queue件数がpush、pop、clearの結果と一致する
- Queue件数がcapacityを超えない
- subscription件数がsubscribe、unsubscribeの結果と一致する
- subscription件数がcapacityを超えない
- dispatchの戻り値と実行handler数が矛盾しない
- ASan／UBSanが範囲外access、use-after-free、整数UBを報告しない

短時間検査はGCCでも実行できる。

```sh
make utility-fuzz-smoke
```

Clangがある環境ではcoverage-guidedなlibFuzzerを継続実行できる。

```sh
make utility-fuzz
```

検出した入力は最小化し、原因修正後に通常の単体テストへ移植する。
