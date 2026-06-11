/**
 * @file utility_event_executor.c
 * @brief Event Executorの実装。
 */
#include "utility_event_executor.h"

#include <string.h>

/**
 * uint64_t counterをwrapさせず1増加する。
 *
 * @param counter 更新するcounter。
 */
static void increment_saturated(uint64_t *counter)
{
    if (*counter < UINT64_MAX) {
        (*counter)++;
    }
}

/**
 * Executorの必須接続を検査する。
 *
 * @param executor 検査するExecutor。
 * @return 利用可能なら真、それ以外は偽。
 */
static int executor_is_valid(const ut_event_executor_t *executor)
{
    return (executor != NULL) && (executor->queue != NULL) &&
        (executor->dispatcher != NULL);
}

/**
 * Timer発行結果をMetricsへ反映する。
 *
 * @param metrics 任意のMetrics。NULLなら何もしない。
 * @param emitted_count 発行成功Event数。
 * @param timer_result Timer処理結果。
 */
static void record_timer_metrics(
    ut_event_metrics_t *metrics,
    size_t emitted_count,
    ut_event_result_t timer_result)
{
    size_t index;

    if (metrics == NULL) {
        return;
    }
    for (index = 0u; index < emitted_count; index++) {
        increment_saturated(&metrics->timer_emitted_count);
        increment_saturated(&metrics->published_count);
    }
    if (timer_result == UT_EVENT_FULL) {
        increment_saturated(&metrics->timer_blocked_count);
        increment_saturated(&metrics->publish_full_count);
    }
}

/**
 * 1件のDispatcher結果をMetricsへ反映する。
 *
 * @param metrics 任意のMetrics。NULLなら何もしない。
 * @param result Dispatcher結果。
 */
static void record_dispatch_metrics(
    ut_event_metrics_t *metrics,
    ut_event_result_t result)
{
    if (metrics == NULL) {
        return;
    }

    increment_saturated(&metrics->dequeued_count);
    if (result == UT_EVENT_OK) {
        increment_saturated(&metrics->dispatched_count);
    } else if (result == UT_EVENT_NOT_FOUND) {
        increment_saturated(&metrics->unhandled_count);
    } else {
        increment_saturated(&metrics->dispatch_error_count);
    }
}

/**
 * 接続済みTimerを処理してreportとMetricsへ反映する。
 *
 * @param executor 実行中Executor。
 * @param now 現在の単調tick。
 * @param report 更新するstep report。
 * @return Timer未接続時UT_EVENT_OK、それ以外はTimer処理結果。
 */
static ut_event_result_t process_timers(
    ut_event_executor_t *executor,
    uint64_t now,
    ut_event_executor_report_t *report)
{
    if (executor->timer_scheduler == NULL) {
        return UT_EVENT_OK;
    }

    report->timer_result = ut_event_timer_process(
        executor->timer_scheduler,
        now,
        executor->queue,
        &report->timer_emitted_count);
    record_timer_metrics(
        executor->metrics,
        report->timer_emitted_count,
        report->timer_result);
    return report->timer_result;
}

/**
 * Queueから取得済みの1 Eventを検証して配送する。
 *
 * @param executor 実行中Executor。
 * @param event 処理するEvent。
 * @param report 更新するstep report。
 * @return 配送結果。Contract拒否は後続処理可能なためUT_EVENT_OK。
 */
static ut_event_result_t process_event(
    ut_event_executor_t *executor,
    const ut_event_t *event,
    ut_event_executor_report_t *report)
{
    ut_event_result_t result;

    if ((executor->contract_registry != NULL) &&
        (ut_event_contract_validate(
            executor->contract_registry,
            event) != UT_EVENT_OK)) {
        report->rejected_count++;
        if (executor->metrics != NULL) {
            increment_saturated(&executor->metrics->dequeued_count);
            increment_saturated(&executor->metrics->rejected_count);
        }
        return UT_EVENT_OK;
    }

    result = ut_event_dispatch(executor->dispatcher, event, NULL);
    record_dispatch_metrics(executor->metrics, result);
    if (result == UT_EVENT_OK) {
        report->dispatched_count++;
    } else if (result == UT_EVENT_NOT_FOUND) {
        report->unhandled_count++;
    } else {
        report->dispatch_error_count++;
        if (report->first_dispatch_error == UT_EVENT_OK) {
            report->first_dispatch_error = result;
        }
    }
    return result;
}

