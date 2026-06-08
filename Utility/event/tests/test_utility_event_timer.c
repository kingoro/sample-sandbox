/**
 * @file test_utility_event_timer.c
 * @brief Timer Eventのone-shot、periodic、満杯、異常系単体テスト。
 */
#include "utility_event_timer.h"

#include "test_cases.h"
#include "test_support.h"

/** 単体テスト用Timer storage容量。 */
#define TEST_TIMER_CAPACITY 3u
/** 単体テスト用Event Queue容量。 */
#define TEST_TIMER_QUEUE_CAPACITY 3u

/**
 * one-shot TimerのdeadlineとEvent発行を検証する。
 *
 * @return 成功時は0、失敗時は1。
 */
static int test_one_shot_timer(void)
{
    ut_event_timer_slot_t timer_storage[TEST_TIMER_CAPACITY];
    ut_event_t queue_storage[TEST_TIMER_QUEUE_CAPACITY];
    ut_event_timer_scheduler_t scheduler;
    ut_event_queue_t queue;
    const ut_event_t expected = {20u, 2u, NULL, 0u};
    ut_event_t actual;
    size_t emitted_count = 99u;
    uint64_t deadline = 0u;

    CHECK(ut_event_timer_scheduler_init(
        &scheduler,
        timer_storage,
        TEST_TIMER_CAPACITY) == UT_EVENT_OK);
    CHECK(ut_event_queue_init(
        &queue,
        queue_storage,
        TEST_TIMER_QUEUE_CAPACITY) == UT_EVENT_OK);
    CHECK(ut_event_timer_start(
        &scheduler,
        1u,
        &expected,
        100u,
        25u,
        0u) == UT_EVENT_OK);
    CHECK(ut_event_timer_count(&scheduler) == 1u);
    CHECK(ut_event_timer_next_deadline(&scheduler, &deadline) == UT_EVENT_OK);
    CHECK(deadline == 125u);

    CHECK(ut_event_timer_process(
        &scheduler,
        124u,
        &queue,
        &emitted_count) == UT_EVENT_OK);
    CHECK(emitted_count == 0u);
    CHECK(ut_event_timer_process(
        &scheduler,
        125u,
        &queue,
        &emitted_count) == UT_EVENT_OK);
    CHECK(emitted_count == 1u);
    CHECK(ut_event_timer_count(&scheduler) == 0u);
    CHECK(ut_event_queue_pop(&queue, &actual) == UT_EVENT_OK);
    CHECK(actual.id == expected.id);
    CHECK(actual.source == expected.source);
    CHECK(ut_event_timer_next_deadline(&scheduler, &deadline) ==
        UT_EVENT_NOT_FOUND);
    return 0;
}

/**
 * periodic Timerが遅延回数分をburstせず次deadlineへ進むことを検証する。
 *
 * @return 成功時は0、失敗時は1。
 */
static int test_periodic_timer_coalesces_missed_periods(void)
{
    ut_event_timer_slot_t timer_storage[TEST_TIMER_CAPACITY];
    ut_event_t queue_storage[TEST_TIMER_QUEUE_CAPACITY];
    ut_event_timer_scheduler_t scheduler;
    ut_event_queue_t queue;
    const ut_event_t event = {21u, 3u, NULL, 0u};
    size_t emitted_count = 0u;
    uint64_t deadline = 0u;

    CHECK(ut_event_timer_scheduler_init(
        &scheduler,
        timer_storage,
        TEST_TIMER_CAPACITY) == UT_EVENT_OK);
    CHECK(ut_event_queue_init(
        &queue,
        queue_storage,
        TEST_TIMER_QUEUE_CAPACITY) == UT_EVENT_OK);
    CHECK(ut_event_timer_start(
        &scheduler,
        2u,
        &event,
        10u,
        5u,
        10u) == UT_EVENT_OK);

    CHECK(ut_event_timer_process(
        &scheduler,
        46u,
        &queue,
        &emitted_count) == UT_EVENT_OK);
    CHECK(emitted_count == 1u);
    CHECK(ut_event_queue_count(&queue) == 1u);
    CHECK(ut_event_timer_count(&scheduler) == 1u);
    CHECK(ut_event_timer_next_deadline(&scheduler, &deadline) == UT_EVENT_OK);
    CHECK(deadline == 55u);
    return 0;
}

