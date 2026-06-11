/**
 * @file test_utility_event_dispatcher.c
 * @brief Event Dispatcherの配送、登録、再入、異常系単体テスト。
 */
#include "utility_event_dispatcher.h"

#include "test_cases.h"
#include "test_support.h"

/** Test handlerの呼出結果と再入結果を保持するcontext。 */
typedef struct handler_state {
    /** handlerが呼び出された回数。 */
    size_t call_count;
    /** handlerが最後に受信したEvent ID。 */
    uint32_t last_id;
    /** 再帰dispatch検証で使用するDispatcher。 */
    ut_event_dispatcher_t *dispatcher;
    /** handler内から実行した再帰dispatchの結果。 */
    ut_event_result_t nested_result;
} handler_state_t;

/**
 * Event IDと呼出回数をhandler_state_tへ記録する。
 *
 * @param event 受信したEvent。
 * @param user_context handler_state_tへのpointer。
 */
static void record_event(const ut_event_t *event, void *user_context)
{
    handler_state_t *state = (handler_state_t *)user_context;

    state->call_count++;
    state->last_id = event->id;
}

/**
 * handler内から同じDispatcherへ再帰dispatchを試みる。
 *
 * @param event 再配送を試みるEvent。
 * @param user_context handler_state_tへのpointer。
 */
static void try_nested_dispatch(
    const ut_event_t *event,
    void *user_context)
{
    handler_state_t *state = (handler_state_t *)user_context;

    state->nested_result =
        ut_event_dispatch(state->dispatcher, event, NULL);
}

/**
 * Event ID一致、全Event購読、重複登録、登録解除を検証する。
 *
 * @return 成功時は0、失敗時は1。
 */
static int test_routes_and_unsubscribes(void)
{
    ut_event_subscription_t storage[3];
    ut_event_dispatcher_t dispatcher;
    handler_state_t exact = {0};
    handler_state_t any = {0};
    handler_state_t other = {0};
    const ut_event_t event = {7u, 1u, NULL, 0u};
    size_t handler_count = 0u;

    CHECK(ut_event_dispatcher_init(&dispatcher, storage, 3u) ==
        UT_EVENT_OK);
    CHECK(ut_event_subscribe(&dispatcher, 7u, record_event, &exact) ==
        UT_EVENT_OK);
    CHECK(ut_event_subscribe(
        &dispatcher, UT_EVENT_ID_ANY, record_event, &any) == UT_EVENT_OK);
    CHECK(ut_event_subscribe(&dispatcher, 9u, record_event, &other) ==
        UT_EVENT_OK);
    CHECK(ut_event_subscription_count(&dispatcher) == 3u);
    CHECK(ut_event_subscribe(&dispatcher, 7u, record_event, &exact) ==
        UT_EVENT_ALREADY_EXISTS);

    CHECK(ut_event_dispatch(&dispatcher, &event, &handler_count) ==
        UT_EVENT_OK);
    CHECK(handler_count == 2u);
    CHECK(exact.call_count == 1u);
    CHECK(exact.last_id == 7u);
    CHECK(any.call_count == 1u);
    CHECK(other.call_count == 0u);

    CHECK(ut_event_unsubscribe(&dispatcher, 7u, record_event, &exact) ==
        UT_EVENT_OK);
    CHECK(ut_event_unsubscribe(&dispatcher, 7u, record_event, &exact) ==
        UT_EVENT_NOT_FOUND);
    CHECK(ut_event_subscription_count(&dispatcher) == 2u);
    return 0;
}

/**
 * subscription容量不足と再帰dispatch拒否を検証する。
 *
 * @return 成功時は0、失敗時は1。
 */
static int test_capacity_and_reentrancy(void)
{
    ut_event_subscription_t storage[1];
    ut_event_dispatcher_t dispatcher;
    handler_state_t state = {0};
    handler_state_t extra = {0};
    const ut_event_t event = {5u, 0u, NULL, 0u};

    state.dispatcher = &dispatcher;
    CHECK(ut_event_dispatcher_init(&dispatcher, storage, 1u) ==
        UT_EVENT_OK);
    CHECK(ut_event_dispatcher_init(
        &dispatcher, storage, (SIZE_MAX / sizeof(storage[0])) + 1u) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_dispatcher_init(&dispatcher, storage, 1u) ==
        UT_EVENT_OK);
    CHECK(ut_event_subscribe(
        &dispatcher, 5u, try_nested_dispatch, &state) == UT_EVENT_OK);
    CHECK(ut_event_subscribe(&dispatcher, 5u, record_event, &extra) ==
        UT_EVENT_FULL);
    CHECK(ut_event_dispatch(&dispatcher, &event, NULL) == UT_EVENT_OK);
    CHECK(state.nested_result == UT_EVENT_BUSY);

    CHECK(ut_event_unsubscribe(
        &dispatcher, 5u, try_nested_dispatch, &state) == UT_EVENT_OK);
    CHECK(ut_event_dispatch(&dispatcher, &event, NULL) ==
        UT_EVENT_NOT_FOUND);
    return 0;
}

/**
 * NULL、未初期化context、dispatch中の登録変更拒否を検証する。
 *
 * @return 成功時は0、失敗時は1。
 */
static int test_invalid_arguments_and_busy_state(void)
{
    ut_event_subscription_t storage[1];
    ut_event_dispatcher_t dispatcher = {0};
    handler_state_t state = {0};
    const ut_event_t event = {1u, 0u, NULL, 0u};

    CHECK(ut_event_dispatcher_init(NULL, storage, 1u) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_dispatcher_init(&dispatcher, NULL, 1u) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_dispatcher_init(&dispatcher, storage, 0u) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_subscribe(
        &dispatcher, 1u, record_event, &state) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_unsubscribe(
        &dispatcher, 1u, record_event, &state) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_dispatch(&dispatcher, &event, NULL) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_subscription_count(&dispatcher) == 0u);

    CHECK(ut_event_dispatcher_init(&dispatcher, storage, 1u) ==
        UT_EVENT_OK);
    CHECK(ut_event_subscribe(&dispatcher, 1u, NULL, &state) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_unsubscribe(&dispatcher, 1u, NULL, &state) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_dispatch(&dispatcher, NULL, NULL) ==
        UT_EVENT_INVALID_ARGUMENT);

    dispatcher.dispatching = 1u;
    CHECK(ut_event_subscribe(
        &dispatcher, 1u, record_event, &state) == UT_EVENT_BUSY);
    CHECK(ut_event_unsubscribe(
        &dispatcher, 1u, record_event, &state) == UT_EVENT_BUSY);
    CHECK(ut_event_dispatch(&dispatcher, &event, NULL) ==
        UT_EVENT_BUSY);
    dispatcher.dispatching = 0u;

    dispatcher.count = dispatcher.capacity + 1u;
    CHECK(ut_event_subscription_count(&dispatcher) == 0u);
    return 0;
}

int run_utility_event_dispatcher_tests(void)
{
    CHECK(test_routes_and_unsubscribes() == 0);
    CHECK(test_capacity_and_reentrancy() == 0);
    CHECK(test_invalid_arguments_and_busy_state() == 0);
    return 0;
}
