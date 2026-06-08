/**
 * @file test_utility_event_integration.c
 * @brief Queue、Dispatcher、State Machine、Trace、Logの結合シナリオテスト。
 */
#include "utility_event.h"

#include "test_support.h"

#include <string.h>

/** 結合シナリオのQueue容量。 */
#define INTEGRATION_QUEUE_CAPACITY 8u
/** 結合シナリオのsubscription容量。 */
#define INTEGRATION_SUBSCRIPTION_CAPACITY 2u
/** 結合シナリオのLog Ring容量。 */
#define INTEGRATION_LOG_CAPACITY 16u

/** 結合シナリオの状態ID。 */
enum {
    /** 待機状態。 */
    INTEGRATION_STATE_IDLE = 1u,
    /** 実行状態。 */
    INTEGRATION_STATE_RUNNING = 2u,
    /** fault状態。 */
    INTEGRATION_STATE_FAULT = 3u
};

/** 結合シナリオのEvent ID。 */
enum {
    /** 実行開始Event。 */
    INTEGRATION_EVENT_START = 10u,
    /** 状態遷移しない診断Event。 */
    INTEGRATION_EVENT_DIAGNOSTIC = 11u,
    /** fault遷移Event。 */
    INTEGRATION_EVENT_ERROR = 12u,
    /** fault解除Event。 */
    INTEGRATION_EVENT_RESET = 13u
};

/** シナリオ中のApplication観測状態。 */
typedef struct integration_context {
    /** RUNNING entry呼出回数。 */
    size_t running_entry_count;
    /** FAULT entry呼出回数。 */
    size_t fault_entry_count;
    /** reset action呼出回数。 */
    size_t reset_action_count;
    /** State MachineがEventを処理した回数。 */
    size_t state_dispatch_count;
    /** 遷移が見つからなかった回数。 */
    size_t state_not_found_count;
    /** 最後のState Machine dispatch結果。 */
    ut_event_result_t last_state_result;
} integration_context_t;

/**
 * RUNNING状態へのentryを記録する。
 *
 * @param machine 実行中のState Machine。
 * @param event 遷移を発生させたEvent。
 * @param user_context integration_context_tへのpointer。
 */
static void enter_running(
    ut_event_state_machine_t *machine,
    const ut_event_t *event,
    void *user_context)
{
    integration_context_t *context =
        (integration_context_t *)user_context;

    (void)machine;
    (void)event;
    context->running_entry_count++;
}

/**
 * FAULT状態へのentryを記録する。
 *
 * @param machine 実行中のState Machine。
 * @param event 遷移を発生させたEvent。
 * @param user_context integration_context_tへのpointer。
 */
static void enter_fault(
    ut_event_state_machine_t *machine,
    const ut_event_t *event,
    void *user_context)
{
    integration_context_t *context =
        (integration_context_t *)user_context;

    (void)machine;
    (void)event;
    context->fault_entry_count++;
}

/**
 * fault解除actionを記録する。
 *
 * @param machine 実行中のState Machine。
 * @param event 遷移を発生させたEvent。
 * @param user_context integration_context_tへのpointer。
 */
static void record_reset(
    ut_event_state_machine_t *machine,
    const ut_event_t *event,
    void *user_context)
{
    integration_context_t *context =
        (integration_context_t *)user_context;

    (void)machine;
    (void)event;
    context->reset_action_count++;
}

/**
 * DispatcherからState MachineへEventを渡すadapter。
 *
 * 遷移対象外EventはDispatcher全体の失敗にはせず、観測値として記録する。
 *
 * @param event Dispatcherから受信したEvent。
 * @param user_context ut_event_state_machine_tへのpointer。
 */
