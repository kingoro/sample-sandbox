/**
 * @file memory_buffer_event_adapter.c
 * @brief Memory Buffer Event adapterの実装。
 */
#include "memory_buffer_event_adapter.h"

/**
 * Memory Buffer resultをEvent Utility resultへ対応付ける。
 *
 * @param result Memory Buffer result。
 * @return Event Utility result。
 */
static ut_event_result_t map_result(mb_result_t result)
{
    switch (result) {
    case MB_OK:
        return UT_EVENT_OK;
    case MB_OUT_OF_MEMORY:
        return UT_EVENT_FULL;
    case MB_BUSY:
        return UT_EVENT_BUSY;
    case MB_INVALID_HANDLE:
        return UT_EVENT_NOT_FOUND;
    default:
        return UT_EVENT_INVALID_ARGUMENT;
    }
}

/**
 * Memory Buffer allocation callback。
 *
 * @param pool_context mb_context_tへのpointer。
 * @param capacity 要求容量。
 * @param out_handle handle格納先。
 * @return Event Utility result。
 */
static ut_event_result_t adapter_alloc(
    void *pool_context,
    size_t capacity,
    ut_event_buffer_handle_t *out_handle)
{
    return map_result(mb_alloc(
        (mb_context_t *)pool_context,
        capacity,
        (mb_handle_t *)out_handle));
}

/**
 * Memory Buffer free callback。
 *
 * @param pool_context mb_context_tへのpointer。
 * @param handle 解放handle。
 * @return Event Utility result。
 */
static ut_event_result_t adapter_free(
    void *pool_context,
    ut_event_buffer_handle_t handle)
{
    return map_result(mb_free(
        (mb_context_t *)pool_context,
        (mb_handle_t)handle));
}

/**
 * Memory Buffer write callback。
 *
 * @param pool_context mb_context_tへのpointer。
 * @param handle 書込みhandle。
 * @param offset 書込みoffset。
 * @param source copy元。
 * @param length copy長。
 * @return Event Utility result。
 */
static ut_event_result_t adapter_write(
    void *pool_context,
    ut_event_buffer_handle_t handle,
    size_t offset,
    const uint8_t *source,
    size_t length)
{
    return map_result(mb_write(
        (mb_context_t *)pool_context,
        (mb_handle_t)handle,
        offset,
        source,
        length));
}

/**
 * Memory Buffer read callback。
 *
 * @param pool_context mb_context_tへのpointer。
 * @param handle 読取りhandle。
 * @param offset 読取りoffset。
 * @param destination copy先。
 * @param length copy長。
 * @return Event Utility result。
 */
static ut_event_result_t adapter_read(
    void *pool_context,
    ut_event_buffer_handle_t handle,
    size_t offset,
    uint8_t *destination,
    size_t length)
{
    return map_result(mb_read(
        (mb_context_t *)pool_context,
        (mb_handle_t)handle,
        offset,
        destination,
        length));
}

ut_event_buffer_pool_t mb_event_buffer_pool(mb_context_t *context)
{
    const ut_event_buffer_pool_t pool = {
        .context = context,
        .alloc = adapter_alloc,
        .free = adapter_free,
        .write = adapter_write,
        .read = adapter_read
    };

    return pool;
}
