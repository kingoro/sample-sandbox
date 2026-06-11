/**
 * @file utility_logger.h
 * @brief Console出力とRAM Ring蓄積を管理するLogger API。
 */
#ifndef UTILITY_LOGGER_H
#define UTILITY_LOGGER_H

#include "utility_log_types.h"

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Logger初期化時の依存関係と初期設定。 */
typedef struct ut_logger_config {
    /** Console出力callback。NULLの場合はConsole出力を行わない。 */
    ut_log_console_write_fn console_write;
    /** console_writeへ渡すcontext。 */
    void *console_context;
    /** 時刻取得callback。NULLの場合、timestampは0。 */
    ut_log_clock_fn clock;
    /** clockへ渡すcontext。 */
    void *clock_context;
    /** 排他開始callback。不要な場合はNULL。 */
    ut_log_lock_fn lock;
    /** 排他終了callback。lockと対で指定する。 */
    ut_log_unlock_fn unlock;
    /** lockとunlockへ渡すcontext。 */
    void *lock_context;
    /** Consoleへ出す最低level。 */
    ut_log_level_t console_level;
    /** RAM Ringへ保存する最低level。 */
    ut_log_level_t ring_level;
    /** 初期状態でConsole出力を有効にする場合true。 */
    bool console_enabled;
    /** 初期状態でRAM Ring保存を有効にする場合true。 */
    bool ring_enabled;
} ut_logger_config_t;

/**
 * ConsoleとRAM Ringを管理するLogger context。
 *
 * fieldは公開しているが利用側が直接変更してはならない。heapは使わず、
 * 呼出側所有のstorageへRecord本体をcopyする。
 */
typedef struct ut_logger {
    /** RAM Ringとして使用する呼出側所有配列。 */
    ut_log_record_t *storage;
    /** storageへ格納できるRecord件数。 */
    size_t capacity;
    /** RAM Ring内の最古Recordのindex。 */
    size_t head;
    /** RAM Ringに保存中のRecord件数。 */
    size_t count;
    /** 次に割り当てるsequence。 */
    uint64_t next_sequence;
    /** RAM Ring満杯により上書きしたRecord件数。 */
    uint64_t overwritten_count;
    /** Console出力callback。 */
    ut_log_console_write_fn console_write;
    /** Console callback context。 */
    void *console_context;
    /** 時刻取得callback。 */
    ut_log_clock_fn clock;
    /** clock callback context。 */
    void *clock_context;
    /** 排他開始callback。 */
    ut_log_lock_fn lock;
    /** 排他終了callback。 */
    ut_log_unlock_fn unlock;
    /** 排他callback context。 */
    void *lock_context;
    /** Console最低level。 */
    ut_log_level_t console_level;
    /** RAM Ring最低level。 */
    ut_log_level_t ring_level;
    /** Console出力有効状態。 */
    bool console_enabled;
    /** RAM Ring保存有効状態。 */
    bool ring_enabled;
} ut_logger_t;

/**
 * Loggerを初期化する。
 *
 * loggerとstorageは再初期化または利用終了まで有効に保つ。thread間で共有する
 * 場合、configのlockとunlockを両方指定する。
 *
 * @param logger 初期化するLogger context。
 * @param storage RAM Ring用の呼出側所有Record配列。
 * @param capacity storageへ格納できるRecord件数。
 * @param config callbackと初期設定。
 * @return UT_LOG_OKまたはUT_LOG_INVALID_ARGUMENT。
 */
ut_log_result_t ut_logger_init(
    ut_logger_t *logger,
    ut_log_record_t *storage,
    size_t capacity,
    const ut_logger_config_t *config);

/**
 * Loggerを初期化し、プロセス既定Loggerとして登録する。
 *
 * 通常のApplicationは起動時に1回だけ呼び、以後はUT_LOG_INFO等のmacroと
 * ut_log_set_console等の既定Logger APIを使用する。loggerとstorageは
 * ut_log_shutdown()またはprocess終了まで有効に保つ。初期化とshutdownは、
 * Logを使用するthreadを開始する前または停止した後に行う。
 *
 * @param logger Applicationが所有するSingleton Logger context。
 * @param storage RAM Ring用のApplication所有Record配列。
 * @param capacity storageへ格納できるRecord件数。
 * @param config callbackと初期設定。
 * @return UT_LOG_OKまたはUT_LOG_INVALID_ARGUMENT。
 */
ut_log_result_t ut_log_initialize(
    ut_logger_t *logger,
    ut_log_record_t *storage,
    size_t capacity,
    const ut_logger_config_t *config);

/**
 * プロセス既定Loggerの登録を解除する。
 *
 * Loggerのstorageは消去しない。Logを使用するthreadを停止してから呼ぶ。
 */
void ut_log_shutdown(void);

/**
 * プロセス既定Loggerが登録済みか確認する。
 *
 * @param level 確認するlevel。
 * @return 指定levelがいずれかの出力先で有効ならtrue。
 */
bool ut_log_default_is_enabled(ut_log_level_t level);

