/**
 * @file utility_time_rfc3339.c
 * @brief Proleptic Gregorian calendarとRFC3339 UTC変換実装。
 */
#include "utility_time.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

/** RFC3339 format可能な最小Unix秒。 */
#define MIN_RFC3339_SECONDS INT64_C(-62135596800)
/** RFC3339 format可能な最大Unix秒。 */
#define MAX_RFC3339_SECONDS INT64_C(253402300799)

/**
 * Civil dateを1970-01-01からの日数へ変換する。
 *
 * @param year Gregorian year。
 * @param month 1始まりの月。
 * @param day 1始まりの日。
 * @return Unix epochからの日数。
 */
static int64_t days_from_civil(int64_t year, unsigned month, unsigned day)
{
    int64_t era;
    unsigned year_of_era;
    unsigned day_of_year;
    unsigned day_of_era;
    year -= month <= 2u ? 1 : 0;
    era = (year >= 0 ? year : year - 399) / 400;
    year_of_era = (unsigned)(year - era * 400);
    day_of_year =
        (153u * (month > 2u ? month - 3u : month + 9u) + 2u) / 5u +
        day - 1u;
    day_of_era =
        year_of_era * 365u + year_of_era / 4u - year_of_era / 100u +
        day_of_year;
    return era * INT64_C(146097) + (int64_t)day_of_era - INT64_C(719468);
}

/**
 * 1970-01-01からの日数をCivil dateへ変換する。
 *
 * @param days Unix epochからの日数。
 * @param year year出力先。
 * @param month month出力先。
 * @param day day出力先。
 */
static void civil_from_days(
    int64_t days,
    int *year,
    unsigned *month,
    unsigned *day)
{
    int64_t era;
    unsigned day_of_era;
    unsigned year_of_era;
    int year_value;
    unsigned day_of_year;
    unsigned month_prime;
    days += INT64_C(719468);
    era = (days >= 0 ? days : days - 146096) / 146097;
    day_of_era = (unsigned)(days - era * 146097);
    year_of_era =
        (day_of_era - day_of_era / 1460u + day_of_era / 36524u -
         day_of_era / 146096u) /
        365u;
    year_value = (int)(year_of_era + (unsigned)(era * 400));
    day_of_year = day_of_era -
        (365u * year_of_era + year_of_era / 4u - year_of_era / 100u);
    month_prime = (5u * day_of_year + 2u) / 153u;
    *day = day_of_year - (153u * month_prime + 2u) / 5u + 1u;
    *month = month_prime < 10u ? month_prime + 3u : month_prime - 9u;
    *year = year_value + (*month <= 2u ? 1 : 0);
}

/**
 * Gregorian leap yearか返す。
 *
 * @param year 判定するyear。
 * @return leap yearならtrue。
 */
static bool is_leap_year(unsigned year)
{
    return year % 4u == 0u && (year % 100u != 0u || year % 400u == 0u);
}

/**
 * Gregorian年月日の妥当性を返す。
 *
 * @param year Gregorian year。
 * @param month month。
 * @param day day。
 * @return 対応範囲の実在日ならtrue。
 */
static bool valid_date(unsigned year, unsigned month, unsigned day)
{
    static const unsigned month_days[12] = {
        31u, 28u, 31u, 30u, 31u, 30u,
        31u, 31u, 30u, 31u, 30u, 31u
    };
    unsigned maximum;
    if (year < 1u || year > 9999u || month < 1u || month > 12u) {
        return false;
    }
    maximum = month_days[month - 1u];
    if (month == 2u && is_leap_year(year)) {
        maximum++;
    }
    return day >= 1u && day <= maximum;
}

/**
 * 固定桁の10進数を読む。
 *
 * @param text 入力先頭。
 * @param count 読む桁数。
 * @param value 数値出力先。
 * @return 全桁が数字ならtrue。
 */
static bool parse_digits(const char *text, size_t count, unsigned *value)
{
    size_t index;
    unsigned result = 0u;
    for (index = 0u; index < count; index++) {
        if (text[index] < '0' || text[index] > '9') {
            return false;
        }
        result = result * 10u + (unsigned)(text[index] - '0');
    }
    *value = result;
    return true;
}

/**
 * RFC3339の固定位置date/time fieldを読む。
 *
 * @param text 入力文字列。
 * @param year year出力先。
 * @param month month出力先。
 * @param day day出力先。
 * @param hour hour出力先。
 * @param minute minute出力先。
 * @param second second出力先。
 * @return delimiterと全fieldが構文上有効ならtrue。
 */
