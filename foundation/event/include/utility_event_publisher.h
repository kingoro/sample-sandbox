/**
 * @file utility_event_publisher.h
 * @brief payload値copy、Queue、Dispatcherを統合するEvent Publisher API。
 *
 * Publisherは呼出側提供storageだけを使い、publish時にpayload本体を固定長slotへ
 * copyする。lock callbackを設定すると複数producerからpublishできる。
 */
#ifndef UTILITY_EVENT_PUBLISHER_H
#define UTILITY_EVENT_PUBLISHER_H

#include "utility_event_dispatcher.h"
#include "utility_event_queue.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Publisher共有領域へ入るcallback。
 *
 * @param context 利用側が登録したlock context。
 */
typedef void (*ut_event_publisher_lock_fn)(void *context);

/**
 * Publisher共有領域から出るcallback。
 *
 * @param context 利用側が登録したlock context。
 */
typedef void (*ut_event_publisher_unlock_fn)(void *context);

/**
 * payloadを値所有して同期配送するPublisher。
 *
 * fieldは公開しているが利用側が直接変更してはならない。heap、thread、mutexを
 * 所有せず、storageと排他callbackは呼出側が提供する。dispatchは単一実行主体から
 * 呼び、publishだけを複数producerから呼べる。
 */
typedef struct ut_event_publisher {
    /** Event記述子を保持する内部Queue。 */
    ut_event_queue_t queue;
    /** handlerを同期実行する内部Dispatcher。 */
    ut_event_dispatcher_t dispatcher;
    /** payload slotを保持する呼出側所有byte配列。 */
    uint8_t *payload_storage;
    /** payload slotの使用状態を保持する呼出側所有配列。 */
    uint8_t *occupied_storage;
    /** Eventおよびpayload slot数。 */
    size_t capacity;
    /** 1 payload slotのbyte容量。 */
    size_t payload_capacity;
    /** 任意の排他開始callback。 */
    ut_event_publisher_lock_fn lock;
    /** 任意の排他終了callback。 */
    ut_event_publisher_unlock_fn unlock;
    /** lockとunlockへ渡すcontext。 */
    void *lock_context;
    /** dispatch実行中の場合true。 */
    bool dispatching;
} ut_event_publisher_t;

/**
 * Publisherを呼出側storageで初期化する。
 *
 * @param publisher 初期化するPublisher。
 * @param event_storage capacity件のEvent記述子配列。
 * @param payload_storage capacity * payload_capacity byte以上の領域。
 * 配送handlerがpayloadを型pointerへ変換する場合、その型に必要なalignmentを持たせる。
 * @param occupied_storage capacity byte以上の使用状態領域。
 * @param capacity Eventおよびpayload slot数。
 * @param payload_capacity 1 Eventへcopyできる最大payload byte数。
 * @param subscription_storage Dispatcher登録領域。
 * @param subscription_capacity subscription_storageの要素数。
 * @param lock 任意の排他開始callback。不要ならNULL。
 * @param unlock 任意の排他終了callback。lockと対で指定する。
 * @param lock_context lockとunlockへ渡すcontext。
 * @return UT_EVENT_OKまたはUT_EVENT_INVALID_ARGUMENT。
 *
 * 渡したstorageとlock contextはPublisher利用終了まで有効に保つ。
 */
ut_event_result_t ut_event_publisher_init(ut_event_publisher_t *publisher, ut_event_t *event_storage, uint8_t *payload_storage, uint8_t *occupied_storage, size_t capacity, size_t payload_capacity, ut_event_subscription_t *subscription_storage, size_t subscription_capacity, ut_event_publisher_lock_fn lock, ut_event_publisher_unlock_fn unlock, void *lock_context);

/**
 * Event購読handlerを登録する。
 *
 * @param publisher 初期化済みPublisher。
 * @param event_id 購読するEvent ID。
 * @param handler Event受信handler。
 * @param user_context handlerへ渡すcontext。
 * @return Dispatcherが返すresult code。
 */
ut_event_result_t ut_event_publisher_subscribe(ut_event_publisher_t *publisher, uint32_t event_id, ut_event_handler_t handler, void *user_context);

/**
 * payloadを値copyしてEventを発行する。
 *
 * @param publisher 初期化済みPublisher。
 * @param event_id 発行するEvent ID。
 * @param source Event発行元ID。
 * @param payload copy元。payload_sizeが0の場合はNULL。
 * @param payload_size copyするbyte数。
 * @return UT_EVENT_OK、UT_EVENT_FULL、UT_EVENT_INVALID_ARGUMENT。
 *
 * 成功後はcopy元payloadの寿命に依存しない。payload_sizeは初期化時の
 * payload_capacity以下でなければならない。
 */
ut_event_result_t ut_event_publisher_publish_copy(ut_event_publisher_t *publisher, uint32_t event_id, uint32_t source, const void *payload, size_t payload_size);

/**
 * Queue内Eventを最大budget件同期配送する。
 *
 * @param publisher 初期化済みPublisher。
 * @param budget 1回で配送する最大件数。0は指定できない。
 * @param out_dispatched 配送したEvent件数の任意格納先。
 * @return UT_EVENT_OK、UT_EVENT_BUSY、UT_EVENT_INVALID_ARGUMENT、
 * または最初のDispatcher error。
 *
 * handlerがpublishしたEventもbudget内なら同じ呼び出しで配送する。
 * 購読先なしはEventを消費して処理を継続する。
 */
ut_event_result_t ut_event_publisher_dispatch(ut_event_publisher_t *publisher, size_t budget, size_t *out_dispatched);

/**
 * 未配送Event件数を返す。
 *
 * @param publisher 初期化済みPublisher。
 * @return Queue内Event件数。無効なPublisherでは0。
 */
size_t ut_event_publisher_count(const ut_event_publisher_t *publisher);

#ifdef __cplusplus
}
#endif

#endif
