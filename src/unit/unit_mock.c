/**
 * @file unit_mock.c
 * @brief 入出力ポートを持つ非同期Mock Unitの実装。
 */
/** POSIX threadおよびnanosleep APIを公開する。 */
#define _POSIX_C_SOURCE 200809L

#include "unit_mock.h"
#include "utility_log.h"

#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

/**
 * Unit固有のMock処理を実行する関数型。
 *
 * @return 成功時true、失敗時false。
 */
typedef bool (*unit_mock_operation_t)(void);

/**
 * Mock Unitの内部状態。
 */
struct unit_mock {
    /** Unit処理を実行する専用worker thread。 */
    pthread_t worker;
    /** InputとOutputを保護するmutex。 */
    pthread_mutex_t mutex;
    /** 新しいInputをworkerへ通知するcondition variable。 */
    pthread_cond_t input_ready;
    /** Unit処理の終了を破棄処理へ通知するcondition variable。 */
    pthread_cond_t operation_finished;
    /** workerが消費するInput snapshot。 */
    unit_mock_input_t input;
    /** 上位層へ公開するOutput snapshot。 */
    unit_mock_output_t output;
    /** 完了・エラー結果の通知先。 */
    unit_mock_result_output_t result_output;
    /** Unit番号に対応するMock処理。 */
    unit_mock_operation_t operation;
    /** 1始まりのUnit番号。 */
    uint8_t unit_number;
    /** 未処理Inputが存在する場合true。 */
    bool has_input;
    /** workerの終了が要求された場合true。 */
    bool stop_requested;
};

/**
 * Unit処理結果を登録先へ通知する。
 *
 * @param unit 結果を保持するUnit。
 * @param succeeded Unit処理が成功した場合true。
 */
static void unit_mock_notify_result(unit_mock_t *unit, bool succeeded)
{
    const unit_mock_result_t result = {
        .unit_number = unit->unit_number,
        .request_id = unit->output.request_id,
        .error = succeeded
            ? UNIT_MOCK_ERROR_NONE
            : UNIT_MOCK_ERROR_EXECUTION,
    };

    unit->result_output.handler(&result, unit->result_output.context);
}

/**
 * 関数名を表示して約500ミリ秒待機する。
 *
 * @param function_name 表示するUnit処理の関数名。呼び出し中は有効であること。
 * @return 待機完了時true、待機失敗時false。
 */
static bool unit_mock_sleep(const char *function_name)
{
    struct timespec remaining = {
        .tv_sec = 0,
        .tv_nsec = 500000000L,
    };

    UT_LOG_INFO("UNIT", "%s", function_name);
    while (nanosleep(&remaining, &remaining) != 0) {
        if (errno != EINTR) {
            return false;
        }
    }
    return true;
}

/** @brief Unit 01のMock処理。 @return 処理成功時true。 */
static bool unit_mock_execute_01(void) { return unit_mock_sleep(__func__); }
/** @brief Unit 02のMock処理。 @return 処理成功時true。 */
static bool unit_mock_execute_02(void) { return unit_mock_sleep(__func__); }
/** @brief Unit 03のMock処理。 @return 処理成功時true。 */
static bool unit_mock_execute_03(void) { return unit_mock_sleep(__func__); }
/** @brief Unit 04のMock処理。 @return 処理成功時true。 */
static bool unit_mock_execute_04(void) { return unit_mock_sleep(__func__); }
/** @brief Unit 05のMock処理。 @return 処理成功時true。 */
static bool unit_mock_execute_05(void) { return unit_mock_sleep(__func__); }
/** @brief Unit 06のMock処理。 @return 処理成功時true。 */
static bool unit_mock_execute_06(void) { return unit_mock_sleep(__func__); }
/** @brief Unit 07のMock処理。 @return 処理成功時true。 */
static bool unit_mock_execute_07(void) { return unit_mock_sleep(__func__); }
/** @brief Unit 08のMock処理。 @return 処理成功時true。 */
static bool unit_mock_execute_08(void) { return unit_mock_sleep(__func__); }
/** @brief Unit 09のMock処理。 @return 処理成功時true。 */
static bool unit_mock_execute_09(void) { return unit_mock_sleep(__func__); }
/** @brief Unit 10のMock処理。 @return 処理成功時true。 */
static bool unit_mock_execute_10(void) { return unit_mock_sleep(__func__); }

/**
 * Unit番号に対応するMock処理を取得する。
 *
 * @param unit_number 1から10までのUnit番号。
 * @return 対応する処理。範囲外の場合はNULL。
 */
static unit_mock_operation_t unit_mock_find_operation(uint8_t unit_number)
{
    static const unit_mock_operation_t operations[] = {
        unit_mock_execute_01,
        unit_mock_execute_02,
        unit_mock_execute_03,
        unit_mock_execute_04,
        unit_mock_execute_05,
        unit_mock_execute_06,
        unit_mock_execute_07,
        unit_mock_execute_08,
        unit_mock_execute_09,
        unit_mock_execute_10,
    };

    if (unit_number == 0U
        || unit_number > sizeof(operations) / sizeof(operations[0])) {
        return NULL;
    }
    return operations[unit_number - 1U];
}

/**
 * Inputを待ち、Unit処理の結果をOutputへ反映するworker。
 *
 * @param argument unit_mock_tへのポインタ。worker終了まで有効。
 * @return 常にNULL。
 */
