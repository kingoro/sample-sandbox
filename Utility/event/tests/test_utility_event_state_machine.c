/**
 * @file test_utility_event_state_machine.c
 * @brief Event State Machineの遷移、hook、guard、trace単体テスト。
 */
#include "utility_event.h"

#include "test_cases.h"
#include "test_support.h"

/** テスト用状態ID。 */
enum {
    /** 待機状態。 */
    TEST_STATE_IDLE = 1u,
    /** 実行状態。 */
    TEST_STATE_RUNNING = 2u,
    /** fault状態。 */
    TEST_STATE_FAULT = 3u
};

/** テスト用Event ID。 */
enum {
    /** 開始Event。 */
    TEST_EVENT_START = 10u,
    /** 停止Event。 */
    TEST_EVENT_STOP = 11u,
    /** fault Event。 */
    TEST_EVENT_FAULT = 12u,
    /** 無視されるEvent。 */
    TEST_EVENT_IGNORE = 13u
};

/** hookとtraceの観測状態。 */
typedef struct state_machine_test_context {
    /** entry callback呼出回数。 */
    size_t entry_count;
    /** exit callback呼出回数。 */
    size_t exit_count;
    /** action callback呼出回数。 */
    size_t action_count;
    /** guard callback呼出回数。 */
    size_t guard_count;
    /** guardが許可を返す場合true。 */
    bool guard_allows;
    /** action内で観測したmachine context。 */
    void *machine_context_seen;
    /** action内で試みた再帰dispatchの結果。 */
    ut_event_result_t nested_result;
    /** trace sink呼出回数。 */
    size_t trace_count;
    /** 最後に受信したtrace record。 */
    ut_event_trace_record_t last_trace;
} state_machine_test_context_t;

/**
 * entry callback呼出を記録する。
 *
 * @param machine 実行中のState Machine。
 * @param event 遷移を発生させたEvent。
 * @param user_context state_machine_test_context_tへのpointer。
 */
static void record_entry(
    ut_event_state_machine_t *machine,
    const ut_event_t *event,
    void *user_context)
{
    state_machine_test_context_t *context =
        (state_machine_test_context_t *)user_context;

    (void)machine;
    (void)event;
    context->entry_count++;
}

/**
 * exit callback呼出を記録する。
 *
 * @param machine 実行中のState Machine。
 * @param event 遷移を発生させたEvent。
 * @param user_context state_machine_test_context_tへのpointer。
 */
static void record_exit(
    ut_event_state_machine_t *machine,
    const ut_event_t *event,
    void *user_context)
{
    state_machine_test_context_t *context =
        (state_machine_test_context_t *)user_context;

    (void)machine;
    (void)event;
    context->exit_count++;
}

/**
 * guard callback呼出を記録して許可可否を返す。
 *
 * @param machine 実行中のState Machine。
 * @param event 入力Event。
 * @param user_context state_machine_test_context_tへのpointer。
 * @return contextのguard_allows。
 */
static bool guard_from_context(
    const ut_event_state_machine_t *machine,
    const ut_event_t *event,
    void *user_context)
{
    state_machine_test_context_t *context =
        (state_machine_test_context_t *)user_context;

    (void)machine;
    (void)event;
    context->guard_count++;
    return context->guard_allows;
}

/**
 * action callback呼出とmachine contextを記録する。
 *
 * @param machine 実行中のState Machine。
 * @param event 遷移を発生させたEvent。
 * @param user_context state_machine_test_context_tへのpointer。
 */
static void record_action(
    ut_event_state_machine_t *machine,
    const ut_event_t *event,
    void *user_context)
{
    state_machine_test_context_t *context =
        (state_machine_test_context_t *)user_context;

    (void)event;
    context->action_count++;
    context->machine_context_seen = ut_event_state_machine_context(machine);
}

/**
 * action内から再帰dispatchを試みる。
 *
 * @param machine 実行中のState Machine。
 * @param event 遷移を発生させたEvent。
 * @param user_context state_machine_test_context_tへのpointer。
 */
static void try_nested_dispatch(
    ut_event_state_machine_t *machine,
    const ut_event_t *event,
    void *user_context)
{
    state_machine_test_context_t *context =
        (state_machine_test_context_t *)user_context;

    context->nested_result =
        ut_event_state_machine_dispatch(machine, event, NULL);
}

/**
 * trace recordをcopyする。
 *
 * @param record 受信したtrace record。
 * @param user_context state_machine_test_context_tへのpointer。
 */
static void capture_state_trace(
    const ut_event_trace_record_t *record,
    void *user_context)
{
    state_machine_test_context_t *context =
        (state_machine_test_context_t *)user_context;

    context->trace_count++;
    context->last_trace = *record;
}

