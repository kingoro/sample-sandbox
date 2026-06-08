/**
 * @file fuzz_event_operations.c
 * @brief QueueとDispatcherへ任意の操作列を適用するfuzz target。
 */
#include "utility_event.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

/** Fuzz対象Queueの固定capacity。 */
#define FUZZ_QUEUE_CAPACITY 8u
/** Fuzz対象Dispatcherの固定subscription capacity。 */
#define FUZZ_SUBSCRIPTION_CAPACITY 4u
/** Fuzzで使い分けるhandler context数。 */
#define FUZZ_HANDLER_COUNT 4u
/** Fuzzで使い分けるEvent ID数。 */
#define FUZZ_EVENT_ID_COUNT 4u
/** Fuzz対象Timerの固定capacity。 */
#define FUZZ_TIMER_CAPACITY 4u
/** Timer Event発行先Queueの固定capacity。 */
#define FUZZ_TIMER_QUEUE_CAPACITY 8u

/** Fuzz handlerの観測状態。 */
typedef struct fuzz_handler_state {
    /** handler呼出回数。 */
    size_t calls;
    /** 最後に受信したEvent ID。 */
    uint32_t last_event_id;
} fuzz_handler_state_t;

/**
 * 配送されたEventをfuzz_handler_state_tへ記録する。
 *
 * @param event 配送されたEvent。
 * @param user_context fuzz_handler_state_tへのpointer。
 */
static void fuzz_handler(const ut_event_t *event, void *user_context)
{
    fuzz_handler_state_t *state = (fuzz_handler_state_t *)user_context;

    state->calls++;
    state->last_event_id = event->id;
}

/**
 * Fuzz不変条件を検査し、違反時にprocessを異常終了させる。
 *
 * @param condition 検査する条件。
 */
static void require_condition(int condition)
{
    if (!condition) {
        abort();
    }
}

/**
 * 1件の4 byte命令をQueue操作として実行し、参照modelと照合する。
 *
 * @param queue 操作対象Queue。
 * @param model_count 参照model上のQueue件数。
 * @param command 4 byte以上の命令列。
 */
static void fuzz_queue_operation(
    ut_event_queue_t *queue,
    size_t *model_count,
    const uint8_t *command)
{
    const ut_event_t event = {
        command[1],
        command[2],
        NULL,
        0u
    };
    ut_event_t output;
    ut_event_result_t result;

    switch (command[0] % 4u) {
    case 0u:
        result = ut_event_queue_push(queue, &event);
        if (*model_count == FUZZ_QUEUE_CAPACITY) {
            require_condition(result == UT_EVENT_FULL);
        } else {
            require_condition(result == UT_EVENT_OK);
            (*model_count)++;
        }
        break;
    case 1u:
        result = ut_event_queue_pop(queue, &output);
        if (*model_count == 0u) {
            require_condition(result == UT_EVENT_EMPTY);
        } else {
            require_condition(result == UT_EVENT_OK);
            (*model_count)--;
        }
        break;
    case 2u:
        result = ut_event_queue_peek(queue, &output);
        require_condition(
            result == ((*model_count == 0u) ? UT_EVENT_EMPTY : UT_EVENT_OK));
        break;
    default:
        require_condition(ut_event_queue_clear(queue) == UT_EVENT_OK);
        *model_count = 0u;
        break;
    }

    require_condition(ut_event_queue_count(queue) == *model_count);
    require_condition(*model_count <= FUZZ_QUEUE_CAPACITY);
}

/**
 * 1件の4 byte命令をDispatcher操作として実行し、参照modelと照合する。
 *
 * @param dispatcher 操作対象Dispatcher。
 * @param handler_states handlerごとの観測状態。
 * @param registered Event IDとhandlerの登録状態model。
 * @param model_count 参照model上のsubscription件数。
 * @param command 4 byte以上の命令列。
 */
static void fuzz_dispatcher_operation(
    ut_event_dispatcher_t *dispatcher,
    fuzz_handler_state_t *handler_states,
    uint8_t registered[FUZZ_HANDLER_COUNT][FUZZ_EVENT_ID_COUNT],
    size_t *model_count,
    const uint8_t *command)
{
    const size_t handler_index = command[2] % FUZZ_HANDLER_COUNT;
    const uint32_t event_id = command[1] % FUZZ_EVENT_ID_COUNT;
    const ut_event_t event = {event_id, command[3], NULL, 0u};
    ut_event_result_t result;
    size_t handler_count;

    switch (command[0] % 3u) {
    case 0u:
        result = ut_event_subscribe(
            dispatcher,
            event_id,
            fuzz_handler,
            &handler_states[handler_index]);
        if (registered[handler_index][event_id] != 0u) {
            require_condition(result == UT_EVENT_ALREADY_EXISTS);
        } else if (*model_count == FUZZ_SUBSCRIPTION_CAPACITY) {
            require_condition(result == UT_EVENT_FULL);
        } else {
            require_condition(result == UT_EVENT_OK);
            registered[handler_index][event_id] = 1u;
            (*model_count)++;
        }
        break;
    case 1u:
        result = ut_event_unsubscribe(
            dispatcher,
            event_id,
            fuzz_handler,
            &handler_states[handler_index]);
        if (registered[handler_index][event_id] == 0u) {
            require_condition(result == UT_EVENT_NOT_FOUND);
        } else {
            require_condition(result == UT_EVENT_OK);
            registered[handler_index][event_id] = 0u;
            (*model_count)--;
        }
        break;
    default:
        handler_count = 0u;
        result = ut_event_dispatch(dispatcher, &event, &handler_count);
        require_condition(
            ((result == UT_EVENT_OK) && (handler_count > 0u)) ||
            ((result == UT_EVENT_NOT_FOUND) && (handler_count == 0u)));
        break;
    }

    require_condition(
        ut_event_subscription_count(dispatcher) == *model_count);
    require_condition(*model_count <= FUZZ_SUBSCRIPTION_CAPACITY);
}

