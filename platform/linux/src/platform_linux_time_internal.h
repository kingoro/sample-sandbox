/**
 * @file platform_linux_time_internal.h
 * @brief Linux Time adapterの純粋変換test用内部API。
 */
#ifndef PLATFORM_LINUX_TIME_INTERNAL_H
#define PLATFORM_LINUX_TIME_INTERNAL_H

#include "platform_linux_time.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 符号と絶対値で表した秒をint64_tへ変換する。
 *
 * 32/64 bit signed/unsigned `time_t`相当の境界をOS型に依存せず検証するための
 * 内部helperである。
 *
 * @param negative 負値ならtrue。
 * @param magnitude 秒の絶対値。
 * @param seconds 変換結果の出力先。
 * @return 成功、引数不正、または範囲外。
 */
platform_linux_time_result_t platform_linux_time_seconds_from_parts(
    bool negative,
    uint64_t magnitude,
    int64_t *seconds);

/**
 * Snapshotをmonotonic nanosecondへ変換する。
 *
 * @param snapshot 入力snapshot。
 * @param tick 変換結果の出力先。
 * @return 成功、引数不正、または範囲外。
 */
platform_linux_time_result_t platform_linux_time_snapshot_to_tick(
    const platform_linux_clock_snapshot_t *snapshot,
    ut_time_tick_ns_t *tick);

/**
 * SnapshotをUTCへ変換する。
 *
 * @param snapshot 入力snapshot。
 * @param utc 変換結果の出力先。
 * @return 成功または引数不正。
 */
platform_linux_time_result_t platform_linux_time_snapshot_to_utc(
    const platform_linux_clock_snapshot_t *snapshot,
    ut_time_utc_t *utc);

#ifdef __cplusplus
}
#endif

#endif
