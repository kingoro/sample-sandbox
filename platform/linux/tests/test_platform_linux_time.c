/**
 * @file test_platform_linux_time.c
 * @brief Linux Time adapterの注入clock単体テスト。
 */
#include "platform_linux_time.h"
#include "../src/platform_linux_time_internal.h"

#include <stdio.h>

/** 条件失敗時にtestを終了する。 */
#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
            return 1; \
        } \
    } while (0)

/** Fake clock状態。 */
typedef struct fake_clock {
    /** callback戻り値。 */
    platform_linux_time_result_t result;
    /** callbackが返す値。 */
    platform_linux_clock_snapshot_t value;
    /** 最後に要求されたclock。 */
    platform_linux_clock_kind_t requested;
} fake_clock_t;

/**
 * 注入用fake clock。
 *
 * @param kind 要求clock。
 * @param value snapshot出力先。
 * @param context fake_clock_t context。
 * @return fake設定のadapter result。
 */
static platform_linux_time_result_t fake_gettime(
    platform_linux_clock_kind_t kind,
    platform_linux_clock_snapshot_t *value,
    void *context)
{
    fake_clock_t *fake = context;
    fake->requested = kind;
    *value = fake->value;
    return fake->result;
}

/**
 * unsigned 64-bit time_t相当の範囲外を変換して返す。
 *
 * @param kind 要求clock。
 * @param value snapshot出力先。
 * @param context 未使用。
 * @return 常にPLATFORM_LINUX_TIME_OUT_OF_RANGE。
 */
static platform_linux_time_result_t unsigned_time_t_overflow_gettime(
    platform_linux_clock_kind_t kind,
    platform_linux_clock_snapshot_t *value,
    void *context)
{
    int64_t seconds = 0;
    (void)kind;
    (void)value;
    (void)context;
    return platform_linux_time_seconds_from_parts(
        false, UINT64_C(0x8000000000000000), &seconds);
}

/** @brief 正常なmonotonic/UTC変換を検証する。 @return 成功時0。 */
static int test_success(void)
{
    fake_clock_t fake = {
        PLATFORM_LINUX_TIME_OK, {12, 345}, 0
    };
    platform_linux_time_adapter_t adapter = {fake_gettime, &fake};
    ut_time_tick_ns_t tick;
    ut_time_utc_t utc;
    CHECK(platform_linux_time_monotonic(&adapter, &tick) ==
        PLATFORM_LINUX_TIME_OK);
    CHECK(fake.requested == PLATFORM_LINUX_CLOCK_MONOTONIC);
    CHECK(tick == UINT64_C(12000000345));
    fake.value.seconds = -2;
    fake.value.nanosecond = 7;
    CHECK(platform_linux_time_utc(&adapter, &utc) == PLATFORM_LINUX_TIME_OK);
    CHECK(fake.requested == PLATFORM_LINUX_CLOCK_REALTIME);
    CHECK(utc.seconds == -2 && utc.nanosecond == 7u);
    return 0;
}

/** @brief failure、不正snapshot、overflowを検証する。 @return 成功時0。 */
static int test_failures(void)
{
    fake_clock_t fake = {
        PLATFORM_LINUX_TIME_CLOCK_ERROR, {0, 0}, 0
    };
    platform_linux_time_adapter_t adapter = {fake_gettime, &fake};
    ut_time_tick_ns_t tick;
    ut_time_utc_t utc;
    CHECK(platform_linux_time_monotonic(&adapter, &tick) ==
        PLATFORM_LINUX_TIME_CLOCK_ERROR);
    fake.result = PLATFORM_LINUX_TIME_OK;
    fake.value.nanosecond = -1;
    CHECK(platform_linux_time_utc(&adapter, &utc) ==
        PLATFORM_LINUX_TIME_INVALID_ARGUMENT);
    fake.value.nanosecond = 1000000000;
    CHECK(platform_linux_time_monotonic(&adapter, &tick) ==
        PLATFORM_LINUX_TIME_INVALID_ARGUMENT);
    fake.value.nanosecond = 0;
    fake.value.seconds = -1;
    CHECK(platform_linux_time_monotonic(&adapter, &tick) ==
        PLATFORM_LINUX_TIME_INVALID_ARGUMENT);
    fake.value.seconds = INT64_C(18446744074);
    CHECK(platform_linux_time_monotonic(&adapter, &tick) ==
        PLATFORM_LINUX_TIME_OUT_OF_RANGE);
    fake.result = PLATFORM_LINUX_TIME_CLOCK_ERROR;
    CHECK(platform_linux_time_utc(&adapter, &utc) ==
        PLATFORM_LINUX_TIME_CLOCK_ERROR);
    adapter.gettime = unsigned_time_t_overflow_gettime;
    utc = (ut_time_utc_t){77, 77u};
    CHECK(platform_linux_time_utc(&adapter, &utc) ==
        PLATFORM_LINUX_TIME_OUT_OF_RANGE);
    CHECK(utc.seconds == 77 && utc.nanosecond == 77u);
    return 0;
}

