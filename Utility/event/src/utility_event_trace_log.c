/**
 * @file utility_event_trace_log.c
 * @brief Event trace recordをLog Utilityへ出力するadapter実装。
 */
#include "utility_event_trace_log.h"

#include <stdint.h>

/** Log recordへ設定する擬似source file名。 */
#define UT_EVENT_TRACE_LOG_FILE "utility_event_trace"
/** Log recordへ設定する擬似line番号。 */
#define UT_EVENT_TRACE_LOG_LINE 0u
/** Log recordへ設定する擬似function名。 */
#define UT_EVENT_TRACE_LOG_FUNCTION "ut_event_trace_log_sink"

/**
 * NULL許容のmodule名を解決する。
 *
 * @param module 設定されたmodule名。
 * @return Logへ渡すmodule名。
 */
static const char *resolve_module(const char *module)
{
    return (module != NULL) ? module : UT_EVENT_TRACE_LOG_DEFAULT_MODULE;
}

void ut_event_trace_log_sink(
    const ut_event_trace_record_t *record,
    void *user_context)
{
    const ut_event_trace_log_config_t *config =
        (const ut_event_trace_log_config_t *)user_context;

    if ((record == NULL) || (config == NULL)) {
        return;
    }

    if (record->kind == UT_EVENT_TRACE_KIND_EVENT) {
        if (config->logger != NULL) {
            (void)ut_log_write(config->logger, config->level,
                resolve_module(config->module),
                UT_EVENT_TRACE_LOG_FILE,
                (uint32_t)UT_EVENT_TRACE_LOG_LINE,
                UT_EVENT_TRACE_LOG_FUNCTION,
                "event id=%u source=%u payload_size=%zu",
                record->event_id,
                record->source,
                record->payload_size);
        } else {
            (void)ut_log_write_default(config->level,
                resolve_module(config->module),
                UT_EVENT_TRACE_LOG_FILE,
                (uint32_t)UT_EVENT_TRACE_LOG_LINE,
                UT_EVENT_TRACE_LOG_FUNCTION,
                "event id=%u source=%u payload_size=%zu",
                record->event_id,
                record->source,
                record->payload_size);
        }
    } else if (record->kind == UT_EVENT_TRACE_KIND_STATE_TRANSITION) {
        if (config->logger != NULL) {
            (void)ut_log_write(config->logger, config->level,
                resolve_module(config->module),
                UT_EVENT_TRACE_LOG_FILE,
                (uint32_t)UT_EVENT_TRACE_LOG_LINE,
                UT_EVENT_TRACE_LOG_FUNCTION,
                "state source=%u from=%u to=%u reason=%u",
                record->source,
                record->from_state,
                record->to_state,
                record->reason);
        } else {
            (void)ut_log_write_default(config->level,
                resolve_module(config->module),
                UT_EVENT_TRACE_LOG_FILE,
                (uint32_t)UT_EVENT_TRACE_LOG_LINE,
                UT_EVENT_TRACE_LOG_FUNCTION,
                "state source=%u from=%u to=%u reason=%u",
                record->source,
                record->from_state,
                record->to_state,
                record->reason);
        }
    } else {
        /* 未知のrecord種別は将来拡張として破棄する。 */
    }
}
