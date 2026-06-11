/**
 * @file utility_event_queue.c
 * @brief 固定長FIFO Event Queueの実装。
 */
#include "utility_event_queue.h"

/**
 * Queue contextの内部不変条件を検査する。
 *
 * @param queue 検査するQueue context。
 * @return 利用可能なら真、それ以外は偽。
 */
static int queue_is_valid(const ut_event_queue_t *queue)
{
    /*
     * 公開structを採用しているため、NULLだけでなくindexと件数の不変条件も
     * API境界で検査する。壊れたcontextを使った配列範囲外accessを防ぐ。
     */
    return (queue != NULL) && (queue->storage != NULL) &&
        (queue->capacity > 0u) && (queue->head < queue->capacity) &&
        (queue->tail < queue->capacity) && (queue->count <= queue->capacity);
}

ut_event_result_t ut_event_queue_init(
    ut_event_queue_t *queue,
    ut_event_t *storage,
    size_t capacity)
{
    if ((queue == NULL) || (storage == NULL) || (capacity == 0u)) {
        return UT_EVENT_INVALID_ARGUMENT;
    }

    queue->storage = storage;
    queue->capacity = capacity;
    queue->head = 0u;
    queue->tail = 0u;
    queue->count = 0u;
    return UT_EVENT_OK;
}

ut_event_result_t ut_event_queue_push(
    ut_event_queue_t *queue,
    const ut_event_t *event)
{
    if (!queue_is_valid(queue) || (event == NULL)) {
        return UT_EVENT_INVALID_ARGUMENT;
    }
    if (queue->count == queue->capacity) {
        return UT_EVENT_FULL;
    }

    /*
     * Event記述子だけを浅くcopyする。payloadが指すデータ本体の所有権と寿命は
     * Queueへ移動しない。
     */
    queue->storage[queue->tail] = *event;
    queue->tail = (queue->tail + 1u) % queue->capacity;
    queue->count++;
    return UT_EVENT_OK;
}

ut_event_result_t ut_event_queue_pop(
    ut_event_queue_t *queue,
    ut_event_t *out_event)
{
    if (!queue_is_valid(queue) || (out_event == NULL)) {
        return UT_EVENT_INVALID_ARGUMENT;
    }
    if (queue->count == 0u) {
        return UT_EVENT_EMPTY;
    }

    *out_event = queue->storage[queue->head];
    queue->head = (queue->head + 1u) % queue->capacity;
    queue->count--;
    return UT_EVENT_OK;
}

ut_event_result_t ut_event_queue_peek(
    const ut_event_queue_t *queue,
    ut_event_t *out_event)
{
    if (!queue_is_valid(queue) || (out_event == NULL)) {
        return UT_EVENT_INVALID_ARGUMENT;
    }
    if (queue->count == 0u) {
        return UT_EVENT_EMPTY;
    }

    *out_event = queue->storage[queue->head];
    return UT_EVENT_OK;
}

ut_event_result_t ut_event_queue_clear(ut_event_queue_t *queue)
{
    if (!queue_is_valid(queue)) {
        return UT_EVENT_INVALID_ARGUMENT;
    }

    queue->head = 0u;
    queue->tail = 0u;
    queue->count = 0u;
    return UT_EVENT_OK;
}

size_t ut_event_queue_count(const ut_event_queue_t *queue)
{
    return queue_is_valid(queue) ? queue->count : 0u;
}

size_t ut_event_queue_capacity(const ut_event_queue_t *queue)
{
    return queue_is_valid(queue) ? queue->capacity : 0u;
}
