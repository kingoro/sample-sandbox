# Event Utility

Cでイベント駆動処理を構成するための、ドメイン非依存の最小Utility。
独立配布ライブラリではなく、`src/`のCファイルを利用側のビルドへ組み込んで使う。

提供する機能:

- 呼出側提供storageを使う固定長FIFO Event Queue
- Event IDに応じてhandlerを同期実行するDispatcher
- Event IDで駆動するtable-driven State Machine
- 外部の単調tickでone-shot/periodic Eventを発行するTimer Scheduler
- Event IDとpayload条件を一元検証するContract Registry
- Timer、契約検証、同期配送をbudget付きで進めるEvent Executor
- Queue使用量、配送結果、Timer詰まりを収集する飽和Metrics
- payloadを固定長slotへ値copyし、QueueとDispatcherを統合するPublisher
- Buffer Pool handleをEventと一緒にmoveする所有Envelope Queue
- Event履歴と状態遷移履歴を外部sinkへ通知するtrace hook
- trace recordをLog Utilityへ出力するadapter
- heap、RTOS、thread、I/Oへ依存しないC11実装
- Queue満杯、空、重複登録、再帰dispatchを明示的なresult codeで通知

## 実装範囲

イベント駆動基盤全体のうち、現時点で実装済みなのは次の範囲である。

| 項目 | 状態 | 内容 |
| --- | --- | --- |
| 基本型・エラー体系 | 一部 | Event Utility内の型とresult codeのみ |
| Ring Queue | 実装済み | `ut_event_t`専用の固定長FIFO |
| Event定義 | 実装済み | ID、発行元、非所有payload参照 |
| Dispatcher | 実装済み | Event IDによる同期配送 |
| State Machine | 実装済み | 状態table、遷移table、guard、action、entry/exit処理 |
| Timer Event | 実装済み | 外部tick、one-shot/periodic、restart/cancel、Queue発行 |
| Event Contract | 実装済み | ID、payload size、所有方式の一元定義と検証 |
| Event Executor | 実装済み | Timer処理、Contract検証、budget付き同期配送 |
| Metrics | 実装済み | Queue high-water mark、満杯、配送、拒否、Timer観測 |
| 値copy Publisher | 実装済み | 固定長payload所有、複数producer publish、budget同期配送 |
| Buffer Pool連携 | 実装済み | callback抽象、所有Envelope、move Queue、明示release |
| ログ・状態遷移trace | 実装済み | Event trace hook、State Machine自動遷移trace、Log Utility adapter |

イベント駆動基盤の主要構成要素は実装済みである。hardware/OS adapterと製品固有の
Event ID、payload schema、排他方針は利用側で定義する。

## Header構成

通常の利用者は外部公開用の`utility_event.h`だけをincludeする。

```text
utility_event.h
├── utility_event_buffer.h
│   ├── utility_event_types.h
│   └── utility_event_result.h
├── utility_event_contract.h
│   ├── utility_event_types.h
│   └── utility_event_result.h
├── utility_event_executor.h
│   ├── utility_event_contract.h
│   ├── utility_event_dispatcher.h
│   ├── utility_event_metrics.h
│   ├── utility_event_queue.h
│   └── utility_event_timer.h
├── utility_event_metrics.h
│   └── utility_event_result.h
├── utility_event_publisher.h
│   ├── utility_event_queue.h
│   └── utility_event_dispatcher.h
├── utility_event_queue.h
│   ├── utility_event_types.h
│   └── utility_event_result.h
├── utility_event_dispatcher.h
│   ├── utility_event_types.h
│   └── utility_event_result.h
├── utility_event_state_machine.h
│   ├── utility_event_types.h
│   ├── utility_event_trace.h
│   └── utility_event_result.h
├── utility_event_timer.h
│   ├── utility_event_queue.h
│   ├── utility_event_types.h
│   └── utility_event_result.h
├── utility_event_trace.h
│   ├── utility_event_types.h
│   └── utility_event_result.h
└── utility_event_trace_log.h
    ├── utility_event_trace.h
    └── utility_log.h
```

Queueだけを使う低レベルmoduleは`utility_event_queue.h`を直接includeしてもよい。

イベントには印刷データ本体を格納しない。`ut_event_t`は通知情報とpayloadへの参照を
値として保持する。印刷データなどの大きなpayloadは別のBuffer Pool等で管理し、
イベントにはその参照またはhandleを渡す。

## State Machineの使い方

State Machineは内部threadを持たない。main loop、RTOS task、Linux workerなど、
`ut_event_state_machine_dispatch()`を呼び出した実行context上で同期動作する。

状態とEventをIDで定義し、状態tableと遷移tableを用意する。