static void dispatch_to_state_machine(
    const ut_event_t *event,
    void *user_context)
{
    ut_event_state_machine_t *machine =
        (ut_event_state_machine_t *)user_context;
    integration_context_t *context =
        (integration_context_t *)ut_event_state_machine_context(machine);

    context->last_state_result =
        ut_event_state_machine_dispatch(machine, event, NULL);
    context->state_dispatch_count++;
    if (context->last_state_result == UT_EVENT_NOT_FOUND) {
        context->state_not_found_count++;
    }
}

/**
 * Log Ring内に指定文字列を含むmessageがあるか確認する。
 *
 * @param logger 検索するLogger。
 * @param text 検索する文字列。
 * @return 見つかった場合1、それ以外は0。
 */
static int log_contains(ut_logger_t *logger, const char *text)
{
    size_t index;

    for (index = 0u; index < ut_logger_count(logger); index++) {
        ut_log_record_t record;

        if ((ut_logger_read(logger, index, &record) == UT_LOG_OK) &&
            (strstr(record.message, text) != NULL)) {
            return 1;
        }
    }
    return 0;
}

/**
 * Event列をQueueからDispatcherへ配送する。
 *
 * @param queue Event入力Queue。
 * @param dispatcher 配送先Dispatcher。
 * @return 全Eventを配送できた場合0、それ以外は1。
 */
static int drain_event_queue(
    ut_event_queue_t *queue,
    ut_event_dispatcher_t *dispatcher)
{
    ut_event_t event;

    while (ut_event_queue_pop(queue, &event) == UT_EVENT_OK) {
        size_t handler_count = 0u;

        CHECK(ut_event_dispatch(dispatcher, &event, &handler_count) ==
            UT_EVENT_OK);
        CHECK(handler_count == 2u);
    }
    return 0;
}

/**
 * IDLEからRUNNING、FAULTを経てIDLEへ戻る結合シナリオを検証する。
 *
 * @return 成功時は0、失敗時は1。
 */
