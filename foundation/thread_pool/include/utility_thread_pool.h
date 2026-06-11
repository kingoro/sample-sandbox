/**
 * @file utility_thread_pool.h
 * @brief POSIX pthread固定長thread pool API。
 */
#ifndef UTILITY_THREAD_POOL_H
#define UTILITY_THREAD_POOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Thread pool操作のresult code型。 */
typedef uint32_t ut_thread_pool_result_t;

/** Thread pool操作のresult code。 */
enum {
    /** 操作が成功した。 */
    UT_THREAD_POOL_OK = 0,
    /** NULL、容量0、または不正なstorageが渡された。 */
    UT_THREAD_POOL_INVALID_ARGUMENT = 1,
    /** 固定長job queueが満杯である。 */
    UT_THREAD_POOL_QUEUE_FULL = 2,
    /** 現在のpool stateでは操作できない。 */
    UT_THREAD_POOL_STATE_ERROR = 3,
    /** pthread APIが失敗した。 */
    UT_THREAD_POOL_SYSTEM_ERROR = 4
};

/** Thread pool状態の型。 */
typedef uint32_t ut_thread_pool_state_t;

/** Thread poolの状態。 */
enum {
    /** 初期化前またはdestroy済み。 */
    UT_THREAD_POOL_UNINITIALIZED = 0,
    /** Job受付中。 */
    UT_THREAD_POOL_RUNNING = 1,
    /** 新規受付を止め、pending jobをdrain中。 */
    UT_THREAD_POOL_DRAINING = 2,
    /** 新規受付を止め、pending jobを破棄して停止中。 */
    UT_THREAD_POOL_STOPPING = 3,
    /** 全workerをjoin済み。 */
    UT_THREAD_POOL_JOINED = 4
};

/**
 * Workerが実行するjob callback。
 *
 * callback実行中のcontext寿命はsubmitした呼出側が保証する。callbackから
 * shutdown、join、destroyを呼んではならない。
 *
 * @param context submit時に指定した呼出側所有context。
 */
typedef void (*ut_thread_pool_job_fn)(void *context);

/** 固定長queueに保存するjob record。 */
typedef struct ut_thread_pool_job {
    /** 実行するcallback。 */
    ut_thread_pool_job_fn callback;
    /** callbackへ渡すcontext。 */
    void *context;
} ut_thread_pool_job_t;

/**
 * POSIX pthread固定長thread pool。
 *
 * fieldは公開されるが直接変更してはならない。worker_storageとqueue_storageは
 * poolをdestroyするまで呼出側が有効に保つ。submitは複数threadから安全に
 * 呼べ、shutdownとの競合も安全である。shutdown完了後、新規submitは
 * UT_THREAD_POOL_STATE_ERRORとなる。join/destroy前には全producer threadを停止し
 * joinして、submit呼出しが存在しないquiescent状態にする必要がある。
 * join/destroyとsubmitの同時実行は契約違反である。shutdown/join/destroyは
 * worker callback外の管理threadから呼び、管理操作同士も呼出側で直列化する。
 */
typedef struct ut_thread_pool {
    /** 呼出側所有のworker ID配列。 */
    pthread_t *workers;
    /** worker数。 */
    size_t worker_count;
    /** 作成済みworker数。 */
    size_t created_workers;
    /** join成功済みworker数。失敗後の再試行開始位置。 */
    size_t joined_workers;
    /** 呼出側所有の固定長job配列。 */
    ut_thread_pool_job_t *queue;
    /** queue容量。 */
    size_t queue_capacity;
    /** 次に取り出すqueue index。 */
    size_t queue_head;
    /** 次に格納するqueue index。 */
    size_t queue_tail;
    /** pending job数。 */
    size_t queue_count;
    /** Pool状態。 */
    ut_thread_pool_state_t state;
    /** Queueとstateを保護するmutex。 */
    pthread_mutex_t mutex;
    /** Job追加またはshutdownをworkerへ通知するcondition。 */
    pthread_cond_t work_available;
    /** mutexが初期化済みで未破棄ならtrue。 */
    bool mutex_initialized;
    /** conditionが初期化済みで未破棄ならtrue。 */
    bool condition_initialized;
} ut_thread_pool_t;

