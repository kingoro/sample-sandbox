/**
 * @file test_utility_event_buffer.c
 * @brief Buffer Pool所有Envelopeとmove Queueの単体テスト。
 */
#include "utility_event_buffer.h"

#include "test_cases.h"
#include "test_support.h"

#include <string.h>

/** fake Poolのbuffer容量。 */
#define FAKE_BUFFER_CAPACITY 32u
/** fake Poolで使用できるhandle値。 */
#define FAKE_BUFFER_HANDLE 7u

/** 単体テスト用fake Buffer Pool。 */
typedef struct fake_buffer_pool {
    /** buffer本体。 */
    uint8_t bytes[FAKE_BUFFER_CAPACITY];
    /** 有効byte数。 */
    size_t length;
    /** handleが割当済みなら1。 */
    uint8_t allocated;
    /** 次のfreeを失敗させる場合1。 */
    uint8_t fail_free;
    /** alloc呼出回数。 */
    size_t alloc_count;
    /** free呼出回数。 */
    size_t free_count;
} fake_buffer_pool_t;

/**
 * fake Poolから1領域を割り当てる。
 *
 * @param pool_context fake_buffer_pool_tへのpointer。
 * @param capacity 要求容量。
 * @param out_handle handle格納先。
 * @return 成功時UT_EVENT_OK、それ以外は対応するresult。
 */
static ut_event_result_t fake_alloc(
    void *pool_context,
    size_t capacity,
    ut_event_buffer_handle_t *out_handle)
{
    fake_buffer_pool_t *pool = (fake_buffer_pool_t *)pool_context;

    if ((pool == NULL) || (out_handle == NULL) ||
        (capacity == 0u) || (capacity > FAKE_BUFFER_CAPACITY)) {
        return UT_EVENT_INVALID_ARGUMENT;
    }
    if (pool->allocated != 0u) {
        return UT_EVENT_FULL;
    }
    pool->allocated = 1u;
    pool->length = 0u;
    pool->alloc_count++;
    *out_handle = FAKE_BUFFER_HANDLE;
    return UT_EVENT_OK;
}

/**
 * fake Poolの領域を解放する。
 *
 * @param pool_context fake_buffer_pool_tへのpointer。
 * @param handle 解放handle。
 * @return 成功時UT_EVENT_OK、それ以外は対応するresult。
 */
static ut_event_result_t fake_free(
    void *pool_context,
    ut_event_buffer_handle_t handle)
{
    fake_buffer_pool_t *pool = (fake_buffer_pool_t *)pool_context;

    if ((pool == NULL) || (pool->allocated == 0u) ||
        (handle != FAKE_BUFFER_HANDLE)) {
        return UT_EVENT_INVALID_ARGUMENT;
    }
    if (pool->fail_free != 0u) {
        pool->fail_free = 0u;
        return UT_EVENT_BUSY;
    }
    pool->allocated = 0u;
    pool->length = 0u;
    pool->free_count++;
    return UT_EVENT_OK;
}

/**
 * fake Poolへbyte列をcopyする。
 *
 * @param pool_context fake_buffer_pool_tへのpointer。
 * @param handle 書込みhandle。
 * @param offset 書込みoffset。
 * @param source copy元。
 * @param length copy長。
 * @return 成功時UT_EVENT_OK、それ以外は対応するresult。
 */
static ut_event_result_t fake_write(
    void *pool_context,
    ut_event_buffer_handle_t handle,
    size_t offset,
    const uint8_t *source,
    size_t length)
{
    fake_buffer_pool_t *pool = (fake_buffer_pool_t *)pool_context;

    if ((pool == NULL) || (pool->allocated == 0u) ||
        (handle != FAKE_BUFFER_HANDLE) ||
        (source == NULL) ||
        (offset > FAKE_BUFFER_CAPACITY) ||
        (length > (FAKE_BUFFER_CAPACITY - offset))) {
        return UT_EVENT_INVALID_ARGUMENT;
    }
    (void)memcpy(&pool->bytes[offset], source, length);
    if ((offset + length) > pool->length) {
        pool->length = offset + length;
    }
    return UT_EVENT_OK;
}

/**
 * fake Poolからbyte列をcopyする。
 *
 * @param pool_context fake_buffer_pool_tへのpointer。
 * @param handle 読取りhandle。
 * @param offset 読取りoffset。
 * @param destination copy先。
 * @param length copy長。
 * @return 成功時UT_EVENT_OK、それ以外は対応するresult。
 */
