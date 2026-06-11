/**
 * @file state_machine_dispatch.c
 * @brief Queue、Dispatcher、State Machine、Traceを接続する基本例。
 *
 * @par 使用コンポーネント
 * - ut_event_queue_t: producerが発行したEventをFIFOで保持する。
 * - ut_event_dispatcher_t: Queueから取り出したEventをhandlerへ同期配送する。
 * - ut_event_state_machine_t: Event IDに応じて状態を遷移させる。
 * - ut_event_trace_t: Event配送と状態遷移を観測する。
 * - Log Foundation: Traceと状態entryの内容を表示する。
 *
 * @par 処理フロー
 * 1. Queue、Dispatcher、State Machine、Trace、Loggerを初期化する。
 * 2. DispatcherへState Machine転送handlerとTrace handlerを登録する。
 * 3. producer相当の処理がSTARTとASYNC_COMPLETEDをQueueへpushする。
 * 4. Event LoopがQueueからEventをpopしてDispatcherへ渡す。
 * 5. DispatcherがState Machineを同期実行し、IDLEからRUNNINGを経てCOMPLETEDへ遷移する。
 * 6. Event配送と状態遷移のTraceをLog Foundationへ出力する。
 */
#include "utility_event.h"
#include "utility_log.h"

#include <stdbool.h>

/** Event Queue容量。 */
#define EXAMPLE_QUEUE_CAPACITY 4U
/** Dispatcher購読容量。 */
#define EXAMPLE_SUBSCRIPTION_CAPACITY 2U
/** Log Ring容量。 */
#define EXAMPLE_LOG_CAPACITY 16U

/** サンプル状態ID。 */
enum example_state {
    /** 要求待ち。 */
    EXAMPLE_STATE_IDLE = 1U,
    /** 非同期処理の完了待ち。 */
    EXAMPLE_STATE_RUNNING = 2U,
    /** 処理完了。 */
    EXAMPLE_STATE_COMPLETED = 3U
};

/** サンプルEvent ID。 */
enum example_event {
    /** 処理開始要求。 */
    EXAMPLE_EVENT_START = 10U,
    /** 非同期producerからの完了通知。 */
    EXAMPLE_EVENT_ASYNC_COMPLETED = 11U
};

/** handlerから観測する状態。 */
typedef struct example_context {
    /** COMPLETEDへ到達した場合true。 */
    bool completed;
} example_context_t;

/**
 * COMPLETED entryで観測状態を更新する。
 *
 * @param machine 実行中State Machine。
 * @param event 遷移を起こしたEvent。
 * @param user_context example_context_tへのpointer。
 */
static void enter_completed(
    ut_event_state_machine_t *machine,
    const ut_event_t *event,
    void *user_context)
{
    example_context_t *context = user_context;

    (void)machine;
    (void)event;
    context->completed = true;
    UT_LOG_INFO("EVENT-STATE", "operation completed");
}

/**
 * DispatcherからState Machineへ同期転送する。
 *
 * @param event 配送されたEvent。
 * @param user_context ut_event_state_machine_tへのpointer。
 */
static void dispatch_to_machine(
    const ut_event_t *event,
    void *user_context)
{
    ut_event_state_machine_t *machine = user_context;

    /*
     * 状態遷移はEventを受け取ったthread、つまりこの例ではmain上で実行される。
     * producer callbackからState Machineを直接操作せず、Queueへ通知する。
     */
    (void)ut_event_state_machine_dispatch(machine, event, NULL);
}

/**
 * State Machine接続パターンを実行する。
 *
 * @return 成功時0、失敗時1。
 */
int main(void)
{
    ut_event_t queue_storage[EXAMPLE_QUEUE_CAPACITY];
    ut_event_subscription_t
        subscriptions[EXAMPLE_SUBSCRIPTION_CAPACITY];
    ut_log_record_t log_storage[EXAMPLE_LOG_CAPACITY];
    ut_event_queue_t queue;
    ut_event_dispatcher_t dispatcher;
    ut_event_state_machine_t machine;
    ut_event_trace_t trace;
    ut_logger_t logger;
    example_context_t context = {0};
    const ut_logger_config_t log_config = {
        .console_write = ut_log_console_write_file,
        .console_level = UT_LOG_LEVEL_INFO,
        .ring_level = UT_LOG_LEVEL_INFO,
        .console_enabled = true,
        .ring_enabled = true,
    };
    const ut_event_trace_log_config_t trace_config = {
        .logger = &logger,
        .module = "EVENT-TRACE",
        .level = UT_LOG_LEVEL_INFO,
    };
    const ut_event_state_t states[] = {
        {EXAMPLE_STATE_IDLE, "idle", NULL, NULL, &context},
        {EXAMPLE_STATE_RUNNING, "running", NULL, NULL, &context},
        {EXAMPLE_STATE_COMPLETED, "completed", enter_completed, NULL, &context},
    };
    const ut_event_state_transition_t transitions[] = {
        {EXAMPLE_STATE_IDLE, EXAMPLE_EVENT_START, EXAMPLE_STATE_RUNNING,
            NULL, NULL, NULL, 0U, 100U},
        {EXAMPLE_STATE_RUNNING, EXAMPLE_EVENT_ASYNC_COMPLETED,
            EXAMPLE_STATE_COMPLETED, NULL, NULL, NULL, 0U, 101U},
    };
    const ut_event_t events[] = {
        {EXAMPLE_EVENT_START, 1U, NULL, 0U},
        {EXAMPLE_EVENT_ASYNC_COMPLETED, 2U, NULL, 0U},
    };
    bool succeeded =
        ut_log_initialize(&logger, log_storage, EXAMPLE_LOG_CAPACITY,
            &log_config) == UT_LOG_OK
        && ut_event_queue_init(&queue, queue_storage,
            EXAMPLE_QUEUE_CAPACITY) == UT_EVENT_OK
        && ut_event_dispatcher_init(&dispatcher, subscriptions,
            EXAMPLE_SUBSCRIPTION_CAPACITY) == UT_EVENT_OK
        && ut_event_trace_init(&trace, ut_event_trace_log_sink,
            (void *)&trace_config) == UT_EVENT_OK
        && ut_event_state_machine_init(&machine, states, 3U, transitions, 2U,
            EXAMPLE_STATE_IDLE, 20U, &context) == UT_EVENT_OK
        && ut_event_state_machine_set_trace(&machine, &trace) == UT_EVENT_OK
        && ut_event_subscribe(&dispatcher, UT_EVENT_ID_ANY,
            dispatch_to_machine, &machine) == UT_EVENT_OK
        && ut_event_subscribe(&dispatcher, UT_EVENT_ID_ANY,
            ut_event_trace_dispatch_handler, &trace) == UT_EVENT_OK;

    for (size_t index = 0U; index < 2U && succeeded; ++index) {
        succeeded = ut_event_queue_push(&queue, &events[index]) == UT_EVENT_OK;
    }
    while ((ut_event_queue_count(&queue) > 0U) && succeeded) {
        ut_event_t event;

        succeeded = ut_event_queue_peek(&queue, &event) == UT_EVENT_OK
            && ut_event_queue_pop(&queue, &event) == UT_EVENT_OK
            && ut_event_dispatch(&dispatcher, &event, NULL) == UT_EVENT_OK;
    }
    succeeded = succeeded && context.completed
        && ut_event_state_machine_current_state(&machine)
            == EXAMPLE_STATE_COMPLETED;
    ut_log_shutdown();
    return succeeded ? 0 : 1;
}
