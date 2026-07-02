#include "function_a_manager.h"

#include "domain_status_dispatcher.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

/**
 * @file main.c
 * @brief Control層からFunction Aを別スレッドで実行するアプリケーション入口。
 *
 * このファイルは、Control層とRunner実行を別スレッドに分けるサンプルです。
 *
 * スレッド構成:
 *
 * - main thread:
 *   Control相当です。Function Aのprepare/start/pause/restart/terminateを指示します。
 * - runner thread:
 *   Function Aの `process_function_a()` を繰り返し呼び、Action Queueを進めます。
 *
 * 注意:
 *
 * Unit処理は同期関数です。
 * そのため、Control threadがpauseを要求しても、現在実行中のUnit操作は途中で止まりません。
 * 現在のActionが戻ったあと、次のActionへ進む前にpause状態になります。
 */

/**
 * @brief ControlとRunner threadで共有する実行コンテキスト。
 */
typedef struct {
    /** Function Aを管理するdomain側オブジェクト。 */
    function_a_manager_t function_a;
    /** Function A Managerを保護するmutex。 */
    pthread_mutex_t lock;
    /** Runner threadへ終了を依頼するフラグ。 */
    bool stop_requested;
} control_runtime_t;

/**
 * @brief Control層が受け取るdomain状態通知。
 *
 * domain層はRunnerやFunction Managerの状態変化をこの関数へ通知します。
 *
 * @param report 進捗やエラー文脈を含む状態レポート。
 * @param user_context 今回のサンプルでは未使用。
 */
static void handle_domain_status(
    const domain_status_report_t *report,
    void *user_context)
{
    (void)user_context;

    if (report == NULL) {
        return;
    }

    printf("[control] domain event=%s, function=%s, scenario=%s, sequence=%s, step=%s, progress=%zu/%zu, error=%d, message=%s\n",
           convert_domain_status_event_to_string(report->event),
           (report->function_name != NULL) ? report->function_name : "",
           (report->scenario_name != NULL) ? report->scenario_name : "",
           (report->sequence_name != NULL) ? report->sequence_name : "",
           (report->step_name != NULL) ? report->step_name : "",
           report->step_index,
           report->total_step_count,
           report->error_code,
           (report->message != NULL) ? report->message : "");

    if (report->event == DOMAIN_STATUS_EVENT_ERROR) {
        printf("[control] emergency stop requested by domain error\n");
        printf("[control] cleanup/reset should be started here\n");
    }
}

/**
 * @brief Function Aが終端状態かどうかを判定する。
 *
 * @param status Function Aの現在状態。
 * @return 完了、終了、エラーのいずれかであれば `true`。
 */
static bool is_function_a_finished(function_a_status_t status)
{
    return (status == FUNCTION_A_STATUS_COMPLETED) ||
           (status == FUNCTION_A_STATUS_TERMINATED) ||
           (status == FUNCTION_A_STATUS_ERROR);
}

/**
 * @brief Function Aの状態をmutex保護下で取得する。
 *
 * @param runtime 共有実行コンテキスト。
 * @param status 状態を書き込む出力先。
 * @return 取得できた場合は `true`。
 */
static bool get_function_a_status_locked(
    control_runtime_t *runtime,
    function_a_status_t *status)
{
    bool result;

    if ((runtime == NULL) || (status == NULL)) {
        return false;
    }

    (void)pthread_mutex_lock(&runtime->lock);
    result = get_function_a_status(&runtime->function_a, status);
    (void)pthread_mutex_unlock(&runtime->lock);
    return result;
}

/**
 * @brief Runner threadの処理本体。
 *
 * このthreadはControl threadからstartされたあと、Function Aが終端状態になるまで
 * `process_function_a()` を繰り返し呼びます。
 *
 * @param argument `control_runtime_t` へのポインタ。
 * @return 常に `NULL`。
 */
