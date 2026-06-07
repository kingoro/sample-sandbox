/**
 * @file utility_event_result.h
 * @brief Event Utility共通のresult codeを定義する。
 */
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
    /** 操作が成功した。 */
    UT_EVENT_OK = 0u,
    /** NULL、容量0、または壊れたcontextが渡された。 */
    UT_EVENT_INVALID_ARGUMENT = 1u,
    /** QueueにEventが存在しない。 */
    UT_EVENT_EMPTY = 2u,
    /** Queueまたはsubscription領域に空きがない。 */
    UT_EVENT_FULL = 3u,
    /** 対象subscriptionまたは配送先が存在しない。 */
    UT_EVENT_NOT_FOUND = 4u,
    /** 同一subscriptionがすでに登録されている。 */
    UT_EVENT_ALREADY_EXISTS = 5u,
    /** dispatch中の登録変更または再帰dispatchが要求された。 */
    UT_EVENT_BUSY = 6u
};

#ifdef __cplusplus
}
#endif

#endif
