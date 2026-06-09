/**
 * @file unit_mock_sample.c
 * @brief 10個のMock UnitへInputを与え、Outputを監視するサンプル。
 */
/** POSIX nanosleep APIを公開する。 */
#define _POSIX_C_SOURCE 200809L

#include "unit_mock.h"
#include "utility_log.h"

#include <pthread.h>
#include <stdbool.h>
#include <time.h>

/** サンプルLoggerのRAM Ring容量。 */
#define UNIT_MOCK_SAMPLE_LOG_CAPACITY 128U

/**
 * Logger共有状態を保護する。
 *
 * @param context pthread_mutex_tへのポインタ。
 */
static void unit_mock_sample_log_lock(void *context)
{
    (void)pthread_mutex_lock(context);
}

/**
 * Logger共有状態の保護を解除する。
 *
 * @param context pthread_mutex_tへのポインタ。
 */
static void unit_mock_sample_log_unlock(void *context)
{
    (void)pthread_mutex_unlock(context);
}

/**
 * Outputの次回確認まで約100ミリ秒待機する。
 */
static void unit_mock_sample_poll(void)
{
    const struct timespec interval = {
        .tv_sec = 0,
        .tv_nsec = 100000000L,
    };

    (void)nanosleep(&interval, NULL);
}

/**
 * Unitの非同期結果通知をLogへ記録する。
 *
 * @param result Unit処理結果。
 * @param context 未使用。
 */
static void unit_mock_sample_on_result(
    const unit_mock_result_t *result,
    void *context)
{
    (void)context;
    UT_LOG_INFO(
        "SAMPLE",
        "Unit %02u event request=%u error=%d",
        result->unit_number,
        result->request_id,
        result->error);
}

/**
 * Mock Unit群を生成して実行命令を入力する。
 *
 * @param units 生成したUnitを格納する10要素の配列。
 * @return 全Unitへの入力成功時true、失敗時false。
 */
static bool unit_mock_sample_start(unit_mock_t *units[10])
{
    bool succeeded = true;

    for (uint8_t index = 0U; index < 10U && succeeded; ++index) {
        units[index] = unit_mock_create((uint8_t)(index + 1U));
        if (units[index] == NULL) {
            UT_LOG_ERROR("SAMPLE", "failed to create Unit %u", index + 1U);
            succeeded = false;
        } else {
            const unit_mock_result_output_t result_output = {
                .handler = unit_mock_sample_on_result,
                .context = NULL,
            };
            const unit_mock_input_t input = {
                .command = UNIT_MOCK_COMMAND_EXECUTE,
                .request_id = (uint32_t)(1001U + index),
            };

            if (unit_mock_set_result_output(units[index], &result_output)
                    != UNIT_MOCK_INPUT_ACCEPTED
                || unit_mock_write_input(units[index], &input)
                != UNIT_MOCK_INPUT_ACCEPTED) {
                UT_LOG_ERROR(
                    "SAMPLE",
                    "Unit %u rejected Input",
                    index + 1U);
                succeeded = false;
            }
        }
    }
    return succeeded;
}

/**
 * 全Mock UnitのOutputが終端状態になるまで監視する。
 *
 * @param units 実行中の10個のUnit。
 * @return 全Unit正常完了時true、エラー終了時false。
 */
static bool unit_mock_sample_wait(unit_mock_t *const units[10])
{
    bool all_finished;
    bool succeeded = true;

    do {
        all_finished = true;
        for (uint8_t index = 0U; index < 10U; ++index) {
            unit_mock_output_t output;

            (void)unit_mock_read_output(units[index], &output);
            if (output.status == UNIT_MOCK_STATUS_RUNNING) {
                all_finished = false;
            } else if (output.status == UNIT_MOCK_STATUS_ERROR) {
                succeeded = false;
            }
            UT_LOG_INFO(
                "SAMPLE",
                "Unit %02u: request=%u status=%d error=%d",
                index + 1U,
                output.request_id,
                output.status,
                output.error);
        }
        if (!all_finished) {
            UT_LOG_DEBUG("SAMPLE", "polling");
            unit_mock_sample_poll();
        }
    } while (!all_finished);

    return succeeded;
}

/**
 * 生成済みのMock Unit群を破棄する。
 *
 * @param units Unitを保持する10要素の配列。NULL要素も許容する。
 */
static void unit_mock_sample_destroy(unit_mock_t *units[10])
{
    for (uint8_t index = 0U; index < 10U; ++index) {
        unit_mock_destroy(units[index]);
    }
}

/**
 * 10個のUnitへ命令を送り、全Outputが終端状態になるまで監視する。
 *
 * @return 全Unit正常完了時0、準備または実行失敗時1。
 */
int main(void)
{
    ut_logger_t logger;
    ut_log_record_t log_storage[UNIT_MOCK_SAMPLE_LOG_CAPACITY];
    pthread_mutex_t log_mutex;
    const ut_logger_config_t log_config = {
        .console_write = ut_log_console_write_file,
        .console_context = NULL,
        .clock = NULL,
        .clock_context = NULL,
        .lock = unit_mock_sample_log_lock,
        .unlock = unit_mock_sample_log_unlock,
        .lock_context = &log_mutex,
        .console_level = UT_LOG_LEVEL_DEBUG,
        .ring_level = UT_LOG_LEVEL_DEBUG,
        .console_enabled = true,
        .ring_enabled = true,
    };
    unit_mock_t *units[10] = { NULL };
    const bool log_mutex_initialized =
        pthread_mutex_init(&log_mutex, NULL) == 0;
    bool succeeded = log_mutex_initialized;

    if (succeeded) {
        succeeded = ut_log_initialize(
            &logger,
            log_storage,
            UNIT_MOCK_SAMPLE_LOG_CAPACITY,
            &log_config) == UT_LOG_OK;
    }
    if (succeeded) {
        succeeded = unit_mock_sample_start(units);
        if (succeeded) {
            succeeded = unit_mock_sample_wait(units);
        }
    }
    unit_mock_sample_destroy(units);
    ut_log_shutdown();
    if (log_mutex_initialized) {
        (void)pthread_mutex_destroy(&log_mutex);
    }
    return succeeded ? 0 : 1;
}
