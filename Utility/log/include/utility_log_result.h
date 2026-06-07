/**
 * @file utility_log_result.h
 * @brief Log Utility共通のresult codeを定義する。
 */
#ifndef UTILITY_LOG_RESULT_H
#define UTILITY_LOG_RESULT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Log Utilityのresult code型。 */
typedef uint32_t ut_log_result_t;

/** Log Utilityのresult code。 */
enum {
    /** 操作が成功した。 */
    UT_LOG_OK = 0u,
    /** NULL、容量0、または壊れたcontextが渡された。 */
    UT_LOG_INVALID_ARGUMENT = 1u,
    /** 指定位置にLog Recordが存在しない。 */
    UT_LOG_NOT_FOUND = 2u,
    /** messageの整形またはConsole出力に失敗した。 */
    UT_LOG_OUTPUT_ERROR = 3u
};

#ifdef __cplusplus
}
#endif

#endif