/**
 * Timer slot内のactive件数を数える。
 *
 * @param slots Timer slot配列。
 * @return active slot件数。
 */
static size_t count_active_timers(
    const ut_event_timer_slot_t slots[FUZZ_TIMER_CAPACITY])
{
    size_t index;
    size_t count = 0u;

    for (index = 0u; index < FUZZ_TIMER_CAPACITY; index++) {
        if (slots[index].active != 0u) {
            count++;
        }
    }
    return count;
}

/**
 * 1件の4 byte命令をTimer操作として実行し、不変条件を検査する。
 *
 * @param scheduler 操作対象Timer Scheduler。
 * @param slots Timer slot配列。
 * @param queue Timer Event発行先Queue。
 * @param command 4 byte以上の命令列。
 */
static void fuzz_timer_operation(
    ut_event_timer_scheduler_t *scheduler,
    ut_event_timer_slot_t slots[FUZZ_TIMER_CAPACITY],
    ut_event_queue_t *queue,
    const uint8_t *command)
{
    const ut_event_timer_id_t timer_id =
        command[1] % FUZZ_TIMER_CAPACITY;
    const ut_event_t event = {
        command[2] % FUZZ_EVENT_ID_COUNT,
        command[3],
        NULL,
        0u
    };
    const uint64_t now = command[3];
    const uint64_t delay = command[2];
    const uint64_t period = command[1] & 0x07u;
    ut_event_result_t result;

    switch ((command[0] >> 2u) % 4u) {
    case 0u:
        result = ut_event_timer_start(
            scheduler,
            timer_id,
            &event,
            now,
            delay,
            period);
        require_condition(
            (result == UT_EVENT_OK) ||
            (result == UT_EVENT_ALREADY_EXISTS) ||
            (result == UT_EVENT_FULL));
        break;
    case 1u:
        result = ut_event_timer_restart(
            scheduler,
            timer_id,
            &event,
            now,
            delay,
            period);
        require_condition(
            (result == UT_EVENT_OK) || (result == UT_EVENT_NOT_FOUND));
        break;
    case 2u:
        result = ut_event_timer_cancel(scheduler, timer_id);
        require_condition(
            (result == UT_EVENT_OK) || (result == UT_EVENT_NOT_FOUND));
        break;
    default:
        result = ut_event_timer_process(scheduler, now, queue, NULL);
        require_condition(
            (result == UT_EVENT_OK) || (result == UT_EVENT_FULL));
        require_condition(ut_event_queue_clear(queue) == UT_EVENT_OK);
        break;
    }

    require_condition(
        ut_event_timer_count(scheduler) == count_active_timers(slots));
    require_condition(
        ut_event_timer_count(scheduler) <= FUZZ_TIMER_CAPACITY);
}

/**
 * libFuzzer互換のEvent Utility fuzz入口。
 *
 * @param data 任意入力byte列。
 * @param size dataのbyte数。
 * @return 常に0。不変条件違反時はabortする。
 */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    ut_event_t queue_storage[FUZZ_QUEUE_CAPACITY];
    ut_event_subscription_t
        subscription_storage[FUZZ_SUBSCRIPTION_CAPACITY];
    ut_event_queue_t queue;
    ut_event_t timer_queue_storage[FUZZ_TIMER_QUEUE_CAPACITY];
    ut_event_queue_t timer_queue;
    ut_event_dispatcher_t dispatcher;
    ut_event_timer_slot_t timer_storage[FUZZ_TIMER_CAPACITY];
    ut_event_timer_scheduler_t timer_scheduler;
    fuzz_handler_state_t handler_states[FUZZ_HANDLER_COUNT] = {{0}};
    uint8_t registered[FUZZ_HANDLER_COUNT][FUZZ_EVENT_ID_COUNT] = {{0}};
    size_t queue_count = 0u;
    size_t subscription_count = 0u;
    size_t offset;

    require_condition(ut_event_queue_init(
        &queue, queue_storage, FUZZ_QUEUE_CAPACITY) == UT_EVENT_OK);
    require_condition(ut_event_dispatcher_init(
        &dispatcher,
        subscription_storage,
        FUZZ_SUBSCRIPTION_CAPACITY) == UT_EVENT_OK);
    require_condition(ut_event_queue_init(
        &timer_queue,
        timer_queue_storage,
        FUZZ_TIMER_QUEUE_CAPACITY) == UT_EVENT_OK);
    require_condition(ut_event_timer_scheduler_init(
        &timer_scheduler,
        timer_storage,
        FUZZ_TIMER_CAPACITY) == UT_EVENT_OK);

    for (offset = 0u; (offset + 4u) <= size; offset += 4u) {
        const uint8_t *command = &data[offset];

        if ((command[0] % 3u) == 0u) {
            fuzz_queue_operation(&queue, &queue_count, command);
        } else if ((command[0] % 3u) == 1u) {
            fuzz_dispatcher_operation(
                &dispatcher,
                handler_states,
                registered,
                &subscription_count,
                command);
        } else {
            fuzz_timer_operation(
                &timer_scheduler,
                timer_storage,
                &timer_queue,
                command);
        }
    }

    return 0;
}
