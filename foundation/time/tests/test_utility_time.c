/**
 * @file test_utility_time.c
 * @brief Time Foundationの変換、演算、RFC3339単体テスト。
 */
#include "utility_time.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

/** 条件失敗時にtestを終了する。 */
#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
            return 1; \
        } \
    } while (0)

/** @brief 単位変換と負値正規化を検証する。 @return 成功時0。 */
static int test_units(void)
{
    ut_time_utc_t utc;
    ut_time_duration_t duration;
    int64_t value;
    CHECK(ut_time_utc_from_seconds(3, &utc) == UT_TIME_OK);
    CHECK(utc.seconds == 3 && utc.nanosecond == 0u);
    CHECK(ut_time_utc_from_milliseconds(-1, &utc) == UT_TIME_OK);
    CHECK(utc.seconds == -1 && utc.nanosecond == 999000000u);
    CHECK(ut_time_utc_to_milliseconds(&utc, &value) == UT_TIME_OK);
    CHECK(value == -1);
    CHECK(ut_time_utc_from_microseconds(-1, &utc) == UT_TIME_OK);
    CHECK(utc.seconds == -1 && utc.nanosecond == 999999000u);
    CHECK(ut_time_utc_to_microseconds(&utc, &value) == UT_TIME_OK);
    CHECK(value == -1);
    CHECK(ut_time_utc_from_nanoseconds(-1, &utc) == UT_TIME_OK);
    CHECK(utc.seconds == -1 && utc.nanosecond == 999999999u);
    CHECK(ut_time_utc_to_nanoseconds(&utc, &value) == UT_TIME_OK);
    CHECK(value == -1);
    CHECK(ut_time_utc_to_seconds(&utc, &value) == UT_TIME_OK && value == -1);
    CHECK(ut_time_duration_from_nanoseconds(-500000000, &duration) == UT_TIME_OK);
    CHECK(duration.seconds == -1 && duration.nanosecond == 500000000u);
    CHECK(ut_time_duration_to_nanoseconds(&duration, &value) == UT_TIME_OK);
    CHECK(value == -500000000);
    return 0;
}

/** @brief UTC演算、比較、overflowを検証する。 @return 成功時0。 */
static int test_arithmetic(void)
{
    const ut_time_utc_t base = {10, 800000000u};
    const ut_time_duration_t positive = {1, 500000000u};
    ut_time_duration_t difference;
    ut_time_utc_t result;
    int ordering;
    CHECK(ut_time_utc_add(&base, &positive, &result) == UT_TIME_OK);
    CHECK(result.seconds == 12 && result.nanosecond == 300000000u);
    CHECK(ut_time_utc_subtract(&result, &positive, &result) == UT_TIME_OK);
    CHECK(result.seconds == base.seconds && result.nanosecond == base.nanosecond);
    CHECK(ut_time_utc_difference(&result, &base, &difference) == UT_TIME_OK);
    CHECK(difference.seconds == 0 && difference.nanosecond == 0u);
    result.seconds = 9;
    result.nanosecond = 900000000u;
    CHECK(ut_time_utc_difference(&result, &base, &difference) == UT_TIME_OK);
    CHECK(difference.seconds == -1 && difference.nanosecond == 100000000u);
    CHECK(ut_time_utc_compare(&result, &base, &ordering) == UT_TIME_OK);
    CHECK(ordering == -1);
    CHECK(ut_time_utc_compare(&base, &result, &ordering) == UT_TIME_OK);
    CHECK(ordering == 1);
    CHECK(ut_time_utc_compare(&base, &base, &ordering) == UT_TIME_OK);
    CHECK(ordering == 0);
    result.seconds = INT64_MAX;
    result.nanosecond = 900000000u;
    CHECK(ut_time_utc_add(&result, &positive, &result) == UT_TIME_OUT_OF_RANGE);
    difference.seconds = INT64_MIN;
    difference.nanosecond = 0u;
    CHECK(ut_time_utc_subtract(&base, &difference, &result) ==
        UT_TIME_OUT_OF_RANGE);
    CHECK(ut_time_utc_subtract(
        &(ut_time_utc_t){0, 0u},
        &(ut_time_duration_t){INT64_MIN, 1u},
        &result) == UT_TIME_OK);
    CHECK(result.seconds == INT64_MAX && result.nanosecond == 999999999u);
    CHECK(ut_time_utc_difference(
        &(ut_time_utc_t){INT64_MAX, 0u},
        &(ut_time_utc_t){-1, 1u},
        &difference) == UT_TIME_OK);
    CHECK(difference.seconds == INT64_MAX &&
        difference.nanosecond == 999999999u);
    CHECK(ut_time_utc_add(
        &(ut_time_utc_t){INT64_MIN, 500000000u},
        &(ut_time_duration_t){-1, 500000000u},
        &result) == UT_TIME_OK);
    CHECK(result.seconds == INT64_MIN && result.nanosecond == 0u);
    CHECK(ut_time_utc_add(
        &(ut_time_utc_t){INT64_MIN, 500000000u},
        &(ut_time_duration_t){INT64_MAX, 500000000u},
        &result) == UT_TIME_OK);
    CHECK(result.seconds == 0 && result.nanosecond == 0u);
    return 0;
}

