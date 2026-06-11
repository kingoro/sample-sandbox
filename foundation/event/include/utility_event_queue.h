/**
 * @file utility_event_queue.h
 * @brief 固定長FIFO Event Queueの公開API。
 */
#ifndef UTILITY_EVENT_QUEUE_H
#define UTILITY_EVENT_QUEUE_H

#include "utility_event_result.h"
#include "utility_event_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 呼出側が提供した固定長storageをリング状に管理するFIFO Queue。
 *
 * fieldは公開しているが、呼出側が直接変更してはならない。heapを使わず、
 * init時に渡されたstorageの範囲だけを使用する。
 */
typedef struct ut_event_queue {
    /** Event記述子を保持する呼出側所有の配列。 */
    ut_event_t *storage;
    /** storageへ格納できるEvent件数。 */
    size_t capacity;
    /** 次にpopするslot index。 */
    size_t head;
    /** 次にpushするslot index。 */
    size_t tail;
    /** 現在Queueに格納されているEvent件数。 */
    size_t count;
} ut_event_queue_t;

/**
 * 固定長Event Queueを初期化する。
 *
 * storageの内容は初期化しない。queueとstorageは再初期化または利用終了まで
 * 有効に保つ。
 *
 * @param queue 初期化するQueue context。
 * @param storage Event記述子を保持する呼出側所有の配列。
 * @param capacity storageへ格納できるEvent件数。
 * @return UT_EVENT_OKまたはUT_EVENT_INVALID_ARGUMENT。
 */
ut_event_result_t ut_event_queue_init(
    ut_event_queue_t *queue,
    ut_event_t *storage,
    size_t capacity);

/**
 * Event記述子をQueue末尾へ浅くcopyする。
 *
 * payloadが指すデータ本体はcopyしない。満杯時は既存Eventを上書きしない。
 *
 * @param queue 初期化済みQueue context。
 * @param event Queueへ浅くcopyするEvent記述子。
 * @return UT_EVENT_OK、UT_EVENT_FULL、UT_EVENT_INVALID_ARGUMENT。
 */
ut_event_result_t ut_event_queue_push(
    ut_event_queue_t *queue,
    const ut_event_t *event);

/**
 * Queue先頭のEventをout_eventへcopyしてQueueから削除する。
 *
 * @param queue 初期化済みQueue context。
 * @param out_event 取り出したEvent記述子の格納先。
 * @return UT_EVENT_OK、UT_EVENT_EMPTY、UT_EVENT_INVALID_ARGUMENT。
 */
ut_event_result_t ut_event_queue_pop(
    ut_event_queue_t *queue,
    ut_event_t *out_event);

/**
 * Queue先頭のEventを削除せずout_eventへcopyする。
 *
 * @param queue 初期化済みQueue context。
 * @param out_event 先頭Event記述子の格納先。
 * @return UT_EVENT_OK、UT_EVENT_EMPTY、UT_EVENT_INVALID_ARGUMENT。
 */
ut_event_result_t ut_event_queue_peek(
    const ut_event_queue_t *queue,
    ut_event_t *out_event);

/**
 * Queueを空にする。payloadの解放処理は行わない。
 *
 * @param queue 初期化済みQueue context。
 * @return UT_EVENT_OKまたはUT_EVENT_INVALID_ARGUMENT。
 */
ut_event_result_t ut_event_queue_clear(ut_event_queue_t *queue);

/**
 * 有効なQueueの現在件数を返す。
 *
 * @param queue Queue context。
 * @return 現在件数。無効なQueueでは0。
 */
size_t ut_event_queue_count(const ut_event_queue_t *queue);

/**
 * 有効なQueueの最大件数を返す。
 *
 * @param queue Queue context。
 * @return 最大件数。無効なQueueでは0。
 */
size_t ut_event_queue_capacity(const ut_event_queue_t *queue);

#ifdef __cplusplus
}
#endif

#endif
