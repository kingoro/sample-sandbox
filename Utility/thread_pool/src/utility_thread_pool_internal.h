/**
 * @file utility_thread_pool_internal.h
 * @brief Thread Pool Utilityのpthread操作差替え用内部API。
 */
#ifndef UTILITY_THREAD_POOL_INTERNAL_H
#define UTILITY_THREAD_POOL_INTERNAL_H

#include <pthread.h>

/**
 * pthread thread入口互換の内部関数型。
 *
 * @param context thread context。
 * @return thread result。
 */
typedef void *(*ut_thread_pool_pthread_start_fn)(void *context);

/**
 * pthread_create互換の内部関数型。
 *
 * @param thread 作成threadの出力先。
 * @param attributes thread attributes。
 * @param start_routine thread入口。
 * @param context thread入口へ渡すcontext。
 * @return pthread互換result。
 */
typedef int (*ut_thread_pool_pthread_create_fn)(
    pthread_t *thread,
    const pthread_attr_t *attributes,
    ut_thread_pool_pthread_start_fn start_routine,
    void *context);

/**
 * pthread_join互換の内部関数型。
 *
 * @param thread join対象thread。
 * @param result thread result出力先。
 * @return pthread互換result。
 */
typedef int (*ut_thread_pool_pthread_join_fn)(
    pthread_t thread,
    void **result);

/**
 * pthread_mutex_init互換の内部関数型。
 *
 * @param mutex 初期化するmutex。
 * @param attributes mutex attributes。
 * @return pthread互換result。
 */
typedef int (*ut_thread_pool_pthread_mutex_init_fn)(
    pthread_mutex_t *mutex,
    const pthread_mutexattr_t *attributes);

/**
 * pthread_mutex_destroy互換の内部関数型。
 *
 * @param mutex 破棄するmutex。
 * @return pthread互換result。
 */
typedef int (*ut_thread_pool_pthread_mutex_destroy_fn)(
    pthread_mutex_t *mutex);

/**
 * pthread_cond_init互換の内部関数型。
 *
 * @param condition 初期化するcondition。
 * @param attributes condition attributes。
 * @return pthread互換result。
 */
typedef int (*ut_thread_pool_pthread_cond_init_fn)(
    pthread_cond_t *condition,
    const pthread_condattr_t *attributes);

/**
 * pthread_cond_destroy互換の内部関数型。
 *
 * @param condition 破棄するcondition。
 * @return pthread互換result。
 */
typedef int (*ut_thread_pool_pthread_cond_destroy_fn)(
    pthread_cond_t *condition);

/**
 * Thread poolが使用するpthread操作。
 *
 * productionでは実pthread関数を使用する。単体テストはpoolが存在しない状態で
 * 差し替え、fault injection完了後に既定値へ戻す。
 */
typedef struct ut_thread_pool_pthread_ops {
    /** Worker作成関数。 */
    ut_thread_pool_pthread_create_fn create;
    /** Worker join関数。 */
    ut_thread_pool_pthread_join_fn join;
    /** Mutex初期化関数。 */
    ut_thread_pool_pthread_mutex_init_fn mutex_init;
    /** Mutex破棄関数。 */
    ut_thread_pool_pthread_mutex_destroy_fn mutex_destroy;
    /** Condition初期化関数。 */
    ut_thread_pool_pthread_cond_init_fn condition_init;
    /** Condition破棄関数。 */
    ut_thread_pool_pthread_cond_destroy_fn condition_destroy;
} ut_thread_pool_pthread_ops_t;

/**
 * 内部pthread操作を差し替える。
 *
 * thread safeではない。pool初期化前、かつ既存poolをすべてdestroyした後に限り
 * testから呼ぶ。
 *
 * @param operations NULLでない関数を持つ操作table。
 */
void ut_thread_pool_internal_set_pthread_ops(
    const ut_thread_pool_pthread_ops_t *operations);

/**
 * 内部pthread操作を実pthread関数へ戻す。
 *
 * thread safeではない。既存poolがない状態でtestから呼ぶ。
 */
void ut_thread_pool_internal_reset_pthread_ops(void);

#endif
