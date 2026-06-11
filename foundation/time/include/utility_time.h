/**
 * @file utility_time.h
 * @brief Time Foundationの公開umbrella API。
 *
 * heap、thread、OS、clock、I/Oに依存しない。すべての関数は呼出側所有値だけを扱い、
 * 異なるobjectに対する同時呼出しを含めthread safeである。
 */
#ifndef UTILITY_TIME_H
#define UTILITY_TIME_H

#include "utility_time_types.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** RFC3339 UTCの最大文字列長を終端NUL込みで表す。 */
#define UT_TIME_RFC3339_BUFFER_SIZE 31u

/**
 * UTC timestampが正規化されているか返す。
 *
 * @param utc 検証するtimestamp。
 * @return non-NULLかつnanosecondが範囲内ならtrue。
 */
bool ut_time_utc_is_valid(const ut_time_utc_t *utc);

/**
 * Durationが正規化されているか返す。
 *
 * @param duration 検証するduration。
 * @return non-NULLかつnanosecondが範囲内ならtrue。
 */
bool ut_time_duration_is_valid(const ut_time_duration_t *duration);

/**
 * Unix秒からUTCへ変換する。
 *
 * @param value Unix秒。
 * @param utc 呼出側所有の出力先。
 * @return UT_TIME_OKまたはUT_TIME_INVALID_ARGUMENT。
 */
ut_time_result_t ut_time_utc_from_seconds(int64_t value, ut_time_utc_t *utc);

/**
 * Unix millisecondからUTCへoverflow安全に変換する。
 *
 * @param value Unix millisecond。
 * @param utc 呼出側所有の出力先。
 * @return UT_TIME_OKまたはUT_TIME_INVALID_ARGUMENT。
 */
ut_time_result_t ut_time_utc_from_milliseconds(
    int64_t value,
    ut_time_utc_t *utc);

/**
 * Unix microsecondからUTCへoverflow安全に変換する。
 *
 * @param value Unix microsecond。
 * @param utc 呼出側所有の出力先。
 * @return UT_TIME_OKまたはUT_TIME_INVALID_ARGUMENT。
 */
ut_time_result_t ut_time_utc_from_microseconds(
    int64_t value,
    ut_time_utc_t *utc);

/**
 * Unix nanosecondからUTCへoverflow安全に変換する。
 *
 * @param value Unix nanosecond。
 * @param utc 呼出側所有の出力先。
 * @return UT_TIME_OKまたはUT_TIME_INVALID_ARGUMENT。
 */
ut_time_result_t ut_time_utc_from_nanoseconds(
    int64_t value,
    ut_time_utc_t *utc);

/**
 * UTCの正規化seconds fieldをUnix秒として返す。
 *
 * @param utc 正規化済みtimestamp。
 * @param value 呼出側所有の出力先。
 * @return UT_TIME_OKまたはUT_TIME_INVALID_ARGUMENT。
 */
ut_time_result_t ut_time_utc_to_seconds(
    const ut_time_utc_t *utc,
    int64_t *value);

/**
 * UTCをUnix millisecondへ変換する。micro/nanosecond部分は切り捨てる。
 *
 * @param utc 正規化済みtimestamp。
 * @param value 呼出側所有の出力先。
 * @return 成功、引数不正、またはoverflow。
 */
ut_time_result_t ut_time_utc_to_milliseconds(
    const ut_time_utc_t *utc,
    int64_t *value);

/**
 * UTCをUnix microsecondへ変換する。nanosecond部分は切り捨てる。
 *
 * @param utc 正規化済みtimestamp。
 * @param value 呼出側所有の出力先。
 * @return 成功、引数不正、またはoverflow。
 */
ut_time_result_t ut_time_utc_to_microseconds(
    const ut_time_utc_t *utc,
    int64_t *value);

/**
 * UTCをUnix nanosecondへ変換する。
 *
 * @param utc 正規化済みtimestamp。
 * @param value 呼出側所有の出力先。
 * @return 成功、引数不正、またはoverflow。
 */
