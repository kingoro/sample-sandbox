/**
 * @file test_time_service.c
 * @brief Time Service同期状態単体テスト。
 */
#include "time_service.h"

#include <limits.h>
#include <stdio.h>

/** 条件失敗時にtestを終了する。 */
#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
            return 1; \
        } \
    } while (0)

/** @brief 同期update、now、statusを検証する。 @return 成功時0。 */
static int test_synchronized_now(void)
{
    time_service_t service;
    time_service_status_t status;
    const ut_time_utc_t sync = {100, 750000000u};
    ut_time_utc_t now;
    CHECK(time_service_init(&service) == TIME_SERVICE_OK);
    CHECK(time_service_update(
        &service, &sync, UINT64_C(2000000000), TIME_SERVICE_SOURCE_NTP,
        UINT64_C(42)) == TIME_SERVICE_OK);
    CHECK(time_service_now(&service, UINT64_C(3500000000), &now) ==
        TIME_SERVICE_OK);
    CHECK(now.seconds == 102 && now.nanosecond == 250000000u);
    CHECK(time_service_get_status(&service, &status) == TIME_SERVICE_OK);
    CHECK(status.available && status.synchronized);
    CHECK(status.source == TIME_SERVICE_SOURCE_NTP);
    CHECK(status.uncertainty_ns == 42u);
    CHECK(status.last_sync_monotonic_ns == UINT64_C(2000000000));
    CHECK(time_service_now(&service, UINT64_C(4500000000), &now) ==
        TIME_SERVICE_OK);
    CHECK(time_service_get_status(&service, &status) == TIME_SERVICE_OK);
    CHECK(status.uncertainty_ns == 42u);
    return 0;
}

/** @brief 未同期、逆行、overflowを検証する。 @return 成功時0。 */
static int test_errors(void)
{
    time_service_t service;
    ut_time_utc_t sync = {0, 0u};
    ut_time_utc_t now;
    CHECK(time_service_init(&service) == TIME_SERVICE_OK);
    CHECK(time_service_now(&service, 0u, &now) == TIME_SERVICE_UNAVAILABLE);
    CHECK(time_service_update(
        &service, &sync, 10u, TIME_SERVICE_SOURCE_RTC, 0u) ==
        TIME_SERVICE_OK);
    CHECK(time_service_now(&service, 9u, &now) ==
        TIME_SERVICE_MONOTONIC_REGRESSION);
    service.last_sync_monotonic_ns = 0u;
    CHECK(time_service_now(&service, UINT64_MAX, &now) == TIME_SERVICE_OK);
    CHECK(now.seconds == INT64_C(18446744073));
    CHECK(now.nanosecond == 709551615u);
    service.synchronized_utc.seconds = INT64_MAX;
    service.last_sync_monotonic_ns = 0u;
    CHECK(time_service_now(&service, UINT64_C(1000000000), &now) ==
        TIME_SERVICE_OVERFLOW);
    time_service_set_unavailable(&service);
    CHECK(!service.available && !service.synchronized);
    CHECK(service.source == TIME_SERVICE_SOURCE_UNKNOWN);
    time_service_set_unavailable(NULL);
    return 0;
}

/** @brief Update逆行とstate invariantを検証する。 @return 成功時0。 */
static int test_update_and_invariants(void)
{
    time_service_t service;
    time_service_t before;
    time_service_status_t status = {true, true, 99u, 99u, 99u};
    ut_time_utc_t now = {99, 99u};
    const ut_time_utc_t utc = {10, 20u};

    CHECK(time_service_init(&service) == TIME_SERVICE_OK);
    CHECK(time_service_update(
        &service, &utc, 100u, TIME_SERVICE_SOURCE_GPS, 7u) ==
        TIME_SERVICE_OK);
    before = service;
    CHECK(time_service_update(
        &service, &utc, 99u, TIME_SERVICE_SOURCE_NTP, 8u) ==
        TIME_SERVICE_MONOTONIC_REGRESSION);
    CHECK(service.last_sync_monotonic_ns == before.last_sync_monotonic_ns);
    CHECK(service.source == before.source && service.uncertainty_ns == 7u);

    service.available = false;
    CHECK(time_service_now(&service, 100u, &now) ==
        TIME_SERVICE_INVALID_ARGUMENT);
    CHECK(now.seconds == 99 && now.nanosecond == 99u);
    CHECK(time_service_get_status(&service, &status) ==
        TIME_SERVICE_INVALID_ARGUMENT);
    CHECK(status.source == 99u && status.uncertainty_ns == 99u);
    CHECK(time_service_update(
        &service, &utc, 101u, TIME_SERVICE_SOURCE_NTP, 8u) ==
        TIME_SERVICE_INVALID_ARGUMENT);

    CHECK(time_service_init(&service) == TIME_SERVICE_OK);
    service.source = TIME_SERVICE_SOURCE_RTC;
    CHECK(time_service_now(&service, 0u, &now) ==
        TIME_SERVICE_INVALID_ARGUMENT);
    service.source = TIME_SERVICE_SOURCE_UNKNOWN;
    service.available = true;
    CHECK(time_service_get_status(&service, &status) ==
        TIME_SERVICE_INVALID_ARGUMENT);
    return 0;
}

/** @brief 不正引数とsourceを検証する。 @return 成功時0。 */
static int test_invalid(void)
{
    time_service_t service;
    time_service_status_t status;
    ut_time_utc_t utc = {0, 0u};
    CHECK(time_service_init(NULL) == TIME_SERVICE_INVALID_ARGUMENT);
    CHECK(time_service_init(&service) == TIME_SERVICE_OK);
    CHECK(time_service_update(
        &service, NULL, 0u, TIME_SERVICE_SOURCE_NTP, 0u) ==
        TIME_SERVICE_INVALID_ARGUMENT);
    CHECK(time_service_update(
        &service, &utc, 0u, TIME_SERVICE_SOURCE_UNKNOWN, 0u) ==
        TIME_SERVICE_INVALID_ARGUMENT);
    CHECK(time_service_update(&service, &utc, 0u, 99u, 0u) ==
        TIME_SERVICE_INVALID_ARGUMENT);
    utc.nanosecond = UT_TIME_NANOSECONDS_PER_SECOND;
    CHECK(time_service_update(
        &service, &utc, 0u, TIME_SERVICE_SOURCE_MANUAL, 0u) ==
        TIME_SERVICE_INVALID_ARGUMENT);
    CHECK(time_service_get_status(NULL, &status) ==
        TIME_SERVICE_INVALID_ARGUMENT);
    CHECK(time_service_get_status(&service, NULL) ==
        TIME_SERVICE_INVALID_ARGUMENT);
    CHECK(time_service_now(NULL, 0u, NULL) == TIME_SERVICE_INVALID_ARGUMENT);
    return 0;
}

/** @brief Time Service単体テストを実行する。 @return 全成功時0。 */
int main(void)
{
    CHECK(test_synchronized_now() == 0);
    CHECK(test_errors() == 0);
    CHECK(test_update_and_invariants() == 0);
    CHECK(test_invalid() == 0);
    return 0;
}
