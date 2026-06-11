/**
 * @file utility_event_buffer.h
 * @brief Buffer Pool handleの所有権をEventと一緒に移譲するAPI。
 */
#ifndef UTILITY_EVENT_BUFFER_H
#define UTILITY_EVENT_BUFFER_H

#include "utility_event_result.h"
#include "utility_event_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Buffer Pool実装が発行するopaque handle型。 */
typedef uint32_t ut_event_buffer_handle_t;

/**
 * Buffer Poolから領域を割り当てるcallback。
 *
 * @param pool_context Buffer Pool実装context。
 * @param capacity 必要なbyte容量。
 * @param out_handle 成功時のhandle格納先。
 * @return 成功時UT_EVENT_OK、それ以外はPool実装が対応付けたEvent result。
 */
typedef ut_event_result_t (*ut_event_buffer_alloc_fn)(void *pool_context, size_t capacity, ut_event_buffer_handle_t *out_handle);

/**
 * Buffer Poolの領域を解放するcallback。
 *
 * @param pool_context Buffer Pool実装context。
 * @param handle 解放するhandle。
 * @return 成功時UT_EVENT_OK、それ以外はPool実装が対応付けたEvent result。
 */
typedef ut_event_result_t (*ut_event_buffer_free_fn)(void *pool_context, ut_event_buffer_handle_t handle);

/**
 * Buffer Poolへbyte列をcopyするcallback。
 *
 * @param pool_context Buffer Pool実装context。
 * @param handle 書込み対象handle。
 * @param offset buffer先頭からのoffset。
 * @param source copy元。
 * @param length copyするbyte数。
 * @return 成功時UT_EVENT_OK、それ以外はPool実装が対応付けたEvent result。
 */
typedef ut_event_result_t (*ut_event_buffer_write_fn)(void *pool_context, ut_event_buffer_handle_t handle, size_t offset, const uint8_t *source, size_t length);

/**
 * Buffer Poolからbyte列をcopyするcallback。
 *
 * @param pool_context Buffer Pool実装context。
 * @param handle 読取り対象handle。
 * @param offset buffer先頭からのoffset。
 * @param destination copy先。
 * @param length copyするbyte数。
 * @return 成功時UT_EVENT_OK、それ以外はPool実装が対応付けたEvent result。
 */
typedef ut_event_result_t (*ut_event_buffer_read_fn)(void *pool_context, ut_event_buffer_handle_t handle, size_t offset, uint8_t *destination, size_t length);

/** Event Foundationが利用するBuffer Pool操作table。 */
typedef struct ut_event_buffer_pool {
    /** Buffer Pool実装context。 */
    void *context;
    /** 領域割当callback。 */
    ut_event_buffer_alloc_fn alloc;
    /** 領域解放callback。 */
    ut_event_buffer_free_fn free;
    /** byte列書込みcallback。 */
    ut_event_buffer_write_fn write;
    /** byte列読取りcallback。 */
    ut_event_buffer_read_fn read;
} ut_event_buffer_pool_t;

/** Event handlerへ渡すBuffer Pool参照。 */
typedef struct ut_event_buffer_ref {
    /** handleを管理するBuffer Pool。 */
    const ut_event_buffer_pool_t *pool;
    /** Buffer Pool内のopaque handle。 */
    ut_event_buffer_handle_t handle;
    /** buffer内の有効byte数。 */
    size_t length;
} ut_event_buffer_ref_t;

/**
 * EventとBuffer Pool handleの所有権を保持するEnvelope。
 *
 * fieldは公開しているが利用側が直接変更してはならない。event.payloadはEnvelope内の
 * buffer_refを指すため、通常のut_event_queue_tへcopyしてはならない。
 */
typedef struct ut_event_buffer_message {
    /** Dispatcherへ渡すEvent記述子。 */
    ut_event_t event;
    /** Event payloadとして公開するBuffer Pool参照。 */
    ut_event_buffer_ref_t buffer_ref;
    /** handle解放責務を保持する場合1。 */
    uint8_t owns_handle;
} ut_event_buffer_message_t;

/** Buffer所有Envelopeをmove semanticsで保持する固定長FIFO Queue。 */
typedef struct ut_event_buffer_queue {
    /** 呼出側所有のEnvelope配列。 */
    ut_event_buffer_message_t *storage;
    /** storageへ格納できるEnvelope件数。 */
    size_t capacity;
    /** 次にpopするslot index。 */
    size_t head;
    /** 次にpushするslot index。 */
    size_t tail;
    /** 現在Queueに格納されているEnvelope件数。 */
    size_t count;
} ut_event_buffer_queue_t;

