/**
 * @file utility_thread_pool.c
 * @brief POSIX pthread固定長thread pool実装。
 */
#include "utility_thread_pool.h"
#include "utility_thread_pool_internal.h"

#include <string.h>

/** 実pthread操作を使用する既定table。 */
static const ut_thread_pool_pthread_ops_t default_pthread_ops = {
    .create = pthread_create,
    .join = pthread_join,
    .mutex_init = pthread_mutex_init,
    .mutex_destroy = pthread_mutex_destroy,
    .condition_init = pthread_cond_init,
    .condition_destroy = pthread_cond_destroy
};

/** 現在使用中のpthread操作table。 */
static ut_thread_pool_pthread_ops_t pthread_ops = {
    .create = pthread_create,
    .join = pthread_join,
    .mutex_init = pthread_mutex_init,
    .mutex_destroy = pthread_mutex_destroy,
    .condition_init = pthread_cond_init,
    .condition_destroy = pthread_cond_destroy
};

void ut_thread_pool_internal_set_pthread_ops(
    const ut_thread_pool_pthread_ops_t *operations)
{
    if (operations != NULL &&
        operations->create != NULL &&
        operations->join != NULL &&
        operations->mutex_init != NULL &&
        operations->mutex_destroy != NULL &&
        operations->condition_init != NULL &&
        operations->condition_destroy != NULL) {
        pthread_ops = *operations;
    }
}

void ut_thread_pool_internal_reset_pthread_ops(void)
{
    pthread_ops = default_pthread_ops;
}

/**
 * Queueからjobを取り出すworker thread入口。
 *
 * @param context ut_thread_pool_t。
 * @return 常にNULL。
 */
static void *worker_main(void *context)
{
    ut_thread_pool_t *pool = context;
    for (;;) {
        ut_thread_pool_job_t job;
        (void)pthread_mutex_lock(&pool->mutex);
        while (pool->queue_count == 0u &&
               pool->state == UT_THREAD_POOL_RUNNING) {
            (void)pthread_cond_wait(&pool->work_available, &pool->mutex);
        }
        if (pool->state == UT_THREAD_POOL_STOPPING ||
            (pool->state == UT_THREAD_POOL_DRAINING &&
             pool->queue_count == 0u)) {
            (void)pthread_mutex_unlock(&pool->mutex);
            break;
        }
        job = pool->queue[pool->queue_head];
        pool->queue_head = (pool->queue_head + 1u) % pool->queue_capacity;
        pool->queue_count--;
        (void)pthread_mutex_unlock(&pool->mutex);
        job.callback(job.context);
    }
    return NULL;
}

/**
 * Worker作成途中のpoolを停止して同期primitiveを破棄する。
 *
 * @param pool 部分初期化済みpool。
 */
static void cleanup_failed_init(ut_thread_pool_t *pool)
{
    (void)pthread_mutex_lock(&pool->mutex);
    pool->state = UT_THREAD_POOL_STOPPING;
    (void)pthread_cond_broadcast(&pool->work_available);
    (void)pthread_mutex_unlock(&pool->mutex);
    if (ut_thread_pool_join(pool) == UT_THREAD_POOL_OK) {
        (void)ut_thread_pool_destroy(pool);
    }
}

ut_thread_pool_result_t ut_thread_pool_init(
    ut_thread_pool_t *pool,
    pthread_t *worker_storage,
    size_t worker_count,
    ut_thread_pool_job_t *queue_storage,
    size_t queue_capacity)
{
    size_t index;
    if (pool == NULL || worker_storage == NULL || worker_count == 0u ||
        queue_storage == NULL || queue_capacity == 0u) {
        return UT_THREAD_POOL_INVALID_ARGUMENT;
    }
    (void)memset(pool, 0, sizeof(*pool));
    pool->workers = worker_storage;
    pool->worker_count = worker_count;
    pool->queue = queue_storage;
    pool->queue_capacity = queue_capacity;
    if (pthread_ops.mutex_init(&pool->mutex, NULL) != 0) {
        return UT_THREAD_POOL_SYSTEM_ERROR;
    }
    pool->mutex_initialized = true;
    if (pthread_ops.condition_init(&pool->work_available, NULL) != 0) {
        pool->state = UT_THREAD_POOL_JOINED;
        (void)ut_thread_pool_destroy(pool);
        return UT_THREAD_POOL_SYSTEM_ERROR;
    }
    pool->condition_initialized = true;
    pool->state = UT_THREAD_POOL_RUNNING;
    for (index = 0u; index < worker_count; ++index) {
        if (pthread_ops.create(
                &pool->workers[index], NULL, worker_main, pool) != 0) {
            cleanup_failed_init(pool);
            return UT_THREAD_POOL_SYSTEM_ERROR;
        }
        pool->created_workers++;
    }
    return UT_THREAD_POOL_OK;
}