/** @brief 全演算の失敗時out不変契約を検証する。 @return 成功時0。 */
static int test_failure_preserves_outputs(void)
{
    const ut_time_utc_t invalid_utc = {0, UT_TIME_NANOSECONDS_PER_SECOND};
    const ut_time_duration_t invalid_duration = {
        0, UT_TIME_NANOSECONDS_PER_SECOND
    };
    ut_time_utc_t utc_out = {123, 456u};
    ut_time_duration_t duration_out = {123, 456u};
    int64_t integer_out = 123;
    int ordering_out = 123;
    char text_out[UT_TIME_RFC3339_BUFFER_SIZE] = "unchanged";

    CHECK(ut_time_utc_to_milliseconds(&invalid_utc, &integer_out) ==
        UT_TIME_INVALID_ARGUMENT);
    CHECK(integer_out == 123);
    CHECK(ut_time_duration_to_nanoseconds(
        &invalid_duration, &integer_out) == UT_TIME_INVALID_ARGUMENT);
    CHECK(integer_out == 123);
    CHECK(ut_time_utc_add(
        &(ut_time_utc_t){INT64_MAX, 999999999u},
        &(ut_time_duration_t){0, 1u}, &utc_out) == UT_TIME_OUT_OF_RANGE);
    CHECK(utc_out.seconds == 123 && utc_out.nanosecond == 456u);
    CHECK(ut_time_utc_add(
        &(ut_time_utc_t){INT64_MIN, 499999999u},
        &(ut_time_duration_t){-1, 500000000u},
        &utc_out) == UT_TIME_OUT_OF_RANGE);
    CHECK(utc_out.seconds == 123 && utc_out.nanosecond == 456u);
    CHECK(ut_time_utc_add(
        &(ut_time_utc_t){0, 500000000u},
        &(ut_time_duration_t){INT64_MAX, 500000000u},
        &utc_out) == UT_TIME_OUT_OF_RANGE);
    CHECK(utc_out.seconds == 123 && utc_out.nanosecond == 456u);
    CHECK(ut_time_utc_subtract(
        &(ut_time_utc_t){INT64_MIN, 0u},
        &(ut_time_duration_t){0, 1u}, &utc_out) == UT_TIME_OUT_OF_RANGE);
    CHECK(utc_out.seconds == 123 && utc_out.nanosecond == 456u);
    CHECK(ut_time_utc_difference(
        &(ut_time_utc_t){INT64_MAX, 1u},
        &(ut_time_utc_t){INT64_MIN, 0u},
        &duration_out) == UT_TIME_OUT_OF_RANGE);
    CHECK(duration_out.seconds == 123 && duration_out.nanosecond == 456u);
    CHECK(ut_time_utc_compare(
        &invalid_utc, &(ut_time_utc_t){0, 0u}, &ordering_out) ==
        UT_TIME_INVALID_ARGUMENT);
    CHECK(ordering_out == 123);
    CHECK(ut_time_rfc3339_format(
        &(ut_time_utc_t){INT64_MAX, 0u}, text_out, sizeof(text_out)) ==
        UT_TIME_OUT_OF_RANGE);
    CHECK(strcmp(text_out, "unchanged") == 0);
    CHECK(ut_time_rfc3339_parse("invalid", &utc_out) ==
        UT_TIME_INVALID_ARGUMENT);
    CHECK(utc_out.seconds == 123 && utc_out.nanosecond == 456u);
    return 0;
}

