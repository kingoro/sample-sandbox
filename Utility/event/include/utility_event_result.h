#ifndef UTILITY_EVENT_RESULT_H
#define UTILITY_EVENT_RESULT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Event Utility内で共通して使用するresult code型。 */
typedef uint32_t ut_event_result_t;

/**
 * Event Utilityのresult code。
 *
 * Queue満杯や配送先なしを異常終了と決めつけず、呼出側が再試行、破棄、
 * fault遷移などを選択できるよう、状態を戻り値として返す。
 */
enum {
    UT_EVENT_OK = 0u,
    UT_EVENT_INVALID_ARGUMENT = 1u,
    UT_EVENT_EMPTY = 2u,
    UT_EVENT_FULL = 3u,
    UT_EVENT_NOT_FOUND = 4u,
    UT_EVENT_ALREADY_EXISTS = 5u,
    UT_EVENT_BUSY = 6u
};

#ifdef __cplusplus
}
#endif

#endif
