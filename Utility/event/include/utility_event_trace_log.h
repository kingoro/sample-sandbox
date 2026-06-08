/**
 * @file utility_event_trace_log.h
 * @brief Event trace recordをLog Utilityへ接続するadapter API。
 */
#ifndef UTILITY_EVENT_TRACE_LOG_H
#define UTILITY_EVENT_TRACE_LOG_H

#include "utility_event_trace.h"
#include "utility_log.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Log adapterで使用する既定module名。 */
#define UT_EVENT_TRACE_LOG_DEFAULT_MODULE "event-trace"

/**
 * Event traceをLog Utilityへ出力するための設定。
 *
 * loggerがNULLの場合はプロセス既定Loggerへ出力する。moduleがNULLの場合は
 * UT_EVENT_TRACE_LOG_DEFAULT_MODULEを使用する。
 */
typedef struct ut_event_trace_log_config {
    /** 出力先Logger。NULLならプロセス既定Logger。 */
    ut_logger_t *logger;
    /** Log module名。NULLなら既定module名。 */
    const char *module;
    /** 出力するLog level。 */
    ut_log_level_t level;
} ut_event_trace_log_config_t;

/**
 * ut_event_trace_tのsinkとして登録できるLog Utility adapter。
 *
 * user_contextにはut_event_trace_log_config_tへのpointerを指定する。contextは
 * trace利用中有効に保つ。Loggerが無効またはlevelが無効な場合、recordは破棄する。
 *
 * @param record callback呼出中のみ有効なtrace record。
 * @param user_context ut_event_trace_log_config_tへのpointer。
 */
void ut_event_trace_log_sink(const ut_event_trace_record_t *record, void *user_context);

#ifdef __cplusplus
}
#endif

#endif
