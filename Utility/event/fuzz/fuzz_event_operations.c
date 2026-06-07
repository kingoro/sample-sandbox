#include "utility_event.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define FUZZ_QUEUE_CAPACITY 8u
#define FUZZ_SUBSCRIPTION_CAPACITY 4u
#define FUZZ_HANDLER_COUNT 4u
#define FUZZ_EVENT_ID_COUNT 4u

typedef struct fuzz_handler_state {
    size_t calls;
    uint32_t last_event_id;
} fuzz_handler_state_t;

static void fuzz_handler(const ut_event_t *event, void *user_context)
{
    fuzz_handler_state_t *state = (fuzz_handler_state_t *)user_context;

    state->calls++;
    state->last_event_id = event->id;
}

static void require_condition(int condition)
{
    if (!condition) {
        abort();
    }
}

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

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    ut_event_t queue_storage[FUZZ_QUEUE_CAPACITY];
    ut_event_subscription_t
        subscription_storage[FUZZ_SUBSCRIPTION_CAPACITY];
    ut_event_queue_t queue;
    ut_event_dispatcher_t dispatcher;
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

    for (offset = 0u; (offset + 4u) <= size; offset += 4u) {
        const uint8_t *command = &data[offset];

        if ((command[0] & 0x80u) == 0u) {
            fuzz_queue_operation(&queue, &queue_count, command);
        } else {
            fuzz_dispatcher_operation(
                &dispatcher,
                handler_states,
                registered,
                &subscription_count,
                command);
        }
    }

    return 0;
}
