/**
 * @file platform_linux_time.c
 * @brief Linux clock_gettime adapter実装。
 */
/** POSIX clock APIを公開するfeature level。 */
#define _POSIX_C_SOURCE 200809L
#include "platform_linux_time.h"
#include "platform_linux_time_internal.h"

#include <limits.h>
#include <stddef.h>
#include <time.h>

_Static_assert(
    sizeof(time_t) <= sizeof(uint64_t),
    "platform_linux_time requires time_t no wider than uint64_t");

/**
 * time_t秒を公開snapshotのint64秒へ変換する。
 *
 * @param value 変換するtime_t秒。
 * @param seconds int64秒の出力先。
 * @return 成功、引数不正、または範囲外。
 */
static platform_linux_time_result_t time_t_to_seconds(
    time_t value,
    int64_t *seconds)
{
    uint64_t magnitude;
    if ((time_t)-1 < (time_t)0 && value < (time_t)0) {
        magnitude = (uint64_t)(-(value + (time_t)1));
        magnitude++;
        return platform_linux_time_seconds_from_parts(
            true, magnitude, seconds);
    }
    return platform_linux_time_seconds_from_parts(
        false, (uint64_t)value, seconds);
}

/**
 * system clock_gettimeを公開callback形へ適合する。
 *
 * @param kind 取得するclock kind。
 * @param snapshot snapshot出力先。
 * @param context 未使用context。
 * @return 成功、clock失敗、変換範囲外、または引数不正。
 */
static platform_linux_time_result_t system_gettime(
    platform_linux_clock_kind_t kind,
    platform_linux_clock_snapshot_t *snapshot,
    void *context)
{
    struct timespec value;
    clockid_t clock_id;
    platform_linux_time_result_t result;
    (void)context;
    if (snapshot == NULL) {
        return PLATFORM_LINUX_TIME_INVALID_ARGUMENT;
    }
    if (kind == PLATFORM_LINUX_CLOCK_MONOTONIC) {
        clock_id = CLOCK_MONOTONIC;
    } else if (kind == PLATFORM_LINUX_CLOCK_REALTIME) {
        clock_id = CLOCK_REALTIME;
    } else {
        return PLATFORM_LINUX_TIME_INVALID_ARGUMENT;
    }
    if (clock_gettime(clock_id, &value) != 0) {
        return PLATFORM_LINUX_TIME_CLOCK_ERROR;
    }
    result = time_t_to_seconds(value.tv_sec, &snapshot->seconds);
    if (result != PLATFORM_LINUX_TIME_OK) {
        return result;
    }
    snapshot->nanosecond = (int32_t)value.tv_nsec;
    return PLATFORM_LINUX_TIME_OK;
}

/**
 * Snapshotのnanosecond fieldを検証する。
 *
 * @param snapshot 検証するsnapshot。
 * @return 正規範囲ならtrue。
 */
static bool valid_nanosecond(
    const platform_linux_clock_snapshot_t *snapshot)
{
    return snapshot != NULL && snapshot->nanosecond >= 0 &&
        (uint32_t)snapshot->nanosecond < UT_TIME_NANOSECONDS_PER_SECOND;
}

platform_linux_time_result_t platform_linux_time_seconds_from_parts(
    bool negative,
    uint64_t magnitude,
    int64_t *seconds)
{
    int64_t temporary;
    if (seconds == NULL) {
        return PLATFORM_LINUX_TIME_INVALID_ARGUMENT;
    }
    if (negative) {
        if (magnitude > UINT64_C(0x8000000000000000)) {
            return PLATFORM_LINUX_TIME_OUT_OF_RANGE;
        }
        if (magnitude == UINT64_C(0x8000000000000000)) {
            temporary = INT64_MIN;
        } else {
            temporary = -(int64_t)magnitude;
        }
    } else {
        if (magnitude > (uint64_t)INT64_MAX) {
            return PLATFORM_LINUX_TIME_OUT_OF_RANGE;
        }
        temporary = (int64_t)magnitude;
    }
    *seconds = temporary;
    return PLATFORM_LINUX_TIME_OK;
}

platform_linux_time_result_t platform_linux_time_snapshot_to_tick(
    const platform_linux_clock_snapshot_t *snapshot,
    ut_time_tick_ns_t *tick)
{
    uint64_t seconds;
    uint64_t temporary;
    if (!valid_nanosecond(snapshot) || tick == NULL ||
        snapshot->seconds < 0) {
        return PLATFORM_LINUX_TIME_INVALID_ARGUMENT;
    }
    seconds = (uint64_t)snapshot->seconds;
    if (seconds > UINT64_MAX / UINT64_C(1000000000) ||
        (seconds == UINT64_MAX / UINT64_C(1000000000) &&
         (uint64_t)snapshot->nanosecond >
            UINT64_MAX % UINT64_C(1000000000))) {
        return PLATFORM_LINUX_TIME_OUT_OF_RANGE;
    }
    temporary = seconds * UINT64_C(1000000000) +
        (uint64_t)snapshot->nanosecond;
    *tick = temporary;
    return PLATFORM_LINUX_TIME_OK;
}

platform_linux_time_result_t platform_linux_time_snapshot_to_utc(
    const platform_linux_clock_snapshot_t *snapshot,
    ut_time_utc_t *utc)
{
    ut_time_utc_t temporary;
    if (!valid_nanosecond(snapshot) || utc == NULL) {
        return PLATFORM_LINUX_TIME_INVALID_ARGUMENT;
    }
    temporary.seconds = snapshot->seconds;
    temporary.nanosecond = (uint32_t)snapshot->nanosecond;
    *utc = temporary;
    return PLATFORM_LINUX_TIME_OK;
}

platform_linux_time_result_t platform_linux_time_init(
    platform_linux_time_adapter_t *adapter)
{
    if (adapter == NULL) {
        return PLATFORM_LINUX_TIME_INVALID_ARGUMENT;
    }
    adapter->gettime = system_gettime;
    adapter->context = NULL;
    return PLATFORM_LINUX_TIME_OK;
}

platform_linux_time_result_t platform_linux_time_monotonic(
    const platform_linux_time_adapter_t *adapter,
    ut_time_tick_ns_t *tick)
{
    platform_linux_clock_snapshot_t snapshot;
    platform_linux_time_result_t result;
    if (adapter == NULL || adapter->gettime == NULL || tick == NULL) {
        return PLATFORM_LINUX_TIME_INVALID_ARGUMENT;
    }
    result = adapter->gettime(
        PLATFORM_LINUX_CLOCK_MONOTONIC, &snapshot, adapter->context);
    if (result != PLATFORM_LINUX_TIME_OK) {
        return result;
    }
    return platform_linux_time_snapshot_to_tick(&snapshot, tick);
}

platform_linux_time_result_t platform_linux_time_utc(
    const platform_linux_time_adapter_t *adapter,
    ut_time_utc_t *utc)
{
    platform_linux_clock_snapshot_t snapshot;
    platform_linux_time_result_t result;
    if (adapter == NULL || adapter->gettime == NULL || utc == NULL) {
        return PLATFORM_LINUX_TIME_INVALID_ARGUMENT;
    }
    result = adapter->gettime(
        PLATFORM_LINUX_CLOCK_REALTIME, &snapshot, adapter->context);
    if (result != PLATFORM_LINUX_TIME_OK) {
        return result;
    }
    return platform_linux_time_snapshot_to_utc(&snapshot, utc);
}