static ut_event_result_t fake_read(
    void *pool_context,
    ut_event_buffer_handle_t handle,
    size_t offset,
    uint8_t *destination,
    size_t length)
{
    fake_buffer_pool_t *pool = (fake_buffer_pool_t *)pool_context;

    if ((pool == NULL) || (pool->allocated == 0u) ||
        (handle != FAKE_BUFFER_HANDLE) ||
        ((destination == NULL) && (length != 0u)) ||
        (offset > pool->length) ||
        (length > (pool->length - offset))) {
        return UT_EVENT_INVALID_ARGUMENT;
    }
    if (length > 0u) {
        (void)memcpy(destination, &pool->bytes[offset], length);
    }
    return UT_EVENT_OK;
}

/**
 * fake Buffer Pool操作tableを作る。
 *
 * @param context fake Pool context。
 * @return 初期化済み操作table。
 */
static ut_event_buffer_pool_t make_pool(fake_buffer_pool_t *context)
{
    const ut_event_buffer_pool_t pool = {
        .context = context,
        .alloc = fake_alloc,
        .free = fake_free,
        .write = fake_write,
        .read = fake_read
    };

    return pool;
}

/**
 * create、Queue move、pop、read、releaseの所有権lifecycleを検証する。
 *
 * @return 成功時は0、失敗時は1。
 */
static int test_owned_message_lifecycle(void)
{
    fake_buffer_pool_t fake = {0};
    const ut_event_buffer_pool_t pool = make_pool(&fake);
    ut_event_buffer_message_t queue_storage[1];
    ut_event_buffer_queue_t queue;
    ut_event_buffer_message_t producer = {0};
    ut_event_buffer_message_t consumer = {0};
    const uint8_t input[] = {1u, 2u, 3u, 4u};
    uint8_t output[sizeof(input)] = {0};
    const ut_event_t *event;

    CHECK(ut_event_buffer_queue_init(&queue, queue_storage, 1u) ==
        UT_EVENT_OK);
    CHECK(ut_event_buffer_message_create_copy(
        &producer,
        &pool,
        100u,
        5u,
        input,
        sizeof(input)) == UT_EVENT_OK);
    CHECK(fake.allocated == 1u);
    CHECK(fake.alloc_count == 1u);

    CHECK(ut_event_buffer_queue_push_move(&queue, &producer) == UT_EVENT_OK);
    CHECK(producer.owns_handle == 0u);
    CHECK(ut_event_buffer_queue_count(&queue) == 1u);
    CHECK(ut_event_buffer_queue_pop_move(&queue, &consumer) == UT_EVENT_OK);
    CHECK(ut_event_buffer_queue_count(&queue) == 0u);

    event = ut_event_buffer_message_event(&consumer);
    CHECK(event != NULL);
    CHECK(event->id == 100u);
    CHECK(event->source == 5u);
    CHECK(event->payload == &consumer.buffer_ref);
    CHECK(consumer.buffer_ref.length == sizeof(input));
    CHECK(ut_event_buffer_message_read(
        &consumer,
        0u,
        output,
        sizeof(output)) == UT_EVENT_OK);
    CHECK(memcmp(input, output, sizeof(input)) == 0);
    CHECK(ut_event_buffer_message_release(&consumer) == UT_EVENT_OK);
    CHECK(fake.allocated == 0u);
    CHECK(fake.free_count == 1u);
    return 0;
}

/**
 * Queue満杯時にproducerが所有権を維持することを検証する。
 *
 * @return 成功時は0、失敗時は1。
 */
static int test_full_queue_preserves_source_ownership(void)
{
    fake_buffer_pool_t first_fake = {0};
    fake_buffer_pool_t second_fake = {0};
    const ut_event_buffer_pool_t first_pool = make_pool(&first_fake);
    const ut_event_buffer_pool_t second_pool = make_pool(&second_fake);
    ut_event_buffer_message_t queue_storage[1];
    ut_event_buffer_queue_t queue;
    ut_event_buffer_message_t first = {0};
    ut_event_buffer_message_t second = {0};
    const uint8_t data = 9u;

    CHECK(ut_event_buffer_queue_init(&queue, queue_storage, 1u) ==
        UT_EVENT_OK);
    CHECK(ut_event_buffer_message_create_copy(
        &first, &first_pool, 1u, 0u, &data, 1u) == UT_EVENT_OK);
    CHECK(ut_event_buffer_message_create_copy(
        &second, &second_pool, 2u, 0u, &data, 1u) == UT_EVENT_OK);
    CHECK(ut_event_buffer_queue_push_move(&queue, &first) == UT_EVENT_OK);
    CHECK(ut_event_buffer_queue_push_move(&queue, &second) == UT_EVENT_FULL);
    CHECK(second.owns_handle == 1u);
    CHECK(second_fake.allocated == 1u);
    CHECK(ut_event_buffer_message_release(&second) == UT_EVENT_OK);
    CHECK(ut_event_buffer_queue_release_all(&queue) == UT_EVENT_OK);
    CHECK(first_fake.allocated == 0u);
    return 0;
}

