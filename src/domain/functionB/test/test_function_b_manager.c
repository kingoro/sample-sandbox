#include "function_b_manager.h"

#include "domain_status_dispatcher.h"

#include <stdio.h>

/**
 * @file test_function_b_manager.c
 * @brief Function B ManagerからRunnerまでの流れを確認するサンプル。
 *
 * Function Aと同じ実行構造で、別機能としてFunction Bを動かします。
 * Function BではUnit C/Dを使うため、Function Aとは違うScenarioがQueueへ積まれます。
 */

/**
 * @brief domain層から通知された状態イベントを表示する。
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
 * @brief Function Bのprepare/start/pause/restart/processの基本動作を確認する。
 *
 * @return 正常終了時は0。
 */
int main(void)
{
    function_b_manager_t manager;
    function_b_status_t status;
    int loop_count;

    if (!initialize_function_b_manager(&manager, handle_domain_status, NULL)) {
        return 1;
    }

    if (!prepare_function_b(&manager)) {
        return 1;
    }

    if (!start_function_b(&manager)) {
        return 1;
    }

    for (loop_count = 0; loop_count < 32; loop_count++) {
        if (loop_count == 2) {
            (void)pause_function_b(&manager);
        }

        if (loop_count == 4) {
            (void)restart_function_b(&manager);
        }

        if (!process_function_b(&manager)) {
            return 1;
        }

        if (!get_function_b_status(&manager, &status)) {
            return 1;
        }

        if (status == FUNCTION_B_STATUS_COMPLETED) {
            break;
        }
    }

    return 0;
}
