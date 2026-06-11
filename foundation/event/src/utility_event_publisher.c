/**
 * @file utility_event_publisher.c
 * @brief payload値copy Event Publisher実装。
 */
#include "utility_event_publisher.h"

#include <string.h>

/**
 * Publisher contextが利用可能か確認する。
 *
 * @param publisher 確認対象。
 * @return 有効な場合true。
 */
static bool publisher_is_valid(const ut_event_publisher_t *publisher)
{
    return publisher != NULL
        && publisher->payload_storage != NULL
        && publisher->occupied_storage != NULL
        && publisher->capacity > 0U
        && publisher->payload_capacity > 0U
        && ((publisher->lock == NULL && publisher->unlock == NULL)
            || (publisher->lock != NULL && publisher->unlock != NULL));
}

/**
 * 設定済みの場合だけ排他領域へ入る。
 *
 * @param publisher 初期化済みPublisher。
 */
static void publisher_lock(ut_event_publisher_t *publisher)
{
    if (publisher->lock != NULL) {
        publisher->lock(publisher->lock_context);
    }
}

/**
 * 設定済みの場合だけ排他領域から出る。
 *
 * @param publisher 初期化済みPublisher。
 */
static void publisher_unlock(ut_event_publisher_t *publisher)
{
    if (publisher->unlock != NULL) {
        publisher->unlock(publisher->lock_context);
    }
}

/**
 * slot indexに対応するpayload先頭を返す。
 *
 * @param publisher 初期化済みPublisher。
 * @param index capacity未満のslot index。
 * @return payload slot先頭。
 */
static uint8_t *payload_at(
    const ut_event_publisher_t *publisher,
    size_t index)
{
    return &publisher->payload_storage[index * publisher->payload_capacity];
}

/**
 * 未使用payload slotを探索する。
 *
 * @param publisher 初期化済みPublisher。
 * @return 未使用slot index。存在しない場合capacity。
 */
static size_t find_free_slot(const ut_event_publisher_t *publisher)
{
    size_t index = 0U;

    while (index < publisher->capacity
        && publisher->occupied_storage[index] != 0U) {
        index++;
    }
    return index;
}

/**
 * payload pointerに対応するslotを探索する。
 *
 * @param publisher 初期化済みPublisher。
 * @param payload Queueから取得したpayload pointer。
 * @return 対応slot index。存在しない場合capacity。
 */
static size_t find_payload_slot(
    const ut_event_publisher_t *publisher,
    const void *payload)
{
    size_t index = 0U;

    while (index < publisher->capacity
        && payload != payload_at(publisher, index)) {
        index++;
    }
    return index;
}

ut_event_result_t ut_event_publisher_init(
    ut_event_publisher_t *publisher,
    ut_event_t *event_storage,
    uint8_t *payload_storage,
    uint8_t *occupied_storage,
    size_t capacity,
    size_t payload_capacity,
    ut_event_subscription_t *subscription_storage,
    size_t subscription_capacity,
    ut_event_publisher_lock_fn lock,
    ut_event_publisher_unlock_fn unlock,
    void *lock_context)
{
    if (publisher == NULL || event_storage == NULL
        || payload_storage == NULL || occupied_storage == NULL
        || capacity == 0U || payload_capacity == 0U
        || capacity > SIZE_MAX / payload_capacity
        || subscription_storage == NULL || subscription_capacity == 0U
        || ((lock == NULL) != (unlock == NULL))) {
        return UT_EVENT_INVALID_ARGUMENT;
    }

    (void)memset(publisher, 0, sizeof(*publisher));
    (void)memset(occupied_storage, 0, capacity);
    publisher->payload_storage = payload_storage;
    publisher->occupied_storage = occupied_storage;
    publisher->capacity = capacity;
    publisher->payload_capacity = payload_capacity;
    publisher->lock = lock;
    publisher->unlock = unlock;
    publisher->lock_context = lock_context;
    if (ut_event_queue_init(
            &publisher->queue,
            event_storage,
            capacity) != UT_EVENT_OK
        || ut_event_dispatcher_init(
            &publisher->dispatcher,
            subscription_storage,
            subscription_capacity) != UT_EVENT_OK) {
        (void)memset(publisher, 0, sizeof(*publisher));
        return UT_EVENT_INVALID_ARGUMENT;
    }
    return UT_EVENT_OK;
}

ut_event_result_t ut_event_publisher_subscribe(
    ut_event_publisher_t *publisher,
    uint32_t event_id,
    ut_event_handler_t handler,
    void *user_context)
{
    if (!publisher_is_valid(publisher)) {
        return UT_EVENT_INVALID_ARGUMENT;
    }
    return ut_event_subscribe(
        &publisher->dispatcher,
        event_id,
        handler,
        user_context);
}