/**
 * プロセス既定LoggerのConsole設定を変更する。
 *
 * @param enabled Console出力を有効にする場合true。
 * @param level 出力する最低level。
 * @return UT_LOG_OKまたはUT_LOG_INVALID_ARGUMENT。
 */
ut_log_result_t ut_log_set_console(bool enabled, ut_log_level_t level);

/**
 * プロセス既定LoggerのRAM Ring設定を変更する。
 *
 * @param enabled RAM Ring保存を有効にする場合true。
 * @param level 保存する最低level。
 * @return UT_LOG_OKまたはUT_LOG_INVALID_ARGUMENT。
 */
ut_log_result_t ut_log_set_ring(bool enabled, ut_log_level_t level);

/**
 * プロセス既定LoggerのRAM Ring内Record件数を返す。
 *
 * @return 保存件数。未初期化時は0。
 */
size_t ut_log_count(void);

/**
 * プロセス既定Loggerで上書きされたRecord件数を返す。
 *
 * @return 上書き件数。未初期化時は0。
 */
uint64_t ut_log_overwritten_count(void);

/**
 * プロセス既定LoggerからRecordをcopyする。
 *
 * @param logical_index 最古Recordを0とする位置。
 * @param out_record copy先。
 * @return UT_LOG_OK、UT_LOG_NOT_FOUND、UT_LOG_INVALID_ARGUMENT。
 */
ut_log_result_t ut_log_read(
    size_t logical_index,
    ut_log_record_t *out_record);

/**
 * プロセス既定LoggerのRAM Ringを古い順にdumpする。
 *
 * @param callback Recordを受け取るcallback。
 * @param context callbackへ渡すcontext。
 * @return UT_LOG_OKまたはUT_LOG_INVALID_ARGUMENT。
 */
ut_log_result_t ut_log_dump(
    ut_log_dump_fn callback,
    void *context);

/**
 * プロセス既定LoggerのRAM Ringを空にする。
 *
 * @return UT_LOG_OKまたはUT_LOG_INVALID_ARGUMENT。
 */
ut_log_result_t ut_log_clear(void);

/**
 * Console出力の有効状態と最低levelを変更する。
 *
 * @param logger 初期化済みLogger。
 * @param enabled Console出力を有効にする場合true。
 * @param level 出力する最低level。
 * @return UT_LOG_OKまたはUT_LOG_INVALID_ARGUMENT。
 */
ut_log_result_t ut_logger_set_console(
    ut_logger_t *logger,
    bool enabled,
    ut_log_level_t level);

/**
 * RAM Ring保存の有効状態と最低levelを変更する。
 *
 * @param logger 初期化済みLogger。
 * @param enabled RAM Ring保存を有効にする場合true。
 * @param level 保存する最低level。
 * @return UT_LOG_OKまたはUT_LOG_INVALID_ARGUMENT。
 */
ut_log_result_t ut_logger_set_ring(
    ut_logger_t *logger,
    bool enabled,
    ut_log_level_t level);

/**
 * 指定levelがConsoleまたはRAM Ringの対象か確認する。
 *
 * Log macroはfalseの場合にformat引数を評価しない。
 *
 * @param logger 初期化済みLogger。
 * @param level 確認するlevel。
 * @return いずれかの出力先が対象ならtrue。
 */
bool ut_log_is_enabled(
    const ut_logger_t *logger,
    ut_log_level_t level);

/**
 * 可変引数からLog Recordを生成して有効な出力先へ配送する。
 *
 * 通常はUT_LOG_TRACE等のlevel別macroを使用する。
 *
 * @param logger 初期化済みLogger。
 * @param level 記録するlevel。
 * @param module 呼出側が定義するmodule名。
 * @param file 呼出元source file名。
 * @param line 呼出元行番号。
 * @param function 呼出元function名。
 * @param format printf互換format文字列。
 * @param args formatに対応する可変引数。
 * @return 成功時UT_LOG_OK、引数不正または出力失敗時は対応するcode。
 */
ut_log_result_t ut_log_write_v(
    ut_logger_t *logger,
    ut_log_level_t level,
    const char *module,
    const char *file,
    uint32_t line,
    const char *function,
    const char *format,
    va_list args);

/**
 * Log Recordを生成して有効な出力先へ配送する。
 *
 * @param logger 初期化済みLogger。
 * @param level 記録するlevel。
 * @param module 呼出側が定義するmodule名。
 * @param file 呼出元source file名。
 * @param line 呼出元行番号。
 * @param function 呼出元function名。
 * @param format printf互換format文字列。
 * @return 成功時UT_LOG_OK、引数不正または出力失敗時は対応するcode。
 */
ut_log_result_t ut_log_write(
    ut_logger_t *logger,
    ut_log_level_t level,
    const char *module,
    const char *file,
    uint32_t line,
    const char *function,
    const char *format,
    ...);

/**
 * プロセス既定LoggerへLog Recordを配送する。
 *
 * 通常はUT_LOG_TRACE等のlevel別macroを使用する。
 *
 * @param level 記録するlevel。
 * @param module 呼出側が定義するmodule名。
 * @param file 呼出元source file名。
 * @param line 呼出元行番号。
 * @param function 呼出元function名。
 * @param format printf互換format文字列。
 * @return 成功時UT_LOG_OK、未初期化・引数不正・出力失敗時は対応するcode。
 */
