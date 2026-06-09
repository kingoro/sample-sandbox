/**
 * @file thread_safe_logger.c
 * @brief 独立LoggerをMutex callback付きで複数threadから使う例。
 *
 * @par 使用コンポーネント
 * - ut_logger_t: 複数threadで共有する独立Logger。
 * - ut_logger_config_t: caller所有Mutexのlock/unlock callbackを登録する。
 * - ut_log_write: 既定Loggerを使わず、指定LoggerへRecordを書き込む。
 * - pthread Mutex/thread: 排他実装と並行producerを提供するサンプル依存。
 *
 * @par 処理フロー
 * 1. callerがMutex、Logger、RAM Ring storageを準備する。
 * 2. lock/unlock callback付きで独立Loggerを初期化する。
 * 3. 2つのworker threadが同じLoggerへRecordを書き込む。
 * 4. Loggerが各書込みをMutex callbackで直列化してRingへ保存する。
 * 5. worker終了後にRecord件数と上書きなしを確認し、Mutexを破棄する。
 */
#include "utility_log.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

/** Log Ring容量。 */
#define EXAMPLE_LOG_CAPACITY 16U
/** 1 threadあたりのLog件数。 */
#define EXAMPLE_WRITES_PER_THREAD 4U

/** workerへ渡すcontext。 */
typedef struct worker_context {
    /** 複数threadで共有するLogger。 */
    ut_logger_t *logger;
    /** worker識別子。 */
    uint32_t worker_id;
} worker_context_t;

/**
 * Loggerの排他領域へ入る。
 *
 * @param context pthread_mutex_tへのpointer。
 */
static void logger_lock(void *context)
{
    (void)pthread_mutex_lock(context);
}

/**
 * Loggerの排他領域から出る。
 *
 * @param context pthread_mutex_tへのpointer。
 */
static void logger_unlock(void *context)
{
    (void)pthread_mutex_unlock(context);
}

/**
 * 独立Loggerへworker Logを書き込む。
 *
 * @param argument worker_context_tへのpointer。
 * @return 常にNULL。
 */
static void *worker_main(void *argument)
{
    worker_context_t *context = argument;

    for (uint32_t index = 0U; index < EXAMPLE_WRITES_PER_THREAD; ++index) {
        /*
         * 既定Logger macroではなく明示的なLoggerを使うと、用途別Ringを
         * 分離できる。lock/unlock callbackがRecord生成とRing更新を保護する。
         */
        (void)ut_log_write(context->logger, UT_LOG_LEVEL_INFO,
            "LOG-THREAD", __FILE__, (uint32_t)__LINE__, __func__,
            "worker=%u index=%u", context->worker_id, index);
    }
    return NULL;
}

/**
 * 2 workerで共有Loggerを使用する。
 *
 * @return 成功時0、失敗時1。
 */
int main(void)
{
    ut_logger_t logger;
    ut_log_record_t storage[EXAMPLE_LOG_CAPACITY];
    pthread_mutex_t mutex;
    pthread_t first_thread;
    pthread_t second_thread;
    worker_context_t first = {&logger, 1U};
    worker_context_t second = {&logger, 2U};
    bool first_started = false;
    bool second_started = false;
    bool succeeded = pthread_mutex_init(&mutex, NULL) == 0;
    const ut_logger_config_t config = {
        .console_write = NULL,
        .clock = NULL,
        .lock = logger_lock,
        .unlock = logger_unlock,
        .lock_context = &mutex,
        .console_level = UT_LOG_LEVEL_INFO,
        .ring_level = UT_LOG_LEVEL_INFO,
        .console_enabled = false,
        .ring_enabled = true,
    };

    if (succeeded) {
        succeeded = ut_logger_init(&logger, storage, EXAMPLE_LOG_CAPACITY,
            &config) == UT_LOG_OK;
    }
    if (succeeded) {
        first_started = pthread_create(
            &first_thread, NULL, worker_main, &first) == 0;
        succeeded = first_started;
    }
    if (succeeded) {
        second_started = pthread_create(
            &second_thread, NULL, worker_main, &second) == 0;
        succeeded = second_started;
    }
    if (first_started) {
        succeeded = pthread_join(first_thread, NULL) == 0 && succeeded;
    }
    if (second_started) {
        succeeded = pthread_join(second_thread, NULL) == 0 && succeeded;
    }
    succeeded = succeeded
        && ut_logger_count(&logger) == 2U * EXAMPLE_WRITES_PER_THREAD
        && ut_logger_overwritten_count(&logger) == 0U;
    (void)pthread_mutex_destroy(&mutex);
    return succeeded ? 0 : 1;
}
