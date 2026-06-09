/**
 * @file domain_service_sample.c
 * @brief Control相当のループからDomain Serviceを駆動するサンプル。
 */
/** POSIX clock、thread、nanosleep APIを公開する。 */
#define _POSIX_C_SOURCE 200809L

#include "domain_service.h"
#include "utility_log.h"

#include <pthread.h>
#include <stdbool.h>
#include <time.h>

/** サンプルが生成するUnit数。 */
#define DOMAIN_SAMPLE_UNIT_COUNT 10U
/** サンプルLoggerのRAM Ring容量。 */
#define DOMAIN_SAMPLE_LOG_CAPACITY 256U

/**
 * Controlが受け取るFeature終端通知。
 */
typedef struct {
    /** Feature終端Eventを受信した場合true。 */
    bool received;
    /** Event受信時のOutput snapshot。 */
    domain_service_output_t output;
} domain_sample_control_result_t;

/**
 * Logger共有状態を保護する。
 *
 * @param context pthread_mutex_tへのポインタ。
 */
static void domain_sample_log_lock(void *context)
{
    (void)pthread_mutex_lock(context);
}

/**
 * Logger共有状態の保護を解除する。
 *
 * @param context pthread_mutex_tへのポインタ。
 */
static void domain_sample_log_unlock(void *context)
{
    (void)pthread_mutex_unlock(context);
}

/**
 * 現在の単調増加ミリ秒tickを返す。
 *
 * @return CLOCK_MONOTONICをミリ秒へ変換した値。
 */
static uint64_t domain_sample_now_ms(void)
{
    struct timespec current = {0};

    (void)clock_gettime(CLOCK_MONOTONIC, &current);
    return ((uint64_t)current.tv_sec * 1000U)
        + ((uint64_t)current.tv_nsec / 1000000U);
}

/**
 * 次のControl周期まで約50ミリ秒待機する。
 */
static void domain_sample_wait_cycle(void)
{
    const struct timespec interval = {
        .tv_sec = 0,
        .tv_nsec = 50000000L,
    };

    (void)nanosleep(&interval, NULL);
}

/**
 * PublisherからFeature終端Eventを受け取る。
 *
 * @param output Feature終端時のOutput。
 * @param context domain_sample_control_result_tへのポインタ。
 */
static void domain_sample_on_feature_event(
    const domain_service_output_t *output,
    void *context)
{
    domain_sample_control_result_t *result = context;

    result->output = *output;
    result->received = true;
    UT_LOG_INFO(
        "CONTROL",
        "feature event request=%u status=%d error=%d",
        output->request_id,
        output->status,
        output->error);
}

/**
 * 10個のMock Unitを生成する。
 *
 * @param units 生成したUnitを格納する配列。
 * @return 全Unit生成成功時true。
 */
static bool domain_sample_create_units(
    unit_mock_t *units[DOMAIN_SAMPLE_UNIT_COUNT])
{
    bool succeeded = true;

    for (uint8_t index = 0U;
         index < DOMAIN_SAMPLE_UNIT_COUNT && succeeded;
         ++index) {
        units[index] = unit_mock_create((uint8_t)(index + 1U));
        if (units[index] == NULL) {
            UT_LOG_ERROR("SAMPLE", "failed to create Unit %u", index + 1U);
            succeeded = false;
        }
    }
    return succeeded;
}

/**
 * 生成済みMock Unitを破棄する。
 *
 * @param units Unitを保持する配列。NULL要素も許容する。
 */
static void domain_sample_destroy_units(
    unit_mock_t *units[DOMAIN_SAMPLE_UNIT_COUNT])
{
    for (uint8_t index = 0U; index < DOMAIN_SAMPLE_UNIT_COUNT; ++index) {
        unit_mock_destroy(units[index]);
    }
}

/**
 * Domain ServiceのOutputが終端状態になるまで周期実行する。
 *
 * @param service 実行要求を受け付け済みのDomain Service。
 * @param result Controlが受け取ったFeature終端Eventの格納先。
 * @return 正常完了時true。
 */
static bool domain_sample_run(
    domain_service_t *service,
    domain_sample_control_result_t *result)
{
    do {
        domain_service_process(service, domain_sample_now_ms());
        if (!result->received) {
            domain_sample_wait_cycle();
        }
    } while (!result->received);

    return result->output.status == DOMAIN_STATUS_COMPLETED;
}

/**
 * Logger、Unit、Domain Serviceを構成してA機能を実行する。
 *
 * @return Scenario正常完了時0、準備または実行失敗時1。
 */
int main(void)
{
    ut_logger_t logger;
    ut_log_record_t log_storage[DOMAIN_SAMPLE_LOG_CAPACITY];
    pthread_mutex_t log_mutex;
    const ut_logger_config_t log_config = {
        .console_write = ut_log_console_write_file,
        .console_context = NULL,
        .clock = NULL,
        .clock_context = NULL,
        .lock = domain_sample_log_lock,
        .unlock = domain_sample_log_unlock,
        .lock_context = &log_mutex,
        .console_level = UT_LOG_LEVEL_INFO,
        .ring_level = UT_LOG_LEVEL_DEBUG,
        .console_enabled = true,
        .ring_enabled = true,
    };
    const domain_service_input_t input = {
        .feature = DOMAIN_FEATURE_A,
        .condition = 1U,
        .request_id = 1000U,
    };
    unit_mock_t *units[DOMAIN_SAMPLE_UNIT_COUNT] = {NULL};
    domain_service_t *service = NULL;
    domain_sample_control_result_t control_result = {0};
    const bool mutex_initialized =
        pthread_mutex_init(&log_mutex, NULL) == 0;
    bool succeeded = mutex_initialized;

    if (succeeded) {
        succeeded = ut_log_initialize(
            &logger,
            log_storage,
            DOMAIN_SAMPLE_LOG_CAPACITY,
            &log_config) == UT_LOG_OK;
    }
    if (succeeded) {
        succeeded = domain_sample_create_units(units);
    }
    if (succeeded) {
        service = domain_service_create(units, DOMAIN_SAMPLE_UNIT_COUNT);
        succeeded = service != NULL;
    }
    if (succeeded) {
        succeeded = domain_service_set_event_handler(
            service,
            domain_sample_on_feature_event,
            &control_result) == DOMAIN_INPUT_ACCEPTED;
    }
    if (succeeded) {
        succeeded = domain_service_write_input(service, &input)
            == DOMAIN_INPUT_ACCEPTED;
    }
    if (succeeded) {
        succeeded = domain_sample_run(service, &control_result);
    }

    domain_service_destroy(service);
    domain_sample_destroy_units(units);
    ut_log_shutdown();
    if (mutex_initialized) {
        (void)pthread_mutex_destroy(&log_mutex);
    }
    return succeeded ? 0 : 1;
}
