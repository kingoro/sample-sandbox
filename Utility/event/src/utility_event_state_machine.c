/**
 * @file utility_event_state_machine.c
 * @brief table-driven State Machineの実装。
 */
#include "utility_event_state_machine.h"

#include <string.h>

/**
 * state table内で状態IDを探索する。
 *
 * @param states 状態定義table。
 * @param count 状態定義数。
 * @param state_id 探索する状態ID。
 * @return 見つかった状態定義。見つからない場合NULL。
 */
static const ut_event_state_t *find_state(
    const ut_event_state_t *states,
    size_t count,
    uint32_t state_id)
{
    size_t index;

    for (index = 0u; index < count; index++) {
        if (states[index].id == state_id) {
            return &states[index];
        }
    }
    return NULL;
}

/**
 * State Machine contextの内部不変条件を検査する。
 *
 * @param machine 検査するState Machine context。
 * @return 利用可能なら真、それ以外は偽。
 */
static int machine_is_valid(const ut_event_state_machine_t *machine)
{
    return (machine != NULL) &&
        (machine->states != NULL) &&
        (machine->state_count > 0u) &&
        (machine->transitions != NULL) &&
        (machine->transition_count > 0u) &&
        (find_state(
            machine->states,
            machine->state_count,
            machine->current_state) != NULL);
}

/**
 * state tableの重複と予約ID使用を検査する。
 *
 * @param states 状態定義table。
 * @param count 状態定義数。
 * @return 有効なら真、それ以外は偽。
 */
static int states_are_valid(const ut_event_state_t *states, size_t count)
{
    size_t outer;
    size_t inner;

    if ((states == NULL) || (count == 0u)) {
        return 0;
    }
    for (outer = 0u; outer < count; outer++) {
        if (states[outer].id == UT_EVENT_STATE_ID_ANY) {
            return 0;
        }
        for (inner = outer + 1u; inner < count; inner++) {
            if (states[outer].id == states[inner].id) {
                return 0;
            }
        }
    }
    return 1;
}

/**
 * transition tableがstate tableだけを参照しているか検査する。
 *
 * @param states 状態定義table。
 * @param state_count 状態定義数。
 * @param transitions 遷移定義table。
 * @param transition_count 遷移定義数。
 * @return 有効なら真、それ以外は偽。
 */
static int transitions_are_valid(
    const ut_event_state_t *states,
    size_t state_count,
    const ut_event_state_transition_t *transitions,
    size_t transition_count)
{
    size_t index;

    if ((transitions == NULL) || (transition_count == 0u)) {
        return 0;
    }
    for (index = 0u; index < transition_count; index++) {
        const uint32_t from_state = transitions[index].from_state;
        const uint32_t to_state = transitions[index].to_state;

        if ((from_state != UT_EVENT_STATE_ID_ANY) &&
            (find_state(states, state_count, from_state) == NULL)) {
            return 0;
        }
        if (find_state(states, state_count, to_state) == NULL) {
            return 0;
        }
    }
    return 1;
}

/**
 * 現在状態とEventに一致する遷移を登録順で探索する。
 *
 * @param machine 初期化済みState Machine context。
 * @param event 入力Event。
 * @return 最初に一致しguardを通過した遷移。見つからない場合NULL。
 */
static const ut_event_state_transition_t *find_transition(
    const ut_event_state_machine_t *machine,
    const ut_event_t *event)
{
    size_t index;

    for (index = 0u; index < machine->transition_count; index++) {
        const ut_event_state_transition_t *transition =
            &machine->transitions[index];
        const int state_matches =
            (transition->from_state == machine->current_state) ||
            (transition->from_state == UT_EVENT_STATE_ID_ANY);
        const int event_matches =
            (transition->event_id == event->id) ||
            (transition->event_id == UT_EVENT_ID_ANY);

        if (state_matches && event_matches &&
            ((transition->guard == NULL) ||
                transition->guard(machine, event, transition->user_context))) {
            return transition;
        }
    }
    return NULL;
}