/** @brief Gregorian calendarとRFC3339 Z限定契約を検証する。 @return 成功時0。 */
static int test_rfc3339(void)
{
    ut_time_utc_t utc;
    char buffer[UT_TIME_RFC3339_BUFFER_SIZE];
    CHECK(ut_time_rfc3339_parse(
        "2000-02-29T23:59:59.123456789Z", &utc) == UT_TIME_OK);
    CHECK(ut_time_rfc3339_format(&utc, buffer, sizeof(buffer)) == UT_TIME_OK);
    CHECK(strcmp(buffer, "2000-02-29T23:59:59.123456789Z") == 0);
    CHECK(ut_time_rfc3339_parse("1970-01-01T00:00:00Z", &utc) == UT_TIME_OK);
    CHECK(utc.seconds == 0 && utc.nanosecond == 0u);
    CHECK(ut_time_rfc3339_parse("1969-12-31T23:59:59.5Z", &utc) == UT_TIME_OK);
    CHECK(utc.seconds == -1 && utc.nanosecond == 500000000u);
    CHECK(ut_time_rfc3339_parse("1900-02-29T00:00:00Z", &utc) ==
        UT_TIME_INVALID_ARGUMENT);
    CHECK(ut_time_rfc3339_parse("2000-02-30T00:00:00Z", &utc) ==
        UT_TIME_INVALID_ARGUMENT);
    CHECK(ut_time_rfc3339_parse("2000-01-01T00:00:60Z", &utc) ==
        UT_TIME_INVALID_ARGUMENT);
    CHECK(ut_time_rfc3339_parse("2000-01-01T00:00:00+00:00", &utc) ==
        UT_TIME_INVALID_ARGUMENT);
    CHECK(ut_time_rfc3339_parse("2000-01-01T00:00:00.Z", &utc) ==
        UT_TIME_INVALID_ARGUMENT);
    CHECK(ut_time_rfc3339_parse("2000-01-01T00:00:00.1234567890Z", &utc) ==
        UT_TIME_INVALID_ARGUMENT);
    utc.seconds = INT64_MAX;
    utc.nanosecond = 0u;
    CHECK(ut_time_rfc3339_format(&utc, buffer, sizeof(buffer)) ==
        UT_TIME_OUT_OF_RANGE);
    utc.seconds = 0;
    CHECK(ut_time_rfc3339_format(&utc, buffer, sizeof(buffer) - 1u) ==
        UT_TIME_BUFFER_TOO_SMALL);
    return 0;
}

