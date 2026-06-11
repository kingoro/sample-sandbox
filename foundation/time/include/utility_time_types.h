/**
 * @file utility_time_types.h
 * @brief OS非依存Time Foundationの型とresult code。
 */
#ifndef UTILITY_TIME_TYPES_H
#define UTILITY_TIME_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 1秒あたりのnanosecond数。 */
#define UT_TIME_NANOSECONDS_PER_SECOND UINT32_C(1000000000)

/** Time Foundation APIのresult code型。 */
typedef uint32_t ut_time_result_t;

/** Time Foundation APIのresult code。 */
enum {
    /** 操作が成功した。 */
    UT_TIME_OK = 0,
    /** NULL、不正な値、または不正な書式が渡された。 */
    UT_TIME_INVALID_ARGUMENT = 1,
    /** 変換または演算結果が表現範囲外である。 */
    UT_TIME_OUT_OF_RANGE = 2,
    /** 出力bufferが不足している。 */
    UT_TIME_BUFFER_TOO_SMALL = 3
};

/**
 * UTC Unix epoch timestamp。
 *
 * secondsは1970-01-01T00:00:00Zからの符号付き秒、nanosecondは常に
 * 0以上1,000,000,000未満である。値を所有し、寿命制約はなく、copyはthread safe。
 */
typedef struct ut_time_utc {
    /** Unix epochからの秒。 */
    int64_t seconds;
    /** 秒内の正規化nanosecond。 */
    uint32_t nanosecond;
} ut_time_utc_t;

/**
 * 符号付きduration。
 *
 * secondsとnanosecondはUTCと同じ正規化規則を使うため、負のsubsecond durationは
 * 例として-1秒+500,000,000nsのように表す。
 */
typedef struct ut_time_duration {
    /** durationの整数秒floor。 */
    int64_t seconds;
    /** 秒内の正規化nanosecond。 */
    uint32_t nanosecond;
} ut_time_duration_t;

/** Monotonic clockのnanosecond tick。 */
typedef uint64_t ut_time_tick_ns_t;

#ifdef __cplusplus
}
#endif

#endif