/**
 * Thread poolを初期化しworkerを開始する。
 *
 * heapを使わない。POSIX pthread環境専用。画像処理等のCPU job向けであり、
 * I/O event dispatch用途には使用しない。UT_THREAD_POOL_SYSTEM_ERRORで戻った場合、
 * poolがSTOPPINGならworker作成途中のcleanupでjoinに失敗している。producerは
 * 存在しないため、ut_thread_pool_join()を再試行し、成功後destroyする。JOINED
 * ならworker回収済み、またはcondition初期化失敗後にmutexの破棄が失敗している。
 * mutex_initializedまたはcondition_initializedがtrueの間はdestroyを再試行する。
 * UNINITIALIZEDなら内部cleanupは完了済みである。
 *
 * @param pool 初期化する呼出側所有pool。
 * @param worker_storage worker_count個以上のpthread_t配列。
 * @param worker_count 固定worker数。1以上。
 * @param queue_storage queue_capacity個以上のjob配列。
 * @param queue_capacity 固定queue長。1以上。
 * @return 成功時UT_THREAD_POOL_OK、失敗時は対応するcode。
 */
ut_thread_pool_result_t ut_thread_pool_init(
    ut_thread_pool_t *pool,
    pthread_t *worker_storage,
    size_t worker_count,
    ut_thread_pool_job_t *queue_storage,
    size_t queue_capacity);

/**
 * Jobをqueueへ追加する。
 *
 * callbackとcontextはcopyするがcontextが指すobjectは所有しない。queue満杯時は
 * 待機せずUT_THREAD_POOL_QUEUE_FULLを返す。shutdownとの競合はmutexで直列化され、
 * shutdownより先に受付済みならそのshutdown方式に従い、後ならSTATE_ERRORとなる。
 * join/destroyとの同時実行は禁止する。
 *
 * @param pool RUNNING状態のpool。
 * @param callback workerが実行するcallback。
 * @param context callbackへ渡すcontext。NULL可。
 * @return 成功、queue full、state error、または引数error。
 */
ut_thread_pool_result_t ut_thread_pool_submit(
    ut_thread_pool_t *pool,
    ut_thread_pool_job_fn callback,
    void *context);

/**
 * 新規受付を停止し、pending jobをすべて実行してからworkerを終了させる。
 * submitとの競合は安全である。戻った後はproducerを停止・joinし、submit呼出しが
 * quiescentになってからpoolをjoinする。
 *
 * @param pool RUNNING状態のpool。
 * @return 成功時UT_THREAD_POOL_OK、状態不正時UT_THREAD_POOL_STATE_ERROR。
 */
ut_thread_pool_result_t ut_thread_pool_shutdown_drain(
    ut_thread_pool_t *pool);

/**
 * 新規受付を停止し、pending jobを破棄してworkerを終了させる。
 *
 * 実行中callbackは中断しない。pending jobのcallbackは実行せず、poolはその
 * contextを解放しない。全producer停止後かつjoin成功後に、呼出側がpending jobの
 * contextを回収できる。
 *
 * @param pool RUNNING状態のpool。
 * @return 成功時UT_THREAD_POOL_OK、状態不正時UT_THREAD_POOL_STATE_ERROR。
 */
ut_thread_pool_result_t ut_thread_pool_shutdown_immediate(
    ut_thread_pool_t *pool);

/**
 * Shutdown後の全workerをjoinする。
 *
 * worker callbackから呼ぶとdeadlockするため禁止する。全producer threadを停止し
 * joinしてsubmitをquiescentにしてから呼ぶ。submitとの同時実行は契約違反。
 * pthread_joinが途中で失敗した場合、成功済みworker数を保持し、再試行は最初の
 * 未join workerから再開する。
 *
 * @param pool DRAININGまたはSTOPPING状態のpool。
 * @return 成功時UT_THREAD_POOL_OK、失敗時はstate/system error。
 */
ut_thread_pool_result_t ut_thread_pool_join(ut_thread_pool_t *pool);

/**
 * Join済みpoolのpthread同期primitiveを破棄する。
 *
 * worker callbackから呼んではならない。全producer停止とjoin成功が前提であり、
 * submitとの同時実行は契約違反。worker/queue storageは返却後に再利用可。
 * condition、mutexの順に破棄する。途中で失敗した場合は破棄済みprimitiveの
 * initialized fieldをfalseに保ち、再試行時に二重破棄しない。
 *
 * @param pool JOINED状態のpool。
 * @return 成功時UT_THREAD_POOL_OK、失敗時はstate/system error。
 */
ut_thread_pool_result_t ut_thread_pool_destroy(ut_thread_pool_t *pool);

#ifdef __cplusplus
}
#endif

#endif