/** @brief 秒変換とmonotonic加算境界を検証する。 @return 成功時0。 */
static int test_conversion_boundaries(void)
{
    int64_t seconds = 77;
    ut_time_tick_ns_t tick = 77u;
    ut_time_utc_t utc = {77, 77u};
    platform_linux_clock_snapshot_t snapshot;

    CHECK(platform_linux_time_seconds_from_parts(
        false, UINT32_MAX, &seconds) == PLATFORM_LINUX_TIME_OK);
    CHECK(seconds == (int64_t)UINT32_MAX);
    CHECK(platform_linux_time_seconds_from_parts(
        true, UINT64_C(2147483648), &seconds) == PLATFORM_LINUX_TIME_OK);
    CHECK(seconds == INT64_C(-2147483648));
    CHECK(platform_linux_time_seconds_from_parts(
        false, (uint64_t)INT64_MAX, &seconds) == PLATFORM_LINUX_TIME_OK);
    CHECK(seconds == INT64_MAX);
    CHECK(platform_linux_time_seconds_from_parts(
        true, UINT64_C(0x8000000000000000), &seconds) ==
        PLATFORM_LINUX_TIME_OK);
    CHECK(seconds == INT64_MIN);
    CHECK(platform_linux_time_seconds_from_parts(
        false, UINT64_C(0x8000000000000000), &seconds) ==
        PLATFORM_LINUX_TIME_OUT_OF_RANGE);
    CHECK(seconds == INT64_MIN);
    CHECK(platform_linux_time_seconds_from_parts(
        true, UINT64_MAX, &seconds) == PLATFORM_LINUX_TIME_OUT_OF_RANGE);
    CHECK(seconds == INT64_MIN);
    CHECK(platform_linux_time_seconds_from_parts(false, 0u, NULL) ==
        PLATFORM_LINUX_TIME_INVALID_ARGUMENT);
    CHECK(platform_linux_time_seconds_from_parts(
        true, UINT64_C(123), &seconds) == PLATFORM_LINUX_TIME_OK);
    CHECK(seconds == -123);

    snapshot.seconds = (int64_t)(UINT64_MAX / UINT64_C(1000000000));
    snapshot.nanosecond =
        (int32_t)(UINT64_MAX % UINT64_C(1000000000));
    CHECK(platform_linux_time_snapshot_to_tick(&snapshot, &tick) ==
        PLATFORM_LINUX_TIME_OK);
    CHECK(tick == UINT64_MAX);
    snapshot.nanosecond++;
    CHECK(platform_linux_time_snapshot_to_tick(&snapshot, &tick) ==
        PLATFORM_LINUX_TIME_OUT_OF_RANGE);
    CHECK(tick == UINT64_MAX);
    snapshot.seconds++;
    snapshot.nanosecond = 0;
    CHECK(platform_linux_time_snapshot_to_tick(&snapshot, &tick) ==
        PLATFORM_LINUX_TIME_OUT_OF_RANGE);
    CHECK(tick == UINT64_MAX);
    CHECK(platform_linux_time_snapshot_to_tick(NULL, &tick) ==
        PLATFORM_LINUX_TIME_INVALID_ARGUMENT);
    CHECK(platform_linux_time_snapshot_to_tick(&snapshot, NULL) ==
        PLATFORM_LINUX_TIME_INVALID_ARGUMENT);
    snapshot.seconds = 1;
    snapshot.nanosecond = -1;
    CHECK(platform_linux_time_snapshot_to_utc(&snapshot, &utc) ==
        PLATFORM_LINUX_TIME_INVALID_ARGUMENT);
    CHECK(utc.seconds == 77 && utc.nanosecond == 77u);
    snapshot.nanosecond = 1000000000;
    CHECK(platform_linux_time_snapshot_to_utc(&snapshot, &utc) ==
        PLATFORM_LINUX_TIME_INVALID_ARGUMENT);
    CHECK(platform_linux_time_snapshot_to_utc(&snapshot, NULL) ==
        PLATFORM_LINUX_TIME_INVALID_ARGUMENT);
    return 0;
}

/** @brief System adapterのLinux clock正常経路を検証する。 @return 成功時0。 */
static int test_system_adapter(void)
{
    platform_linux_time_adapter_t adapter;
    ut_time_tick_ns_t tick;
    ut_time_utc_t utc;
    CHECK(platform_linux_time_init(&adapter) == PLATFORM_LINUX_TIME_OK);
    CHECK(platform_linux_time_monotonic(&adapter, &tick) ==
        PLATFORM_LINUX_TIME_OK);
    CHECK(platform_linux_time_utc(&adapter, &utc) ==
        PLATFORM_LINUX_TIME_OK);
    CHECK(utc.nanosecond < UT_TIME_NANOSECONDS_PER_SECOND);
    return 0;
}

/** @brief 初期化とNULL引数を検証する。 @return 成功時0。 */
static int test_init_and_invalid(void)
{
    platform_linux_time_adapter_t adapter;
    ut_time_tick_ns_t tick;
    CHECK(platform_linux_time_init(&adapter) == PLATFORM_LINUX_TIME_OK);
    CHECK(adapter.gettime != NULL);
    CHECK(platform_linux_time_init(NULL) ==
        PLATFORM_LINUX_TIME_INVALID_ARGUMENT);
    adapter.gettime = NULL;
    CHECK(platform_linux_time_monotonic(&adapter, &tick) ==
        PLATFORM_LINUX_TIME_INVALID_ARGUMENT);
    CHECK(platform_linux_time_monotonic(
        &(platform_linux_time_adapter_t){fake_gettime, NULL}, NULL) ==
        PLATFORM_LINUX_TIME_INVALID_ARGUMENT);
    CHECK(platform_linux_time_utc(NULL, NULL) ==
        PLATFORM_LINUX_TIME_INVALID_ARGUMENT);
    return 0;
}

/** @brief Linux Time adapter単体テストを実行する。 @return 全成功時0。 */
int main(void)
{
    CHECK(test_success() == 0);
    CHECK(test_failures() == 0);
    CHECK(test_conversion_boundaries() == 0);
    CHECK(test_system_adapter() == 0);
    CHECK(test_init_and_invalid() == 0);
    return 0;
}