static int test_operational_fault_recovery_scenario(void)
{
    ut_event_t queue_storage[INTEGRATION_QUEUE_CAPACITY];
    ut_event_subscription_t
        subscription_storage[INTEGRATION_SUBSCRIPTION_CAPACITY];
    ut_log_record_t log_storage[INTEGRATION_LOG_CAPACITY];
    ut_event_queue_t queue;
    ut_event_dispatcher_t dispatcher;
    ut_event_state_machine_t machine;
    ut_event_trace_t state_trace;
    ut_event_trace_t event_trace;
    ut_logger_t logger;
    integration_context_t context = {0};
    ut_event_trace_log_config_t trace_log_config;
    const ut_logger_config_t logger_config = {
        .console_write = NULL,
        .console_context = NULL,
        .clock = NULL,
        .clock_context = NULL,
        .lock = NULL,
        .unlock = NULL,
        .lock_context = NULL,
        .console_level = UT_LOG_LEVEL_TRACE,
        .ring_level = UT_LOG_LEVEL_TRACE,
        .console_enabled = false,
        .ring_enabled = true
    };
    const ut_event_state_t states[] = {
        {INTEGRATION_STATE_IDLE, "idle", NULL, NULL, &context},
        {
            INTEGRATION_STATE_RUNNING,
            "running",
            enter_running,
            NULL,
            &context
        },
        {
            INTEGRATION_STATE_FAULT,
            "fault",
            enter_fault,
            NULL,
            &context
        }
    };
    const ut_event_state_transition_t transitions[] = {
        {
            INTEGRATION_STATE_IDLE,
            INTEGRATION_EVENT_START,
            INTEGRATION_STATE_RUNNING,
            NULL,
            NULL,
            NULL,
            0u,
            101u
        },
        {
            UT_EVENT_STATE_ID_ANY,
            INTEGRATION_EVENT_ERROR,
            INTEGRATION_STATE_FAULT,
            NULL,
            NULL,
            NULL,
            0u,
            102u
        },
        {
            INTEGRATION_STATE_FAULT,
            INTEGRATION_EVENT_RESET,
            INTEGRATION_STATE_IDLE,
            NULL,
            record_reset,
            &context,
            0u,
            103u
        }
    };
    const ut_event_t events[] = {
        {INTEGRATION_EVENT_START, 1u, NULL, 0u},
        {INTEGRATION_EVENT_DIAGNOSTIC, 2u, NULL, 0u},
        {INTEGRATION_EVENT_ERROR, 3u, NULL, 0u},
        {INTEGRATION_EVENT_RESET, 4u, NULL, 0u}
    };
    size_t index;

    CHECK(ut_event_queue_init(
        &queue,
        queue_storage,
        INTEGRATION_QUEUE_CAPACITY) == UT_EVENT_OK);
    CHECK(ut_event_dispatcher_init(
        &dispatcher,
        subscription_storage,
        INTEGRATION_SUBSCRIPTION_CAPACITY) == UT_EVENT_OK);
    CHECK(ut_logger_init(
        &logger,
        log_storage,
        INTEGRATION_LOG_CAPACITY,
        &logger_config) == UT_LOG_OK);

    trace_log_config.logger = &logger;
    trace_log_config.module = "scenario";
    trace_log_config.level = UT_LOG_LEVEL_TRACE;
    CHECK(ut_event_trace_init(
        &state_trace,
        ut_event_trace_log_sink,
        &trace_log_config) == UT_EVENT_OK);
    CHECK(ut_event_trace_init(
        &event_trace,
        ut_event_trace_log_sink,
        &trace_log_config) == UT_EVENT_OK);
    CHECK(ut_event_state_machine_init(
        &machine,
        states,
        3u,
        transitions,
        3u,
        INTEGRATION_STATE_IDLE,
        20u,
        &context) == UT_EVENT_OK);
    CHECK(ut_event_state_machine_set_trace(&machine, &state_trace) ==
        UT_EVENT_OK);

    CHECK(ut_event_subscribe(
        &dispatcher,
        UT_EVENT_ID_ANY,
        dispatch_to_state_machine,
        &machine) == UT_EVENT_OK);
    CHECK(ut_event_subscribe(
        &dispatcher,
        UT_EVENT_ID_ANY,
        ut_event_trace_dispatch_handler,
        &event_trace) == UT_EVENT_OK);

    for (index = 0u; index < (sizeof(events) / sizeof(events[0])); index++) {
        CHECK(ut_event_queue_push(&queue, &events[index]) == UT_EVENT_OK);
    }
    CHECK(drain_event_queue(&queue, &dispatcher) == 0);

    CHECK(ut_event_queue_count(&queue) == 0u);
    CHECK(context.state_dispatch_count == 4u);
    CHECK(context.state_not_found_count == 1u);
    CHECK(context.last_state_result == UT_EVENT_OK);
    CHECK(context.running_entry_count == 1u);
    CHECK(context.fault_entry_count == 1u);
    CHECK(context.reset_action_count == 1u);
    CHECK(ut_event_state_machine_current_state(&machine) ==
        INTEGRATION_STATE_IDLE);

    CHECK(ut_logger_count(&logger) == 7u);
    CHECK(log_contains(&logger, "event id=10") != 0);
    CHECK(log_contains(&logger, "event id=11") != 0);
    CHECK(log_contains(&logger, "event id=12") != 0);
    CHECK(log_contains(&logger, "event id=13") != 0);
    CHECK(log_contains(&logger, "state source=20 from=1 to=2 reason=101") != 0);
    CHECK(log_contains(&logger, "state source=20 from=2 to=3 reason=102") != 0);
    CHECK(log_contains(&logger, "state source=20 from=3 to=1 reason=103") != 0);
    return 0;
}

/**
 * Event Utility結合シナリオテストの実行入口。
 *
 * @return 全テスト成功時は0、失敗時は1。
 */
int main(void)
{
    CHECK(test_operational_fault_recovery_scenario() == 0);
    return 0;
}