```c
enum {
    APP_STATE_IDLE = 1u,
    APP_STATE_RUNNING = 2u,
    APP_STATE_FAULT = 3u
};

enum {
    APP_EVENT_START = 10u,
    APP_EVENT_STOP = 11u,
    APP_EVENT_ERROR = 12u
};

static const ut_event_state_t states[] = {
    {APP_STATE_IDLE, "idle", NULL, NULL, NULL},
    {APP_STATE_RUNNING, "running", NULL, NULL, NULL},
    {APP_STATE_FAULT, "fault", NULL, NULL, NULL}
};

static const ut_event_state_transition_t transitions[] = {
    {APP_STATE_IDLE, APP_EVENT_START, APP_STATE_RUNNING,
        NULL, NULL, NULL, 0u, 1u},
    {APP_STATE_RUNNING, APP_EVENT_STOP, APP_STATE_IDLE,
        NULL, NULL, NULL, 0u, 2u},
    {UT_EVENT_STATE_ID_ANY, APP_EVENT_ERROR, APP_STATE_FAULT,
        NULL, NULL, NULL, 0u, 3u}
};

static ut_event_state_machine_t state_machine;
```

起動時にtableと初期状態を登録する。

```c
void app_state_machine_init(void)
{
    (void)ut_event_state_machine_init(
        &state_machine,
        states,
        sizeof(states) / sizeof(states[0]),
        transitions,
        sizeof(transitions) / sizeof(transitions[0]),
        APP_STATE_IDLE,
        1u,
        NULL);
}
```

Event Queueから取り出したEventをState Machineへ渡す。

```c
void app_event_loop_step(void)
{
    ut_event_t event;

    if (ut_event_queue_pop(&event_queue, &event) == UT_EVENT_OK) {
        (void)ut_event_state_machine_dispatch(
            &state_machine,
            &event,
            NULL);
    }
}
```

遷移tableは上から評価され、現在状態、Event ID、任意のguardが一致した最初の遷移を
実行する。通常遷移の処理順は`exit`、`action`、状態更新、`entry`、traceである。
同じState Machineを複数threadから直接呼ばず、単一のEvent処理contextへ集約する。

guard、entry/exit、action、traceを含む例は[利用方法](docs/usage.md)を参照する。

## Timer Eventの使い方

Timer Schedulerはclockやthreadを所有しない。main loop、RTOS task、Linux workerが
monotonicな現在tickを取得し、`ut_event_timer_process()`へ渡す。

```c
static ut_event_timer_slot_t timer_storage[4];
static ut_event_timer_scheduler_t timer_scheduler;

void app_timer_init(void)
{
    (void)ut_event_timer_scheduler_init(
        &timer_scheduler,
        timer_storage,
        sizeof(timer_storage) / sizeof(timer_storage[0]));
}
```

one-shot timeoutを開始する。

```c
const ut_event_t timeout_event = {
    APP_EVENT_ERROR,
    2u,
    NULL,
    0u
};

(void)ut_event_timer_start(
    &timer_scheduler,
    1u,
    &timeout_event,
    now_ticks,
    500u,
    0u);
```

Event loopで期限到達TimerをQueueへ発行する。

```c
(void)ut_event_timer_process(
    &timer_scheduler,
    now_ticks,
    &event_queue,
    NULL);
```

`period`を0より大きくするとperiodic Timerになる。処理が遅れて複数周期を通過しても
過去回数分をburst発行せず、1 Eventだけ発行して次の未来deadlineへ進む。Queue満杯時は
Timerをactiveなまま残すため、Queueを処理した後に再実行できる。

## Contract、Executor、Metricsの使い方

Event IDごとのpayload条件をtableとして一元定義する。

```c
static const ut_event_contract_t event_contracts[] = {
    {APP_EVENT_START, "start", 0u, 0u, UT_EVENT_PAYLOAD_NONE},
    {
        APP_EVENT_DATA_READY,
        "data-ready",
        sizeof(app_data_ref_t),
        sizeof(app_data_ref_t),
        UT_EVENT_PAYLOAD_BORROWED
    }
};

static ut_event_contract_registry_t contract_registry;
static ut_event_metrics_t event_metrics;
static ut_event_executor_t event_executor;

void app_event_runtime_init(void)
{
    (void)ut_event_contract_registry_init(
        &contract_registry,
        event_contracts,
        sizeof(event_contracts) / sizeof(event_contracts[0]));
    (void)ut_event_metrics_init(&event_metrics);
    (void)ut_event_executor_init(
        &event_executor,
        &event_queue,
        &dispatcher,
        &timer_scheduler,
        &contract_registry,
        &event_metrics);
}
```

main loopやRTOS taskから、1回に処理する最大Event数を指定して進める。

```c
void app_event_loop_step(uint64_t now_ticks)
{
    ut_event_executor_report_t report;

    (void)ut_event_executor_run_once(
        &event_executor,
        now_ticks,
        8u,
        &report);
}
```

