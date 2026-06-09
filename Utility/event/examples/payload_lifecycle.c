/**
 * @file payload_lifecycle.c
 * @brief 小payloadの値copyと大payloadのBuffer所有権を示す例。
 *
 * @par 使用コンポーネント
 * - ut_event_publisher_t: 小さなpayloadを固定長slotへ値copyして配送する。
 * - ut_event_dispatcher_t: Publisher内部でEventをhandlerへ同期配送する。
 * - ut_event_buffer_pool_t: 大きなpayloadの保存領域をcallbackで抽象化する。
 * - ut_event_buffer_message_t: Buffer handleの所有権をEventと一緒に保持する。
 * - ut_event_buffer_queue_t: Buffer messageの所有権をmoveして受け渡す。
 * - Log Utility: 小payloadを受信したhandlerの結果を表示する。
 *
 * @par 処理フロー
 * 1. Publisher、購読handler、Loggerを初期化する。
 * 2. stack上の小payloadをPublisherへ値copyしてからcopy元を書き換える。
 * 3. Publisherがcopy済みpayloadをhandlerへ配送し、元の値が保たれることを確認する。
 * 4. 大payloadをBuffer Poolへcopyし、messageにhandle所有権を持たせる。
 * 5. push_moveで所有権をproducerからQueueへ、pop_moveでQueueからconsumerへ移す。
 * 6. consumerがpayloadをreadした後、messageをreleaseしてPoolへ領域を返す。
 */
#include "utility_event.h"
#include "utility_log.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/** Publisher容量。 */
#define EXAMPLE_PUBLISHER_CAPACITY 2U
/** Log Ring容量。 */
#define EXAMPLE_LOG_CAPACITY 16U
/** fake Buffer容量。 */
#define EXAMPLE_BUFFER_CAPACITY 32U
/** 小payload Event ID。 */
#define EXAMPLE_EVENT_RESULT 30U
/** 大payload Event ID。 */
#define EXAMPLE_EVENT_DATA 31U

/** Publisherで値copyする小payload。 */
typedef struct result_payload {
    /** 要求ID。 */
    uint32_t request_id;
    /** 処理結果。 */
    uint32_t value;
} result_payload_t;

/** 1領域だけ持つ説明用Buffer Pool。 */
typedef struct example_pool {
    /** payload本体。 */
    uint8_t bytes[EXAMPLE_BUFFER_CAPACITY];
    /** 有効byte数。 */
    size_t length;
    /** 割当中の場合true。 */
    bool allocated;
} example_pool_t;

/** サンプルの観測状態。 */
typedef struct payload_context {
    /** 値copyされた結果。 */
    uint32_t result;
} payload_context_t;

/**
 * fake Poolから領域を割り当てる。
 *
 * @param context example_pool_tへのpointer。
 * @param capacity 要求byte数。
 * @param out_handle handle格納先。
 * @return 処理結果。
 */
static ut_event_result_t pool_alloc(
    void *context,
    size_t capacity,
    ut_event_buffer_handle_t *out_handle)
{
    example_pool_t *pool = context;

    if ((capacity == 0U) || (capacity > EXAMPLE_BUFFER_CAPACITY)
        || pool->allocated) {
        return UT_EVENT_FULL;
    }
    pool->allocated = true;
    *out_handle = 1U;
    return UT_EVENT_OK;
}

/**
 * fake Pool領域を解放する。
 *
 * @param context example_pool_tへのpointer。
 * @param handle 解放handle。
 * @return 処理結果。
 */
static ut_event_result_t pool_free(
    void *context,
    ut_event_buffer_handle_t handle)
{
    example_pool_t *pool = context;

    if (!pool->allocated || (handle != 1U)) {
        return UT_EVENT_INVALID_ARGUMENT;
    }
    pool->allocated = false;
    pool->length = 0U;
    return UT_EVENT_OK;
}

/**
 * fake Poolへ書き込む。
 *
 * @param context example_pool_tへのpointer。
 * @param handle 書込みhandle。
 * @param offset 書込み位置。
 * @param source copy元。
 * @param length copy byte数。
 * @return 処理結果。
 */
static ut_event_result_t pool_write(
    void *context,
    ut_event_buffer_handle_t handle,
    size_t offset,
    const uint8_t *source,
    size_t length)
{
    example_pool_t *pool = context;

    if (!pool->allocated || (handle != 1U)
        || (offset + length > EXAMPLE_BUFFER_CAPACITY)) {
        return UT_EVENT_INVALID_ARGUMENT;
    }
    (void)memcpy(&pool->bytes[offset], source, length);
    pool->length = offset + length;
    return UT_EVENT_OK;
}

/**
 * fake Poolから読み出す。
 *
 * @param context example_pool_tへのpointer。
 * @param handle 読出しhandle。
 * @param offset 読出し位置。
 * @param destination copy先。
 * @param length copy byte数。
 * @return 処理結果。
 */
