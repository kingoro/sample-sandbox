/**
 * @file test_utility_thread_pool.c
 * @brief Thread pool並行性とshutdown単体テスト。
 */
#include "utility_thread_pool.h"
#include "../src/utility_thread_pool_internal.h"
#include "test_support.h"

#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>

/** Blocking jobを制御するtest gate。 */
typedef struct test_gate {
    /** Gate mutex。 */
    pthread_mutex_t mutex;
    /** Gate condition。 */
    pthread_cond_t condition;
    /** Callback開始済みならtrue。 */
    bool started;
    /** Callbackを解放する場合true。 */
    bool released;
    /** 実行callback数。 */
    size_t calls;
} test_gate_t;

/** Count jobの共有context。 */
typedef struct count_context {
    /** Count保護mutex。 */
    pthread_mutex_t mutex;
    /** 実行済みjob数。 */
    size_t count;
} count_context_t;

/** Shutdown境界を制御するproducer context。 */
typedef struct producer_context {
    /** Producer同期mutex。 */
    pthread_mutex_t mutex;
    /** Producer同期condition。 */
    pthread_cond_t condition;
    /** Submit先pool。 */
    ut_thread_pool_t *pool;
    /** Jobへ渡すcount context。 */
    count_context_t *counter;
    /** 1回目submit完了ならtrue。 */
    bool first_done;
    /** 2回目submitを許可する場合true。 */
    bool allow_second;
    /** 1回目submit結果。 */
    ut_thread_pool_result_t first_result;
    /** 2回目submit結果。 */
    ut_thread_pool_result_t second_result;
} producer_context_t;

/** pthread fault injection状態。 */
typedef struct pthread_fault_state {
    /** pthread_create呼出回数。 */
    size_t create_calls;
    /** pthread_join呼出回数。 */
    size_t join_calls;
    /** pthread_mutex_init呼出回数。 */
    size_t mutex_init_calls;
    /** pthread_mutex_destroy呼出回数。 */
    size_t mutex_destroy_calls;
    /** pthread_cond_init呼出回数。 */
    size_t condition_init_calls;
    /** pthread_cond_destroy呼出回数。 */
    size_t condition_destroy_calls;
    /** 失敗させるcreate呼出番号。0なら失敗なし。 */
    size_t fail_create_call;
    /** 失敗させるjoin呼出番号。0なら失敗なし。 */
    size_t fail_join_call;
    /** 失敗させるmutex init呼出番号。0なら失敗なし。 */
    size_t fail_mutex_init_call;
    /** 失敗させるmutex destroy呼出番号。0なら失敗なし。 */
    size_t fail_mutex_destroy_call;
    /** 失敗させるcondition init呼出番号。0なら失敗なし。 */
    size_t fail_condition_init_call;
    /** 失敗させるcondition destroy呼出番号。0なら失敗なし。 */
    size_t fail_condition_destroy_call;
} pthread_fault_state_t;

/** 現在のpthread fault injection状態。 */
static pthread_fault_state_t pthread_fault;

/**
 * 指定回だけ失敗するpthread_create wrapper。
 *
 * @param thread 作成thread出力先。
 * @param attributes pthread attributes。
 * @param start_routine thread入口。
 * @param context thread context。
 * @return fault時EAGAIN、それ以外はpthread_create結果。
 */
static int fault_pthread_create(
    pthread_t *thread,
    const pthread_attr_t *attributes,
    void *(*start_routine)(void *),
    void *context)
{
    pthread_fault.create_calls++;
    if (pthread_fault.create_calls == pthread_fault.fail_create_call) {
        return EAGAIN;
    }
    return pthread_create(thread, attributes, start_routine, context);
}

/**
 * 指定回だけ失敗するpthread_join wrapper。
 *
 * @param thread join対象。
 * @param result thread結果出力先。
 * @return fault時EINVAL、それ以外はpthread_join結果。
 */
static int fault_pthread_join(pthread_t thread, void **result)
{
    pthread_fault.join_calls++;
    if (pthread_fault.join_calls == pthread_fault.fail_join_call) {
        return EINVAL;
    }
    return pthread_join(thread, result);
}

/**
 * 指定回だけ失敗するpthread_mutex_init wrapper。
 *
 * @param mutex 初期化対象。
 * @param attributes mutex attributes。
 * @return fault時ENOMEM、それ以外はpthread_mutex_init結果。
 */
