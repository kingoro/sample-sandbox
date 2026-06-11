/**
 * @file test_utility_event_queue.c
 * @brief Event Queueの正常系、境界値、異常系単体テスト。
 */
#include "utility_event_queue.h"

#include "test_cases.h"
#include "test_support.h"

/**
 * FIFO順序、満杯検出、peek、リング終端のwraparoundを検証する。
 *
 * @return 成功時は0、失敗時は1。
 */
static int test_fifo_and_wraparound(void)
{
    ut_event_t storage[2];
    ut_event_queue_t queue;
    const uint32_t payload = 42u;
    const ut_event_t first = {1u, 10u, &payload, sizeof(payload)};
    const ut_event_t second = {2u, 20u, NULL, 0u};
    const ut_event_t third = {3u, 30u, NULL, 0u};
    ut_event_t output;

    CHECK(ut_event_queue_init(&queue, storage, 2u) == UT_EVENT_OK);
    CHECK(ut_event_queue_count(&queue) == 0u);
    CHECK(ut_event_queue_capacity(&queue) == 2u);
    CHECK(ut_event_queue_pop(&queue, &output) == UT_EVENT_EMPTY);

    CHECK(ut_event_queue_push(&queue, &first) == UT_EVENT_OK);
    CHECK(ut_event_queue_push(&queue, &second) == UT_EVENT_OK);
    CHECK(ut_event_queue_push(&queue, &third) == UT_EVENT_FULL);
    CHECK(ut_event_queue_peek(&queue, &output) == UT_EVENT_OK);
    CHECK(output.id == first.id);
    CHECK(output.payload == &payload);

    CHECK(ut_event_queue_pop(&queue, &output) == UT_EVENT_OK);
    CHECK(output.id == first.id);
    CHECK(ut_event_queue_push(&queue, &third) == UT_EVENT_OK);
    CHECK(ut_event_queue_pop(&queue, &output) == UT_EVENT_OK);
    CHECK(output.id == second.id);
    CHECK(ut_event_queue_pop(&queue, &output) == UT_EVENT_OK);
    CHECK(output.id == third.id);
    CHECK(ut_event_queue_count(&queue) == 0u);
    return 0;
}

/**
 * clear、NULL引数、空Queue、壊れたcontextの拒否を検証する。
 *
 * @return 成功時は0、失敗時は1。
 */
static int test_clear_and_invalid_arguments(void)
{
    ut_event_t storage[1];
    ut_event_queue_t queue = {0};
    const ut_event_t event = {1u, 0u, NULL, 0u};
    ut_event_t output;

    CHECK(ut_event_queue_init(NULL, storage, 1u) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_queue_init(&queue, NULL, 1u) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_queue_init(&queue, storage, 0u) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_queue_push(&queue, &event) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_queue_pop(&queue, &output) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_queue_peek(&queue, &output) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_queue_clear(&queue) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_queue_count(&queue) == 0u);
    CHECK(ut_event_queue_capacity(&queue) == 0u);

    CHECK(ut_event_queue_init(&queue, storage, 1u) == UT_EVENT_OK);
    CHECK(ut_event_queue_push(&queue, NULL) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_queue_pop(&queue, NULL) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_queue_peek(&queue, NULL) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_queue_peek(&queue, &output) == UT_EVENT_EMPTY);
    CHECK(ut_event_queue_push(&queue, &event) == UT_EVENT_OK);
    CHECK(ut_event_queue_clear(&queue) == UT_EVENT_OK);
    CHECK(ut_event_queue_count(&queue) == 0u);

    queue.head = queue.capacity;
    CHECK(ut_event_queue_push(&queue, &event) ==
        UT_EVENT_INVALID_ARGUMENT);
    queue.head = 0u;
    queue.tail = queue.capacity;
    CHECK(ut_event_queue_push(&queue, &event) ==
        UT_EVENT_INVALID_ARGUMENT);
    queue.tail = 0u;
    queue.count = queue.capacity + 1u;
    CHECK(ut_event_queue_push(&queue, &event) ==
        UT_EVENT_INVALID_ARGUMENT);
    return 0;
}

int run_utility_event_queue_tests(void)
{
    CHECK(test_fifo_and_wraparound() == 0);
    CHECK(test_clear_and_invalid_arguments() == 0);
    return 0;
}