static bool parse_date_time(
    const char *text,
    unsigned *year,
    unsigned *month,
    unsigned *day,
    unsigned *hour,
    unsigned *minute,
    unsigned *second)
{
    return text[4] == '-' && text[7] == '-' && text[10] == 'T' &&
        text[13] == ':' && text[16] == ':' &&
        parse_digits(text, 4u, year) &&
        parse_digits(text + 5, 2u, month) &&
        parse_digits(text + 8, 2u, day) &&
        parse_digits(text + 11, 2u, hour) &&
        parse_digits(text + 14, 2u, minute) &&
        parse_digits(text + 17, 2u, second);
}

/**
 * Optional fractional secondと終端Zを読む。
 *
 * @param text 入力文字列。
 * @param length 入力長。
 * @param nanosecond 正規化nanosecond出力先。
 * @return fractionが0または1から9桁で末尾がZならtrue。
 */
static bool parse_fraction_and_zone(
    const char *text,
    size_t length,
    uint32_t *nanosecond)
{
    size_t index = 19u;
    size_t digits = 0u;
    uint32_t value = 0u;
    if (text[index] == '.') {
        index++;
        while (index < length && text[index] >= '0' && text[index] <= '9') {
            if (digits == 9u) {
                return false;
            }
            value = value * 10u + (uint32_t)(text[index] - '0');
            digits++;
            index++;
        }
        if (digits == 0u) {
            return false;
        }
        while (digits < 9u) {
            value *= 10u;
            digits++;
        }
    }
    if (index + 1u != length || text[index] != 'Z') {
        return false;
    }
    *nanosecond = value;
    return true;
}

ut_time_result_t ut_time_rfc3339_format(
    const ut_time_utc_t *utc,
    char *buffer,
    size_t size)
{
    int year;
    unsigned month;
    unsigned day;
    int64_t days;
    int64_t second_of_day;
    int written;
    if (!ut_time_utc_is_valid(utc) || buffer == NULL) {
        return UT_TIME_INVALID_ARGUMENT;
    }
    if (size < UT_TIME_RFC3339_BUFFER_SIZE) {
        return UT_TIME_BUFFER_TOO_SMALL;
    }
    if (utc->seconds < MIN_RFC3339_SECONDS ||
        utc->seconds > MAX_RFC3339_SECONDS) {
        return UT_TIME_OUT_OF_RANGE;
    }
    days = utc->seconds / INT64_C(86400);
    second_of_day = utc->seconds % INT64_C(86400);
    if (second_of_day < 0) {
        days--;
        second_of_day += INT64_C(86400);
    }
    civil_from_days(days, &year, &month, &day);
    written = snprintf(
        buffer, size, "%04d-%02u-%02uT%02" PRId64 ":%02" PRId64
        ":%02" PRId64 ".%09" PRIu32 "Z",
        year, month, day, second_of_day / INT64_C(3600),
        second_of_day / INT64_C(60) % INT64_C(60),
        second_of_day % INT64_C(60), utc->nanosecond);
    return written == 30 ? UT_TIME_OK : UT_TIME_OUT_OF_RANGE;
}

ut_time_result_t ut_time_rfc3339_parse(
    const char *text,
    ut_time_utc_t *utc)
{
    unsigned year;
    unsigned month;
    unsigned day;
    unsigned hour;
    unsigned minute;
    unsigned second;
    uint32_t nanosecond = 0u;
    size_t length;
    int64_t days;
    if (text == NULL || utc == NULL) {
        return UT_TIME_INVALID_ARGUMENT;
    }
    length = strlen(text);
    if (length < 20u || length > 30u ||
        !parse_date_time(
            text, &year, &month, &day, &hour, &minute, &second)) {
        return UT_TIME_INVALID_ARGUMENT;
    }
    if (!parse_fraction_and_zone(text, length, &nanosecond) ||
        !valid_date(year, month, day) || hour > 23u || minute > 59u ||
        second > 59u) {
        return UT_TIME_INVALID_ARGUMENT;
    }
    days = days_from_civil((int64_t)year, month, day);
    utc->seconds = days * INT64_C(86400) + (int64_t)hour * INT64_C(3600) +
        (int64_t)minute * INT64_C(60) + (int64_t)second;
    utc->nanosecond = nanosecond;
    return UT_TIME_OK;
}