static int fault_pthread_mutex_init(
    pthread_mutex_t *mutex,
    const pthread_mutexattr_t *attributes)
{
    pthread_fault.mutex_init_calls++;
    if (pthread_fault.mutex_init_calls ==
        pthread_fault.fail_mutex_init_call) {
        return ENOMEM;
    }
    return pthread_mutex_init(mutex, attributes);
}

/**
 * 指定回だけ失敗するpthread_mutex_destroy wrapper。
 *
 * @param mutex 破棄対象。
 * @return fault時EBUSY、それ以外はpthread_mutex_destroy結果。
 */
static int fault_pthread_mutex_destroy(pthread_mutex_t *mutex)
{
    pthread_fault.mutex_destroy_calls++;
    if (pthread_fault.mutex_destroy_calls ==
        pthread_fault.fail_mutex_destroy_call) {
        return EBUSY;
    }
    return pthread_mutex_destroy(mutex);
}

/**
 * 指定回だけ失敗するpthread_cond_init wrapper。
 *
 * @param condition 初期化対象。
 * @param attributes condition attributes。
 * @return fault時ENOMEM、それ以外はpthread_cond_init結果。
 */
static int fault_pthread_condition_init(
    pthread_cond_t *condition,
    const pthread_condattr_t *attributes)
{
    pthread_fault.condition_init_calls++;
    if (pthread_fault.condition_init_calls ==
        pthread_fault.fail_condition_init_call) {
        return ENOMEM;
    }
    return pthread_cond_init(condition, attributes);
}

/**
 * 指定回だけ失敗するpthread_cond_destroy wrapper。
 *
 * @param condition 破棄対象。
 * @return fault時EBUSY、それ以外はpthread_cond_destroy結果。
 */
static int fault_pthread_condition_destroy(pthread_cond_t *condition)
{
    pthread_fault.condition_destroy_calls++;
    if (pthread_fault.condition_destroy_calls ==
        pthread_fault.fail_condition_destroy_call) {
        return EBUSY;
    }
    return pthread_cond_destroy(condition);
}

/**
 * Fault injection pthread操作を設定する。
 *
 * @param fail_create_call 失敗させるcreate呼出番号。
 * @param fail_join_call 失敗させるjoin呼出番号。
 */
static void install_pthread_faults(
    size_t fail_create_call,
    size_t fail_join_call)
{
    const ut_thread_pool_pthread_ops_t operations = {
        .create = fault_pthread_create,
        .join = fault_pthread_join,
        .mutex_init = fault_pthread_mutex_init,
        .mutex_destroy = fault_pthread_mutex_destroy,
        .condition_init = fault_pthread_condition_init,
        .condition_destroy = fault_pthread_condition_destroy
    };
    pthread_fault = (pthread_fault_state_t){
        .fail_create_call = fail_create_call,
        .fail_join_call = fail_join_call
    };
    ut_thread_pool_internal_set_pthread_ops(&operations);
}

/**
 * Gateが開くまでworkerを待機させる。
 *
 * @param context test_gate_t。
 */
static void gated_job(void *context)
{
    test_gate_t *gate = context;
    (void)pthread_mutex_lock(&gate->mutex);
    gate->started = true;
    gate->calls++;
    (void)pthread_cond_broadcast(&gate->condition);
    while (!gate->released) {
        (void)pthread_cond_wait(&gate->condition, &gate->mutex);
    }
    (void)pthread_mutex_unlock(&gate->mutex);
}

/**
 * 共有countを増やす。
 *
 * @param context count_context_t。
 */
static void count_job(void *context)
{
    count_context_t *counter = context;
    (void)pthread_mutex_lock(&counter->mutex);
    counter->count++;
    (void)pthread_mutex_unlock(&counter->mutex);
}

/**
 * Shutdown前後に1回ずつsubmitするproducer。
 *
 * @param context producer_context_t。
 * @return 常にNULL。
 */
static void *boundary_producer(void *context)
{
    producer_context_t *producer = context;
    producer->first_result = ut_thread_pool_submit(
        producer->pool, count_job, producer->counter);
    (void)pthread_mutex_lock(&producer->mutex);
    producer->first_done = true;
    (void)pthread_cond_broadcast(&producer->condition);
    while (!producer->allow_second) {
        (void)pthread_cond_wait(&producer->condition, &producer->mutex);
    }
    (void)pthread_mutex_unlock(&producer->mutex);
    producer->second_result = ut_thread_pool_submit(
        producer->pool, count_job, producer->counter);
    return NULL;
}

