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
/** Fuzz用Buffer Poolのhandle容量。 */
#define FUZZ_BUFFER_CAPACITY 4u
/** Fuzz用Buffer Envelope Queue容量。 */
#define FUZZ_BUFFER_QUEUE_CAPACITY 4u
/** Fuzz対象Executor Queue容量。 */
#define FUZZ_EXECUTOR_QUEUE_CAPACITY 4u

/** Fuzz handlerの観測状態。 */
typedef struct fuzz_handler_state {
    /** handler呼出回数。 */
    size_t calls;
    /** 最後に受信したEvent ID。 */
    uint32_t last_event_id;
} fuzz_handler_state_t;

/** Fuzz用の最小Buffer Pool model。 */
typedef struct fuzz_buffer_pool {
    /** handleごとの1 byte payload。 */
    uint8_t bytes[FUZZ_BUFFER_CAPACITY];
    /** handleごとの割当状態。 */
    uint8_t allocated[FUZZ_BUFFER_CAPACITY];
} fuzz_buffer_pool_t;

/**
 * Fuzz Poolから1 byte領域を割り当てる。
 *
 * @param pool_context fuzz_buffer_pool_tへのpointer。
 * @param capacity 要求容量。
 * @param out_handle handle格納先。
 * @return 成功時UT_EVENT_OK、空きなしならUT_EVENT_FULL。
 */
static ut_event_result_t fuzz_buffer_alloc(
    void *pool_context,
    size_t capacity,
    ut_event_buffer_handle_t *out_handle)
{
    fuzz_buffer_pool_t *pool = (fuzz_buffer_pool_t *)pool_context;
    size_t index;

    if ((pool == NULL) || (out_handle == NULL) || (capacity != 1u)) {
        return UT_EVENT_INVALID_ARGUMENT;
    }
    for (index = 0u; index < FUZZ_BUFFER_CAPACITY; index++) {
        if (pool->allocated[index] == 0u) {
            pool->allocated[index] = 1u;
            *out_handle = (ut_event_buffer_handle_t)(index + 1u);
            return UT_EVENT_OK;
        }
    }
    return UT_EVENT_FULL;
}

/**
 * Fuzz Poolのhandleを解放する。
 *
 * @param pool_context fuzz_buffer_pool_tへのpointer。
 * @param handle 解放handle。
 * @return 成功時UT_EVENT_OK、それ以外はUT_EVENT_INVALID_ARGUMENT。
 */
static ut_event_result_t fuzz_buffer_free(
    void *pool_context,
    ut_event_buffer_handle_t handle)
{
    fuzz_buffer_pool_t *pool = (fuzz_buffer_pool_t *)pool_context;
    const size_t index = (size_t)handle - 1u;

    if ((pool == NULL) || (handle == 0u) ||
        (index >= FUZZ_BUFFER_CAPACITY) ||
        (pool->allocated[index] == 0u)) {
        return UT_EVENT_INVALID_ARGUMENT;
    }
    pool->allocated[index] = 0u;
    return UT_EVENT_OK;
}

/**
 * Fuzz Poolへ1 byteを書き込む。
 *
 * @param pool_context fuzz_buffer_pool_tへのpointer。
 * @param handle 書込みhandle。
 * @param offset 書込みoffset。
 * @param source copy元。
 * @param length copy長。
 * @return 成功時UT_EVENT_OK、それ以外はUT_EVENT_INVALID_ARGUMENT。
 */
static ut_event_result_t fuzz_buffer_write(
    void *pool_context,
    ut_event_buffer_handle_t handle,
    size_t offset,
    const uint8_t *source,
    size_t length)
{
    fuzz_buffer_pool_t *pool = (fuzz_buffer_pool_t *)pool_context;
    const size_t index = (size_t)handle - 1u;

    if ((pool == NULL) || (handle == 0u) ||
        (index >= FUZZ_BUFFER_CAPACITY) ||
        (pool->allocated[index] == 0u) ||
        (source == NULL) || (offset != 0u) || (length != 1u)) {
        return UT_EVENT_INVALID_ARGUMENT;
    }
    pool->bytes[index] = source[0];
    return UT_EVENT_OK;
}

/**
 * Fuzz Poolから1 byteを読み取る。
 *
 * @param pool_context fuzz_buffer_pool_tへのpointer。
 * @param handle 読取りhandle。
 * @param offset 読取りoffset。
 * @param destination copy先。
 * @param length copy長。
 * @return 成功時UT_EVENT_OK、それ以外はUT_EVENT_INVALID_ARGUMENT。
 */