/** @brief 不正引数と変換overflowを検証する。 @return 成功時0。 */
static int test_invalid(void)
{
    ut_time_utc_t invalid = {0, UT_TIME_NANOSECONDS_PER_SECOND};
    ut_time_duration_t invalid_duration = {0, UT_TIME_NANOSECONDS_PER_SECOND};
    ut_time_utc_t utc = {INT64_MAX, 0u};
    ut_time_utc_t minimum = {INT64_MIN, 0u};
    ut_time_utc_t result;
    ut_time_duration_t duration;
    int64_t value;
    int ordering;
    CHECK(!ut_time_utc_is_valid(NULL));
    CHECK(!ut_time_utc_is_valid(&invalid));
    CHECK(!ut_time_duration_is_valid(NULL));
    CHECK(!ut_time_duration_is_valid(&invalid_duration));
    CHECK(ut_time_utc_from_seconds(0, NULL) == UT_TIME_INVALID_ARGUMENT);
    CHECK(ut_time_utc_from_milliseconds(0, NULL) == UT_TIME_INVALID_ARGUMENT);
    CHECK(ut_time_utc_from_microseconds(0, NULL) == UT_TIME_INVALID_ARGUMENT);
    CHECK(ut_time_utc_from_nanoseconds(0, NULL) == UT_TIME_INVALID_ARGUMENT);
    CHECK(ut_time_duration_from_nanoseconds(0, NULL) ==
        UT_TIME_INVALID_ARGUMENT);
    CHECK(ut_time_utc_to_seconds(NULL, &value) == UT_TIME_INVALID_ARGUMENT);
    CHECK(ut_time_utc_to_seconds(&utc, NULL) == UT_TIME_INVALID_ARGUMENT);
    CHECK(ut_time_utc_to_milliseconds(&invalid, &value) ==
        UT_TIME_INVALID_ARGUMENT);
    CHECK(ut_time_utc_to_milliseconds(&(ut_time_utc_t){0, 0u}, NULL) ==
        UT_TIME_INVALID_ARGUMENT);
    CHECK(ut_time_utc_to_milliseconds(&utc, &value) == UT_TIME_OUT_OF_RANGE);
    CHECK(ut_time_utc_to_microseconds(&(ut_time_utc_t){1, 5000u}, &value) ==
        UT_TIME_OK && value == 1000005);
    CHECK(ut_time_utc_to_microseconds(&minimum, &value) ==
        UT_TIME_OUT_OF_RANGE);
    CHECK(ut_time_utc_to_microseconds(&invalid, &value) ==
        UT_TIME_INVALID_ARGUMENT);
    CHECK(ut_time_utc_to_nanoseconds(&utc, &value) == UT_TIME_OUT_OF_RANGE);
    CHECK(ut_time_utc_to_nanoseconds(&invalid, &value) ==
        UT_TIME_INVALID_ARGUMENT);
    CHECK(ut_time_duration_to_nanoseconds(&invalid_duration, &value) ==
        UT_TIME_INVALID_ARGUMENT);
    duration.seconds = INT64_MAX;
    duration.nanosecond = 0u;
    CHECK(ut_time_duration_to_nanoseconds(&duration, &value) ==
        UT_TIME_OUT_OF_RANGE);
    duration.seconds = 0;
    duration.nanosecond = 0u;
    CHECK(ut_time_duration_to_nanoseconds(&duration, NULL) ==
        UT_TIME_INVALID_ARGUMENT);
    CHECK(ut_time_utc_add(NULL, &duration, &result) ==
        UT_TIME_INVALID_ARGUMENT);
    CHECK(ut_time_utc_add(&utc, &invalid_duration, &result) ==
        UT_TIME_INVALID_ARGUMENT);
    CHECK(ut_time_utc_add(&utc, &duration, NULL) ==
        UT_TIME_INVALID_ARGUMENT);
    duration.seconds = -1;
    duration.nanosecond = 0u;
    CHECK(ut_time_utc_add(&minimum, &duration, &result) ==
        UT_TIME_OUT_OF_RANGE);
    duration.seconds = 0;
    duration.nanosecond = 999999999u;
    utc.seconds = INT64_MAX;
    utc.nanosecond = 1u;
    CHECK(ut_time_utc_add(&utc, &duration, &result) ==
        UT_TIME_OUT_OF_RANGE);
    duration.seconds = INT64_MIN;
    duration.nanosecond = 1u;
    CHECK(ut_time_utc_subtract(&(ut_time_utc_t){0, 0u}, &duration, &result) ==
        UT_TIME_OK);
    CHECK(result.seconds == INT64_MAX && result.nanosecond == 999999999u);
    duration.seconds = 0;
    duration.nanosecond = 500000000u;
    CHECK(ut_time_utc_subtract(&(ut_time_utc_t){0, 0u}, &duration, &result) ==
        UT_TIME_OK);
    CHECK(result.seconds == -1 && result.nanosecond == 500000000u);
    CHECK(ut_time_utc_subtract(NULL, &duration, &result) ==
        UT_TIME_INVALID_ARGUMENT);
    CHECK(ut_time_utc_difference(NULL, &minimum, &duration) ==
        UT_TIME_INVALID_ARGUMENT);
    CHECK(ut_time_utc_difference(&utc, &minimum, &duration) ==
        UT_TIME_OUT_OF_RANGE);
    CHECK(ut_time_utc_difference(&minimum, &utc, &duration) ==
        UT_TIME_OUT_OF_RANGE);
    minimum.nanosecond = 0u;
    CHECK(ut_time_utc_difference(
        &minimum, &(ut_time_utc_t){0, 1u}, &duration) ==
        UT_TIME_OUT_OF_RANGE);
    CHECK(ut_time_utc_compare(&invalid, &utc, &ordering) ==
        UT_TIME_INVALID_ARGUMENT);
    CHECK(ut_time_utc_compare(&utc, &utc, NULL) == UT_TIME_INVALID_ARGUMENT);
    CHECK(ut_time_rfc3339_parse(NULL, &utc) == UT_TIME_INVALID_ARGUMENT);
    CHECK(ut_time_rfc3339_format(NULL, NULL, 0u) == UT_TIME_INVALID_ARGUMENT);
    return 0;
}