/**
 * source byte列をPoolへcopyし、handleを所有するEnvelopeを作る。
 *
 * allocation後のwriteに失敗した場合は内部でhandleを解放する。
 *
 * @param message 出力Envelope。未所有状態でなければならない。
 * @param pool 使用するBuffer Pool操作table。
 * @param event_id Event ID。
 * @param source_id Event発行元module ID。
 * @param source copy元byte列。
 * @param length copyするbyte数。0は指定できない。
 * @return UT_EVENT_OKまたは引数・Pool操作に対応するresult。
 */
ut_event_result_t ut_event_buffer_message_create_copy(ut_event_buffer_message_t *message, const ut_event_buffer_pool_t *pool, uint32_t event_id, uint32_t source_id, const uint8_t *source, size_t length);

/**
 * Envelopeが所有するhandleを解放し、Envelopeを空にする。
 *
 * 解放に失敗した場合は所有状態を維持するため再試行できる。
 *
 * @param message handleを所有するEnvelope。
 * @return UT_EVENT_OKまたは引数・Pool解放に対応するresult。
 */
ut_event_result_t ut_event_buffer_message_release(ut_event_buffer_message_t *message);

/**
 * Envelope内のEvent記述子を返す。
 *
 * @param message handleを所有するEnvelope。
 * @return Dispatcherへ渡せるEvent。無効なEnvelopeではNULL。
 */
const ut_event_t *ut_event_buffer_message_event(const ut_event_buffer_message_t *message);

/**
 * Envelope内Bufferからbyte列をcopyする。
 *
 * @param message handleを所有するEnvelope。
 * @param offset buffer先頭からのoffset。
 * @param destination copy先。
 * @param length copyするbyte数。
 * @return UT_EVENT_OKまたは引数・Pool読取りに対応するresult。
 */
ut_event_result_t ut_event_buffer_message_read(const ut_event_buffer_message_t *message, size_t offset, uint8_t *destination, size_t length);

/**
 * Buffer所有Envelope Queueを初期化する。
 *
 * @param queue 初期化するQueue。
 * @param storage Envelopeを保持する呼出側所有配列。
 * @param capacity storageのslot数。
 * @return UT_EVENT_OKまたはUT_EVENT_INVALID_ARGUMENT。
 */
ut_event_result_t ut_event_buffer_queue_init(ut_event_buffer_queue_t *queue, ut_event_buffer_message_t *storage, size_t capacity);

/**
 * EnvelopeをQueueへmoveする。
 *
 * 成功時はsourceの所有権をQueueへ移し、sourceを空にする。Queue満杯など失敗時は
 * sourceが所有権を維持する。
 *
 * @param queue 初期化済みQueue。
 * @param source move元Envelope。
 * @return UT_EVENT_OK、UT_EVENT_FULL、UT_EVENT_INVALID_ARGUMENT。
 */
ut_event_result_t ut_event_buffer_queue_push_move(ut_event_buffer_queue_t *queue, ut_event_buffer_message_t *source);

/**
 * Queue先頭Envelopeをdestinationへmoveする。
 *
 * destinationは未所有状態でなければならない。成功後はdestinationがhandle解放責務を
 * 持つ。
 *
 * @param queue 初期化済みQueue。
 * @param destination move先Envelope。
 * @return UT_EVENT_OK、UT_EVENT_EMPTY、UT_EVENT_INVALID_ARGUMENT。
 */
ut_event_result_t ut_event_buffer_queue_pop_move(ut_event_buffer_queue_t *queue, ut_event_buffer_message_t *destination);

/**
 * Queue内Envelope件数を返す。
 *
 * @param queue Buffer所有Envelope Queue。
 * @return 現在件数。無効なQueueでは0。
 */
size_t ut_event_buffer_queue_count(const ut_event_buffer_queue_t *queue);

/**
 * Queue内の全handleを解放して空にする。
 *
 * 途中の解放失敗時はその位置で停止し、未解放EnvelopeをQueueへ残す。
 *
 * @param queue 初期化済みQueue。
 * @return UT_EVENT_OKまたは最初のPool解放error。
 */
ut_event_result_t ut_event_buffer_queue_release_all(ut_event_buffer_queue_t *queue);

#ifdef __cplusplus
}
#endif

#endif
