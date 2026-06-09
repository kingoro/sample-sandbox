/**
 * @file utility_event_metrics.h
 * @brief Event処理量と異常を飽和counterで収集するMetrics API。
 */
#ifndef UTILITY_EVENT_METRICS_H
#define UTILITY_EVENT_METRICS_H

#include "utility_event_result.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Event Utilityの累積Metrics。各counterはUINT64_MAXで飽和する。 */
typedef struct ut_event_metrics {
    /** publish成功回数。 */
    uint64_t published_count;
    /** Queue満杯によりpublishできなかった回数。 */
    uint64_t publish_full_count;
    /** Queueから取り出したEvent数。 */
    uint64_t dequeued_count;
    /** 1個以上のhandlerへ配送できたEvent数。 */
    uint64_t dispatched_count;
    /** 購読handlerが存在しなかったEvent数。 */
    uint64_t unhandled_count;
    /** Contract違反または未登録で拒否したEvent数。 */
    uint64_t rejected_count;
    /** DispatcherがNOT_FOUND以外のerrorを返した回数。 */
    uint64_t dispatch_error_count;
    /** TimerからQueueへ発行したEvent数。 */
    uint64_t timer_emitted_count;
    /** Queue満杯によりTimer発行が停止した回数。 */
    uint64_t timer_blocked_count;
    /** Executor step実行回数。 */
    uint64_t executor_run_count;
    /** Event処理budgetを使い切り、QueueにEventが残った回数。 */
    uint64_t budget_exhausted_count;
    /** 観測したQueue件数の最大値。 */
    size_t queue_high_watermark;
} ut_event_metrics_t;

/**
 * Metricsを0へ初期化する。
 *
 * @param metrics 初期化するMetrics。
 * @return UT_EVENT_OKまたはUT_EVENT_INVALID_ARGUMENT。
 */
ut_event_result_t ut_event_metrics_init(ut_event_metrics_t *metrics);

/**
 * Queue publish結果と操作後Queue件数を記録する。
 *
 * producerはut_event_queue_push直後に呼び出す。UT_EVENT_OKとUT_EVENT_FULLだけを
 * counterへ反映し、それ以外のresultではhigh-water markだけを観測する。
 *
 * @param metrics 初期化済みMetrics。
 * @param publish_result Queue pushの結果。
 * @param queue_depth 操作後のQueue件数。
 * @return UT_EVENT_OKまたはUT_EVENT_INVALID_ARGUMENT。
 */
ut_event_result_t ut_event_metrics_record_publish(
    ut_event_metrics_t *metrics,
    ut_event_result_t publish_result,
    size_t queue_depth);

/**
 * 現在のQueue件数をhigh-water markへ反映する。
 *
 * @param metrics 初期化済みMetrics。
 * @param queue_depth 観測したQueue件数。
 * @return UT_EVENT_OKまたはUT_EVENT_INVALID_ARGUMENT。
 */
ut_event_result_t ut_event_metrics_observe_queue(
    ut_event_metrics_t *metrics,
    size_t queue_depth);

/**
 * Metrics snapshotを値copyする。
 *
 * 内部lockは持たないため、更新主体と同時に呼ぶ場合は利用側で直列化する。
 *
 * @param metrics copy元Metrics。
 * @param out_snapshot snapshot格納先。
 * @return UT_EVENT_OKまたはUT_EVENT_INVALID_ARGUMENT。
 */
ut_event_result_t ut_event_metrics_snapshot(
    const ut_event_metrics_t *metrics,
    ut_event_metrics_t *out_snapshot);

/**
 * Metricsをすべて0へ戻す。
 *
 * @param metrics resetするMetrics。
 * @return UT_EVENT_OKまたはUT_EVENT_INVALID_ARGUMENT。
 */
ut_event_result_t ut_event_metrics_reset(ut_event_metrics_t *metrics);

#ifdef __cplusplus
}
#endif

#endif