static ut_event_result_t pool_read(
    void *context,
    ut_event_buffer_handle_t handle,
    size_t offset,
    uint8_t *destination,
    size_t length)
{
    example_pool_t *pool = context;

    if (!pool->allocated || (handle != 1U)
        || (offset + length > pool->length)) {
        return UT_EVENT_INVALID_ARGUMENT;
    }
    (void)memcpy(destination, &pool->bytes[offset], length);
    return UT_EVENT_OK;
}

/**
 * 値copy Eventを処理する。
 *
 * @param event Result Event。
 * @param user_context payload_context_tへのpointer。
 */
static void on_result(const ut_event_t *event, void *user_context)
{
    payload_context_t *context = user_context;
    const result_payload_t *payload = event->payload;

    context->result = payload->value;
    UT_LOG_INFO("EVENT-PAYLOAD", "request=%u result=%u",
        payload->request_id, payload->value);
}

/**
 * 2種類のpayload寿命管理を実行する。
 *
 * @return 成功時0、失敗時1。
 */
int main(void)
{
    ut_event_publisher_t publisher;
    ut_event_t event_storage[EXAMPLE_PUBLISHER_CAPACITY];
    result_payload_t payload_storage[EXAMPLE_PUBLISHER_CAPACITY];
    uint8_t occupied[EXAMPLE_PUBLISHER_CAPACITY];
    ut_event_subscription_t subscriptions[1];
    ut_event_buffer_message_t queue_storage[1];
    ut_event_buffer_queue_t buffer_queue;
    ut_event_buffer_message_t producer = {0};
    ut_event_buffer_message_t consumer = {0};
    ut_log_record_t log_storage[EXAMPLE_LOG_CAPACITY];
    ut_logger_t logger;
    example_pool_t pool_context = {0};
    payload_context_t context = {0};
    result_payload_t result = {7U, 42U};
    uint8_t received[4] = {0};
    const uint8_t data[4] = {1U, 2U, 3U, 4U};
    const ut_event_buffer_pool_t pool = {
        .context = &pool_context,
        .alloc = pool_alloc,
        .free = pool_free,
        .write = pool_write,
        .read = pool_read,
    };
    const ut_logger_config_t log_config = {
        .console_write = ut_log_console_write_file,
        .console_level = UT_LOG_LEVEL_INFO,
        .ring_level = UT_LOG_LEVEL_INFO,
        .console_enabled = true,
        .ring_enabled = true,
    };
    bool succeeded =
        ut_log_initialize(&logger, log_storage, EXAMPLE_LOG_CAPACITY,
            &log_config) == UT_LOG_OK
        && ut_event_publisher_init(&publisher, event_storage,
            (uint8_t *)payload_storage, occupied, EXAMPLE_PUBLISHER_CAPACITY,
            sizeof(payload_storage[0]), subscriptions, 1U,
            NULL, NULL, NULL) == UT_EVENT_OK
        && ut_event_publisher_subscribe(&publisher, EXAMPLE_EVENT_RESULT,
            on_result, &context) == UT_EVENT_OK
        && ut_event_publisher_publish_copy(&publisher, EXAMPLE_EVENT_RESULT,
            40U, &result, sizeof(result)) == UT_EVENT_OK;

    /* 発行元stackを書き換えてもPublisher内のcopyは変化しない。 */
    result.value = 999U;
    if (succeeded) {
        succeeded = ut_event_publisher_count(&publisher) == 1U
            && ut_event_publisher_dispatch(&publisher, 1U, NULL) == UT_EVENT_OK
            && context.result == 42U;
    }

    /*
     * 大payloadは固定slotへcopyせずBuffer Poolへ格納する。
     * push成功で所有権はproducerからQueueへ、popでconsumerへ移動する。
     */
    if (succeeded) {
        succeeded = ut_event_buffer_queue_init(&buffer_queue,
            queue_storage, 1U) == UT_EVENT_OK
            && ut_event_buffer_message_create_copy(&producer, &pool,
                EXAMPLE_EVENT_DATA, 41U, data, sizeof(data)) == UT_EVENT_OK
            && ut_event_buffer_queue_push_move(&buffer_queue, &producer)
                == UT_EVENT_OK
            && ut_event_buffer_queue_pop_move(&buffer_queue, &consumer)
                == UT_EVENT_OK
            && ut_event_buffer_message_event(&consumer)->id
                == EXAMPLE_EVENT_DATA
            && ut_event_buffer_message_read(&consumer, 0U, received,
                sizeof(received)) == UT_EVENT_OK
            && memcmp(data, received, sizeof(data)) == 0
            && ut_event_buffer_message_release(&consumer) == UT_EVENT_OK
            && !pool_context.allocated
            && ut_event_buffer_queue_release_all(&buffer_queue) == UT_EVENT_OK;
    }
    ut_log_shutdown();
    return succeeded ? 0 : 1;
}