/**
 * Queue満杯時にTimerがactiveなまま残り再試行できることを検証する。
 *
 * @return 成功時は0、失敗時は1。
 */
static int test_queue_full_retries_timer(void)
{
    ut_event_timer_slot_t timer_storage[1];
    ut_event_t queue_storage[1];
    ut_event_timer_scheduler_t scheduler;
    ut_event_queue_t queue;
    const ut_event_t occupied = {30u, 0u, NULL, 0u};
    const ut_event_t timeout = {31u, 0u, NULL, 0u};
    ut_event_t actual;
    size_t emitted_count = 99u;

    CHECK(ut_event_timer_scheduler_init(
        &scheduler,
        timer_storage,
        1u) == UT_EVENT_OK);
    CHECK(ut_event_queue_init(&queue, queue_storage, 1u) == UT_EVENT_OK);
    CHECK(ut_event_queue_push(&queue, &occupied) == UT_EVENT_OK);
    CHECK(ut_event_timer_start(
        &scheduler,
        3u,
        &timeout,
        0u,
        0u,
        0u) == UT_EVENT_OK);

    CHECK(ut_event_timer_process(
        &scheduler,
        0u,
        &queue,
        &emitted_count) == UT_EVENT_FULL);
    CHECK(emitted_count == 0u);
    CHECK(ut_event_timer_count(&scheduler) == 1u);
    CHECK(ut_event_queue_pop(&queue, &actual) == UT_EVENT_OK);
    CHECK(ut_event_timer_process(
        &scheduler,
        0u,
        &queue,
        &emitted_count) == UT_EVENT_OK);
    CHECK(emitted_count == 1u);
    CHECK(ut_event_timer_count(&scheduler) == 0u);
    CHECK(ut_event_queue_pop(&queue, &actual) == UT_EVENT_OK);
    CHECK(actual.id == timeout.id);
    return 0;
}

/**
 * Timerの重複、restart、cancel、capacityを検証する。
 *
 * @return 成功時は0、失敗時は1。
 */
static int test_restart_cancel_and_capacity(void)
{
    ut_event_timer_slot_t timer_storage[1];
    ut_event_timer_scheduler_t scheduler;
    const ut_event_t first = {40u, 0u, NULL, 0u};
    const ut_event_t second = {41u, 0u, NULL, 0u};
    uint64_t deadline = 0u;

    CHECK(ut_event_timer_scheduler_init(
        &scheduler,
        timer_storage,
        1u) == UT_EVENT_OK);
    CHECK(ut_event_timer_start(
        &scheduler,
        4u,
        &first,
        10u,
        5u,
        0u) == UT_EVENT_OK);
    CHECK(ut_event_timer_start(
        &scheduler,
        4u,
        &first,
        10u,
        5u,
        0u) == UT_EVENT_ALREADY_EXISTS);
    CHECK(ut_event_timer_start(
        &scheduler,
        5u,
        &first,
        10u,
        5u,
        0u) == UT_EVENT_FULL);
    CHECK(ut_event_timer_restart(
        &scheduler,
        4u,
        &second,
        20u,
        7u,
        3u) == UT_EVENT_OK);
    CHECK(ut_event_timer_next_deadline(&scheduler, &deadline) == UT_EVENT_OK);
    CHECK(deadline == 27u);
    CHECK(ut_event_timer_restart(
        &scheduler,
        5u,
        &second,
        20u,
        7u,
        0u) == UT_EVENT_NOT_FOUND);
    CHECK(ut_event_timer_cancel(&scheduler, 4u) == UT_EVENT_OK);
    CHECK(ut_event_timer_cancel(&scheduler, 4u) == UT_EVENT_NOT_FOUND);
    CHECK(ut_event_timer_count(&scheduler) == 0u);
    return 0;
}

