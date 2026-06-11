/**
 * @file utility_event_dispatcher.h
 * @brief Event handlerの登録と同期配送を行う公開API。
 */
#ifndef UTILITY_EVENT_DISPATCHER_H
#define UTILITY_EVENT_DISPATCHER_H

#include "utility_event_result.h"
#include "utility_event_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Event受信handler。
 *
 * handlerはut_event_dispatchの呼出しthread上で同期実行される。handlerから
 * Eventやpayloadのpointerを後で使うために保持してはならない。
 *
 * @param event 配送されたEvent。
 * @param user_context subscription登録時に指定した呼出側context。
 */
typedef void (*ut_event_handler_t)(
    const ut_event_t *event,
    void *user_context);

/**
 * Dispatcherが使用する1件のhandler登録領域。
 * 呼出側は配列storageを提供するだけで、fieldを直接変更してはならない。
 */
typedef struct ut_event_subscription {
    /** このsubscriptionが受信するEvent ID。 */
    uint32_t event_id;
    /** Event受信時に同期実行するhandler。 */
    ut_event_handler_t handler;
    /** handlerへ渡す呼出側所有context。 */
    void *user_context;
    /** slotが使用中なら1、未使用なら0。 */
    uint8_t occupied;
} ut_event_subscription_t;

/**
 * 呼出側が提供した固定長subscription storageを管理するDispatcher。
 * 内部ロックは持たず、同じDispatcherへの並行操作は呼出側が直列化する。
 */
typedef struct ut_event_dispatcher {
    /** 呼出側が提供するsubscription配列。 */
    ut_event_subscription_t *subscriptions;
    /** subscription配列のslot数。 */
    size_t capacity;
    /** 現在登録されているsubscription数。 */
    size_t count;
    /** handler配送中なら1。再入と登録変更の拒否に使用する。 */
    uint8_t dispatching;
} ut_event_dispatcher_t;

/**
 * Dispatcherを初期化し、subscription storageを空にする。
 *
 * @param dispatcher 初期化するDispatcher context。
 * @param storage subscriptionを保持する呼出側所有の配列。
 * @param capacity storageのslot数。
 * @return UT_EVENT_OKまたはUT_EVENT_INVALID_ARGUMENT。
 */
ut_event_result_t ut_event_dispatcher_init(
    ut_event_dispatcher_t *dispatcher,
    ut_event_subscription_t *storage,
    size_t capacity);

/**
 * Event ID、handler、user_contextの組を登録する。
 *
 * 同じ3要素の組を重複登録できない。UT_EVENT_ID_ANYを指定すると全Eventに
 * 一致する。
 *
 * @param dispatcher 初期化済みDispatcher context。
 * @param event_id 購読するEvent IDまたはUT_EVENT_ID_ANY。
 * @param handler Event受信時に同期実行する関数。
 * @param user_context handlerへそのまま渡す呼出側所有context。
 * @return UT_EVENT_OK、UT_EVENT_ALREADY_EXISTS、UT_EVENT_FULL、
 * UT_EVENT_BUSY、UT_EVENT_INVALID_ARGUMENT。
 */
ut_event_result_t ut_event_subscribe(
    ut_event_dispatcher_t *dispatcher,
    uint32_t event_id,
    ut_event_handler_t handler,
    void *user_context);

/**
 * Event ID、handler、user_contextがすべて一致する登録を解除する。
 *
 * @param dispatcher 初期化済みDispatcher context。
 * @param event_id 解除するEvent ID。
 * @param handler 解除するhandler。
 * @param user_context 解除する登録のuser context。
 * @return UT_EVENT_OK、UT_EVENT_NOT_FOUND、UT_EVENT_BUSY、
 * UT_EVENT_INVALID_ARGUMENT。
 */
ut_event_result_t ut_event_unsubscribe(
    ut_event_dispatcher_t *dispatcher,
    uint32_t event_id,
    ut_event_handler_t handler,
    void *user_context);

/**
 * 一致するhandlerを登録順に同期実行する。
 *
 * handler実行中のsubscribe、unsubscribe、再帰dispatchはUT_EVENT_BUSYになる。
 * out_handler_countはNULLでもよく、指定時は実行したhandler数を格納する。
 * handlerが見つからない場合はUT_EVENT_NOT_FOUNDを返す。
 *
 * @param dispatcher 初期化済みDispatcher context。
 * @param event 配送するEvent記述子。
 * @param out_handler_count 実行handler数の任意の格納先。不要ならNULL。
 * @return UT_EVENT_OK、UT_EVENT_NOT_FOUND、UT_EVENT_BUSY、
 * UT_EVENT_INVALID_ARGUMENT。
 */
ut_event_result_t ut_event_dispatch(
    ut_event_dispatcher_t *dispatcher,
    const ut_event_t *event,
    size_t *out_handler_count);

/**
 * 有効なDispatcherの登録件数を返す。
 *
 * @param dispatcher Dispatcher context。
 * @return 登録件数。無効なDispatcherでは0。
 */
size_t ut_event_subscription_count(
    const ut_event_dispatcher_t *dispatcher);

#ifdef __cplusplus
}
#endif

#endif
