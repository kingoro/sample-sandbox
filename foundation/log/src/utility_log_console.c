/**
 * @file utility_log_console.c
 * @brief 標準C FILE stream用Console adapter実装。
 */
#include "utility_log_console.h"

const char *ut_log_level_name(ut_log_level_t level)
{
    static const char *const names[UT_LOG_LEVEL_COUNT] = {
        "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
    };

    if (level < UT_LOG_LEVEL_TRACE || level >= UT_LOG_LEVEL_COUNT) {
        return "UNKNOWN";
    }
    return names[level];
}

ut_log_result_t ut_log_console_write_file(
    const ut_log_record_t *record,
    void *context)
{
    FILE *stream = context != NULL ? (FILE *)context : stderr;
    int result;

    if (record == NULL) {
        return UT_LOG_INVALID_ARGUMENT;
    }
    result = fprintf(stream, "[%llu] %-5s %-23s %s (%s:%u %s)\n",
        (unsigned long long)record->timestamp,
        ut_log_level_name(record->level),
        record->module,
        record->message,
        record->file,
        record->line,
        record->function);
    return result < 0 ? UT_LOG_OUTPUT_ERROR : UT_LOG_OK;
}