ut_time_result_t ut_time_utc_to_nanoseconds(
    const ut_time_utc_t *utc,
    int64_t *value);

/**
 * Nanosecond総数からdurationへ変換する。
 *
 * @param value 符号付きnanosecond。
 * @param duration 呼出側所有の出力先。
 * @return UT_TIME_OKまたはUT_TIME_INVALID_ARGUMENT。
 */
ut_time_result_t ut_time_duration_from_nanoseconds(
    int64_t value,
    ut_time_duration_t *duration);

/**
 * Durationをnanosecond総数へ変換する。
 *
 * @param duration 正規化済みduration。
 * @param value 呼出側所有の出力先。
 * @return 成功、引数不正、またはoverflow。
 */
ut_time_result_t ut_time_duration_to_nanoseconds(
    const ut_time_duration_t *duration,
    int64_t *value);

/**
 * UTCへdurationを加算する。
 *
 * @param utc 正規化済みtimestamp。
 * @param duration 正規化済みduration。
 * @param result 呼出側所有の出力先。入力と同一objectでもよい。
 * @return 成功、引数不正、またはoverflow。
 */
ut_time_result_t ut_time_utc_add(
    const ut_time_utc_t *utc,
    const ut_time_duration_t *duration,
    ut_time_utc_t *result);

/**
 * UTCからdurationを減算する。
 *
 * @param utc 正規化済みtimestamp。
 * @param duration 正規化済みduration。
 * @param result 呼出側所有の出力先。入力と同一objectでもよい。
 * @return 成功、引数不正、またはoverflow。
 */
ut_time_result_t ut_time_utc_subtract(
    const ut_time_utc_t *utc,
    const ut_time_duration_t *duration,
    ut_time_utc_t *result);

/**
 * 2つのUTCの差end-startをdurationとして返す。
 *
 * @param end 終点。
 * @param start 始点。
 * @param result 呼出側所有の出力先。
 * @return 成功、引数不正、またはoverflow。
 */
ut_time_result_t ut_time_utc_difference(
    const ut_time_utc_t *end,
    const ut_time_utc_t *start,
    ut_time_duration_t *result);

/**
 * 2つのUTCを比較する。
 *
 * @param left 左辺。
 * @param right 右辺。
 * @param ordering 呼出側所有出力。左辺が小なら-1、同値なら0、大なら1。
 * @return UT_TIME_OKまたはUT_TIME_INVALID_ARGUMENT。
 */
ut_time_result_t ut_time_utc_compare(
    const ut_time_utc_t *left,
    const ut_time_utc_t *right,
    int *ordering);

/**
 * UTCをRFC3339の`YYYY-MM-DDTHH:MM:SS.nnnnnnnnnZ`へformatする。
 *
 * 対応calendar範囲は西暦0001年から9999年。timezone offset、DST、leap secondは
 * 扱わない。
 *
 * @param utc 正規化済みtimestamp。
 * @param buffer 呼出側所有の出力buffer。
 * @param size buffer byte数。UT_TIME_RFC3339_BUFFER_SIZE以上が必要。
 * @return 成功、引数不正、範囲外、またはbuffer不足。
 */
ut_time_result_t ut_time_rfc3339_format(
    const ut_time_utc_t *utc,
    char *buffer,
    size_t size);

/**
 * Z timezoneのみのRFC3339文字列をUTCへparseする。
 *
 * fractional secondは省略または1から9桁を受理する。calendar範囲は西暦0001年
 * から9999年で、timezone offsetとleap secondは拒否する。
 *
 * @param text 終端NUL付き入力。関数は保持しない。
 * @param utc 呼出側所有の出力先。
 * @return 成功、書式不正、または範囲外。
 */
ut_time_result_t ut_time_rfc3339_parse(
    const char *text,
    ut_time_utc_t *utc);

#ifdef __cplusplus
}
#endif

#endif