ut_thread_pool_result_t ut_thread_pool_submit(
    ut_thread_pool_t *pool,
    ut_thread_pool_job_fn callback,
    void *context)
{
    if (pool == NULL || callback == NULL ||
        !pool->mutex_initialized || !pool->condition_initialized) {
        return UT_THREAD_POOL_INVALID_ARGUMENT;
    }
    if (pthread_mutex_lock(&pool->mutex) != 0) {
        return UT_THREAD_POOL_SYSTEM_ERROR;
    }
    if (pool->state != UT_THREAD_POOL_RUNNING) {
        (void)pthread_mutex_unlock(&pool->mutex);
        return UT_THREAD_POOL_STATE_ERROR;
    }
    if (pool->queue_count == pool->queue_capacity) {
        (void)pthread_mutex_unlock(&pool->mutex);
        return UT_THREAD_POOL_QUEUE_FULL;
    }
    pool->queue[pool->queue_tail].callback = callback;
    pool->queue[pool->queue_tail].context = context;
    pool->queue_tail = (pool->queue_tail + 1u) % pool->queue_capacity;
    pool->queue_count++;
    (void)pthread_cond_signal(&pool->work_available);
    (void)pthread_mutex_unlock(&pool->mutex);
    return UT_THREAD_POOL_OK;
}

/**
 * 指定shutdown stateへ遷移してworkerを起床する。
 *
 * @param pool RUNNING状態のpool。
 * @param state DRAININGまたはSTOPPING。
 * @return 成功、引数不正、state error、またはsystem error。
 */
static ut_thread_pool_result_t shutdown_pool(
    ut_thread_pool_t *pool,
    ut_thread_pool_state_t state)
{
    if (pool == NULL ||
        !pool->mutex_initialized ||
        !pool->condition_initialized) {
        return UT_THREAD_POOL_INVALID_ARGUMENT;
    }
    if (pthread_mutex_lock(&pool->mutex) != 0) {
        return UT_THREAD_POOL_SYSTEM_ERROR;
    }
    if (pool->state != UT_THREAD_POOL_RUNNING) {
        (void)pthread_mutex_unlock(&pool->mutex);
        return UT_THREAD_POOL_STATE_ERROR;
    }
    pool->state = state;
    if (state == UT_THREAD_POOL_STOPPING) {
        pool->queue_head = 0u;
        pool->queue_tail = 0u;
        pool->queue_count = 0u;
    }
    (void)pthread_cond_broadcast(&pool->work_available);
    (void)pthread_mutex_unlock(&pool->mutex);
    return UT_THREAD_POOL_OK;
}

ut_thread_pool_result_t ut_thread_pool_shutdown_drain(
    ut_thread_pool_t *pool)
{
    return shutdown_pool(pool, UT_THREAD_POOL_DRAINING);
}

ut_thread_pool_result_t ut_thread_pool_shutdown_immediate(
    ut_thread_pool_t *pool)
{
    return shutdown_pool(pool, UT_THREAD_POOL_STOPPING);
}

ut_thread_pool_result_t ut_thread_pool_join(ut_thread_pool_t *pool)
{
    if (pool == NULL ||
        !pool->mutex_initialized ||
        !pool->condition_initialized) {
        return UT_THREAD_POOL_INVALID_ARGUMENT;
    }
    if (pool->state != UT_THREAD_POOL_DRAINING &&
        pool->state != UT_THREAD_POOL_STOPPING) {
        return UT_THREAD_POOL_STATE_ERROR;
    }
    while (pool->joined_workers < pool->created_workers) {
        if (pthread_ops.join(
                pool->workers[pool->joined_workers], NULL) != 0) {
            return UT_THREAD_POOL_SYSTEM_ERROR;
        }
        pool->joined_workers++;
    }
    pool->state = UT_THREAD_POOL_JOINED;
    return UT_THREAD_POOL_OK;
}

ut_thread_pool_result_t ut_thread_pool_destroy(ut_thread_pool_t *pool)
{
    if (pool == NULL ||
        (!pool->mutex_initialized && !pool->condition_initialized)) {
        return UT_THREAD_POOL_INVALID_ARGUMENT;
    }
    if (pool->state != UT_THREAD_POOL_JOINED) {
        return UT_THREAD_POOL_STATE_ERROR;
    }
    if (pool->condition_initialized) {
        if (pthread_ops.condition_destroy(&pool->work_available) != 0) {
            return UT_THREAD_POOL_SYSTEM_ERROR;
        }
        pool->condition_initialized = false;
    }
    if (pool->mutex_initialized) {
        if (pthread_ops.mutex_destroy(&pool->mutex) != 0) {
            return UT_THREAD_POOL_SYSTEM_ERROR;
        }
        pool->mutex_initialized = false;
    }
    (void)memset(pool, 0, sizeof(*pool));
    return UT_THREAD_POOL_OK;
}