static void *run_function_a_thread(void *argument)
{
    control_runtime_t *runtime = (control_runtime_t *)argument;

    if (runtime == NULL) {
        return NULL;
    }

    for (;;) {
        function_a_status_t status;
        bool stop_requested;

        /*
         * Function A ManagerはControl threadからも操作されるため、mutexで保護します。
         * process_function_a() の中でUnit処理が実行されるので、Unit処理中はこのmutexを保持します。
         * そのためpause/restartは現在のAction完了後に反映されます。
         */
        (void)pthread_mutex_lock(&runtime->lock);
        stop_requested = runtime->stop_requested;

        if (!stop_requested) {
            if (!process_function_a(&runtime->function_a)) {
                printf("[runner-thread] process_function_a() failed\n");
                runtime->stop_requested = true;
            }
        }

        if (!get_function_a_status(&runtime->function_a, &status)) {
            printf("[runner-thread] get_function_a_status() failed\n");
            runtime->stop_requested = true;
            status = FUNCTION_A_STATUS_ERROR;
        }

        stop_requested = runtime->stop_requested;
        (void)pthread_mutex_unlock(&runtime->lock);

        if (stop_requested || is_function_a_finished(status)) {
            break;
        }

        /*
         * busy loopを避けるための待ちです。
         * Unit操作自体は各Unit関数の中で1秒sleepします。
         */
        (void)sleep(1U);
    }

    printf("[runner-thread] end\n");
    return NULL;
}

/**
 * @brief Function Aを別スレッドで実行し、Control threadからpause/restartを指示する。
 *
 * @return 正常完了なら0、失敗なら1。
 */
int main(void)
{
    control_runtime_t runtime;
    pthread_t runner_thread;
    function_a_status_t status;

    printf("[control] application start\n");

    runtime.stop_requested = false;

    if (pthread_mutex_init(&runtime.lock, NULL) != 0) {
        printf("[control] pthread_mutex_init() failed\n");
        return 1;
    }

    if (!initialize_function_a_manager(&runtime.function_a, handle_domain_status, NULL)) {
        printf("[control] initialize_function_a_manager() failed\n");
        (void)pthread_mutex_destroy(&runtime.lock);
        return 1;
    }

    if (!prepare_function_a(&runtime.function_a)) {
        printf("[control] prepare_function_a() failed\n");
        (void)pthread_mutex_destroy(&runtime.lock);
        return 1;
    }

    if (!start_function_a(&runtime.function_a)) {
        printf("[control] start_function_a() failed\n");
        (void)pthread_mutex_destroy(&runtime.lock);
        return 1;
    }

    if (pthread_create(&runner_thread, NULL, run_function_a_thread, &runtime) != 0) {
        printf("[control] pthread_create() failed\n");
        (void)pthread_mutex_destroy(&runtime.lock);
        return 1;
    }

    /*
     * Control threadから一時停止を要求します。
     * 現在実行中のActionがある場合、そのActionが終わったあとにpause状態になります。
     */
    (void)sleep(3U);
    printf("[control] request pause\n");
    (void)pthread_mutex_lock(&runtime.lock);
    if (!pause_function_a(&runtime.function_a)) {
        printf("[control] pause_function_a() was not accepted\n");
    }
    (void)pthread_mutex_unlock(&runtime.lock);

    /*
     * しばらく停止状態を保ったあと、再開します。
     */
    (void)sleep(3U);
    printf("[control] request restart\n");
    (void)pthread_mutex_lock(&runtime.lock);
    if (!restart_function_a(&runtime.function_a)) {
        printf("[control] restart_function_a() was not accepted\n");
    }
    (void)pthread_mutex_unlock(&runtime.lock);

    if (pthread_join(runner_thread, NULL) != 0) {
        printf("[control] pthread_join() failed\n");
        (void)pthread_mutex_destroy(&runtime.lock);
        return 1;
    }

    if (!get_function_a_status_locked(&runtime, &status)) {
        printf("[control] get_function_a_status() failed\n");
        (void)pthread_mutex_destroy(&runtime.lock);
        return 1;
    }

    if (status != FUNCTION_A_STATUS_COMPLETED) {
        printf("[control] Function A did not complete. status=%d\n", (int)status);
        (void)pthread_mutex_destroy(&runtime.lock);
        return 1;
    }

    (void)pthread_mutex_destroy(&runtime.lock);
    printf("[control] Function A completed\n");
    printf("[control] application end\n");
    return 0;
}
