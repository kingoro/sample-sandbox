/**
 * @file utility_time.c
 * @brief Time Foundationの正規化、単位変換、duration演算実装。
 */
#include "utility_time.h"

#include <limits.h>
#include <stddef.h>

/**
 * 符号付き値を正規化した秒と正の剰余へ分割する。
 *
 * @param value 分割する値。
 * @param units_per_second 1秒あたりの単位数。
 * @param seconds 正規化秒の出力先。
 * @param fraction 正の剰余の出力先。
 */
static void split_units(
    int64_t value,
    int64_t units_per_second,
    int64_t *seconds,
    uint32_t *fraction)
{
    int64_t quotient = value / units_per_second;
    int64_t remainder = value % units_per_second;
    if (remainder < 0) {
        quotient--;
        remainder += units_per_second;
    }
    *seconds = quotient;
    *fraction = (uint32_t)remainder;
}

/**
 * 正規化値を指定単位へ変換する。
 *
 * @param seconds 正規化秒。
 * @param nanosecond 秒内nanosecond。
 * @param units_per_second 1秒あたりの出力単位数。
 * @param nanoseconds_per_unit 1出力単位あたりのnanosecond数。
 * @param value 変換結果の出力先。
 * @return 成功、引数不正、またはoverflow。
 */
static ut_time_result_t to_units(
    int64_t seconds,
    uint32_t nanosecond,
    int64_t units_per_second,
    uint32_t nanoseconds_per_unit,
    int64_t *value)
{
    int64_t fraction;
    if (value == NULL || nanosecond >= UT_TIME_NANOSECONDS_PER_SECOND) {
        return UT_TIME_INVALID_ARGUMENT;
    }
    fraction = (int64_t)(nanosecond / nanoseconds_per_unit);
    if (seconds > INT64_MAX / units_per_second ||
        seconds < INT64_MIN / units_per_second) {
        return UT_TIME_OUT_OF_RANGE;
    }
    if (seconds == INT64_MAX / units_per_second &&
        fraction > INT64_MAX % units_per_second) {
        return UT_TIME_OUT_OF_RANGE;
    }
    *value = seconds * units_per_second + fraction;
    return UT_TIME_OK;
}

bool ut_time_utc_is_valid(const ut_time_utc_t *utc)
{
    return utc != NULL &&
        utc->nanosecond < UT_TIME_NANOSECONDS_PER_SECOND;
}

bool ut_time_duration_is_valid(const ut_time_duration_t *duration)
{
    return duration != NULL &&
        duration->nanosecond < UT_TIME_NANOSECONDS_PER_SECOND;
}

ut_time_result_t ut_time_utc_from_seconds(int64_t value, ut_time_utc_t *utc)
{
    if (utc == NULL) {
        return UT_TIME_INVALID_ARGUMENT;
    }
    utc->seconds = value;
    utc->nanosecond = 0u;
    return UT_TIME_OK;
}

/**
 * 指定subsecond単位からUTCへ変換する。
 *
 * @param value Unix epochからの単位数。
 * @param units_per_second 1秒あたりの入力単位数。
 * @param nanoseconds_per_unit 1入力単位あたりのnanosecond数。
 * @param utc UTC出力先。
 * @return 成功または引数不正。
 */
static ut_time_result_t from_units(
    int64_t value,
    int64_t units_per_second,
    uint32_t nanoseconds_per_unit,
    ut_time_utc_t *utc)
{
    uint32_t fraction;
    if (utc == NULL) {
        return UT_TIME_INVALID_ARGUMENT;
    }
    split_units(value, units_per_second, &utc->seconds, &fraction);
    utc->nanosecond = fraction * nanoseconds_per_unit;
    return UT_TIME_OK;
}

ut_time_result_t ut_time_utc_from_milliseconds(
    int64_t value,
    ut_time_utc_t *utc)
{
    return from_units(value, INT64_C(1000), UINT32_C(1000000), utc);
}

ut_time_result_t ut_time_utc_from_microseconds(
    int64_t value,
    ut_time_utc_t *utc)
{
    return from_units(value, INT64_C(1000000), UINT32_C(1000), utc);
}

ut_time_result_t ut_time_utc_from_nanoseconds(
    int64_t value,
    ut_time_utc_t *utc)
{
    return from_units(
        value,
        INT64_C(1000000000),
        UINT32_C(1),
        utc);
}

