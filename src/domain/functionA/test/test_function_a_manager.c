#include "function_a_manager.h"

#include "domain_status_dispatcher.h"

#include <stdio.h>

/**
 * @file test_function_a_manager.c
 * @brief Function A ManagerからRunnerまでの流れを確認するサンプル。
 *
 * このファイルはControlの代わりにdomainを呼び出す確認用プログラムです。
 * 実際のControl層がまだなくても、domain単体で次の流れを確認できます。
 *
 * 1. Function A Managerを初期化する。
 * 2. prepareでScenarioをAction Queueへ展開する。
 * 3. startでRunnerを開始する。
 * 4. processを繰り返してActionを1つずつ実行する。
 * 5. 途中でpause/restartを呼び、止まって再開できることを見る。
 */

/**
 * @brief domain層から通知された状態イベントを表示する。
 *
 * 本来はControl層に置かれる処理です。
 * サンプルでは、Controlの代わりにこの関数でイベントを受け取ります。
 *
 * @param report 進捗やエラー文脈を含む状態レポート。
 * @param user_context 今回は使用しない任意情報。
 */
static void handle_domain_status(
    const domain_status_report_t *report,
    void *user_context)
{
    (void)user_context;

    if (report == NULL) {
        return;
    }

    printf("[control-like] event=%s, function=%s, scenario=%s, sequence=%s, step=%s, progress=%zu/%zu, error=%d, message=%s\n",
           convert_domain_status_event_to_string(report->event),
           (report->function_name != NULL) ? report->function_name : "",
           (report->scenario_name != NULL) ? report->scenario_name : "",
           (report->sequence_name != NULL) ? report->sequence_name : "",
           (report->step_name != NULL) ? report->step_name : "",
           report->step_index,
           report->total_step_count,
           report->error_code,
           (report->message != NULL) ? report->message : "");
}

/**
 * @brief prepare/start/pause/restart/processの基本動作を確認する。
 *
 * @return 正常終了時は0。
 */
int main(void)
{
    function_a_manager_t manager;
    function_a_status_t status;
    int loop_count;

    if (!initialize_function_a_manager(&manager, handle_domain_status, NULL)) {
        return 1;
    }

    /*
     * prepareではUnitはまだ動きません。
     * Scenario/Sequence/StepをAction Queueへ積むだけです。
     */
    if (!prepare_function_a(&manager)) {
        return 1;
    }

    /*
     * startでRunnerがRUNNINGになります。
     * ただしActionが実行されるのは、この後のprocess呼び出し時です。
     */
    if (!start_function_a(&manager)) {
        return 1;
    }

    /*
     * 1回のprocessでActionを1つ実行します。
     * loop_count == 3でpauseし、loop_count == 5でrestartすることで、
     * Queueの途中から再開できることを確認します。
     */
    for (loop_count = 0; loop_count < 32; loop_count++) {
        if (loop_count == 3) {
            (void)pause_function_a(&manager);
        }

        if (loop_count == 5) {
            (void)restart_function_a(&manager);
        }

        if (!process_function_a(&manager)) {
            return 1;
        }

        if (!get_function_a_status(&manager, &status)) {
            return 1;
        }

        if (status == FUNCTION_A_STATUS_COMPLETED) {
            break;
        }
    }

    return 0;
}