/** @brief RFC3339 calendar境界と書式検証分岐を検証する。 @return 成功時0。 */
static int test_rfc3339_boundaries(void)
{
    ut_time_utc_t utc;
    char buffer[UT_TIME_RFC3339_BUFFER_SIZE];
    CHECK(ut_time_rfc3339_parse("0001-01-01T00:00:00Z", &utc) == UT_TIME_OK);
    CHECK(ut_time_rfc3339_format(&utc, buffer, sizeof(buffer)) == UT_TIME_OK);
    CHECK(strcmp(buffer, "0001-01-01T00:00:00.000000000Z") == 0);
    CHECK(ut_time_rfc3339_parse("9999-12-31T23:59:59Z", &utc) == UT_TIME_OK);
    CHECK(ut_time_rfc3339_format(&utc, buffer, sizeof(buffer)) == UT_TIME_OK);
    CHECK(strcmp(buffer, "9999-12-31T23:59:59.000000000Z") == 0);
    CHECK(ut_time_rfc3339_parse("2004-02-29T00:00:00.1Z", &utc) == UT_TIME_OK);
    CHECK(utc.nanosecond == 100000000u);
    CHECK(ut_time_rfc3339_parse("2100-02-28T23:59:59Z", &utc) == UT_TIME_OK);
    CHECK(ut_time_rfc3339_parse("2100-02-29T00:00:00Z", &utc) ==
        UT_TIME_INVALID_ARGUMENT);
    CHECK(ut_time_rfc3339_parse("0000-01-01T00:00:00Z", &utc) ==
        UT_TIME_INVALID_ARGUMENT);
    CHECK(ut_time_rfc3339_parse("2000-00-01T00:00:00Z", &utc) ==
        UT_TIME_INVALID_ARGUMENT);
    CHECK(ut_time_rfc3339_parse("2000-13-01T00:00:00Z", &utc) ==
        UT_TIME_INVALID_ARGUMENT);
    CHECK(ut_time_rfc3339_parse("2000-01-00T00:00:00Z", &utc) ==
        UT_TIME_INVALID_ARGUMENT);
    CHECK(ut_time_rfc3339_parse("2000-04-31T00:00:00Z", &utc) ==
        UT_TIME_INVALID_ARGUMENT);
    CHECK(ut_time_rfc3339_parse("2000-01-01T24:00:00Z", &utc) ==
        UT_TIME_INVALID_ARGUMENT);
    CHECK(ut_time_rfc3339_parse("2000-01-01T00:60:00Z", &utc) ==
        UT_TIME_INVALID_ARGUMENT);
    CHECK(ut_time_rfc3339_parse("2000/01-01T00:00:00Z", &utc) ==
        UT_TIME_INVALID_ARGUMENT);
    CHECK(ut_time_rfc3339_parse("200A-01-01T00:00:00Z", &utc) ==
        UT_TIME_INVALID_ARGUMENT);
    CHECK(ut_time_rfc3339_parse("short", &utc) == UT_TIME_INVALID_ARGUMENT);
    CHECK(ut_time_rfc3339_parse("2000-01-01T00:00:00.123456789Zx", &utc) ==
        UT_TIME_INVALID_ARGUMENT);
    return 0;
}

/** @brief Time Foundation単体テストを実行する。 @return 全成功時0。 */
int main(void)
{
    CHECK(test_units() == 0);
    CHECK(test_arithmetic() == 0);
    CHECK(test_rfc3339() == 0);
    CHECK(test_rfc3339_boundaries() == 0);
    CHECK(test_invalid() == 0);
    CHECK(test_failure_preserves_outputs() == 0);
    return 0;
}
