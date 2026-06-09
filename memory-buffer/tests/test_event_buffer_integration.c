/**
 * @file test_event_buffer_integration.c
 * @brief Memory BufferとEvent Buffer所有Envelopeの結合テスト。
 */
#include "memory_buffer.h"
#include "memory_buffer_event_adapter.h"
#include "utility_event_buffer.h"
#include "utility_event_dispatcher.h"

#include <stdio.h>
#include <string.h>

/** 結合テスト用arena容量。 */
#define INTEGRATION_ARENA_CAPACITY 256u
/** 結合テスト用Envelope Queue容量。 */
#define INTEGRATION_BUFFER_QUEUE_CAPACITY 2u
/** 結合テスト用subscription容量。 */
#define INTEGRATION_SUBSCRIPTION_CAPACITY 1u
/** 結合テスト用Event ID。 */
#define INTEGRATION_BUFFER_EVENT_ID 100u

/**
 * 条件が偽なら失敗場所を表示してmainを失敗させる。
 *
 * @param condition 検証する真偽式。
 */
#define INTEGRATION_CHECK(condition)                                          \
    do {                                                                      \
        if (!(condition)) {                                                   \
            (void)fprintf(                                                    \
                stderr, "%s:%d: CHECK failed: %s\n",                         \
                __FILE__, __LINE__, #condition);                              \
            return 1;                                                         \
        }                                                                     \
    } while (0)

/** Dispatcher handlerの観測状態。 */
typedef struct integration_handler_state {
    /** handler呼出回数。 */
    size_t calls;
    /** handlerが読み取ったpayload。 */
    uint8_t bytes[8];
    /** 読み取ったbyte数。 */
    size_t length;
    /** Buffer読取り結果。 */
    ut_event_result_t read_result;
} integration_handler_state_t;

/**
 * Event payloadのBuffer Pool参照からbyte列を読み取る。
 *
 * @param event 配送されたEvent。
 * @param user_context integration_handler_state_tへのpointer。
 */
static void consume_buffer_event(
    const ut_event_t *event,
    void *user_context)
{
    integration_handler_state_t *state =
        (integration_handler_state_t *)user_context;
    const ut_event_buffer_ref_t *buffer_ref =
        (const ut_event_buffer_ref_t *)event->payload;

    state->calls++;
    state->length = buffer_ref->length;
    state->read_result = buffer_ref->pool->read(
        buffer_ref->pool->context,
        buffer_ref->handle,
        0u,
        state->bytes,
        buffer_ref->length);
}

/**
 * Memory BufferとEvent Buffer Envelopeの所有権lifecycleを検証する。
 *
 * @return 成功時は0、失敗時は1。
 */
int main(void)
{
    mb_context_t memory_context;
    uint8_t arena[INTEGRATION_ARENA_CAPACITY];
    ut_event_buffer_pool_t pool;
    ut_event_buffer_message_t
        queue_storage[INTEGRATION_BUFFER_QUEUE_CAPACITY];
    ut_event_buffer_queue_t queue;
    ut_event_buffer_message_t producer = {0};
    ut_event_buffer_message_t consumer = {0};
    ut_event_subscription_t
        subscriptions[INTEGRATION_SUBSCRIPTION_CAPACITY];
    ut_event_dispatcher_t dispatcher;
    integration_handler_state_t handler_state = {0};
    const uint8_t input[] = {0x10u, 0x20u, 0x30u, 0x40u};
    ut_event_buffer_handle_t released_handle;
    size_t handler_count = 0u;

    INTEGRATION_CHECK(mb_init(
        &memory_context,
        sizeof(memory_context),
        arena,
        sizeof(arena)) == MB_OK);
    pool = mb_event_buffer_pool(&memory_context);
    INTEGRATION_CHECK(ut_event_buffer_queue_init(
        &queue,
        queue_storage,
        INTEGRATION_BUFFER_QUEUE_CAPACITY) == UT_EVENT_OK);
    INTEGRATION_CHECK(ut_event_dispatcher_init(
        &dispatcher,
        subscriptions,
        INTEGRATION_SUBSCRIPTION_CAPACITY) == UT_EVENT_OK);
    INTEGRATION_CHECK(ut_event_subscribe(
        &dispatcher,
        INTEGRATION_BUFFER_EVENT_ID,
        consume_buffer_event,
        &handler_state) == UT_EVENT_OK);

    INTEGRATION_CHECK(ut_event_buffer_message_create_copy(
        &producer,
        &pool,
        INTEGRATION_BUFFER_EVENT_ID,
        9u,
        input,
        sizeof(input)) == UT_EVENT_OK);
    released_handle = producer.buffer_ref.handle;
    INTEGRATION_CHECK(ut_event_buffer_queue_push_move(
        &queue,
        &producer) == UT_EVENT_OK);
    INTEGRATION_CHECK(producer.owns_handle == 0u);
    INTEGRATION_CHECK(ut_event_buffer_queue_pop_move(
        &queue,
        &consumer) == UT_EVENT_OK);
    INTEGRATION_CHECK(ut_event_dispatch(
        &dispatcher,
        ut_event_buffer_message_event(&consumer),
        &handler_count) == UT_EVENT_OK);
    INTEGRATION_CHECK(handler_count == 1u);
    INTEGRATION_CHECK(handler_state.calls == 1u);
    INTEGRATION_CHECK(handler_state.read_result == UT_EVENT_OK);
    INTEGRATION_CHECK(handler_state.length == sizeof(input));
    INTEGRATION_CHECK(memcmp(
        input,
        handler_state.bytes,
        sizeof(input)) == 0);

    INTEGRATION_CHECK(ut_event_buffer_message_release(&consumer) ==
        UT_EVENT_OK);
    INTEGRATION_CHECK(mb_free(
        &memory_context,
        (mb_handle_t)released_handle) == MB_INVALID_HANDLE);
    return 0;
}
