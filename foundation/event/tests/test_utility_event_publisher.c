/**
 * @file test_utility_event_publisher.c
 * @brief 値copy Event Publisherの正常系、境界値、異常系単体テスト。
 */
#include "utility_event_publisher.h"

#include "test_cases.h"
#include "test_support.h"

#include <stddef.h>
#include <stdalign.h>
#include <string.h>

/** テストPublisherのEvent slot数。 */
#define TEST_PUBLISHER_CAPACITY 3U
/** テストPublisherの1 payload最大byte数。 */
#define TEST_PUBLISHER_PAYLOAD_CAPACITY 16U
/** テストPublisherの購読slot数。 */
#define TEST_PUBLISHER_SUBSCRIPTION_CAPACITY 3U

/**
 * テストで配送するpayload。
 */
typedef struct {
    /** テスト値。 */
    uint32_t value;
} test_publisher_payload_t;

/**
 * handlerとlock callbackの観測context。
 */
typedef struct {
    /** handler呼出回数。 */
    size_t handler_count;
    /** 最後に受信した値。 */
    uint32_t last_value;
    /** lock呼出回数。 */
    size_t lock_count;
    /** unlock呼出回数。 */
    size_t unlock_count;
    /** handlerから再publishするPublisher。 */
    ut_event_publisher_t *publisher;
} test_publisher_context_t;

/**
 * テストPublisher一式。
 */
typedef struct {
    /** Publisher context。 */
    ut_event_publisher_t publisher;
    /** Event Queue storage。 */
    ut_event_t events[TEST_PUBLISHER_CAPACITY];
    /** payload storage。 */
    alignas(max_align_t) uint8_t payloads[
        TEST_PUBLISHER_CAPACITY * TEST_PUBLISHER_PAYLOAD_CAPACITY];
    /** payload slot使用状態。 */
    uint8_t occupied[TEST_PUBLISHER_CAPACITY];
    /** Dispatcher購読storage。 */
    ut_event_subscription_t
        subscriptions[TEST_PUBLISHER_SUBSCRIPTION_CAPACITY];
} test_publisher_fixture_t;

/**
 * lock callback呼出しを記録する。
 *
 * @param context test_publisher_context_t。
 */
static void test_lock(void *context)
{
    test_publisher_context_t *test_context = context;

    test_context->lock_count++;
}

/**
 * unlock callback呼出しを記録する。
 *
 * @param context test_publisher_context_t。
 */
static void test_unlock(void *context)
{
    test_publisher_context_t *test_context = context;

    test_context->unlock_count++;
}

/**
 * payloadを値として観測する。
 *
 * @param event 配送Event。
 * @param context test_publisher_context_t。
 */
static void capture_handler(
    const ut_event_t *event,
    void *context)
{
    test_publisher_context_t *test_context = context;
    const test_publisher_payload_t *payload = event->payload;

    test_context->handler_count++;
    test_context->last_value = payload != NULL ? payload->value : 0U;
}

/**
 * handler内から次Eventを発行する。
 *
 * @param event 配送Event。
 * @param context test_publisher_context_t。
 */
static void republish_handler(
    const ut_event_t *event,
    void *context)
{
    test_publisher_context_t *test_context = context;
    const test_publisher_payload_t next = {99U};

    (void)event;
    test_context->handler_count++;
    (void)ut_event_publisher_publish_copy(
        test_context->publisher,
        2U,
        20U,
        &next,
        sizeof(next));
}

/**
 * fixtureを初期化する。
 *
 * @param fixture 初期化するfixture。
 * @param context lock callback context。
 * @return 初期化result。
 */
static ut_event_result_t initialize_fixture(
    test_publisher_fixture_t *fixture,
    test_publisher_context_t *context)
{
    (void)memset(fixture, 0, sizeof(*fixture));
    return ut_event_publisher_init(
        &fixture->publisher,
        fixture->events,
        fixture->payloads,
        fixture->occupied,
        TEST_PUBLISHER_CAPACITY,
        TEST_PUBLISHER_PAYLOAD_CAPACITY,
        fixture->subscriptions,
        TEST_PUBLISHER_SUBSCRIPTION_CAPACITY,
        test_lock,
        test_unlock,
        context);
}

/**
 * payload copy、FIFO配送、排他callbackを検証する。
 *
 * @return 成功時0。
 */
static int test_copy_dispatch_and_lock(void)
{
    test_publisher_fixture_t fixture;
    test_publisher_context_t context = {0};
    test_publisher_payload_t payload = {42U};
    size_t dispatched = 0U;

    CHECK(initialize_fixture(&fixture, &context) == UT_EVENT_OK);
    CHECK(ut_event_publisher_subscribe(
        &fixture.publisher, 1U, capture_handler, &context) == UT_EVENT_OK);
    CHECK(ut_event_publisher_publish_copy(
        &fixture.publisher,
        1U,
        10U,
        &payload,
        sizeof(payload)) == UT_EVENT_OK);
    payload.value = 7U;
    CHECK(ut_event_publisher_count(&fixture.publisher) == 1U);
    CHECK(ut_event_publisher_dispatch(
        &fixture.publisher, 1U, &dispatched) == UT_EVENT_OK);
    CHECK(dispatched == 1U);
    CHECK(context.handler_count == 1U);
    CHECK(context.last_value == 42U);
    CHECK(context.lock_count == context.unlock_count);
    CHECK(ut_event_publisher_count(&fixture.publisher) == 0U);
    return 0;
}

