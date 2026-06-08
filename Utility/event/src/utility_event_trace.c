/**
 * @file utility_event_trace.c
 * @brief Event trace contextとrecord生成の実装。
 */
#include "utility_event_trace.h"

/**
 * trace contextの内部不変条件を検査する。
 *
 * @param trace 検査するtrace context。
 * @return 利用可能なら真、それ以外は偽。
 */
static int trace_is_valid(const ut_event_trace_t *trace)
{
    return (trace != NULL) && (trace->sink != NULL);
}

ut_event_result_t ut_event_trace_init(
    ut_event_trace_t *trace,
    ut_event_trace_sink_t sink,
    void *user_context)
{
    if ((trace == NULL) || (sink == NULL)) {
        return UT_EVENT_INVALID_ARGUMENT;
    }

    trace->sink = sink;
    trace->user_context = user_context;
    return UT_EVENT_OK;
}

ut_event_result_t ut_event_trace_event(
    ut_event_trace_t *trace,
    const ut_event_t *event)
{
    const ut_event_trace_record_t record = {
        .kind = UT_EVENT_TRACE_KIND_EVENT,
        .event_id = (event != NULL) ? event->id : 0u,
        .source = (event != NULL) ? event->source : 0u,
        .from_state = UT_EVENT_TRACE_STATE_NONE,
        .to_state = UT_EVENT_TRACE_STATE_NONE,
        .reason = 0u,
        .payload_size = (event != NULL) ? event->payload_size : 0u
    };

    if (!trace_is_valid(trace) || (event == NULL)) {
        return UT_EVENT_INVALID_ARGUMENT;
    }

    trace->sink(&record, trace->user_context);
    return UT_EVENT_OK;
}

ut_event_result_t ut_event_trace_state_transition(
    ut_event_trace_t *trace,
    uint32_t source,
    uint32_t from_state,
    uint32_t to_state,
    uint32_t reason)
{
    const ut_event_trace_record_t record = {
        .kind = UT_EVENT_TRACE_KIND_STATE_TRANSITION,
        .event_id = UT_EVENT_ID_ANY,
        .source = source,
        .from_state = from_state,
        .to_state = to_state,
        .reason = reason,
        .payload_size = 0u
    };

    if (!trace_is_valid(trace)) {
        return UT_EVENT_INVALID_ARGUMENT;
    }

    trace->sink(&record, trace->user_context);
    return UT_EVENT_OK;
}

void ut_event_trace_dispatch_handler(
    const ut_event_t *event,
    void *user_context)
{
    (void)ut_event_trace_event((ut_event_trace_t *)user_context, event);
}
