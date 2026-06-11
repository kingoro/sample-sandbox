/**
 * @file utility_log_console.h
 * @brief 標準C streamへLog Recordを表示するConsole adapter。
 */
#ifndef UTILITY_LOG_CONSOLE_H
#define UTILITY_LOG_CONSOLE_H

#include "utility_log_types.h"

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * FILE streamへ1件のRecordを1行で出力する。
 *
 * contextにはFILEポインタを指定する。NULLの場合はstderrを使用する。
 *
 * @param record 出力するRecord。
 * @param context FILEポインタまたはNULL。
 * @return 成功時UT_LOG_OK、失敗時UT_LOG_OUTPUT_ERROR。
 */
ut_log_result_t ut_log_console_write_file(
    const ut_log_record_t *record,
    void *context);

/**
 * Log Levelを固定文字列へ変換する。
 *
 * @param level 変換するLog Level。
 * @return 静的領域にある文字列。無効値では"UNKNOWN"。
 */
const char *ut_log_level_name(ut_log_level_t level);

#ifdef __cplusplus
}
#endif

#endif