/**
 * handler内再publishとbudget継続配送を検証する。
 *
 * @return 成功時0。
 */
static int test_republish_and_budget(void)
{
    test_publisher_fixture_t fixture;
    test_publisher_context_t context = {0};
    const test_publisher_payload_t payload = {1U};
    size_t dispatched = 0U;

    CHECK(initialize_fixture(&fixture, &context) == UT_EVENT_OK);
    context.publisher = &fixture.publisher;
    CHECK(ut_event_publisher_subscribe(
        &fixture.publisher, 1U, republish_handler, &context) == UT_EVENT_OK);
    CHECK(ut_event_publisher_subscribe(
        &fixture.publisher, 2U, capture_handler, &context) == UT_EVENT_OK);
    CHECK(ut_event_publisher_publish_copy(
        &fixture.publisher,
        1U,
        10U,
        &payload,
        sizeof(payload)) == UT_EVENT_OK);
    CHECK(ut_event_publisher_dispatch(
        &fixture.publisher, 1U, &dispatched) == UT_EVENT_OK);
    CHECK(dispatched == 1U);
    CHECK(ut_event_publisher_count(&fixture.publisher) == 1U);
    CHECK(ut_event_publisher_dispatch(
        &fixture.publisher, 2U, &dispatched) == UT_EVENT_OK);
    CHECK(dispatched == 1U);
    CHECK(context.handler_count == 2U);
    CHECK(context.last_value == 99U);
    return 0;
}

/**
 * Queue満杯、payloadなし、購読先なしを検証する。
 *
 * @return 成功時0。
 */
static int test_capacity_and_payloadless_event(void)
{
    test_publisher_fixture_t fixture;
    test_publisher_context_t context = {0};
    const test_publisher_payload_t payload = {3U};
    size_t dispatched = 0U;

    CHECK(initialize_fixture(&fixture, &context) == UT_EVENT_OK);
    CHECK(ut_event_publisher_publish_copy(
        &fixture.publisher, 9U, 1U, &payload, sizeof(payload)) == UT_EVENT_OK);
    CHECK(ut_event_publisher_publish_copy(
        &fixture.publisher, 9U, 1U, &payload, sizeof(payload)) == UT_EVENT_OK);
    CHECK(ut_event_publisher_publish_copy(
        &fixture.publisher, 9U, 1U, NULL, 0U) == UT_EVENT_OK);
    CHECK(ut_event_publisher_publish_copy(
        &fixture.publisher, 9U, 1U, &payload, sizeof(payload)) == UT_EVENT_FULL);
    CHECK(ut_event_publisher_dispatch(
        &fixture.publisher, 3U, &dispatched) == UT_EVENT_OK);
    CHECK(dispatched == 3U);
    CHECK(ut_event_publisher_count(&fixture.publisher) == 0U);
    return 0;
}

/**
 * 初期化、publish、dispatchの引数異常を検証する。
 *
 * @return 成功時0。
 */
static int test_invalid_arguments(void)
{
    test_publisher_fixture_t fixture;
    test_publisher_context_t context = {0};
    const test_publisher_payload_t payload = {1U};

    CHECK(initialize_fixture(&fixture, &context) == UT_EVENT_OK);
    CHECK(ut_event_publisher_init(
        NULL, fixture.events, fixture.payloads, fixture.occupied,
        1U, 1U, fixture.subscriptions, 1U,
        NULL, NULL, NULL) == UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_publisher_init(
        &fixture.publisher, fixture.events, fixture.payloads, fixture.occupied,
        1U, 1U, fixture.subscriptions, 1U,
        test_lock, NULL, &context) == UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_publisher_publish_copy(
        NULL, 1U, 1U, &payload, sizeof(payload))
        == UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_publisher_publish_copy(
        &fixture.publisher, 1U, 1U, NULL, sizeof(payload))
        == UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_publisher_publish_copy(
        &fixture.publisher, 1U, 1U, &payload,
        TEST_PUBLISHER_PAYLOAD_CAPACITY + 1U)
        == UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_publisher_dispatch(
        &fixture.publisher, 0U, NULL) == UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_publisher_subscribe(
        NULL, 1U, capture_handler, &context) == UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_publisher_count(NULL) == 0U);
    return 0;
}

int run_utility_event_publisher_tests(void)
{
    CHECK(test_copy_dispatch_and_lock() == 0);
    CHECK(test_republish_and_budget() == 0);
    CHECK(test_capacity_and_payloadless_event() == 0);
    CHECK(test_invalid_arguments() == 0);
    return 0;
}