static ut_event_result_t fuzz_buffer_read(
    void *pool_context,
    ut_event_buffer_handle_t handle,
    size_t offset,
    uint8_t *destination,
    size_t length)
{
    fuzz_buffer_pool_t *pool = (fuzz_buffer_pool_t *)pool_context;
    const size_t index = (size_t)handle - 1u;

    if ((pool == NULL) || (handle == 0u) ||
        (index >= FUZZ_BUFFER_CAPACITY) ||
        (pool->allocated[index] == 0u) ||
        (destination == NULL) || (offset != 0u) || (length != 1u)) {
        return UT_EVENT_INVALID_ARGUMENT;
    }
    destination[0] = pool->bytes[index];
    return UT_EVENT_OK;
}

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
 * Fuzz Poolの割当handle件数を数える。
 *
 * @param pool Fuzz Pool。
 * @return 割当件数。
 */
static size_t count_allocated_buffers(const fuzz_buffer_pool_t *pool)
{
    size_t index;
    size_t count = 0u;

    for (index = 0u; index < FUZZ_BUFFER_CAPACITY; index++) {
        if (pool->allocated[index] != 0u) {
            count++;
        }
    }
    return count;
}

/**
 * 1件の命令をBuffer Envelope操作として実行する。
 *
 * @param pool Buffer Pool操作table。
 * @param pool_model Pool状態model。
 * @param queue Buffer Envelope Queue。
 * @param producer producer所有Envelope。
 * @param consumer consumer所有Envelope。
 * @param command 4 byte以上の命令列。
 */
static void fuzz_buffer_operation(
    const ut_event_buffer_pool_t *pool,
    const fuzz_buffer_pool_t *pool_model,
    ut_event_buffer_queue_t *queue,
    ut_event_buffer_message_t *producer,
    ut_event_buffer_message_t *consumer,
    const uint8_t *command)
{
    ut_event_result_t result;
    const uint8_t value = command[3];

    switch ((command[0] >> 3u) % 5u) {
    case 0u:
        if (producer->owns_handle == 0u) {
            result = ut_event_buffer_message_create_copy(
                producer,
                pool,
                command[1],
                command[2],
                &value,
                1u);
            require_condition(
                (result == UT_EVENT_OK) || (result == UT_EVENT_FULL));
        }
        break;
    case 1u:
        if (producer->owns_handle != 0u) {
            result = ut_event_buffer_queue_push_move(queue, producer);
            require_condition(
                (result == UT_EVENT_OK) || (result == UT_EVENT_FULL));
        }
        break;
    case 2u:
        if (consumer->owns_handle == 0u) {
            result = ut_event_buffer_queue_pop_move(queue, consumer);
            require_condition(
                (result == UT_EVENT_OK) || (result == UT_EVENT_EMPTY));
        }
        break;
    case 3u:
        if (consumer->owns_handle != 0u) {
            require_condition(
                ut_event_buffer_message_release(consumer) == UT_EVENT_OK);
        } else if (producer->owns_handle != 0u) {
            require_condition(
                ut_event_buffer_message_release(producer) == UT_EVENT_OK);
        }
        break;
    default:
        require_condition(
            ut_event_buffer_queue_release_all(queue) == UT_EVENT_OK);
        break;
    }

    require_condition(
        count_allocated_buffers(pool_model) ==
        ut_event_buffer_queue_count(queue) +
            (size_t)(producer->owns_handle != 0u) +
            (size_t)(consumer->owns_handle != 0u));
}

/**
 * 1件の命令をExecutor、Contract、Metricsへ適用する。
 *
 * @param executor 操作対象Executor。
 * @param queue Executor入力Queue。
 * @param metrics Metrics収集先。
 * @param command 4 byte以上の命令列。
 */