ut_time_result_t ut_time_utc_to_seconds(
    const ut_time_utc_t *utc,
    int64_t *value)
{
    if (!ut_time_utc_is_valid(utc) || value == NULL) {
        return UT_TIME_INVALID_ARGUMENT;
    }
    *value = utc->seconds;
    return UT_TIME_OK;
}

ut_time_result_t ut_time_utc_to_milliseconds(
    const ut_time_utc_t *utc,
    int64_t *value)
{
    if (!ut_time_utc_is_valid(utc)) {
        return UT_TIME_INVALID_ARGUMENT;
    }
    return to_units(
        utc->seconds, utc->nanosecond, INT64_C(1000), UINT32_C(1000000),
        value);
}

ut_time_result_t ut_time_utc_to_microseconds(
    const ut_time_utc_t *utc,
    int64_t *value)
{
    if (!ut_time_utc_is_valid(utc)) {
        return UT_TIME_INVALID_ARGUMENT;
    }
    return to_units(
        utc->seconds, utc->nanosecond, INT64_C(1000000), UINT32_C(1000),
        value);
}

ut_time_result_t ut_time_utc_to_nanoseconds(
    const ut_time_utc_t *utc,
    int64_t *value)
{
    if (!ut_time_utc_is_valid(utc)) {
        return UT_TIME_INVALID_ARGUMENT;
    }
    return to_units(
        utc->seconds, utc->nanosecond, INT64_C(1000000000), UINT32_C(1),
        value);
}

ut_time_result_t ut_time_duration_from_nanoseconds(
    int64_t value,
    ut_time_duration_t *duration)
{
    if (duration == NULL) {
        return UT_TIME_INVALID_ARGUMENT;
    }
    split_units(
        value, INT64_C(1000000000), &duration->seconds,
        &duration->nanosecond);
    return UT_TIME_OK;
}

ut_time_result_t ut_time_duration_to_nanoseconds(
    const ut_time_duration_t *duration,
    int64_t *value)
{
    if (!ut_time_duration_is_valid(duration)) {
        return UT_TIME_INVALID_ARGUMENT;
    }
    return to_units(
        duration->seconds, duration->nanosecond, INT64_C(1000000000),
        UINT32_C(1), value);
}

/**
 * 2つの正規化値を加算する。
 *
 * @param left_seconds 左辺秒。
 * @param left_nanosecond 左辺nanosecond。
 * @param right_seconds 右辺秒。
 * @param right_nanosecond 右辺nanosecond。
 * @param result 加算結果の出力先。
 * @return 成功またはoverflow。
 */
static ut_time_result_t add_normalized(
    int64_t left_seconds,
    uint32_t left_nanosecond,
    int64_t right_seconds,
    uint32_t right_nanosecond,
    ut_time_utc_t *result)
{
    uint64_t nanosecond =
        (uint64_t)left_nanosecond + (uint64_t)right_nanosecond;
    uint32_t carry =
        nanosecond >= UT_TIME_NANOSECONDS_PER_SECOND ? 1u : 0u;
    int64_t adjusted_right;
    int64_t seconds;

    if (carry != 0u && right_seconds == INT64_MAX) {
        if (left_seconds >= 0) {
            return UT_TIME_OUT_OF_RANGE;
        }
        seconds = (INT64_MAX + left_seconds) + INT64_C(1);
    } else {
        adjusted_right = right_seconds + (int64_t)carry;
        if ((adjusted_right > 0 &&
             left_seconds > INT64_MAX - adjusted_right) ||
            (adjusted_right < 0 &&
             left_seconds < INT64_MIN - adjusted_right)) {
            return UT_TIME_OUT_OF_RANGE;
        }
        seconds = left_seconds + adjusted_right;
    }
    result->seconds = seconds;
    result->nanosecond =
        (uint32_t)(nanosecond % UT_TIME_NANOSECONDS_PER_SECOND);
    return UT_TIME_OK;
}

/**
 * `left - right - borrow`をoverflow安全に計算する。
 *
 * borrowは0または1。rightが負で中間の`left - right`だけがoverflowしても、
 * borrow適用後の最終値が表現可能なら成功する。
 *
 * @param left 左辺秒。
 * @param right 減算する秒。
 * @param borrow nanosecond正規化で借りる秒。
 * @param result 秒差の出力先。
 * @return 成功またはoverflow。
 */
