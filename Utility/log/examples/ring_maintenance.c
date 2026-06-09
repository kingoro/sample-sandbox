/**
 * @file ring_maintenance.c
 * @brief RAM Ringを外部consumerからread/dumpする例。
 *
 * @par 使用コンポーネント
 * - ut_logger_t: 固定長RAM RingへLog Recordを値copyする。
 * - UT_LOG_* macro: 既定Loggerへlevel別Recordを書き込む。
 * - ut_log_read: logical indexを指定してRecordを1件copyする。
 * - ut_log_dump: Ringを古いRecordからcallbackへ渡す。
 * - ut_log_clear: Ring内Recordを破棄する。
 *
 * @par 処理フロー
 * 1. Console無効、Ring有効の既定Loggerを容量3で初期化する。
 * 2. 4件のRecordを書き込み、最古Recordを1件上書きする。
 * 3. countとoverwritten_countを取得し、readで現在の最古Recordを確認する。
 * 4. dump callbackで全RecordとERROR以上の件数を集計する。
 * 5. Ringをclearして空になったことを確認し、既定Loggerをshutdownする。
 */
#include "utility_log.h"

#include <stdbool.h>
#include <stddef.h>

/** 上書きを見せる小さなRing容量。 */
#define EXAMPLE_LOG_CAPACITY 3U

/** dump callbackの観測状態。 */
typedef struct dump_context {
    /** dumpされたRecord数。 */
    size_t count;
    /** ERROR以上のRecord数。 */
    size_t error_count;
} dump_context_t;

/**
 * 外部consumerの代わりにRecordを集計する。
 *
 * @param record Ringから渡されたRecord。
 * @param context dump_context_tへのpointer。
 * @return dumpを続けるためtrue。
 */
static bool collect_record(
    const ut_log_record_t *record,
    void *context)
{
    dump_context_t *dump = context;

    dump->count++;
    if (record->level >= UT_LOG_LEVEL_ERROR) {
        dump->error_count++;
    }
    return true;
}

/**
 * Ring上書き、read、dump、clearを示す。
 *
 * @return 成功時0、失敗時1。
 */
int main(void)
{
    ut_logger_t logger;
    ut_log_record_t storage[EXAMPLE_LOG_CAPACITY];
    ut_log_record_t oldest;
    dump_context_t dump = {0};
    const ut_logger_config_t config = {
        .console_write = NULL,
        .console_level = UT_LOG_LEVEL_FATAL,
        .ring_level = UT_LOG_LEVEL_INFO,
        .console_enabled = false,
        .ring_enabled = true,
    };
    bool succeeded = ut_log_initialize(
        &logger, storage, EXAMPLE_LOG_CAPACITY, &config) == UT_LOG_OK;

    if (succeeded) {
        UT_LOG_INFO("LOG-RING", "first");
        UT_LOG_WARN("LOG-RING", "second");
        UT_LOG_ERROR("LOG-RING", "third");
        UT_LOG_FATAL("LOG-RING", "fourth");

        /*
         * 容量3へ4件書いたためfirstは上書き済み。
         * dump callback中は同じLoggerへ書き戻してはならない。
         */
        succeeded = ut_log_count() == EXAMPLE_LOG_CAPACITY
            && ut_log_overwritten_count() == 1U
            && ut_log_read(0U, &oldest) == UT_LOG_OK
            && oldest.level == UT_LOG_LEVEL_WARN
            && ut_log_dump(collect_record, &dump) == UT_LOG_OK
            && dump.count == EXAMPLE_LOG_CAPACITY
            && dump.error_count == 2U
            && ut_log_clear() == UT_LOG_OK
            && ut_log_count() == 0U;
    }
    ut_log_shutdown();
    return succeeded ? 0 : 1;
}
