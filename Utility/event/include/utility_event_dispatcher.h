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
 */
typedef void (*ut_event_handler_t)(
    const ut_event_t *event,
    void *user_context);

/**
 * Dispatcherが使用する1件のhandler登録領域。
 * 呼出側は配列storageを提供するだけで、fieldを直接変更してはならない。
 */
typedef struct ut_event_subscription {
    uint32_t event_id;
    ut_event_handler_t handler;
    void *user_context;
    uint8_t occupied;
} ut_event_subscription_t;

/**
 * 呼出側が提供した固定長subscription storageを管理するDispatcher。
 * 内部ロックは持たず、同じDispatcherへの並行操作は呼出側が直列化する。
 */
typedef struct ut_event_dispatcher {
    ut_event_subscription_t *subscriptions;
    size_t capacity;
    size_t count;
    uint8_t dispatching;
} ut_event_dispatcher_t;

/**
 * Dispatcherを初期化し、subscription storageを空にする。
 *
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
 */
ut_event_result_t ut_event_dispatch(
    ut_event_dispatcher_t *dispatcher,
    const ut_event_t *event,
    size_t *out_handler_count);

/** 有効なDispatcherの登録件数を返す。無効なDispatcherでは0を返す。 */
size_t ut_event_subscription_count(
    const ut_event_dispatcher_t *dispatcher);

#ifdef __cplusplus
}
#endif

#endif
