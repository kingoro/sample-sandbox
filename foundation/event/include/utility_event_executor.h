/**
 * @file utility_event_executor.h
 * @brief Timer、Contract検証、同期dispatchを統括するEvent Executor API。
 */
#ifndef UTILITY_EVENT_EXECUTOR_H
#define UTILITY_EVENT_EXECUTOR_H

#include "utility_event_contract.h"
#include "utility_event_dispatcher.h"
#include "utility_event_metrics.h"
#include "utility_event_queue.h"
#include "utility_event_timer.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 1回のExecutor stepで観測した処理結果。 */
typedef struct ut_event_executor_report {
    /** TimerからQueueへ発行したEvent数。 */
    size_t timer_emitted_count;
    /** Queueから取り出したEvent数。 */
    size_t dequeued_count;
    /** handlerへ配送できたEvent数。 */
    size_t dispatched_count;
    /** 購読先がなかったEvent数。 */
    size_t unhandled_count;
    /** Contract検証で拒否したEvent数。 */
    size_t rejected_count;
    /** Dispatcher errorとなったEvent数。 */
    size_t dispatch_error_count;
    /** budget到達時にQueueへEventが残っていれば1。 */
    uint8_t budget_exhausted;
    /** Timer処理結果。 */
    ut_event_result_t timer_result;
    /** 最初に発生したDispatcher error。なければUT_EVENT_OK。 */
    ut_event_result_t first_dispatch_error;
} ut_event_executor_report_t;

/**
 * Event Queueを同期実行するExecutor。
 *
 * thread、clock、sleep、lockを所有しない。queueとdispatcherは必須、timer、
 * contract_registry、metricsは任意接続であり、利用終了まで有効に保つ。
 */
typedef struct ut_event_executor {
    /** Event入力Queue。 */
    ut_event_queue_t *queue;
    /** 同期配送先Dispatcher。 */
    ut_event_dispatcher_t *dispatcher;
    /** 任意のTimer Scheduler。 */
    ut_event_timer_scheduler_t *timer_scheduler;
    /** 任意のEvent Contract Registry。 */
    const ut_event_contract_registry_t *contract_registry;
    /** 任意のMetrics収集先。 */
    ut_event_metrics_t *metrics;
    /** step実行中なら1。再帰実行を拒否する。 */
    uint8_t running;
} ut_event_executor_t;

/**
 * Executorを初期化する。
 *
 * @param executor 初期化するExecutor。
 * @param queue Event入力Queue。
 * @param dispatcher Event配送先Dispatcher。
 * @param timer_scheduler 任意のTimer Scheduler。不要ならNULL。
 * @param contract_registry 任意のContract Registry。不要ならNULL。
 * @param metrics 任意のMetrics。不要ならNULL。
 * @return UT_EVENT_OKまたはUT_EVENT_INVALID_ARGUMENT。
 */
ut_event_result_t ut_event_executor_init(
    ut_event_executor_t *executor,
    ut_event_queue_t *queue,
    ut_event_dispatcher_t *dispatcher,
    ut_event_timer_scheduler_t *timer_scheduler,
    const ut_event_contract_registry_t *contract_registry,
    ut_event_metrics_t *metrics);

/**
 * Timer処理後、Queueから最大event_budget件を取り出して同期配送する。
 *
 * Contract未登録または違反EventはQueueから除去して配送しない。購読先なしは
 * unhandledとして記録し、step自体は継続する。TimerがQueue満杯になった場合も、
 * 既存Queueをbudgetまで処理してUT_EVENT_FULLを返す。
 *
 * @param executor 初期化済みExecutor。
 * @param now Timerへ渡す現在の単調tick。Timer未接続時は無視する。
 * @param event_budget 1回でQueueから取り出す最大Event数。0は指定できない。
 * @param out_report 任意の結果格納先。不要ならNULL。
 * @return UT_EVENT_OK、UT_EVENT_FULL、UT_EVENT_BUSY、
 * UT_EVENT_INVALID_ARGUMENT。
 */
ut_event_result_t ut_event_executor_run_once(
    ut_event_executor_t *executor,
    uint64_t now,
    size_t event_budget,
    ut_event_executor_report_t *out_report);

#ifdef __cplusplus
}
#endif

#endif