/**
 * 基本遷移、hook順序、context、traceを検証する。
 *
 * @return 成功時は0、失敗時は1。
 */
static int test_transition_hooks_context_and_trace(void)
{
    state_machine_test_context_t context = {0};
    ut_event_state_machine_t machine;
    ut_event_trace_t trace;
    const ut_event_state_t states[] = {
        {TEST_STATE_IDLE, "idle", record_entry, record_exit, &context},
        {TEST_STATE_RUNNING, "running", record_entry, record_exit, &context},
        {TEST_STATE_FAULT, "fault", NULL, NULL, &context}
    };
    const ut_event_state_transition_t transitions[] = {
        {
            TEST_STATE_IDLE,
            TEST_EVENT_START,
            TEST_STATE_RUNNING,
            guard_from_context,
            record_action,
            &context,
            0u,
            100u
        },
        {
            TEST_STATE_RUNNING,
            TEST_EVENT_STOP,
            TEST_STATE_IDLE,
            NULL,
            NULL,
            NULL,
            0u,
            101u
        }
    };
    const ut_event_t event = {TEST_EVENT_START, 7u, NULL, 0u};
    uint32_t new_state = 0u;

    context.guard_allows = true;
    CHECK(ut_event_trace_init(&trace, capture_state_trace, &context) ==
        UT_EVENT_OK);
    CHECK(ut_event_state_machine_init(
        &machine,
        states,
        3u,
        transitions,
        2u,
        TEST_STATE_IDLE,
        9u,
        &context) == UT_EVENT_OK);
    CHECK(ut_event_state_machine_set_trace(&machine, &trace) == UT_EVENT_OK);

    CHECK(ut_event_state_machine_dispatch(&machine, &event, &new_state) ==
        UT_EVENT_OK);
    CHECK(new_state == TEST_STATE_RUNNING);
    CHECK(ut_event_state_machine_current_state(&machine) ==
        TEST_STATE_RUNNING);
    CHECK(context.guard_count == 1u);
    CHECK(context.exit_count == 1u);
    CHECK(context.action_count == 1u);
    CHECK(context.entry_count == 1u);
    CHECK(context.machine_context_seen == &context);
    CHECK(context.trace_count == 1u);
    CHECK(context.last_trace.kind == UT_EVENT_TRACE_KIND_STATE_TRANSITION);
    CHECK(context.last_trace.source == 9u);
    CHECK(context.last_trace.from_state == TEST_STATE_IDLE);
    CHECK(context.last_trace.to_state == TEST_STATE_RUNNING);
    CHECK(context.last_trace.reason == 100u);
    return 0;
}

/**
 * guard拒否、遷移未発見、内部遷移を検証する。
 *
 * @return 成功時は0、失敗時は1。
 */
static int test_guard_not_found_and_internal_transition(void)
{
    state_machine_test_context_t context = {0};
    ut_event_state_machine_t machine;
    const ut_event_state_t states[] = {
        {TEST_STATE_IDLE, "idle", record_entry, record_exit, &context},
        {TEST_STATE_RUNNING, "running", record_entry, record_exit, &context}
    };
    const ut_event_state_transition_t transitions[] = {
        {
            TEST_STATE_IDLE,
            TEST_EVENT_START,
            TEST_STATE_RUNNING,
            guard_from_context,
            record_action,
            &context,
            0u,
            0u
        },
        {
            TEST_STATE_IDLE,
            TEST_EVENT_IGNORE,
            TEST_STATE_IDLE,
            NULL,
            record_action,
            &context,
            UT_EVENT_STATE_TRANSITION_INTERNAL,
            0u
        }
    };
    const ut_event_t start = {TEST_EVENT_START, 0u, NULL, 0u};
    const ut_event_t ignore = {TEST_EVENT_IGNORE, 0u, NULL, 0u};
    const ut_event_t stop = {TEST_EVENT_STOP, 0u, NULL, 0u};

    CHECK(ut_event_state_machine_init(
        &machine,
        states,
        2u,
        transitions,
        2u,
        TEST_STATE_IDLE,
        0u,
        &context) == UT_EVENT_OK);
    CHECK(ut_event_state_machine_dispatch(&machine, &start, NULL) ==
        UT_EVENT_NOT_FOUND);
    CHECK(context.guard_count == 1u);
    CHECK(context.action_count == 0u);
    CHECK(ut_event_state_machine_current_state(&machine) == TEST_STATE_IDLE);

    CHECK(ut_event_state_machine_dispatch(&machine, &stop, NULL) ==
        UT_EVENT_NOT_FOUND);
    CHECK(ut_event_state_machine_dispatch(&machine, &ignore, NULL) ==
        UT_EVENT_OK);
    CHECK(context.action_count == 1u);
    CHECK(context.entry_count == 0u);
    CHECK(context.exit_count == 0u);
    CHECK(ut_event_state_machine_current_state(&machine) == TEST_STATE_IDLE);
    return 0;
}

