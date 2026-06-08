# Event Utility

Cでイベント駆動処理を構成するための、ドメイン非依存の最小Utility。
独立配布ライブラリではなく、`src/`のCファイルを利用側のビルドへ組み込んで使う。

提供する機能:

- 呼出側提供storageを使う固定長FIFO Event Queue
- Event IDに応じてhandlerを同期実行するDispatcher
- Event IDで駆動するtable-driven State Machine
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
| Timer Event | 未実装 | clock adapter、deadline、timeout発行 |
| Buffer Pool連携 | 未実装 | handle所有権移譲、解放規則 |
| ログ・状態遷移trace | 一部実装 | Event trace hook、状態遷移trace record、Log Utility adapter |

このため、現状をイベント駆動基盤一式とは扱わない。State Machine以降は、
時刻源、Buffer Pool API、状態遷移規則を定めてから追加する。

## Header構成

通常の利用者は外部公開用の`utility_event.h`だけをincludeする。

```text
utility_event.h
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

## 資料

- [初心者向け はじめてのEvent Utility](README_BEGINNER.md)
- [構成と責務](docs/architecture.md)
- [API仕様](docs/api.md)
- [利用方法](docs/usage.md)
- [テスト方針](docs/quality.md)

## テスト

repository rootで実行する。

```sh
make utility-event-test
make utility-static-analysis
make utility-event-fuzz-smoke
```

`make utility-event-test`はAPI単体テストに加え、Queue、Dispatcher、State Machine、
Trace、Log Ringを接続した結合シナリオテストも実行する。
