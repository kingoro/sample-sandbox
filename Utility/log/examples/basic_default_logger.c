/**
 * @file basic_default_logger.c
 * @brief 既定Loggerとlevel macroを使う基本例。
 *
 * @par 使用コンポーネント
 * - ut_logger_t: Console出力とRAM Ringを管理する。
 * - ut_logger_config_t: clock、出力先、level条件を設定する。
 * - ut_log_console_write_file: Log Recordを標準C streamへ出力する。
 * - UT_LOG_* macro: 既定Loggerへ呼出元情報付きRecordを書き込む。
 *
 * @par 処理フロー
 * 1. caller所有のLoggerとRAM Ring storageをut_log_initializeで登録する。
 * 2. DEBUG、INFO、WARNをmacroから書き込み、出力先ごとのlevel条件を適用する。
 * 3. 実行中にConsoleとRingの最低levelを変更する。
 * 4. INFOとERRORを書き込み、変更後の振り分けを確認する。
 * 5. Ring件数を確認し、ut_log_shutdownで既定Logger登録を解除する。
 */
#include "utility_log.h"

#include <stdbool.h>
#include <stdint.h>

/** Log Ring容量。 */
#define EXAMPLE_LOG_CAPACITY 16U

/**
 * サンプル用の単調tickを返す。
 *
 * @param context uint64_t tickへのpointer。
 * @return 更新後tick。
 */
static uint64_t example_clock(void *context)
{
    uint64_t *tick = context;

    *tick += 10U;
    return *tick;
}

/**
 * 既定Loggerの初期化、macro出力、実行時level変更を示す。
 *
 * @return 成功時0、失敗時1。
 */
int main(void)
{
    ut_logger_t logger;
    ut_log_record_t storage[EXAMPLE_LOG_CAPACITY];
    uint64_t tick = 0U;
    const ut_logger_config_t config = {
        .console_write = ut_log_console_write_file,
        .console_context = NULL,
        .clock = example_clock,
        .clock_context = &tick,
        .lock = NULL,
        .unlock = NULL,
        .lock_context = NULL,
        .console_level = UT_LOG_LEVEL_INFO,
        .ring_level = UT_LOG_LEVEL_DEBUG,
        .console_enabled = true,
        .ring_enabled = true,
    };
    bool succeeded = ut_log_initialize(
        &logger, storage, EXAMPLE_LOG_CAPACITY, &config) == UT_LOG_OK;

    if (succeeded) {
        /*
         * 通常コードはlevel別macroを使う。module、file、line、function、
         * timestamp、sequenceがRecordへ値copyされる。
         */
        UT_LOG_DEBUG("LOG-BASIC", "ring only value=%u", 1U);
        UT_LOG_INFO("LOG-BASIC", "logger initialized");
        UT_LOG_WARN("LOG-BASIC", "retry count=%u", 2U);

        /* 運用中にConsoleだけERROR以上へ絞る代表パターン。 */
        succeeded = ut_log_set_console(true, UT_LOG_LEVEL_ERROR) == UT_LOG_OK
            && ut_log_set_ring(true, UT_LOG_LEVEL_INFO) == UT_LOG_OK;
        UT_LOG_INFO("LOG-BASIC", "stored in ring only");
        UT_LOG_ERROR("LOG-BASIC", "operation failed code=%u", 5U);
    }
    succeeded = succeeded && ut_log_count() == 5U;
    ut_log_shutdown();
    return succeeded ? 0 : 1;
}