ut_log_result_t ut_log_write_default(
    ut_log_level_t level,
    const char *module,
    const char *file,
    uint32_t line,
    const char *function,
    const char *format,
    ...);

/**
 * RAM Ring内のRecord件数を返す。
 *
 * @param logger 初期化済みLogger。
 * @return 保存件数。無効なLoggerでは0。
 */
size_t ut_logger_count(const ut_logger_t *logger);

/**
 * RAM Ring満杯により上書きされたRecord件数を返す。
 *
 * @param logger 初期化済みLogger。
 * @return 上書き件数。無効なLoggerでは0。
 */
uint64_t ut_logger_overwritten_count(const ut_logger_t *logger);

/**
 * RAM Ringの古い方からlogical_index番目をcopyする。
 *
 * @param logger 初期化済みLogger。
 * @param logical_index 最古Recordを0とする位置。
 * @param out_record copy先。
 * @return UT_LOG_OK、UT_LOG_NOT_FOUND、UT_LOG_INVALID_ARGUMENT。
 */
ut_log_result_t ut_logger_read(
    ut_logger_t *logger,
    size_t logical_index,
    ut_log_record_t *out_record);

/**
 * RAM Ringを古いRecordからcallbackへ渡す。
 *
 * dump中は排他を保持するため、callbackから同じLogger APIを呼んではならない。
 * callbackがfalseを返すと正常終了として中断する。
 *
 * @param logger 初期化済みLogger。
 * @param callback Recordを受け取るcallback。
 * @param context callbackへ渡すcontext。
 * @return UT_LOG_OKまたはUT_LOG_INVALID_ARGUMENT。
 */
ut_log_result_t ut_logger_dump(
    ut_logger_t *logger,
    ut_log_dump_fn callback,
    void *context);

/**
 * RAM Ringを空にする。sequenceと上書き件数は維持する。
 *
 * @param logger 初期化済みLogger。
 * @return UT_LOG_OKまたはUT_LOG_INVALID_ARGUMENT。
 */
ut_log_result_t ut_logger_clear(ut_logger_t *logger);

/** TRACE levelのLogを呼出元情報付きで記録する。 */
#define UT_LOG_TRACE(module, ...)                                             \
    do {                                                                      \
        if (ut_log_default_is_enabled(UT_LOG_LEVEL_TRACE)) {                  \
            (void)ut_log_write_default(UT_LOG_LEVEL_TRACE, (module),          \
                __FILE__, (uint32_t)__LINE__, __func__, __VA_ARGS__);         \
        }                                                                     \
    } while (0)

/** DEBUG levelのLogを呼出元情報付きで記録する。 */
#define UT_LOG_DEBUG(module, ...)                                             \
    do {                                                                      \
        if (ut_log_default_is_enabled(UT_LOG_LEVEL_DEBUG)) {                  \
            (void)ut_log_write_default(UT_LOG_LEVEL_DEBUG, (module),          \
                __FILE__, (uint32_t)__LINE__, __func__, __VA_ARGS__);         \
        }                                                                     \
    } while (0)

/** INFO levelのLogを呼出元情報付きで記録する。 */
#define UT_LOG_INFO(module, ...)                                              \
    do {                                                                      \
        if (ut_log_default_is_enabled(UT_LOG_LEVEL_INFO)) {                   \
            (void)ut_log_write_default(UT_LOG_LEVEL_INFO, (module),           \
                __FILE__, (uint32_t)__LINE__, __func__, __VA_ARGS__);         \
        }                                                                     \
    } while (0)

/** WARN levelのLogを呼出元情報付きで記録する。 */
#define UT_LOG_WARN(module, ...)                                              \
    do {                                                                      \
        if (ut_log_default_is_enabled(UT_LOG_LEVEL_WARN)) {                   \
            (void)ut_log_write_default(UT_LOG_LEVEL_WARN, (module),           \
                __FILE__, (uint32_t)__LINE__, __func__, __VA_ARGS__);         \
        }                                                                     \
    } while (0)

/** ERROR levelのLogを呼出元情報付きで記録する。 */
#define UT_LOG_ERROR(module, ...)                                             \
    do {                                                                      \
        if (ut_log_default_is_enabled(UT_LOG_LEVEL_ERROR)) {                  \
            (void)ut_log_write_default(UT_LOG_LEVEL_ERROR, (module),          \
                __FILE__, (uint32_t)__LINE__, __func__, __VA_ARGS__);         \
        }                                                                     \
    } while (0)

/** FATAL levelのLogを呼出元情報付きで記録する。 */
#define UT_LOG_FATAL(module, ...)                                             \
    do {                                                                      \
        if (ut_log_default_is_enabled(UT_LOG_LEVEL_FATAL)) {                  \
            (void)ut_log_write_default(UT_LOG_LEVEL_FATAL, (module),          \
                __FILE__, (uint32_t)__LINE__, __func__, __VA_ARGS__);         \
        }                                                                     \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif
