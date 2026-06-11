/**
 * @file platform_linux_time.h
 * @brief Linux clock_gettimeをTime Foundation型へ接続するadapter。
 *
 * `/dev/rtc`は扱わない。RTC device操作はdriver層の責務である。
 */
#ifndef PLATFORM_LINUX_TIME_H
#define PLATFORM_LINUX_TIME_H

#include "utility_time.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Linux Time adapterのresult code型。 */
typedef uint32_t platform_linux_time_result_t;

/** Linux Time adapterのresult code。 */
enum {
    /** clock取得と変換が成功した。 */
    PLATFORM_LINUX_TIME_OK = 0,
    /** NULL callback/outputまたは不正snapshotを検出した。 */
    PLATFORM_LINUX_TIME_INVALID_ARGUMENT = 1,
    /** clock callbackが失敗した。 */
    PLATFORM_LINUX_TIME_CLOCK_ERROR = 2,
    /** Foundation型へ変換できない範囲だった。 */
    PLATFORM_LINUX_TIME_OUT_OF_RANGE = 3
};

/** Adapterが要求するclock kind型。 */
typedef uint32_t platform_linux_clock_kind_t;

/** Adapterが要求するclock kind。 */
enum {
    /** 経過時間用monotonic clock。 */
    PLATFORM_LINUX_CLOCK_MONOTONIC = 0,
    /** UTC wall clock用realtime clock。 */
    PLATFORM_LINUX_CLOCK_REALTIME = 1
};

/**
 * OS clock callbackが返すsnapshot。
 *
 * nanosecondはcallback境界で未検証でもよく、adapterが0以上1,000,000,000未満を
 * 検証する。
 */
typedef struct platform_linux_clock_snapshot {
    /** Clockの秒値。 */
    int64_t seconds;
    /** 秒内nanosecond。 */
    int32_t nanosecond;
} platform_linux_clock_snapshot_t;

/**
 * 注入可能なOS clock callback。
 *
 * callbackとcontextの所有権は呼出側にあり、呼出し中のみ有効でなければならない。
 * thread safetyはcallback実装に従う。
 *
 * @param kind monotonicまたはrealtime。
 * @param snapshot callbackが設定するsnapshot。
 * @param context 呼出側指定context。
 * @return 成功、clock失敗、変換範囲外、または引数不正。
 */
typedef platform_linux_time_result_t (*platform_linux_clock_gettime_fn)(
    platform_linux_clock_kind_t kind,
    platform_linux_clock_snapshot_t *snapshot,
    void *context);

/**
 * Linux clock adapter設定。
 *
 * 値を所有しない。callback/contextは各取得呼出しの完了まで有効でなければならない。
 */
typedef struct platform_linux_time_adapter {
    /** clock_gettime互換callback。 */
    platform_linux_clock_gettime_fn gettime;
    /** callbackへ渡す任意context。 */
    void *context;
} platform_linux_time_adapter_t;

/**
 * system clock_gettimeを使うadapterを初期化する。
 *
 * @param adapter 呼出側所有の出力先。
 * @return 成功または引数不正。
 */
platform_linux_time_result_t platform_linux_time_init(
    platform_linux_time_adapter_t *adapter);

/**
 * Linux monotonic clockをnanosecond tickとして取得する。
 *
 * @param adapter 初期化済みまたはcallback注入済みadapter。
 * @param tick 呼出側所有の出力先。
 * @return 成功、引数不正、clock失敗、または範囲外。
 */
platform_linux_time_result_t platform_linux_time_monotonic(
    const platform_linux_time_adapter_t *adapter,
    ut_time_tick_ns_t *tick);

/**
 * Linux realtime clockをUTCとして取得する。
 *
 * @param adapter 初期化済みまたはcallback注入済みadapter。
 * @param utc 呼出側所有の出力先。
 * @return 成功、引数不正、clock失敗、または範囲外。
 */
platform_linux_time_result_t platform_linux_time_utc(
    const platform_linux_time_adapter_t *adapter,
    ut_time_utc_t *utc);

#ifdef __cplusplus
}
#endif

#endif
