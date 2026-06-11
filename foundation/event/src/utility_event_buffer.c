/**
 * @file utility_event_buffer.c
 * @brief Buffer Pool handle所有Envelopeとmove Queueの実装。
 */
#include "utility_event_buffer.h"

#include <string.h>

/**
 * Buffer Pool操作tableが有効か確認する。
 *
 * @param pool 検査するPool。
 * @return 有効なら真、それ以外は偽。
 */
static int pool_is_valid(const ut_event_buffer_pool_t *pool)
{
    return (pool != NULL) &&
        (pool->alloc != NULL) &&
        (pool->free != NULL) &&
        (pool->write != NULL) &&
        (pool->read != NULL);
}

/**
 * Envelopeがhandleを所有しているか確認する。
 *
 * @param message 検査するEnvelope。
 * @return 所有中なら真、それ以外は偽。
 */
static int message_is_owned(const ut_event_buffer_message_t *message)
{
    return (message != NULL) &&
        (message->owns_handle != 0u) &&
        pool_is_valid(message->buffer_ref.pool) &&
        (message->event.payload == &message->buffer_ref) &&
        (message->event.payload_size == sizeof(message->buffer_ref));
}

/**
 * Queueの内部不変条件を検査する。
 *
 * @param queue 検査するQueue。
 * @return 有効なら真、それ以外は偽。
 */
static int queue_is_valid(const ut_event_buffer_queue_t *queue)
{
    return (queue != NULL) &&
        (queue->storage != NULL) &&
        (queue->capacity > 0u) &&
        (queue->head < queue->capacity) &&
        (queue->tail < queue->capacity) &&
        (queue->count <= queue->capacity);
}

/**
 * Envelopeをdestinationへmoveしてsourceを空にする。
 *
 * @param destination move先。
 * @param source move元。
 */
static void move_message(
    ut_event_buffer_message_t *destination,
    ut_event_buffer_message_t *source)
{
    *destination = *source;
    destination->event.payload = &destination->buffer_ref;
    (void)memset(source, 0, sizeof(*source));
}

ut_event_result_t ut_event_buffer_message_create_copy(
    ut_event_buffer_message_t *message,
    const ut_event_buffer_pool_t *pool,
    uint32_t event_id,
    uint32_t source_id,
    const uint8_t *source,
    size_t length)
{
    ut_event_buffer_handle_t handle;
    ut_event_result_t result;

    if ((message == NULL) || (message->owns_handle != 0u) ||
        !pool_is_valid(pool) || (source == NULL) || (length == 0u)) {
        return UT_EVENT_INVALID_ARGUMENT;
    }

    result = pool->alloc(pool->context, length, &handle);
    if (result != UT_EVENT_OK) {
        return result;
    }
    result = pool->write(pool->context, handle, 0u, source, length);
    if (result != UT_EVENT_OK) {
        (void)pool->free(pool->context, handle);
        return result;
    }

    message->buffer_ref.pool = pool;
    message->buffer_ref.handle = handle;
    message->buffer_ref.length = length;
    message->event.id = event_id;
    message->event.source = source_id;
    message->event.payload = &message->buffer_ref;
    message->event.payload_size = sizeof(message->buffer_ref);
    message->owns_handle = 1u;
    return UT_EVENT_OK;
}

ut_event_result_t ut_event_buffer_message_release(
    ut_event_buffer_message_t *message)
{
    ut_event_result_t result;

    if (!message_is_owned(message)) {
        return UT_EVENT_INVALID_ARGUMENT;
    }

    result = message->buffer_ref.pool->free(
        message->buffer_ref.pool->context,
        message->buffer_ref.handle);
    if (result == UT_EVENT_OK) {
        (void)memset(message, 0, sizeof(*message));
    }
    return result;
}

const ut_event_t *ut_event_buffer_message_event(
    const ut_event_buffer_message_t *message)
{
    return message_is_owned(message) ? &message->event : NULL;
}

ut_event_result_t ut_event_buffer_message_read(
    const ut_event_buffer_message_t *message,
    size_t offset,
    uint8_t *destination,
    size_t length)
{
    if (!message_is_owned(message) ||
        ((destination == NULL) && (length != 0u)) ||
        (offset > message->buffer_ref.length) ||
        (length > (message->buffer_ref.length - offset))) {
        return UT_EVENT_INVALID_ARGUMENT;
    }
    return message->buffer_ref.pool->read(
        message->buffer_ref.pool->context,
        message->buffer_ref.handle,
        offset,
        destination,
        length);
}

ut_event_result_t ut_event_buffer_queue_init(
    ut_event_buffer_queue_t *queue,
    ut_event_buffer_message_t *storage,
    size_t capacity)
{
    if ((queue == NULL) || (storage == NULL) || (capacity == 0u) ||
        (capacity > (SIZE_MAX / sizeof(storage[0])))) {
        return UT_EVENT_INVALID_ARGUMENT;
    }

    (void)memset(storage, 0, capacity * sizeof(storage[0]));
    queue->storage = storage;
    queue->capacity = capacity;
    queue->head = 0u;
    queue->tail = 0u;
    queue->count = 0u;
    return UT_EVENT_OK;
}

ut_event_result_t ut_event_buffer_queue_push_move(
    ut_event_buffer_queue_t *queue,
    ut_event_buffer_message_t *source)
{
    if (!queue_is_valid(queue) || !message_is_owned(source)) {
        return UT_EVENT_INVALID_ARGUMENT;
    }
    if (queue->count == queue->capacity) {
        return UT_EVENT_FULL;
    }

    move_message(&queue->storage[queue->tail], source);
    queue->tail = (queue->tail + 1u) % queue->capacity;
    queue->count++;
    return UT_EVENT_OK;
}

ut_event_result_t ut_event_buffer_queue_pop_move(
    ut_event_buffer_queue_t *queue,
    ut_event_buffer_message_t *destination)
{
    if (!queue_is_valid(queue) || (destination == NULL) ||
        (destination->owns_handle != 0u)) {
        return UT_EVENT_INVALID_ARGUMENT;
    }
    if (queue->count == 0u) {
        return UT_EVENT_EMPTY;
    }

    move_message(destination, &queue->storage[queue->head]);
    queue->head = (queue->head + 1u) % queue->capacity;
    queue->count--;
    return UT_EVENT_OK;
}

size_t ut_event_buffer_queue_count(
    const ut_event_buffer_queue_t *queue)
{
    return queue_is_valid(queue) ? queue->count : 0u;
}

ut_event_result_t ut_event_buffer_queue_release_all(
    ut_event_buffer_queue_t *queue)
{
    while (queue_is_valid(queue) && (queue->count > 0u)) {
        ut_event_buffer_message_t *message = &queue->storage[queue->head];
        const ut_event_result_t result =
            ut_event_buffer_message_release(message);

        if (result != UT_EVENT_OK) {
            return result;
        }
        queue->head = (queue->head + 1u) % queue->capacity;
        queue->count--;
    }
    return queue_is_valid(queue) ? UT_EVENT_OK : UT_EVENT_INVALID_ARGUMENT;
}
