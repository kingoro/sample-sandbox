/**
 * @file utility_event_metrics.c
 * @brief Event Metricsの実装。
 */
#include "utility_event_metrics.h"

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

ut_event_result_t ut_event_metrics_init(ut_event_metrics_t *metrics)
{
    return ut_event_metrics_reset(metrics);
}

ut_event_result_t ut_event_metrics_record_publish(
    ut_event_metrics_t *metrics,
    ut_event_result_t publish_result,
    size_t queue_depth)
{
    if (metrics == NULL) {
        return UT_EVENT_INVALID_ARGUMENT;
    }

    if (publish_result == UT_EVENT_OK) {
        increment_saturated(&metrics->published_count);
    } else if (publish_result == UT_EVENT_FULL) {
        increment_saturated(&metrics->publish_full_count);
    }
    if (queue_depth > metrics->queue_high_watermark) {
        metrics->queue_high_watermark = queue_depth;
    }
    return UT_EVENT_OK;
}

ut_event_result_t ut_event_metrics_observe_queue(
    ut_event_metrics_t *metrics,
    size_t queue_depth)
{
    if (metrics == NULL) {
        return UT_EVENT_INVALID_ARGUMENT;
    }
    if (queue_depth > metrics->queue_high_watermark) {
        metrics->queue_high_watermark = queue_depth;
    }
    return UT_EVENT_OK;
}

ut_event_result_t ut_event_metrics_snapshot(
    const ut_event_metrics_t *metrics,
    ut_event_metrics_t *out_snapshot)
{
    if ((metrics == NULL) || (out_snapshot == NULL)) {
        return UT_EVENT_INVALID_ARGUMENT;
    }
    *out_snapshot = *metrics;
    return UT_EVENT_OK;
}

ut_event_result_t ut_event_metrics_reset(ut_event_metrics_t *metrics)
{
    if (metrics == NULL) {
        return UT_EVENT_INVALID_ARGUMENT;
    }
    (void)memset(metrics, 0, sizeof(*metrics));
    return UT_EVENT_OK;
}
