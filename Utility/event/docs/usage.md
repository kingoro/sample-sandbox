# 利用方法

## 初期化

```c
#include "utility_event.h"

static ut_event_t event_storage[16];
static ut_event_queue_t event_queue;

static ut_event_subscription_t subscription_storage[8];
static ut_event_dispatcher_t dispatcher;

void app_event_init(void)
{
    (void)ut_event_queue_init(
        &event_queue, event_storage, 16u);
    (void)ut_event_dispatcher_init(
        &dispatcher, subscription_storage, 8u);
}
```

## 発行と処理

```c
enum {
    APP_EVENT_PRINT_DATA_READY = 1u
};

static void on_print_data_ready(
    const ut_event_t *event,
    void *user_context)
{
    (void)user_context;
    /* event->payloadをこの呼出し中に利用する。 */
}

void app_register_handlers(void)
{
    (void)ut_event_subscribe(
        &dispatcher,
        APP_EVENT_PRINT_DATA_READY,
        on_print_data_ready,
        NULL);
}

ut_event_result_t app_post_print_data(const void *data, size_t size)
{
    const ut_event_t event = {
        APP_EVENT_PRINT_DATA_READY,
        2u,
        data,
        size
    };

    return ut_event_queue_push(&event_queue, &event);
}

void app_event_loop_step(void)
{
    ut_event_t event;

    if (ut_event_queue_pop(&event_queue, &event) == UT_EVENT_OK) {
        (void)ut_event_dispatch(&dispatcher, &event, NULL);
    }
}
```

Queueはpayload本体をcopyしない。この例で`data`が一時変数や再利用される受信領域を
指す場合は不正になる。非同期に保持するデータはBuffer Poolへcopyし、その参照を
Eventへ設定し、handler完了後に所有権規則に従って解放する。

`app_post_print_data`が`UT_EVENT_FULL`を返した場合、呼出側は再試行、明示的な破棄、
fault遷移などapplicationで定めた方針を実行する。

## State Machine

```c
enum {
    APP_STATE_IDLE = 1u,
    APP_STATE_PRINTING = 2u,
    APP_STATE_FAULT = 3u
};

enum {
    APP_EVENT_START = 10u,
    APP_EVENT_DONE = 11u,
    APP_EVENT_ERROR = 12u
};

static void enter_printing(
    ut_event_state_machine_t *machine,
    const ut_event_t *event,
    void *user_context)
{
    (void)machine;
    (void)event;
    (void)user_context;
}

static const ut_event_state_t states[] = {
    {APP_STATE_IDLE, "idle", NULL, NULL, NULL},
    {APP_STATE_PRINTING, "printing", enter_printing, NULL, NULL},
    {APP_STATE_FAULT, "fault", NULL, NULL, NULL}
};

static const ut_event_state_transition_t transitions[] = {
    {APP_STATE_IDLE, APP_EVENT_START, APP_STATE_PRINTING,
        NULL, NULL, NULL, 0u, 1u},
    {APP_STATE_PRINTING, APP_EVENT_DONE, APP_STATE_IDLE,
        NULL, NULL, NULL, 0u, 2u},
    {UT_EVENT_STATE_ID_ANY, APP_EVENT_ERROR, APP_STATE_FAULT,
        NULL, NULL, NULL, 0u, 3u}
};

static ut_event_state_machine_t print_sm;

void app_state_machine_init(void)
{
    (void)ut_event_state_machine_init(
        &print_sm,
        states,
        sizeof(states) / sizeof(states[0]),
        transitions,
        sizeof(transitions) / sizeof(transitions[0]),
        APP_STATE_IDLE,
        2u,
        NULL);
}
```

Event LoopまたはDispatcher handlerから、入力Eventを渡す。

```c
static void on_state_machine_event(
    const ut_event_t *event,
    void *user_context)
{
    (void)ut_event_state_machine_dispatch(
        (ut_event_state_machine_t *)user_context,
        event,
        NULL);
}
```

この設計では、状態と遷移の一覧がCのtableとして見える。小規模なら`switch`でも十分だが、
entry、exit、guard、traceが増えた時に処理順序がmoduleごとにばらつかない。遷移tableを
単体テストやreviewの対象にできるため、長期保守ではtable-driven方式を標準形にする。

## Timer Event

Timer Schedulerへ固定長slot storageを渡す。

```c
static ut_event_timer_slot_t timer_storage[8];
static ut_event_timer_scheduler_t timer_scheduler;

void app_timer_init(void)
{
    (void)ut_event_timer_scheduler_init(
        &timer_scheduler,
        timer_storage,
        sizeof(timer_storage) / sizeof(timer_storage[0]));
}
```

one-shot watchdogを開始する。時刻単位はApplication内で統一する。

```c
enum {
    APP_TIMER_WATCHDOG = 1u,
    APP_EVENT_WATCHDOG_TIMEOUT = 20u
};

void app_watchdog_start(uint64_t now_ticks)
{
    const ut_event_t event = {
        APP_EVENT_WATCHDOG_TIMEOUT,
        2u,
        NULL,
        0u
    };

    (void)ut_event_timer_start(
        &timer_scheduler,
        APP_TIMER_WATCHDOG,
        &event,
        now_ticks,
        500u,
        0u);
}
```

Event loopはclock adapterから単調tickを取得し、Timer Eventを既存Queueへ移す。

```c
void app_event_loop_step(uint64_t now_ticks)
{
    ut_event_t event;

    (void)ut_event_timer_process(
        &timer_scheduler,
        now_ticks,
        &event_queue,
        NULL);

    while (ut_event_queue_pop(&event_queue, &event) == UT_EVENT_OK) {
        (void)ut_event_dispatch(&dispatcher, &event, NULL);
    }
}
```

Linuxでは`CLOCK_MONOTONIC`、RTOSではuptime tick、bare metalではhardware counterを
adapterで`uint64_t`へ変換できる。UTCや日時clockをtimeout判定へ使わない。

## Event traceとLog連携

```c
#include "utility_event.h"

static ut_event_trace_t event_trace;
static ut_event_trace_log_config_t trace_log_config;

void app_trace_init(ut_logger_t *logger)
{
    trace_log_config.logger = logger;
    trace_log_config.module = "event";
    trace_log_config.level = UT_LOG_LEVEL_TRACE;

    (void)ut_event_trace_init(
        &event_trace,
        ut_event_trace_log_sink,
        &trace_log_config);
    (void)ut_event_subscribe(
        &dispatcher,
        UT_EVENT_ID_ANY,
        ut_event_trace_dispatch_handler,
        &event_trace);
}
```

状態遷移を記録したいmoduleは、遷移が確定した後に次のように通知する。

```c
(void)ut_event_trace_state_transition(
    &event_trace,
    2u,
    old_state,
    new_state,
    reason_code);
```

Trace APIはState Machine本体を所有しない。状態ID、理由code、出力先Loggerの寿命と
並行アクセスはapplication側で定義する。
