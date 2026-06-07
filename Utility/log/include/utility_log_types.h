/**
 * @file utility_log_types.h
 * @brief Log Utilityのlevel、record、callback型を定義する。
 */
#ifndef UTILITY_LOG_TYPES_H
#define UTILITY_LOG_TYPES_H

#include "utility_log_result.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Log Recordへ保存するmodule名の最大長。終端NULを含む。 */
#define UT_LOG_MODULE_CAPACITY 24u
/** Log Recordへ保存するmessageの最大長。終端NULを含む。 */
#define UT_LOG_MESSAGE_CAPACITY 160u
/** Log Recordへ保存するsource file名の最大長。終端NULを含む。 */
#define UT_LOG_FILE_CAPACITY 64u
/** Log Recordへ保存するfunction名の最大長。終端NULを含む。 */
#define UT_LOG_FUNCTION_CAPACITY 48u

/** Logの重要度。値が大きいほど重要度が高い。 */
typedef enum ut_log_level {
    /** 関数出入口や細粒度の状態追跡。 */
    UT_LOG_LEVEL_TRACE = 0,
    /** 開発時の診断情報。 */
    UT_LOG_LEVEL_DEBUG = 1,
    /** 通常動作を示す情報。 */
    UT_LOG_LEVEL_INFO = 2,
    /** 処理継続可能な注意状態。 */
    UT_LOG_LEVEL_WARN = 3,
    /** 処理失敗または復旧が必要な異常。 */
    UT_LOG_LEVEL_ERROR = 4,
    /** 継続不能な重大異常。 */
    UT_LOG_LEVEL_FATAL = 5,
    /** 有効なLog Levelの個数。閾値として使用してはならない。 */
    UT_LOG_LEVEL_COUNT = 6
} ut_log_level_t;

/** RAM Ringへ値として保存される固定長Log Record。 */
typedef struct ut_log_record {
    /** Logger内で単調増加する記録順序番号。 */
    uint64_t sequence;
    /** 呼出側のclock callbackが返した時刻値。未設定時は0。 */
    uint64_t timestamp;
    /** Logの重要度。 */
    ut_log_level_t level;
    /** 呼出元sourceの行番号。 */
    uint32_t line;
    /** 呼出元module名。長すぎる場合は切り詰める。 */
    char module[UT_LOG_MODULE_CAPACITY];
    /** 整形済みmessage。長すぎる場合は切り詰める。 */
    char message[UT_LOG_MESSAGE_CAPACITY];
    /** 呼出元source file名。長すぎる場合は切り詰める。 */
    char file[UT_LOG_FILE_CAPACITY];
    /** 呼出元function名。長すぎる場合は切り詰める。 */
    char function[UT_LOG_FUNCTION_CAPACITY];
} ut_log_record_t;

/**
 * 現在時刻を取得するcallback。
 *
 * @param context 利用側が登録したcontext。
 * @return 製品内で定義した単位の時刻値。
 */
typedef uint64_t (*ut_log_clock_fn)(void *context);

/**
 * ConsoleへRecordを出力するcallback。
 *
 * @param record 出力中のみ有効なRecord。
 * @param context 利用側が登録したcontext。
 * @return 成功時UT_LOG_OK、それ以外は出力失敗。
 */
typedef ut_log_result_t (*ut_log_console_write_fn)(const ut_log_record_t *record, void *context);

/**
 * Loggerの排他領域へ入るcallback。
 *
 * @param context 利用側が登録したlock context。
 */
typedef void (*ut_log_lock_fn)(void *context);

/**
 * Loggerの排他領域から出るcallback。
 *
 * @param context 利用側が登録したlock context。
 */
typedef void (*ut_log_unlock_fn)(void *context);

/**
 * RAM RingのRecordをダンプするcallback。
 *
 * @param record callback呼出中のみ有効なRecord。
 * @param context 利用側が登録したcontext。
 * @return 継続時true、中断時false。
 */
typedef bool (*ut_log_dump_fn)(const ut_log_record_t *record, void *context);

#ifdef __cplusplus
}
#endif

#endif