ut_event_result_t ut_event_publisher_publish_copy(
    ut_event_publisher_t *publisher,
    uint32_t event_id,
    uint32_t source,
    const void *payload,
    size_t payload_size)
{
    ut_event_result_t result;
    size_t slot;

    if (!publisher_is_valid(publisher)
        || (payload_size == 0U && payload != NULL)
        || (payload_size > 0U && payload == NULL)
        || payload_size > publisher->payload_capacity) {
        return UT_EVENT_INVALID_ARGUMENT;
    }

    publisher_lock(publisher);
    if (payload_size == 0U) {
        const ut_event_t event = {
            .id = event_id,
            .source = source,
            .payload = NULL,
            .payload_size = 0U,
        };

        result = ut_event_queue_push(&publisher->queue, &event);
    } else {
        slot = find_free_slot(publisher);
        if (slot >= publisher->capacity) {
            result = UT_EVENT_FULL;
        } else {
            const ut_event_t event = {
                .id = event_id,
                .source = source,
                .payload = payload_at(publisher, slot),
                .payload_size = payload_size,
            };

            (void)memcpy(
                (void *)event.payload,
                payload,
                payload_size);
            publisher->occupied_storage[slot] = 1U;
            result = ut_event_queue_push(&publisher->queue, &event);
            if (result != UT_EVENT_OK) {
                publisher->occupied_storage[slot] = 0U;
            }
        }
    }
    publisher_unlock(publisher);
    return result;
}

/**
 * Queueから1 Eventを取得し、payload slotを検証する。
 *
 * @param publisher 初期化済みPublisher。
 * @param event 取得Eventの格納先。
 * @param out_slot payload slot indexの格納先。payloadなしではcapacity。
 * @return Queue popまたはpayload検証result。
 */
static ut_event_result_t publisher_pop_event(
    ut_event_publisher_t *publisher,
    ut_event_t *event,
    size_t *out_slot)
{
    ut_event_result_t result;

    *out_slot = publisher->capacity;
    publisher_lock(publisher);
    result = ut_event_queue_pop(&publisher->queue, event);
    if (result == UT_EVENT_OK && event->payload != NULL) {
        *out_slot = find_payload_slot(publisher, event->payload);
        if (*out_slot >= publisher->capacity
            || publisher->occupied_storage[*out_slot] == 0U) {
            result = UT_EVENT_INVALID_ARGUMENT;
        }
    }
    publisher_unlock(publisher);
    return result;
}

/**
 * 配送済みpayload slotを未使用へ戻す。
 *
 * @param publisher 初期化済みPublisher。
 * @param slot payload slot index。capacityの場合は何もしない。
 */
static void publisher_release_slot(
    ut_event_publisher_t *publisher,
    size_t slot)
{
    publisher_lock(publisher);
    if (slot < publisher->capacity) {
        publisher->occupied_storage[slot] = 0U;
    }
    publisher_unlock(publisher);
}

/**
 * 1 Eventを配送し、購読先なしを正常扱いへ変換する。
 *
 * @param publisher 初期化済みPublisher。
 * @param event 配送するEvent。
 * @return 配送成功または購読先なしではUT_EVENT_OK。それ以外はDispatcher error。
 */
static ut_event_result_t publisher_dispatch_event(
    ut_event_publisher_t *publisher,
    const ut_event_t *event)
{
    const ut_event_result_t result =
        ut_event_dispatch(&publisher->dispatcher, event, NULL);

    return result == UT_EVENT_NOT_FOUND ? UT_EVENT_OK : result;
}

ut_event_result_t ut_event_publisher_dispatch(
    ut_event_publisher_t *publisher,
    size_t budget,
    size_t *out_dispatched)
{
    ut_event_result_t result = UT_EVENT_OK;
    size_t dispatched = 0U;

    if (!publisher_is_valid(publisher) || budget == 0U) {
        return UT_EVENT_INVALID_ARGUMENT;
    }
    if (publisher->dispatching) {
        return UT_EVENT_BUSY;
    }

    publisher->dispatching = true;
    while (dispatched < budget && result == UT_EVENT_OK) {
        ut_event_t event;
        size_t slot = publisher->capacity;
        const ut_event_result_t pop_result =
            publisher_pop_event(publisher, &event, &slot);

        if (pop_result == UT_EVENT_EMPTY) {
            break;
        }
        if (pop_result != UT_EVENT_OK) {
            result = pop_result;
        } else {
            result = publisher_dispatch_event(publisher, &event);
            publisher_release_slot(publisher, slot);
            dispatched++;
        }
    }
    publisher->dispatching = false;
    if (out_dispatched != NULL) {
        *out_dispatched = dispatched;
    }
    return result;
}

size_t ut_event_publisher_count(
    const ut_event_publisher_t *publisher)
{
    size_t count = 0U;
    ut_event_publisher_t *mutable_publisher =
        (ut_event_publisher_t *)publisher;

    if (publisher_is_valid(publisher)) {
        publisher_lock(mutable_publisher);
        count = ut_event_queue_count(&publisher->queue);
        publisher_unlock(mutable_publisher);
    }
    return count;
}