static void fuzz_executor_operation(
    ut_event_executor_t *executor,
    ut_event_queue_t *queue,
    ut_event_metrics_t *metrics,
    const uint8_t *command)
{
    const ut_event_t event = {
        command[1] % (FUZZ_EVENT_ID_COUNT + 1u),
        command[2],
        NULL,
        0u
    };
    ut_event_metrics_t snapshot;
    ut_event_result_t result;

    switch ((command[0] >> 3u) % 3u) {
    case 0u:
        result = ut_event_queue_push(queue, &event);
        require_condition(
            (result == UT_EVENT_OK) || (result == UT_EVENT_FULL));
        require_condition(ut_event_metrics_record_publish(
            metrics,
            result,
            ut_event_queue_count(queue)) == UT_EVENT_OK);
        break;
    case 1u:
        result = ut_event_executor_run_once(
            executor,
            command[3],
            (size_t)(command[2] % FUZZ_EXECUTOR_QUEUE_CAPACITY) + 1u,
            NULL);
        require_condition(result == UT_EVENT_OK);
        break;
    default:
        require_condition(ut_event_metrics_snapshot(
            metrics, &snapshot) == UT_EVENT_OK);
        require_condition(
            snapshot.queue_high_watermark <= FUZZ_EXECUTOR_QUEUE_CAPACITY);
        break;
    }

    require_condition(
        ut_event_queue_count(queue) <= FUZZ_EXECUTOR_QUEUE_CAPACITY);
    require_condition(
        metrics->queue_high_watermark <= FUZZ_EXECUTOR_QUEUE_CAPACITY);
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
    fuzz_buffer_pool_t buffer_pool_model = {{0}, {0}};
    const ut_event_buffer_pool_t buffer_pool = {
        .context = &buffer_pool_model,
        .alloc = fuzz_buffer_alloc,
        .free = fuzz_buffer_free,
        .write = fuzz_buffer_write,
        .read = fuzz_buffer_read
    };
    ut_event_buffer_message_t
        buffer_queue_storage[FUZZ_BUFFER_QUEUE_CAPACITY];
    ut_event_buffer_queue_t buffer_queue;
    ut_event_buffer_message_t producer_message = {0};
    ut_event_buffer_message_t consumer_message = {0};
    ut_event_t executor_queue_storage[FUZZ_EXECUTOR_QUEUE_CAPACITY];
    ut_event_queue_t executor_queue;
    const ut_event_contract_t executor_contracts[] = {
        {0u, "event-0", 0u, 0u, UT_EVENT_PAYLOAD_NONE},
        {1u, "event-1", 0u, 0u, UT_EVENT_PAYLOAD_NONE},
        {2u, "event-2", 0u, 0u, UT_EVENT_PAYLOAD_NONE},
        {3u, "event-3", 0u, 0u, UT_EVENT_PAYLOAD_NONE}
    };
    ut_event_contract_registry_t contract_registry;
    ut_event_metrics_t metrics;
    ut_event_executor_t executor;
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
    require_condition(ut_event_buffer_queue_init(
        &buffer_queue,
        buffer_queue_storage,
        FUZZ_BUFFER_QUEUE_CAPACITY) == UT_EVENT_OK);
    require_condition(ut_event_queue_init(
        &executor_queue,
        executor_queue_storage,
        FUZZ_EXECUTOR_QUEUE_CAPACITY) == UT_EVENT_OK);
    require_condition(ut_event_contract_registry_init(
        &contract_registry,
        executor_contracts,
        FUZZ_EVENT_ID_COUNT) == UT_EVENT_OK);
    require_condition(ut_event_metrics_init(&metrics) == UT_EVENT_OK);
    require_condition(ut_event_executor_init(
        &executor,
        &executor_queue,
        &dispatcher,
        NULL,
        &contract_registry,
        &metrics) == UT_EVENT_OK);

    for (offset = 0u; (offset + 4u) <= size; offset += 4u) {
        const uint8_t *command = &data[offset];

        if ((command[0] % 5u) == 0u) {
            fuzz_queue_operation(&queue, &queue_count, command);
        } else if ((command[0] % 5u) == 1u) {
            fuzz_dispatcher_operation(
                &dispatcher,
                handler_states,
                registered,
                &subscription_count,
                command);
        } else if ((command[0] % 5u) == 2u) {
            fuzz_timer_operation(
                &timer_scheduler,
                timer_storage,
                &timer_queue,
                command);
        } else if ((command[0] % 5u) == 3u) {
            fuzz_buffer_operation(
                &buffer_pool,
                &buffer_pool_model,
                &buffer_queue,
                &producer_message,
                &consumer_message,
                command);
        } else {
            fuzz_executor_operation(
                &executor,
                &executor_queue,
                &metrics,
                command);
        }
    }

    if (producer_message.owns_handle != 0u) {
        require_condition(
            ut_event_buffer_message_release(&producer_message) == UT_EVENT_OK);
    }
    if (consumer_message.owns_handle != 0u) {
        require_condition(
            ut_event_buffer_message_release(&consumer_message) == UT_EVENT_OK);
    }
    require_condition(
        ut_event_buffer_queue_release_all(&buffer_queue) == UT_EVENT_OK);
    return 0;
}
