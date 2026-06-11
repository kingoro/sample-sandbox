/**
 * @file time_service.c
 * @brief Monotonic経過時間を使うUTC同期状態実装。
 */
#include "time_service.h"

#include <limits.h>
#include <stddef.h>

/**
 * 同期sourceとして有効か返す。
 *
 * @param source 判定するsource。
 * @return 同期可能sourceならtrue。
 */
static bool valid_source(time_service_source_t source)
{
    return source >= TIME_SERVICE_SOURCE_RTC &&
        source <= TIME_SERVICE_SOURCE_MANUAL;
}

/**
 * Service stateのfield組合せを検証する。
 *
 * @param service 検証するstate。
 * @return 正常な未同期または同期stateならtrue。
 */
static bool valid_state(const time_service_t *service)
{
    if (service == NULL) {
        return false;
    }
    if (!service->available && !service->synchronized) {
        return service->source == TIME_SERVICE_SOURCE_UNKNOWN;
    }
    return service->available && service->synchronized &&
        valid_source(service->source) &&
        ut_time_utc_is_valid(&service->synchronized_utc);
}

time_service_result_t time_service_init(time_service_t *service)
{
    if (service == NULL) {
        return TIME_SERVICE_INVALID_ARGUMENT;
    }
    service->synchronized_utc.seconds = 0;
    service->synchronized_utc.nanosecond = 0u;
    service->last_sync_monotonic_ns = 0u;
    service->uncertainty_ns = 0u;
    service->source = TIME_SERVICE_SOURCE_UNKNOWN;
    service->available = false;
    service->synchronized = false;
    return TIME_SERVICE_OK;
}

time_service_result_t time_service_update(
    time_service_t *service,
    const ut_time_utc_t *utc,
    ut_time_tick_ns_t monotonic_ns,
    time_service_source_t source,
    uint64_t uncertainty_ns)
{
    if (!valid_state(service) || !ut_time_utc_is_valid(utc) ||
        !valid_source(source)) {
        return TIME_SERVICE_INVALID_ARGUMENT;
    }
    if (service->synchronized &&
        monotonic_ns < service->last_sync_monotonic_ns) {
        return TIME_SERVICE_MONOTONIC_REGRESSION;
    }
    service->synchronized_utc = *utc;
    service->last_sync_monotonic_ns = monotonic_ns;
    service->uncertainty_ns = uncertainty_ns;
    service->source = source;
    service->available = true;
    service->synchronized = true;
    return TIME_SERVICE_OK;
}

void time_service_set_unavailable(time_service_t *service)
{
    if (service != NULL) {
        service->available = false;
        service->synchronized = false;
        service->source = TIME_SERVICE_SOURCE_UNKNOWN;
    }
}

time_service_result_t time_service_now(
    const time_service_t *service,
    ut_time_tick_ns_t monotonic_ns,
    ut_time_utc_t *utc)
{
    uint64_t elapsed;
    ut_time_duration_t duration;
    ut_time_utc_t temporary;
    ut_time_result_t status;
    if (service == NULL || utc == NULL) {
        return TIME_SERVICE_INVALID_ARGUMENT;
    }
    if (!valid_state(service)) {
        return TIME_SERVICE_INVALID_ARGUMENT;
    }
    if (!service->available) {
        return TIME_SERVICE_UNAVAILABLE;
    }
    if (monotonic_ns < service->last_sync_monotonic_ns) {
        return TIME_SERVICE_MONOTONIC_REGRESSION;
    }
    elapsed = monotonic_ns - service->last_sync_monotonic_ns;
    duration.seconds =
        (int64_t)(elapsed / UINT64_C(1000000000));
    duration.nanosecond =
        (uint32_t)(elapsed % UINT64_C(1000000000));
    status = ut_time_utc_add(
        &service->synchronized_utc, &duration, &temporary);
    if (status != UT_TIME_OK) {
        return TIME_SERVICE_OVERFLOW;
    }
    *utc = temporary;
    return TIME_SERVICE_OK;
}

time_service_result_t time_service_get_status(
    const time_service_t *service,
    time_service_status_t *status)
{
    time_service_status_t temporary;
    if (status == NULL || !valid_state(service)) {
        return TIME_SERVICE_INVALID_ARGUMENT;
    }
    temporary.available = service->available;
    temporary.synchronized = service->synchronized;
    temporary.source = service->source;
    temporary.uncertainty_ns = service->uncertainty_ns;
    temporary.last_sync_monotonic_ns = service->last_sync_monotonic_ns;
    *status = temporary;
    return TIME_SERVICE_OK;
}