/**
 * deadline overflow、壊れたcontext、processing中操作を検証する。
 *
 * @return 成功時は0、失敗時は1。
 */
static int test_invalid_arguments_and_overflow(void)
{
    ut_event_timer_slot_t timer_storage[1];
    ut_event_timer_scheduler_t scheduler = {0};
    ut_event_t queue_storage[1];
    ut_event_queue_t queue;
    const ut_event_t event = {50u, 0u, NULL, 0u};
    uint64_t deadline = 0u;

    CHECK(ut_event_timer_scheduler_init(NULL, timer_storage, 1u) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_timer_scheduler_init(&scheduler, NULL, 1u) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_timer_scheduler_init(&scheduler, timer_storage, 0u) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_timer_start(
        &scheduler,
        1u,
        &event,
        0u,
        0u,
        0u) == UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_timer_scheduler_init(
        &scheduler,
        timer_storage,
        1u) == UT_EVENT_OK);
    CHECK(ut_event_queue_init(&queue, queue_storage, 1u) == UT_EVENT_OK);
    CHECK(ut_event_timer_start(
        &scheduler,
        1u,
        NULL,
        0u,
        0u,
        0u) == UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_timer_start(
        &scheduler,
        1u,
        &event,
        UINT64_MAX,
        1u,
        0u) == UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_timer_restart(
        &scheduler,
        1u,
        &event,
        UINT64_MAX,
        1u,
        0u) == UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_timer_cancel(NULL, 1u) == UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_timer_process(NULL, 0u, &queue, NULL) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_timer_process(&scheduler, 0u, NULL, NULL) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_timer_next_deadline(NULL, &deadline) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_timer_next_deadline(&scheduler, NULL) ==
        UT_EVENT_INVALID_ARGUMENT);

    scheduler.processing = 1u;
    CHECK(ut_event_timer_start(
        &scheduler,
        1u,
        &event,
        0u,
        0u,
        0u) == UT_EVENT_BUSY);
    CHECK(ut_event_timer_restart(
        &scheduler,
        1u,
        &event,
        0u,
        0u,
        0u) == UT_EVENT_BUSY);
    CHECK(ut_event_timer_cancel(&scheduler, 1u) == UT_EVENT_BUSY);
    CHECK(ut_event_timer_process(&scheduler, 0u, &queue, NULL) ==
        UT_EVENT_BUSY);
    return 0;
}

/**
 * periodic deadlineがuint64_t範囲を超える場合に停止することを検証する。
 *
 * @return 成功時は0、失敗時は1。
 */
static int test_periodic_deadline_exhaustion(void)
{
    ut_event_timer_slot_t timer_storage[1];
    ut_event_t queue_storage[1];
    ut_event_timer_scheduler_t scheduler;
    ut_event_queue_t queue;
    const ut_event_t event = {60u, 0u, NULL, 0u};

    CHECK(ut_event_timer_scheduler_init(
        &scheduler,
        timer_storage,
        1u) == UT_EVENT_OK);
    CHECK(ut_event_queue_init(&queue, queue_storage, 1u) == UT_EVENT_OK);
    CHECK(ut_event_timer_start(
        &scheduler,
        6u,
        &event,
        UINT64_MAX - 1u,
        0u,
        1u) == UT_EVENT_OK);
    CHECK(ut_event_timer_process(
        &scheduler,
        UINT64_MAX,
        &queue,
        NULL) == UT_EVENT_OK);
    CHECK(ut_event_timer_count(&scheduler) == 0u);
    CHECK(ut_event_queue_count(&queue) == 1u);
    return 0;
}

int run_utility_event_timer_tests(void)
{
    CHECK(test_one_shot_timer() == 0);
    CHECK(test_periodic_timer_coalesces_missed_periods() == 0);
    CHECK(test_queue_full_retries_timer() == 0);
    CHECK(test_restart_cancel_and_capacity() == 0);
    CHECK(test_invalid_arguments_and_overflow() == 0);
    CHECK(test_periodic_deadline_exhaustion() == 0);
    return 0;
}