/**
 * ANY遷移と再入拒否を検証する。
 *
 * @return 成功時は0、失敗時は1。
 */
static int test_any_transition_and_reentrancy(void)
{
    state_machine_test_context_t context = {0};
    ut_event_state_machine_t machine;
    const ut_event_state_t states[] = {
        {TEST_STATE_IDLE, "idle", NULL, NULL, &context},
        {TEST_STATE_FAULT, "fault", NULL, NULL, &context}
    };
    const ut_event_state_transition_t transitions[] = {
        {
            UT_EVENT_STATE_ID_ANY,
            TEST_EVENT_FAULT,
            TEST_STATE_FAULT,
            NULL,
            try_nested_dispatch,
            &context,
            0u,
            0u
        }
    };
    const ut_event_t event = {TEST_EVENT_FAULT, 0u, NULL, 0u};

    CHECK(ut_event_state_machine_init(
        &machine,
        states,
        2u,
        transitions,
        1u,
        TEST_STATE_IDLE,
        0u,
        &context) == UT_EVENT_OK);
    CHECK(ut_event_state_machine_dispatch(&machine, &event, NULL) ==
        UT_EVENT_OK);
    CHECK(context.nested_result == UT_EVENT_BUSY);
    CHECK(ut_event_state_machine_current_state(&machine) == TEST_STATE_FAULT);
    return 0;
}

/**
 * initとAPIの引数不正を検証する。
 *
 * @return 成功時は0、失敗時は1。
 */
static int test_invalid_arguments(void)
{
    ut_event_state_machine_t machine = {0};
    const ut_event_state_t duplicate_states[] = {
        {TEST_STATE_IDLE, "idle", NULL, NULL, NULL},
        {TEST_STATE_IDLE, "idle2", NULL, NULL, NULL}
    };
    const ut_event_state_t states[] = {
        {TEST_STATE_IDLE, "idle", NULL, NULL, NULL},
        {TEST_STATE_RUNNING, "running", NULL, NULL, NULL}
    };
    const ut_event_state_transition_t transitions[] = {
        {
            TEST_STATE_IDLE,
            TEST_EVENT_START,
            TEST_STATE_RUNNING,
            NULL,
            NULL,
            NULL,
            0u,
            0u
        }
    };
    const ut_event_state_transition_t bad_transition[] = {
        {
            TEST_STATE_IDLE,
            TEST_EVENT_START,
            TEST_STATE_FAULT,
            NULL,
            NULL,
            NULL,
            0u,
            0u
        }
    };
    const ut_event_t event = {TEST_EVENT_START, 0u, NULL, 0u};

    CHECK(ut_event_state_machine_init(
        NULL,
        states,
        2u,
        transitions,
        1u,
        TEST_STATE_IDLE,
        0u,
        NULL) == UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_state_machine_init(
        &machine,
        duplicate_states,
        2u,
        transitions,
        1u,
        TEST_STATE_IDLE,
        0u,
        NULL) == UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_state_machine_init(
        &machine,
        states,
        2u,
        bad_transition,
        1u,
        TEST_STATE_IDLE,
        0u,
        NULL) == UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_state_machine_init(
        &machine,
        states,
        2u,
        transitions,
        1u,
        TEST_STATE_FAULT,
        0u,
        NULL) == UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_state_machine_dispatch(&machine, &event, NULL) ==
        UT_EVENT_INVALID_ARGUMENT);

    CHECK(ut_event_state_machine_init(
        &machine,
        states,
        2u,
        transitions,
        1u,
        TEST_STATE_IDLE,
        0u,
        NULL) == UT_EVENT_OK);
    CHECK(ut_event_state_machine_dispatch(NULL, &event, NULL) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_state_machine_dispatch(&machine, NULL, NULL) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_state_machine_current_state(NULL) == UT_EVENT_STATE_ID_ANY);
    CHECK(ut_event_state_machine_context(NULL) == NULL);
    machine.dispatching = 1u;
    CHECK(ut_event_state_machine_set_trace(&machine, NULL) == UT_EVENT_BUSY);
    CHECK(ut_event_state_machine_dispatch(&machine, &event, NULL) ==
        UT_EVENT_BUSY);
    return 0;
}

int run_utility_event_state_machine_tests(void)
{
    CHECK(test_transition_hooks_context_and_trace() == 0);
    CHECK(test_guard_not_found_and_internal_transition() == 0);
    CHECK(test_any_transition_and_reentrancy() == 0);
    CHECK(test_invalid_arguments() == 0);
    return 0;
}