static void *unit_mock_worker(void *argument)
{
    unit_mock_t *unit = argument;

    (void)pthread_mutex_lock(&unit->mutex);
    for (;;) {
        while (!unit->has_input && !unit->stop_requested) {
            (void)pthread_cond_wait(&unit->input_ready, &unit->mutex);
        }
        if (unit->stop_requested) {
            (void)pthread_mutex_unlock(&unit->mutex);
            return NULL;
        }

        unit->has_input = false;
        (void)pthread_mutex_unlock(&unit->mutex);
        const bool succeeded = unit->operation();
        (void)pthread_mutex_lock(&unit->mutex);

        unit->output.status = succeeded
            ? UNIT_MOCK_STATUS_COMPLETED
            : UNIT_MOCK_STATUS_ERROR;
        unit->output.error = succeeded
            ? UNIT_MOCK_ERROR_NONE
            : UNIT_MOCK_ERROR_EXECUTION;
        (void)pthread_cond_broadcast(&unit->operation_finished);
        (void)pthread_mutex_unlock(&unit->mutex);
        unit_mock_notify_result(unit, succeeded);
        (void)pthread_mutex_lock(&unit->mutex);
    }
}

unit_mock_t *unit_mock_create(uint8_t unit_number)
{
    unit_mock_operation_t operation = unit_mock_find_operation(unit_number);
    unit_mock_t *unit;

    if (operation == NULL) {
        return NULL;
    }
    unit = calloc(1U, sizeof(*unit));
    if (unit == NULL) {
        return NULL;
    }
    if (pthread_mutex_init(&unit->mutex, NULL) != 0) {
        free(unit);
        return NULL;
    }
    if (pthread_cond_init(&unit->input_ready, NULL) != 0) {
        (void)pthread_mutex_destroy(&unit->mutex);
        free(unit);
        return NULL;
    }
    if (pthread_cond_init(&unit->operation_finished, NULL) != 0) {
        (void)pthread_cond_destroy(&unit->input_ready);
        (void)pthread_mutex_destroy(&unit->mutex);
        free(unit);
        return NULL;
    }

    unit->operation = operation;
    unit->unit_number = unit_number;
    unit->output.status = UNIT_MOCK_STATUS_IDLE;
    if (pthread_create(&unit->worker, NULL, unit_mock_worker, unit) != 0) {
        (void)pthread_cond_destroy(&unit->operation_finished);
        (void)pthread_cond_destroy(&unit->input_ready);
        (void)pthread_mutex_destroy(&unit->mutex);
        free(unit);
        return NULL;
    }
    return unit;
}

unit_mock_input_result_t unit_mock_set_result_output(
    unit_mock_t *unit,
    const unit_mock_result_output_t *result_output)
{
    if (unit == NULL || result_output == NULL
        || result_output->handler == NULL) {
        return UNIT_MOCK_INPUT_INVALID_ARGUMENT;
    }

    (void)pthread_mutex_lock(&unit->mutex);
    if (unit->output.status == UNIT_MOCK_STATUS_RUNNING) {
        (void)pthread_mutex_unlock(&unit->mutex);
        return UNIT_MOCK_INPUT_BUSY;
    }
    unit->result_output = *result_output;
    (void)pthread_mutex_unlock(&unit->mutex);
    return UNIT_MOCK_INPUT_ACCEPTED;
}

void unit_mock_destroy(unit_mock_t *unit)
{
    if (unit == NULL) {
        return;
    }

    (void)pthread_mutex_lock(&unit->mutex);
    while (unit->output.status == UNIT_MOCK_STATUS_RUNNING) {
        (void)pthread_cond_wait(&unit->operation_finished, &unit->mutex);
    }
    unit->stop_requested = true;
    (void)pthread_cond_signal(&unit->input_ready);
    (void)pthread_mutex_unlock(&unit->mutex);

    (void)pthread_join(unit->worker, NULL);
    (void)pthread_cond_destroy(&unit->operation_finished);
    (void)pthread_cond_destroy(&unit->input_ready);
    (void)pthread_mutex_destroy(&unit->mutex);
    free(unit);
}

unit_mock_input_result_t unit_mock_write_input(
    unit_mock_t *unit,
    const unit_mock_input_t *input)
{
    if (unit == NULL || input == NULL
        || input->command != UNIT_MOCK_COMMAND_EXECUTE) {
        return UNIT_MOCK_INPUT_INVALID_ARGUMENT;
    }

    (void)pthread_mutex_lock(&unit->mutex);
    if (unit->output.status == UNIT_MOCK_STATUS_RUNNING) {
        (void)pthread_mutex_unlock(&unit->mutex);
        return UNIT_MOCK_INPUT_BUSY;
    }
    if (unit->result_output.handler == NULL) {
        (void)pthread_mutex_unlock(&unit->mutex);
        return UNIT_MOCK_INPUT_INVALID_ARGUMENT;
    }

    unit->input = *input;
    unit->output.status = UNIT_MOCK_STATUS_RUNNING;
    unit->output.request_id = input->request_id;
    unit->output.error = UNIT_MOCK_ERROR_NONE;
    unit->has_input = true;
    (void)pthread_cond_signal(&unit->input_ready);
    (void)pthread_mutex_unlock(&unit->mutex);
    return UNIT_MOCK_INPUT_ACCEPTED;
}

unit_mock_input_result_t unit_mock_read_output(
    unit_mock_t *unit,
    unit_mock_output_t *output)
{
    if (unit == NULL || output == NULL) {
        return UNIT_MOCK_INPUT_INVALID_ARGUMENT;
    }

    (void)pthread_mutex_lock(&unit->mutex);
    *output = unit->output;
    (void)pthread_mutex_unlock(&unit->mutex);
    return UNIT_MOCK_INPUT_ACCEPTED;
}