/** @brief gate初期化。 @param gate 対象。 @return 成功時0。 */
static int gate_init(test_gate_t *gate)
{
    gate->started = false;
    gate->released = false;
    gate->calls = 0u;
    CHECK(pthread_mutex_init(&gate->mutex, NULL) == 0);
    CHECK(pthread_cond_init(&gate->condition, NULL) == 0);
    return 0;
}

/**
 * 指定数のcallbackが開始するまで待つ。
 *
 * @param gate 対象。
 * @param expected_calls 待機するcallback数。
 */
static void gate_wait_calls(test_gate_t *gate, size_t expected_calls)
{
    (void)pthread_mutex_lock(&gate->mutex);
    while (gate->calls < expected_calls) {
        (void)pthread_cond_wait(&gate->condition, &gate->mutex);
    }
    (void)pthread_mutex_unlock(&gate->mutex);
}

/** @brief gateを開いて破棄する。 @param gate 対象。 */
static void gate_release_and_destroy(test_gate_t *gate)
{
    (void)pthread_mutex_lock(&gate->mutex);
    gate->released = true;
    (void)pthread_cond_broadcast(&gate->condition);
    (void)pthread_mutex_unlock(&gate->mutex);
}

/** @brief drainが全jobを並行実行することを検証する。 @return 成功時0。 */
static int test_drain_and_concurrency(void)
{
    pthread_t workers[3];
    ut_thread_pool_job_t queue[32];
    ut_thread_pool_t pool;
    count_context_t counter;
    test_gate_t gate;
    size_t index;
    counter.count = 0u;
    CHECK(pthread_mutex_init(&counter.mutex, NULL) == 0);
    CHECK(gate_init(&gate) == 0);
    CHECK(ut_thread_pool_init(&pool, workers, 3u, queue, 32u) == UT_THREAD_POOL_OK);
    for (index = 0u; index < 3u; ++index) {
        CHECK(ut_thread_pool_submit(&pool, gated_job, &gate) == UT_THREAD_POOL_OK);
    }
    gate_wait_calls(&gate, 3u);
    gate_release_and_destroy(&gate);
    for (index = 0u; index < 24u; ++index) {
        CHECK(ut_thread_pool_submit(&pool, count_job, &counter) == UT_THREAD_POOL_OK);
    }
    CHECK(ut_thread_pool_shutdown_drain(&pool) == UT_THREAD_POOL_OK);
    CHECK(ut_thread_pool_submit(&pool, count_job, &counter) == UT_THREAD_POOL_STATE_ERROR);
    CHECK(ut_thread_pool_join(&pool) == UT_THREAD_POOL_OK);
    CHECK(counter.count == 24u);
    CHECK(ut_thread_pool_join(&pool) == UT_THREAD_POOL_STATE_ERROR);
    CHECK(ut_thread_pool_destroy(&pool) == UT_THREAD_POOL_OK);
    CHECK(pthread_cond_destroy(&gate.condition) == 0);
    CHECK(pthread_mutex_destroy(&gate.mutex) == 0);
    CHECK(pthread_mutex_destroy(&counter.mutex) == 0);
    return 0;
}

/** @brief queue fullとimmediate pending破棄を検証する。 @return 成功時0。 */
static int test_full_and_immediate_shutdown(void)
{
    pthread_t workers[1];
    ut_thread_pool_job_t queue[2];
    ut_thread_pool_t pool;
    test_gate_t gate;
    CHECK(gate_init(&gate) == 0);
    CHECK(ut_thread_pool_init(&pool, workers, 1u, queue, 2u) == UT_THREAD_POOL_OK);
    CHECK(ut_thread_pool_submit(&pool, gated_job, &gate) == UT_THREAD_POOL_OK);
    gate_wait_calls(&gate, 1u);
    CHECK(ut_thread_pool_submit(&pool, gated_job, &gate) == UT_THREAD_POOL_OK);
    CHECK(ut_thread_pool_submit(&pool, gated_job, &gate) == UT_THREAD_POOL_OK);
    CHECK(ut_thread_pool_submit(&pool, gated_job, &gate) == UT_THREAD_POOL_QUEUE_FULL);
    CHECK(ut_thread_pool_shutdown_immediate(&pool) == UT_THREAD_POOL_OK);
    CHECK(ut_thread_pool_shutdown_immediate(&pool) == UT_THREAD_POOL_STATE_ERROR);
    gate_release_and_destroy(&gate);
    CHECK(ut_thread_pool_join(&pool) == UT_THREAD_POOL_OK);
    CHECK(gate.calls == 1u);
    CHECK(ut_thread_pool_destroy(&pool) == UT_THREAD_POOL_OK);
    CHECK(pthread_cond_destroy(&gate.condition) == 0);
    CHECK(pthread_mutex_destroy(&gate.mutex) == 0);
    return 0;
}

