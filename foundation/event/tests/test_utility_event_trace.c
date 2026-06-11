/**
 * @file test_utility_event_trace.c
 * @brief Event traceとLog Foundation adapterの単体テスト。
 */
#include "utility_event.h"

#include "test_cases.h"
#include "test_support.h"

#include <string.h>

/** trace sinkの観測状態。 */
typedef struct trace_sink_state {
    /** sinkが呼び出された回数。 */
    size_t call_count;
    /** 最後に受信したtrace record。 */
    ut_event_trace_record_t last_record;
} trace_sink_state_t;

/**
 * trace recordをtrace_sink_state_tへcopyする。
 *
 * @param record 受信したtrace record。
 * @param user_context trace_sink_state_tへのpointer。
 */
static void capture_trace(
    const ut_event_trace_record_t *record,
    void *user_context)
{
    trace_sink_state_t *state = (trace_sink_state_t *)user_context;

    state->call_count++;
    state->last_record = *record;
}

/**
 * Event trace record生成とDispatcher hookを検証する。
 *
 * @return 成功時は0、失敗時は1。
 */
static int test_event_trace_and_dispatch_hook(void)
{
    ut_event_subscription_t storage[1];
    ut_event_dispatcher_t dispatcher;
    ut_event_trace_t trace = {0};
    trace_sink_state_t sink_state = {0};
    const ut_event_t event = {11u, 3u, NULL, 9u};

    CHECK(ut_event_trace_init(&trace, capture_trace, &sink_state) ==
        UT_EVENT_OK);
    CHECK(ut_event_trace_event(&trace, &event) == UT_EVENT_OK);
    CHECK(sink_state.call_count == 1u);
    CHECK(sink_state.last_record.kind == UT_EVENT_TRACE_KIND_EVENT);
    CHECK(sink_state.last_record.event_id == 11u);
    CHECK(sink_state.last_record.source == 3u);
    CHECK(sink_state.last_record.payload_size == 9u);
    CHECK(sink_state.last_record.from_state == UT_EVENT_TRACE_STATE_NONE);

    CHECK(ut_event_dispatcher_init(&dispatcher, storage, 1u) ==
        UT_EVENT_OK);
    CHECK(ut_event_subscribe(
        &dispatcher,
        UT_EVENT_ID_ANY,
        ut_event_trace_dispatch_handler,
        &trace) == UT_EVENT_OK);
    CHECK(ut_event_dispatch(&dispatcher, &event, NULL) == UT_EVENT_OK);
    CHECK(sink_state.call_count == 2u);
    CHECK(sink_state.last_record.event_id == 11u);
    return 0;
}

/**
 * 状態遷移trace record生成を検証する。
 *
 * @return 成功時は0、失敗時は1。
 */
static int test_state_transition_trace(void)
{
    ut_event_trace_t trace = {0};
    trace_sink_state_t sink_state = {0};

    CHECK(ut_event_trace_init(&trace, capture_trace, &sink_state) ==
        UT_EVENT_OK);
    CHECK(ut_event_trace_state_transition(&trace, 2u, 10u, 20u, 7u) ==
        UT_EVENT_OK);
    CHECK(sink_state.call_count == 1u);
    CHECK(sink_state.last_record.kind ==
        UT_EVENT_TRACE_KIND_STATE_TRANSITION);
    CHECK(sink_state.last_record.event_id == UT_EVENT_ID_ANY);
    CHECK(sink_state.last_record.source == 2u);
    CHECK(sink_state.last_record.from_state == 10u);
    CHECK(sink_state.last_record.to_state == 20u);
    CHECK(sink_state.last_record.reason == 7u);
    CHECK(sink_state.last_record.payload_size == 0u);
    return 0;
}

/**
 * trace APIの引数不正を検証する。
 *
 * @return 成功時は0、失敗時は1。
 */
static int test_trace_invalid_arguments(void)
{
    ut_event_trace_t trace = {0};
    trace_sink_state_t sink_state = {0};
    const ut_event_t event = {1u, 0u, NULL, 0u};

    CHECK(ut_event_trace_init(NULL, capture_trace, &sink_state) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_trace_init(&trace, NULL, &sink_state) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_trace_event(NULL, &event) == UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_trace_event(&trace, &event) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_trace_init(&trace, capture_trace, &sink_state) ==
        UT_EVENT_OK);
    CHECK(ut_event_trace_event(&trace, NULL) == UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_trace_state_transition(NULL, 0u, 0u, 1u, 0u) ==
        UT_EVENT_INVALID_ARGUMENT);
    return 0;
}

/**
 * Event trace Log adapterがLog Foundationへrecordを保存することを検証する。
 *
 * @return 成功時は0、失敗時は1。
 */
static int test_trace_log_adapter(void)
{
    ut_log_record_t records[4];
    ut_logger_t logger;
    ut_event_trace_log_config_t log_config;
    ut_event_trace_t trace;
    ut_log_record_t record;
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
    const ut_event_t event = {42u, 5u, NULL, 12u};

    CHECK(ut_logger_init(&logger, records, 4u, &logger_config) == UT_LOG_OK);
    log_config.logger = &logger;
    log_config.module = "evt";
    log_config.level = UT_LOG_LEVEL_TRACE;
    CHECK(ut_event_trace_init(&trace, ut_event_trace_log_sink, &log_config) ==
        UT_EVENT_OK);

    CHECK(ut_event_trace_event(&trace, &event) == UT_EVENT_OK);
    CHECK(ut_event_trace_state_transition(&trace, 5u, 1u, 2u, 9u) ==
        UT_EVENT_OK);
    CHECK(ut_logger_count(&logger) == 2u);
    CHECK(ut_logger_read(&logger, 0u, &record) == UT_LOG_OK);
    CHECK(strcmp(record.module, "evt") == 0);
    CHECK(strstr(record.message, "event id=42") != NULL);
    CHECK(ut_logger_read(&logger, 1u, &record) == UT_LOG_OK);
    CHECK(strstr(record.message, "state source=5") != NULL);
    return 0;
}

int run_utility_event_trace_tests(void)
{
    CHECK(test_event_trace_and_dispatch_hook() == 0);
    CHECK(test_state_transition_trace() == 0);
    CHECK(test_trace_invalid_arguments() == 0);
    CHECK(test_trace_log_adapter() == 0);
    return 0;
}
