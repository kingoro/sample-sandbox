/**
 * @file test_utility_event_metrics.c
 * @brief Event Metricsの単体テスト。
 */
#include "utility_event_metrics.h"

#include "test_support.h"

/**
 * publish記録、high-water mark、snapshot、resetを確認する。
 *
 * @return 成功時0、失敗時1。
 */
static int test_record_snapshot_and_reset(void)
{
    ut_event_metrics_t metrics;
    ut_event_metrics_t snapshot;

    CHECK(ut_event_metrics_init(&metrics) == UT_EVENT_OK);
    CHECK(ut_event_metrics_record_publish(
        &metrics, UT_EVENT_OK, 1u) == UT_EVENT_OK);
    CHECK(ut_event_metrics_record_publish(
        &metrics, UT_EVENT_FULL, 4u) == UT_EVENT_OK);
    CHECK(ut_event_metrics_record_publish(
        &metrics, UT_EVENT_BUSY, 2u) == UT_EVENT_OK);
    CHECK(ut_event_metrics_observe_queue(&metrics, 3u) == UT_EVENT_OK);
    CHECK(ut_event_metrics_snapshot(&metrics, &snapshot) == UT_EVENT_OK);
    CHECK(snapshot.published_count == 1u);
    CHECK(snapshot.publish_full_count == 1u);
    CHECK(snapshot.queue_high_watermark == 4u);

    CHECK(ut_event_metrics_reset(&metrics) == UT_EVENT_OK);
    CHECK(metrics.published_count == 0u);
    CHECK(metrics.queue_high_watermark == 0u);
    return 0;
}

/**
 * counter飽和とNULL引数拒否を確認する。
 *
 * @return 成功時0、失敗時1。
 */
static int test_saturating_counter_and_invalid_arguments(void)
{
    ut_event_metrics_t metrics;

    CHECK(ut_event_metrics_init(&metrics) == UT_EVENT_OK);
    metrics.published_count = UINT64_MAX;
    CHECK(ut_event_metrics_record_publish(
        &metrics, UT_EVENT_OK, 0u) == UT_EVENT_OK);
    CHECK(metrics.published_count == UINT64_MAX);

    CHECK(ut_event_metrics_init(NULL) == UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_metrics_record_publish(
        NULL, UT_EVENT_OK, 0u) == UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_metrics_observe_queue(NULL, 0u) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_metrics_snapshot(NULL, &metrics) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_metrics_snapshot(&metrics, NULL) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_metrics_reset(NULL) == UT_EVENT_INVALID_ARGUMENT);
    return 0;
}

int run_utility_event_metrics_tests(void)
{
    CHECK(test_record_snapshot_and_reset() == 0);
    CHECK(test_saturating_counter_and_invalid_arguments() == 0);
    return 0;
}