/**
 * release失敗時に所有状態とQueue要素を維持することを検証する。
 *
 * @return 成功時は0、失敗時は1。
 */
static int test_release_failure_is_retryable(void)
{
    fake_buffer_pool_t fake = {0};
    const ut_event_buffer_pool_t pool = make_pool(&fake);
    ut_event_buffer_message_t queue_storage[1];
    ut_event_buffer_queue_t queue;
    ut_event_buffer_message_t message = {0};
    const uint8_t data = 3u;

    CHECK(ut_event_buffer_queue_init(&queue, queue_storage, 1u) ==
        UT_EVENT_OK);
    CHECK(ut_event_buffer_message_create_copy(
        &message, &pool, 1u, 0u, &data, 1u) == UT_EVENT_OK);
    CHECK(ut_event_buffer_queue_push_move(&queue, &message) == UT_EVENT_OK);
    fake.fail_free = 1u;
    CHECK(ut_event_buffer_queue_release_all(&queue) == UT_EVENT_BUSY);
    CHECK(ut_event_buffer_queue_count(&queue) == 1u);
    CHECK(fake.allocated == 1u);
    CHECK(ut_event_buffer_queue_release_all(&queue) == UT_EVENT_OK);
    CHECK(ut_event_buffer_queue_count(&queue) == 0u);
    return 0;
}

/**
 * invalid argumentとdestination所有中のpop拒否を検証する。
 *
 * @return 成功時は0、失敗時は1。
 */
static int test_invalid_arguments(void)
{
    fake_buffer_pool_t fake = {0};
    ut_event_buffer_pool_t pool = make_pool(&fake);
    ut_event_buffer_message_t queue_storage[1];
    ut_event_buffer_queue_t queue = {0};
    ut_event_buffer_message_t message = {0};
    ut_event_buffer_message_t destination = {0};
    const uint8_t data = 1u;
    uint8_t output = 0u;

    CHECK(ut_event_buffer_queue_init(NULL, queue_storage, 1u) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_buffer_queue_init(&queue, NULL, 1u) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_buffer_queue_init(&queue, queue_storage, 0u) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_buffer_message_create_copy(
        NULL, &pool, 1u, 0u, &data, 1u) == UT_EVENT_INVALID_ARGUMENT);
    pool.read = NULL;
    CHECK(ut_event_buffer_message_create_copy(
        &message, &pool, 1u, 0u, &data, 1u) ==
        UT_EVENT_INVALID_ARGUMENT);
    pool = make_pool(&fake);
    CHECK(ut_event_buffer_message_create_copy(
        &message, &pool, 1u, 0u, NULL, 1u) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_buffer_message_create_copy(
        &message, &pool, 1u, 0u, &data, 0u) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_buffer_message_release(&message) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_buffer_message_event(&message) == NULL);
    CHECK(ut_event_buffer_message_read(
        &message, 0u, &output, 1u) == UT_EVENT_INVALID_ARGUMENT);

    CHECK(ut_event_buffer_queue_init(&queue, queue_storage, 1u) ==
        UT_EVENT_OK);
    CHECK(ut_event_buffer_queue_pop_move(&queue, &destination) ==
        UT_EVENT_EMPTY);
    CHECK(ut_event_buffer_message_create_copy(
        &message, &pool, 1u, 0u, &data, 1u) == UT_EVENT_OK);
    CHECK(ut_event_buffer_message_create_copy(
        &destination, &pool, 2u, 0u, &data, 1u) == UT_EVENT_FULL);
    CHECK(ut_event_buffer_queue_push_move(&queue, &message) == UT_EVENT_OK);
    destination.owns_handle = 1u;
    CHECK(ut_event_buffer_queue_pop_move(&queue, &destination) ==
        UT_EVENT_INVALID_ARGUMENT);
    destination.owns_handle = 0u;
    CHECK(ut_event_buffer_queue_release_all(&queue) == UT_EVENT_OK);
    return 0;
}

int run_utility_event_buffer_tests(void)
{
    CHECK(test_owned_message_lifecycle() == 0);
    CHECK(test_full_queue_preserves_source_ownership() == 0);
    CHECK(test_release_failure_is_retryable() == 0);
    CHECK(test_invalid_arguments() == 0);
    return 0;
}
