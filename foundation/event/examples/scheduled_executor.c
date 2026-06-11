/**
 * @file scheduled_executor.c
 * @brief Timer、Contract、Executor、Metricsを接続するEvent Loop例。
 *
 * @par 使用コンポーネント
 * - ut_event_timer_scheduler_t: 外部tickに基づいて周期Eventを発行する。
 * - ut_event_queue_t: Timerが発行したEventを保持する。
 * - ut_event_contract_registry_t: Event IDとpayload条件を検証する。
 * - ut_event_dispatcher_t: 検証済みEventをhandlerへ同期配送する。
 * - ut_event_executor_t: Timer、Queue、Contract、Dispatcherをbudget付きで進める。
 * - ut_event_metrics_t: 処理件数とQueue使用量を累積記録する。
 * - Log Foundation: handler実行結果とMetrics snapshotを表示する。
 *
 * @par 処理フロー
 * 1. Queue、Timer、Contract、Dispatcher、Metrics、Executor、Loggerを初期化する。
 * 2. Heartbeat handlerを登録し、periodic Timerを開始する。
 * 3. 呼出側loopがmonotonic tickと処理budgetをExecutorへ渡す。
 * 4. Executorが期限到達EventをQueueへ発行し、Contract検証後にhandlerへ配送する。
 * 5. Metrics snapshotから実行回数、Timer発行数、Queue最大深度を取得する。
 * 6. Timerをrestartして次回deadlineを確認し、cancelしてMetricsをresetする。
 */
#include "utility_event.h"
#include "utility_log.h"

#include <stdbool.h>
#include <stdint.h>

/** Event Queue容量。 */
#define EXAMPLE_QUEUE_CAPACITY 4U
/** Timer容量。 */
#define EXAMPLE_TIMER_CAPACITY 2U
/** Subscription容量。 */
#define EXAMPLE_SUBSCRIPTION_CAPACITY 2U
/** Log Ring容量。 */
#define EXAMPLE_LOG_CAPACITY 16U
/** 周期Timer ID。 */
#define EXAMPLE_TIMER_HEARTBEAT 1U
/** Heartbeat Event ID。 */
#define EXAMPLE_EVENT_HEARTBEAT 20U

/** handlerが更新する実行状態。 */
typedef struct executor_context {
    /** Heartbeat受信回数。 */
    size_t heartbeat_count;
} executor_context_t;

/**
 * Heartbeatを処理する。
 *
 * @param event Heartbeat Event。
 * @param user_context executor_context_tへのpointer。
 */
static void on_heartbeat(const ut_event_t *event, void *user_context)
{
    executor_context_t *context = user_context;

    (void)event;
    context->heartbeat_count++;
    UT_LOG_INFO("EVENT-LOOP", "heartbeat count=%u",
        (unsigned int)context->heartbeat_count);
}

/**
 * budget付きEvent Loopパターンを実行する。
 *
 * @return 成功時0、失敗時1。
 */
int main(void)
{
    ut_event_t queue_storage[EXAMPLE_QUEUE_CAPACITY];
    ut_event_timer_slot_t timer_storage[EXAMPLE_TIMER_CAPACITY];
    ut_event_subscription_t
        subscriptions[EXAMPLE_SUBSCRIPTION_CAPACITY];
    ut_log_record_t log_storage[EXAMPLE_LOG_CAPACITY];
    ut_event_queue_t queue;
    ut_event_timer_scheduler_t scheduler;
    ut_event_dispatcher_t dispatcher;
    ut_event_contract_registry_t registry;
    ut_event_metrics_t metrics;
    ut_event_metrics_t snapshot;
    ut_event_executor_t executor;
    ut_event_executor_report_t report;
    ut_logger_t logger;
    executor_context_t context = {0};
    const ut_logger_config_t log_config = {
        .console_write = ut_log_console_write_file,
        .console_level = UT_LOG_LEVEL_INFO,
        .ring_level = UT_LOG_LEVEL_INFO,
        .console_enabled = true,
        .ring_enabled = true,
    };
    const ut_event_contract_t contracts[] = {
        {EXAMPLE_EVENT_HEARTBEAT, "heartbeat", 0U, 0U,
            UT_EVENT_PAYLOAD_NONE},
    };
    const ut_event_t heartbeat = {
        EXAMPLE_EVENT_HEARTBEAT, 30U, NULL, 0U,
    };
    bool succeeded =
        ut_log_initialize(&logger, log_storage, EXAMPLE_LOG_CAPACITY,
            &log_config) == UT_LOG_OK
        && ut_event_queue_init(&queue, queue_storage,
            EXAMPLE_QUEUE_CAPACITY) == UT_EVENT_OK
        && ut_event_timer_scheduler_init(&scheduler, timer_storage,
            EXAMPLE_TIMER_CAPACITY) == UT_EVENT_OK
        && ut_event_dispatcher_init(&dispatcher, subscriptions,
            EXAMPLE_SUBSCRIPTION_CAPACITY) == UT_EVENT_OK
        && ut_event_contract_registry_init(&registry, contracts, 1U)
            == UT_EVENT_OK
        && ut_event_metrics_init(&metrics) == UT_EVENT_OK
        && ut_event_executor_init(&executor, &queue, &dispatcher, &scheduler,
            &registry, &metrics) == UT_EVENT_OK
        && ut_event_subscribe(&dispatcher, EXAMPLE_EVENT_HEARTBEAT,
            on_heartbeat, &context) == UT_EVENT_OK
        && ut_event_timer_start(&scheduler, EXAMPLE_TIMER_HEARTBEAT,
            &heartbeat, 0U, 100U, 100U) == UT_EVENT_OK;

    /*
     * Executorはclockやsleepを所有しない。呼出側のloopやschedulerが
     * monotonic tickを渡し、1回の処理量をbudgetで制限する。
     */
    for (uint64_t now = 0U; now <= 300U && succeeded; now += 50U) {
        succeeded = ut_event_executor_run_once(&executor, now, 2U, &report)
            == UT_EVENT_OK;
    }
    if (succeeded) {
        succeeded = ut_event_metrics_snapshot(&metrics, &snapshot)
            == UT_EVENT_OK;
    }
    if (succeeded) {
        UT_LOG_INFO("EVENT-LOOP", "runs=%u timer-events=%u high-water=%u",
            (unsigned int)snapshot.executor_run_count,
            (unsigned int)snapshot.timer_emitted_count,
            (unsigned int)snapshot.queue_high_watermark);
    }
    succeeded = succeeded
        && context.heartbeat_count == 3U
        && snapshot.timer_emitted_count == 3U;

    /*
     * restart/cancelは設定変更時の代表操作。restart後の最短deadlineを確認し、
     * 不要になったTimerを明示的にcancelする。
     */
    if (succeeded) {
        uint64_t deadline = 0U;

        succeeded = ut_event_timer_restart(&scheduler,
            EXAMPLE_TIMER_HEARTBEAT, &heartbeat, 300U, 200U, 0U)
                == UT_EVENT_OK
            && ut_event_timer_next_deadline(&scheduler, &deadline)
                == UT_EVENT_OK
            && deadline == 500U
            && ut_event_timer_cancel(&scheduler, EXAMPLE_TIMER_HEARTBEAT)
                == UT_EVENT_OK
            && ut_event_metrics_reset(&metrics) == UT_EVENT_OK;
    }
    ut_log_shutdown();
    return succeeded ? 0 : 1;
}