/**
 * Producerが存在する状態のshutdown境界を決定的に検証する。
 *
 * @return 成功時0。
 */
static int test_submit_shutdown_boundary(void)
{
    pthread_t worker;
    pthread_t producer_thread;
    ut_thread_pool_job_t queue[2];
    ut_thread_pool_t pool;
    test_gate_t gate;
    count_context_t counter;
    producer_context_t producer;
    counter.count = 0u;
    CHECK(pthread_mutex_init(&counter.mutex, NULL) == 0);
    CHECK(gate_init(&gate) == 0);
    CHECK(ut_thread_pool_init(&pool, &worker, 1u, queue, 2u) == UT_THREAD_POOL_OK);
    CHECK(ut_thread_pool_submit(&pool, gated_job, &gate) == UT_THREAD_POOL_OK);
    gate_wait_calls(&gate, 1u);
    producer = (producer_context_t){
        .pool = &pool,
        .counter = &counter,
        .first_result = UT_THREAD_POOL_SYSTEM_ERROR,
        .second_result = UT_THREAD_POOL_SYSTEM_ERROR
    };
    CHECK(pthread_mutex_init(&producer.mutex, NULL) == 0);
    CHECK(pthread_cond_init(&producer.condition, NULL) == 0);
    CHECK(pthread_create(
        &producer_thread, NULL, boundary_producer, &producer) == 0);
    (void)pthread_mutex_lock(&producer.mutex);
    while (!producer.first_done) {
        (void)pthread_cond_wait(&producer.condition, &producer.mutex);
    }
    (void)pthread_mutex_unlock(&producer.mutex);
    CHECK(producer.first_result == UT_THREAD_POOL_OK);
    CHECK(ut_thread_pool_shutdown_immediate(&pool) == UT_THREAD_POOL_OK);
    (void)pthread_mutex_lock(&producer.mutex);
    producer.allow_second = true;
    (void)pthread_cond_broadcast(&producer.condition);
    (void)pthread_mutex_unlock(&producer.mutex);
    CHECK(pthread_join(producer_thread, NULL) == 0);
    CHECK(producer.second_result == UT_THREAD_POOL_STATE_ERROR);
    gate_release_and_destroy(&gate);
    CHECK(ut_thread_pool_join(&pool) == UT_THREAD_POOL_OK);
    CHECK(counter.count == 0u);
    CHECK(ut_thread_pool_destroy(&pool) == UT_THREAD_POOL_OK);
    CHECK(pthread_cond_destroy(&producer.condition) == 0);
    CHECK(pthread_mutex_destroy(&producer.mutex) == 0);
    CHECK(pthread_cond_destroy(&gate.condition) == 0);
    CHECK(pthread_mutex_destroy(&gate.mutex) == 0);
    CHECK(pthread_mutex_destroy(&counter.mutex) == 0);
    return 0;
}

/**
 * Join部分成功後の再試行が未join workerから再開することを検証する。
 *
 * @return 成功時0。
 */
static int test_join_retry_after_partial_failure(void)
{
    pthread_t workers[3];
    ut_thread_pool_job_t queue[1];
    ut_thread_pool_t pool;
    install_pthread_faults(0u, 2u);
    CHECK(ut_thread_pool_init(&pool, workers, 3u, queue, 1u) == UT_THREAD_POOL_OK);
    CHECK(ut_thread_pool_shutdown_drain(&pool) == UT_THREAD_POOL_OK);
    CHECK(ut_thread_pool_join(&pool) == UT_THREAD_POOL_SYSTEM_ERROR);
    CHECK(pool.joined_workers == 1u);
    CHECK(pool.state == UT_THREAD_POOL_DRAINING);
    pthread_fault.fail_join_call = 0u;
    CHECK(ut_thread_pool_join(&pool) == UT_THREAD_POOL_OK);
    CHECK(pool.joined_workers == 3u);
    CHECK(pthread_fault.join_calls == 4u);
    CHECK(ut_thread_pool_destroy(&pool) == UT_THREAD_POOL_OK);
    ut_thread_pool_internal_reset_pthread_ops();
    return 0;
}