/**
 * 状態遷移traceを設定済みの場合だけ出力する。
 *
 * @param machine 初期化済みState Machine context。
 * @param from_state 遷移前状態ID。
 * @param to_state 遷移後状態ID。
 * @param reason 遷移理由code。
 */
static void trace_transition(
    ut_event_state_machine_t *machine,
    uint32_t from_state,
    uint32_t to_state,
    uint32_t reason)
{
    if (machine->trace != NULL) {
        (void)ut_event_trace_state_transition(
            machine->trace,
            machine->source,
            from_state,
            to_state,
            reason);
    }
}

ut_event_result_t ut_event_state_machine_init(
    ut_event_state_machine_t *machine,
    const ut_event_state_t *states,
    size_t state_count,
    const ut_event_state_transition_t *transitions,
    size_t transition_count,
    uint32_t initial_state,
    uint32_t source,
    void *user_context)
{
    if ((machine == NULL) ||
        !states_are_valid(states, state_count) ||
        !transitions_are_valid(
            states,
            state_count,
            transitions,
            transition_count) ||
        (find_state(states, state_count, initial_state) == NULL)) {
        return UT_EVENT_INVALID_ARGUMENT;
    }

    (void)memset(machine, 0, sizeof(*machine));
    machine->states = states;
    machine->state_count = state_count;
    machine->transitions = transitions;
    machine->transition_count = transition_count;
    machine->current_state = initial_state;
    machine->source = source;
    machine->user_context = user_context;
    return UT_EVENT_OK;
}

ut_event_result_t ut_event_state_machine_set_trace(
    ut_event_state_machine_t *machine,
    ut_event_trace_t *trace)
{
    if (!machine_is_valid(machine)) {
        return UT_EVENT_INVALID_ARGUMENT;
    }
    if (machine->dispatching != 0u) {
        return UT_EVENT_BUSY;
    }
    machine->trace = trace;
    return UT_EVENT_OK;
}

ut_event_result_t ut_event_state_machine_dispatch(
    ut_event_state_machine_t *machine,
    const ut_event_t *event,
    uint32_t *out_new_state)
{
    const ut_event_state_transition_t *transition;
    const ut_event_state_t *from_state;
    const ut_event_state_t *to_state;
    const uint32_t previous_state =
        (machine != NULL) ? machine->current_state : UT_EVENT_STATE_ID_ANY;
    int internal_transition;

    if (!machine_is_valid(machine) || (event == NULL)) {
        return UT_EVENT_INVALID_ARGUMENT;
    }
    if (machine->dispatching != 0u) {
        return UT_EVENT_BUSY;
    }

    transition = find_transition(machine, event);
    if (transition == NULL) {
        return UT_EVENT_NOT_FOUND;
    }
    internal_transition =
        (transition->options & UT_EVENT_STATE_TRANSITION_INTERNAL) != 0u;

    from_state = find_state(machine->states, machine->state_count, previous_state);
    to_state = find_state(machine->states, machine->state_count, transition->to_state);
    if ((from_state == NULL) || (to_state == NULL)) {
        return UT_EVENT_INVALID_ARGUMENT;
    }

    machine->dispatching = 1u;
    if (!internal_transition && (from_state->exit != NULL)) {
        from_state->exit(machine, event, from_state->user_context);
    }
    if (transition->action != NULL) {
        transition->action(machine, event, transition->user_context);
    }
    machine->current_state = transition->to_state;
    if (!internal_transition && (to_state->entry != NULL)) {
        to_state->entry(machine, event, to_state->user_context);
    }
    machine->dispatching = 0u;

    trace_transition(
        machine,
        previous_state,
        machine->current_state,
        transition->reason);
    if (out_new_state != NULL) {
        *out_new_state = machine->current_state;
    }
    return UT_EVENT_OK;
}

uint32_t ut_event_state_machine_current_state(
    const ut_event_state_machine_t *machine)
{
    return machine_is_valid(machine) ?
        machine->current_state :
        UT_EVENT_STATE_ID_ANY;
}

void *ut_event_state_machine_context(
    const ut_event_state_machine_t *machine)
{
    return machine_is_valid(machine) ? machine->user_context : NULL;
}