Executorはthreadやsleepを生成しない。`run_once`を呼んだ実行context上で、Timer発行、
Contract検証、Dispatcherの同期handler実行を行う。budget到達時にQueueが残っていれば
`report.budget_exhausted`とMetricsへ記録し、呼出側が次回実行時期を決める。

Applicationが直接QueueへEventを発行した場合は、その結果をMetricsへ通知する。

```c
ut_event_result_t result = ut_event_queue_push(&event_queue, &event);

(void)ut_event_metrics_record_publish(
    &event_metrics,
    result,
    ut_event_queue_count(&event_queue));
```

snapshotは値copyで取得できる。Metricsはlockを持たないため、複数実行主体から更新する
場合は利用側で直列化する。

## 値copy Publisherの使い方

worker threadなどで生成した小さなpayloadを、発行元stackの寿命から切り離して
Event Loopへ渡す場合は`ut_event_publisher_t`を使用する。

PublisherはheapやMutexを所有しない。Event、payload、使用flag、subscriptionのstorageと
任意のlock callbackを利用側が提供する。publishはpayloadを固定長slotへcopyし、
dispatchは内部QueueからEventを取り出してDispatcherへ同期配送する。

```c
static ut_event_publisher_t publisher;
static ut_event_t event_storage[8];
static app_event_payload_t payload_storage[8];
static uint8_t occupied_storage[8];
static ut_event_subscription_t subscriptions[4];

void app_publisher_init(void)
{
    (void)ut_event_publisher_init(
        &publisher,
        event_storage,
        (uint8_t *)payload_storage,
        occupied_storage,
        8u,
        sizeof(payload_storage[0]),
        subscriptions,
        4u,
        app_lock,
        app_unlock,
        &app_mutex);
}
```

producerはcopy元の寿命を気にせず発行できる。

```c
app_event_payload_t payload = {.request_id = request_id};

(void)ut_event_publisher_publish_copy(
    &publisher,
    APP_EVENT_COMPLETED,
    APP_SOURCE_WORKER,
    &payload,
    sizeof(payload));
```

単一のEvent Loopからbudget付きで配送する。

```c
(void)ut_event_publisher_dispatch(&publisher, 8u, NULL);
```

handler実行中もpayload slotは所有状態を維持するため、handlerはcallback中にpayloadを
安全に参照できる。handlerから新しいEventをpublishでき、budget内なら同じdispatchで
続けて処理される。payloadをcallback後まで保持してはならない。

## Buffer Pool連携の使い方

大きなpayloadは通常の`ut_event_queue_t`へpointerだけを積まず、Buffer Poolへ格納して
`ut_event_buffer_message_t`でhandle所有権を運ぶ。

```c
static ut_event_buffer_message_t buffer_queue_storage[4];
static ut_event_buffer_queue_t buffer_queue;

void app_buffer_event_init(void)
{
    (void)ut_event_buffer_queue_init(
        &buffer_queue,
        buffer_queue_storage,
        sizeof(buffer_queue_storage) / sizeof(buffer_queue_storage[0]));
}
```

producerはPool callback tableを指定してpayloadを作り、Queueへmoveする。

```c
ut_event_buffer_message_t message = {0};

if (ut_event_buffer_message_create_copy(
        &message,
        &buffer_pool,
        APP_EVENT_DATA_READY,
        2u,
        data,
        data_size) == UT_EVENT_OK) {
    if (ut_event_buffer_queue_push_move(
            &buffer_queue,
            &message) != UT_EVENT_OK) {
        (void)ut_event_buffer_message_release(&message);
    }
}
```

push成功時はQueueが所有者になり、失敗時はproducerが所有権を維持する。consumerは
popで所有権を受け取り、同期dispatch完了後にreleaseする。

```c
ut_event_buffer_message_t received = {0};

if (ut_event_buffer_queue_pop_move(
        &buffer_queue,
        &received) == UT_EVENT_OK) {
    (void)ut_event_dispatch(
        &dispatcher,
        ut_event_buffer_message_event(&received),
        NULL);
    (void)ut_event_buffer_message_release(&received);
}
```

`buffer_pool`はalloc/free/read/write callbackを持つ抽象tableであり、既存の
`memory-buffer`、RTOS Memory Pool、製品固有Poolへ接続できる。

## 資料

- [実行可能サンプル3パターン](examples/README.md)
- [初心者向け はじめてのEvent Utility](README_BEGINNER.md)
- [構成と責務](docs/architecture.md)
- [API仕様](docs/api.md)
- [利用方法](docs/usage.md)
- [テスト方針](docs/quality.md)

## テスト

repository rootで実行する。

```sh
make utility-event-test
make utility-event-examples
make utility-static-analysis
make utility-event-fuzz-smoke
```

`make utility-event-test`はAPI単体テストに加え、Queue、Dispatcher、State Machine、
Trace、Log Ringを接続した結合シナリオテストも実行する。