/**
 * Init途中のcreate/join失敗で回収可能な状態が残ることを検証する。
 *
 * @return 成功時0。
 */
static int test_init_failure_recovery(void)
{
    pthread_t workers[3];
    ut_thread_pool_job_t queue[1];
    ut_thread_pool_t pool;
    install_pthread_faults(3u, 1u);
    CHECK(ut_thread_pool_init(&pool, workers, 3u, queue, 1u) ==
        UT_THREAD_POOL_SYSTEM_ERROR);
    CHECK(pool.state == UT_THREAD_POOL_STOPPING);
    CHECK(pool.created_workers == 2u);
    CHECK(pool.joined_workers == 0u);
    CHECK(pool.mutex_initialized);
    CHECK(pool.condition_initialized);
    pthread_fault.fail_join_call = 0u;
    CHECK(ut_thread_pool_join(&pool) == UT_THREAD_POOL_OK);
    CHECK(pool.joined_workers == 2u);
    CHECK(ut_thread_pool_destroy(&pool) == UT_THREAD_POOL_OK);
    ut_thread_pool_internal_reset_pthread_ops();

    install_pthread_faults(2u, 0u);
    CHECK(ut_thread_pool_init(&pool, workers, 3u, queue, 1u) ==
        UT_THREAD_POOL_SYSTEM_ERROR);
    CHECK(pool.state == UT_THREAD_POOL_UNINITIALIZED);
    CHECK(!pool.mutex_initialized);
    CHECK(!pool.condition_initialized);
    ut_thread_pool_internal_reset_pthread_ops();
    return 0;
}

/**
 * Destroy部分成功後に破棄済みprimitiveを再破棄しないことを検証する。
 *
 * @return 成功時0。
 */
static int test_destroy_retry_after_partial_failure(void)
{
    pthread_t worker;
    ut_thread_pool_job_t queue[1];
    ut_thread_pool_t pool;
    install_pthread_faults(0u, 0u);
    CHECK(ut_thread_pool_init(&pool, &worker, 1u, queue, 1u) ==
        UT_THREAD_POOL_OK);
    CHECK(ut_thread_pool_shutdown_drain(&pool) == UT_THREAD_POOL_OK);
    CHECK(ut_thread_pool_join(&pool) == UT_THREAD_POOL_OK);
    pthread_fault.fail_mutex_destroy_call = 1u;
    CHECK(ut_thread_pool_destroy(&pool) == UT_THREAD_POOL_SYSTEM_ERROR);
    CHECK(!pool.condition_initialized);
    CHECK(pool.mutex_initialized);
    CHECK(pthread_fault.condition_destroy_calls == 1u);
    CHECK(pthread_fault.mutex_destroy_calls == 1u);
    pthread_fault.fail_mutex_destroy_call = 0u;
    CHECK(ut_thread_pool_destroy(&pool) == UT_THREAD_POOL_OK);
    CHECK(pthread_fault.condition_destroy_calls == 1u);
    CHECK(pthread_fault.mutex_destroy_calls == 2u);
    ut_thread_pool_internal_reset_pthread_ops();
    return 0;
}

/**
 * Primitive init失敗後の状態とcleanup再試行を検証する。
 *
 * @return 成功時0。
 */
