/**
 * @file utility_event_dispatcher.c
 * @brief Event Dispatcherの実装。
 */
#include "utility_event_dispatcher.h"

#include <string.h>

/**
 * Dispatcher contextの内部不変条件を検査する。
 *
 * @param dispatcher 検査するDispatcher context。
 * @return 利用可能なら真、それ以外は偽。
 */
static int dispatcher_is_valid(const ut_event_dispatcher_t *dispatcher)
{
    /* subscription配列を走査する前に最低限のcontext不変条件を確認する。 */
    return (dispatcher != NULL) && (dispatcher->subscriptions != NULL) &&
        (dispatcher->capacity > 0u) &&
        (dispatcher->count <= dispatcher->capacity);
}

ut_event_result_t ut_event_dispatcher_init(
    ut_event_dispatcher_t *dispatcher,
    ut_event_subscription_t *storage,
    size_t capacity)
{
    if ((dispatcher == NULL) || (storage == NULL) || (capacity == 0u) ||
        (capacity > (SIZE_MAX / sizeof(storage[0])))) {
        return UT_EVENT_INVALID_ARGUMENT;
    }

    (void)memset(storage, 0, capacity * sizeof(storage[0]));
    dispatcher->subscriptions = storage;
    dispatcher->capacity = capacity;
    dispatcher->count = 0u;
    dispatcher->dispatching = 0u;
    return UT_EVENT_OK;
}

ut_event_result_t ut_event_subscribe(
    ut_event_dispatcher_t *dispatcher,
    uint32_t event_id,
    ut_event_handler_t handler,
    void *user_context)
{
    size_t index;
    size_t free_index;

    if (!dispatcher_is_valid(dispatcher) || (handler == NULL)) {
        return UT_EVENT_INVALID_ARGUMENT;
    }
    if (dispatcher->dispatching != 0u) {
        return UT_EVENT_BUSY;
    }

    /*
     * 1回の走査で重複確認と最初の空きslot探索を行う。削除後の空きslotを
     * 再利用するため、登録配列を詰め直す必要はない。
     */
    free_index = dispatcher->capacity;
    for (index = 0u; index < dispatcher->capacity; index++) {
        ut_event_subscription_t *subscription =
            &dispatcher->subscriptions[index];

        if (subscription->occupied == 0u) {
            if (free_index == dispatcher->capacity) {
                free_index = index;
            }
        } else if ((subscription->event_id == event_id) &&
            (subscription->handler == handler) &&
            (subscription->user_context == user_context)) {
            return UT_EVENT_ALREADY_EXISTS;
        }
    }

    if (free_index == dispatcher->capacity) {
        return UT_EVENT_FULL;
    }

    dispatcher->subscriptions[free_index].event_id = event_id;
    dispatcher->subscriptions[free_index].handler = handler;
    dispatcher->subscriptions[free_index].user_context = user_context;
    dispatcher->subscriptions[free_index].occupied = 1u;
    dispatcher->count++;
    return UT_EVENT_OK;
}

ut_event_result_t ut_event_unsubscribe(
    ut_event_dispatcher_t *dispatcher,
    uint32_t event_id,
    ut_event_handler_t handler,
    void *user_context)
{
    size_t index;

    if (!dispatcher_is_valid(dispatcher) || (handler == NULL)) {
        return UT_EVENT_INVALID_ARGUMENT;
    }
    if (dispatcher->dispatching != 0u) {
        return UT_EVENT_BUSY;
    }

    for (index = 0u; index < dispatcher->capacity; index++) {
        ut_event_subscription_t *subscription =
            &dispatcher->subscriptions[index];

        if ((subscription->occupied != 0u) &&
            (subscription->event_id == event_id) &&
            (subscription->handler == handler) &&
            (subscription->user_context == user_context)) {
            (void)memset(subscription, 0, sizeof(*subscription));
            dispatcher->count--;
            return UT_EVENT_OK;
        }
    }

    return UT_EVENT_NOT_FOUND;
}

ut_event_result_t ut_event_dispatch(
    ut_event_dispatcher_t *dispatcher,
    const ut_event_t *event,
    size_t *out_handler_count)
{
    size_t index;
    size_t handler_count;

    if (!dispatcher_is_valid(dispatcher) || (event == NULL)) {
        return UT_EVENT_INVALID_ARGUMENT;
    }
    if (dispatcher->dispatching != 0u) {
        return UT_EVENT_BUSY;
    }

    /*
     * handlerが登録配列を変更すると走査中の意味が変わるため、dispatch中は
     * 登録変更と再帰dispatchを拒否する。これはthread同期用lockではない。
     */
    dispatcher->dispatching = 1u;
    handler_count = 0u;
    for (index = 0u; index < dispatcher->capacity; index++) {
        const ut_event_subscription_t *subscription =
            &dispatcher->subscriptions[index];

        if ((subscription->occupied != 0u) &&
            ((subscription->event_id == event->id) ||
                (subscription->event_id == UT_EVENT_ID_ANY))) {
            subscription->handler(event, subscription->user_context);
            handler_count++;
        }
    }
    dispatcher->dispatching = 0u;

    if (out_handler_count != NULL) {
        *out_handler_count = handler_count;
    }
    return (handler_count == 0u) ? UT_EVENT_NOT_FOUND : UT_EVENT_OK;
}

size_t ut_event_subscription_count(
    const ut_event_dispatcher_t *dispatcher)
{
    return dispatcher_is_valid(dispatcher) ? dispatcher->count : 0u;
}