static ut_time_result_t subtract_seconds(
    int64_t left,
    int64_t right,
    uint32_t borrow,
    int64_t *result)
{
    int64_t adjusted_right;
    if (borrow != 0u && right == INT64_MAX) {
        if (left < 0) {
            return UT_TIME_OUT_OF_RANGE;
        }
        *result = INT64_MIN + left;
        return UT_TIME_OK;
    }
    adjusted_right = right + (int64_t)borrow;
    if ((adjusted_right > 0 && left < INT64_MIN + adjusted_right) ||
        (adjusted_right < 0 && left > INT64_MAX + adjusted_right)) {
        return UT_TIME_OUT_OF_RANGE;
    }
    *result = left - adjusted_right;
    return UT_TIME_OK;
}

ut_time_result_t ut_time_utc_add(
    const ut_time_utc_t *utc,
    const ut_time_duration_t *duration,
    ut_time_utc_t *result)
{
    ut_time_utc_t temporary;
    ut_time_result_t status;
    if (!ut_time_utc_is_valid(utc) ||
        !ut_time_duration_is_valid(duration) || result == NULL) {
        return UT_TIME_INVALID_ARGUMENT;
    }
    status = add_normalized(
        utc->seconds, utc->nanosecond, duration->seconds,
        duration->nanosecond, &temporary);
    if (status == UT_TIME_OK) {
        *result = temporary;
    }
    return status;
}

ut_time_result_t ut_time_utc_subtract(
    const ut_time_utc_t *utc,
    const ut_time_duration_t *duration,
    ut_time_utc_t *result)
{
    ut_time_utc_t temporary;
    uint32_t borrow;
    ut_time_result_t status;
    if (!ut_time_utc_is_valid(utc) ||
        !ut_time_duration_is_valid(duration) || result == NULL) {
        return UT_TIME_INVALID_ARGUMENT;
    }
    borrow = utc->nanosecond < duration->nanosecond ? 1u : 0u;
    status = subtract_seconds(
        utc->seconds, duration->seconds, borrow, &temporary.seconds);
    if (status != UT_TIME_OK) {
        return status;
    }
    if (borrow == 0u) {
        temporary.nanosecond = utc->nanosecond - duration->nanosecond;
    } else {
        temporary.nanosecond = UT_TIME_NANOSECONDS_PER_SECOND -
            (duration->nanosecond - utc->nanosecond);
    }
    *result = temporary;
    return UT_TIME_OK;
}

ut_time_result_t ut_time_utc_difference(
    const ut_time_utc_t *end,
    const ut_time_utc_t *start,
    ut_time_duration_t *result)
{
    ut_time_duration_t temporary;
    uint32_t borrow;
    ut_time_result_t status;
    if (!ut_time_utc_is_valid(end) || !ut_time_utc_is_valid(start) ||
        result == NULL) {
        return UT_TIME_INVALID_ARGUMENT;
    }
    borrow = end->nanosecond < start->nanosecond ? 1u : 0u;
    status = subtract_seconds(
        end->seconds, start->seconds, borrow, &temporary.seconds);
    if (status != UT_TIME_OK) {
        return status;
    }
    if (borrow == 0u) {
        temporary.nanosecond = end->nanosecond - start->nanosecond;
    } else {
        temporary.nanosecond = UT_TIME_NANOSECONDS_PER_SECOND -
            (start->nanosecond - end->nanosecond);
    }
    *result = temporary;
    return UT_TIME_OK;
}

ut_time_result_t ut_time_utc_compare(
    const ut_time_utc_t *left,
    const ut_time_utc_t *right,
    int *ordering)
{
    if (!ut_time_utc_is_valid(left) || !ut_time_utc_is_valid(right) ||
        ordering == NULL) {
        return UT_TIME_INVALID_ARGUMENT;
    }
    if (left->seconds != right->seconds) {
        *ordering = left->seconds < right->seconds ? -1 : 1;
    } else if (left->nanosecond != right->nanosecond) {
        *ordering = left->nanosecond < right->nanosecond ? -1 : 1;
    } else {
        *ordering = 0;
    }
    return UT_TIME_OK;
}