/**
 * Queueをbudget件まで取り出し、各Eventを検証・配送する。
 *
 * @param executor 実行中Executor。
 * @param event_budget 最大取得件数。
 * @param report 更新するstep report。
 * @return 最後に発生した継続可能でないerror。なければUT_EVENT_OK。
 */
static ut_event_result_t process_queue(
    ut_event_executor_t *executor,
    size_t event_budget,
    ut_event_executor_report_t *report)
{
    ut_event_result_t final_result = UT_EVENT_OK;

    while (report->dequeued_count < event_budget) {
        ut_event_t event;
        ut_event_result_t result =
            ut_event_queue_pop(executor->queue, &event);

        if (result == UT_EVENT_EMPTY) {
            break;
        }
        if (result != UT_EVENT_OK) {
            return result;
        }

        report->dequeued_count++;
        result = process_event(executor, &event, report);
        if ((result != UT_EVENT_OK) && (result != UT_EVENT_NOT_FOUND)) {
            final_result = result;
        }
    }
    return final_result;
}

ut_event_result_t ut_event_executor_init(
    ut_event_executor_t *executor,
    ut_event_queue_t *queue,
    ut_event_dispatcher_t *dispatcher,
    ut_event_timer_scheduler_t *timer_scheduler,
    const ut_event_contract_registry_t *contract_registry,
    ut_event_metrics_t *metrics)
{
    if ((executor == NULL) || (queue == NULL) || (dispatcher == NULL)) {
        return UT_EVENT_INVALID_ARGUMENT;
    }

    executor->queue = queue;
    executor->dispatcher = dispatcher;
    executor->timer_scheduler = timer_scheduler;
    executor->contract_registry = contract_registry;
    executor->metrics = metrics;
    executor->running = 0u;
    return UT_EVENT_OK;
}

ut_event_result_t ut_event_executor_run_once(
    ut_event_executor_t *executor,
    uint64_t now,
    size_t event_budget,
    ut_event_executor_report_t *out_report)
{
    ut_event_executor_report_t report;
    ut_event_result_t final_result;
    ut_event_result_t queue_result;

    if (!executor_is_valid(executor) || (event_budget == 0u)) {
        return UT_EVENT_INVALID_ARGUMENT;
    }
    if (executor->running != 0u) {
        return UT_EVENT_BUSY;
    }

    (void)memset(&report, 0, sizeof(report));
    report.timer_result = UT_EVENT_OK;
    report.first_dispatch_error = UT_EVENT_OK;
    executor->running = 1u;

    if (executor->metrics != NULL) {
        increment_saturated(&executor->metrics->executor_run_count);
        (void)ut_event_metrics_observe_queue(
            executor->metrics,
            ut_event_queue_count(executor->queue));
    }

    final_result = process_timers(executor, now, &report);

    if (executor->metrics != NULL) {
        (void)ut_event_metrics_observe_queue(
            executor->metrics,
            ut_event_queue_count(executor->queue));
    }

    queue_result = process_queue(executor, event_budget, &report);
    if (queue_result != UT_EVENT_OK) {
        final_result = queue_result;
    }

    if ((report.dequeued_count == event_budget) &&
        (ut_event_queue_count(executor->queue) > 0u)) {
        report.budget_exhausted = 1u;
        if (executor->metrics != NULL) {
            increment_saturated(&executor->metrics->budget_exhausted_count);
        }
    }

    executor->running = 0u;
    if (out_report != NULL) {
        *out_report = report;
    }
    return final_result;
}