static int test_primitive_init_failure_recovery(void)
{
    pthread_t worker;
    ut_thread_pool_job_t queue[1];
    ut_thread_pool_t pool;
    install_pthread_faults(0u, 0u);
    pthread_fault.fail_condition_init_call = 1u;
    pthread_fault.fail_mutex_destroy_call = 1u;
    CHECK(ut_thread_pool_init(&pool, &worker, 1u, queue, 1u) ==
        UT_THREAD_POOL_SYSTEM_ERROR);
    CHECK(pool.state == UT_THREAD_POOL_JOINED);
    CHECK(pool.mutex_initialized);
    CHECK(!pool.condition_initialized);
    CHECK(pthread_fault.condition_destroy_calls == 0u);
    CHECK(pthread_fault.mutex_destroy_calls == 1u);
    pthread_fault.fail_mutex_destroy_call = 0u;
    CHECK(ut_thread_pool_destroy(&pool) == UT_THREAD_POOL_OK);
    CHECK(pthread_fault.condition_destroy_calls == 0u);
    CHECK(pthread_fault.mutex_destroy_calls == 2u);
    ut_thread_pool_internal_reset_pthread_ops();

    install_pthread_faults(0u, 0u);
    pthread_fault.fail_mutex_init_call = 1u;
    CHECK(ut_thread_pool_init(&pool, &worker, 1u, queue, 1u) ==
        UT_THREAD_POOL_SYSTEM_ERROR);
    CHECK(pool.state == UT_THREAD_POOL_UNINITIALIZED);
    CHECK(!pool.mutex_initialized);
    CHECK(!pool.condition_initialized);
    CHECK(ut_thread_pool_destroy(&pool) ==
        UT_THREAD_POOL_INVALID_ARGUMENT);
    ut_thread_pool_internal_reset_pthread_ops();
    return 0;
}

/** @brief 不正引数とstate契約を検証する。 @return 成功時0。 */
static int test_invalid_arguments_and_states(void)
{
    pthread_t worker;
    ut_thread_pool_job_t queue[1];
    ut_thread_pool_t pool = {0};
    CHECK(ut_thread_pool_init(NULL, &worker, 1u, queue, 1u) == UT_THREAD_POOL_INVALID_ARGUMENT);
    CHECK(ut_thread_pool_init(&pool, NULL, 1u, queue, 1u) == UT_THREAD_POOL_INVALID_ARGUMENT);
    CHECK(ut_thread_pool_init(&pool, &worker, 0u, queue, 1u) == UT_THREAD_POOL_INVALID_ARGUMENT);
    CHECK(ut_thread_pool_init(&pool, &worker, 1u, NULL, 1u) == UT_THREAD_POOL_INVALID_ARGUMENT);
    CHECK(ut_thread_pool_init(&pool, &worker, 1u, queue, 0u) == UT_THREAD_POOL_INVALID_ARGUMENT);
    CHECK(ut_thread_pool_submit(NULL, count_job, NULL) == UT_THREAD_POOL_INVALID_ARGUMENT);
    CHECK(ut_thread_pool_submit(&pool, NULL, NULL) == UT_THREAD_POOL_INVALID_ARGUMENT);
    CHECK(ut_thread_pool_shutdown_drain(NULL) == UT_THREAD_POOL_INVALID_ARGUMENT);
    CHECK(ut_thread_pool_shutdown_immediate(NULL) == UT_THREAD_POOL_INVALID_ARGUMENT);
    CHECK(ut_thread_pool_join(NULL) == UT_THREAD_POOL_INVALID_ARGUMENT);
    CHECK(ut_thread_pool_destroy(NULL) == UT_THREAD_POOL_INVALID_ARGUMENT);
    CHECK(ut_thread_pool_init(&pool, &worker, 1u, queue, 1u) == UT_THREAD_POOL_OK);
    CHECK(ut_thread_pool_join(&pool) == UT_THREAD_POOL_STATE_ERROR);
    CHECK(ut_thread_pool_destroy(&pool) == UT_THREAD_POOL_STATE_ERROR);
    CHECK(ut_thread_pool_shutdown_drain(&pool) == UT_THREAD_POOL_OK);
    CHECK(ut_thread_pool_join(&pool) == UT_THREAD_POOL_OK);
    CHECK(ut_thread_pool_destroy(&pool) == UT_THREAD_POOL_OK);
    return 0;
}

/** @brief Thread Pool Utility単体テストを実行する。 @return 全成功時0。 */
int main(void)
{
    CHECK(test_drain_and_concurrency() == 0);
    CHECK(test_full_and_immediate_shutdown() == 0);
    CHECK(test_submit_shutdown_boundary() == 0);
    CHECK(test_join_retry_after_partial_failure() == 0);
    CHECK(test_init_failure_recovery() == 0);
    CHECK(test_destroy_retry_after_partial_failure() == 0);
    CHECK(test_primitive_init_failure_recovery() == 0);
    CHECK(test_invalid_arguments_and_states() == 0);
    return 0;
}
