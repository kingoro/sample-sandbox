#include "function_b_manager.h"

#include <stdbool.h>
#include <stdio.h>

/**
 * @file error_demo_main.c
 * @brief Function BのエラーScenarioを使い、Control側の異常処理を確認する。
 *
 * 通常実行の例は `main.c` に置いています。
 * このファイルでは、Unitが `false` を返したときに、RunnerがERRORへ遷移し、
 * Controlが清掃処理とリセット処理へ進む流れだけに注目します。
 */

/**
 * @brief Control側で保持するエラー対応状態。
 */
typedef struct {
    /** DomainからERROR通知を受け取ったかどうか。 */
    bool error_report_received;
    /** 清掃処理を実行したかどうか。 */
    bool cleanup_done;
    /** リセット処理を実行したかどうか。 */
    bool reset_done;
} control_error_context_t;

/**
 * @brief NULLの文字列を表示用の空文字へ変換する。
 */
static const char *safe_text(const char *text)
{
    return (text != NULL) ? text : "";
}

/**
 * @brief ERROR発生後の清掃処理を模擬する。
 *
 * 実製品では、出力停止、退避動作、ログ保存、通信通知などがここに入ります。
 * このサンプルでは、どのStepで止まったかを表示するだけにしています。
 */
static void cleanup_after_domain_error(
    control_error_context_t *context,
    const domain_status_report_t *report)
{
    if (context == NULL) {
        return;
    }

    printf("[control-error] cleanup_after_domain_error(step=%s, message=%s)\n",
           safe_text(report->step_name),
           safe_text(report->message));
    context->cleanup_done = true;
}

/**
 * @brief ERROR発生後のリセット処理を模擬する。
 *
 * 実製品では、Unit状態の初期化、Runnerの破棄、再prepare可否の判断などがここに入ります。
 */
static void reset_after_domain_error(
    control_error_context_t *context,
    const domain_status_report_t *report)
{
    if (context == NULL) {
        return;
    }

    printf("[control-error] reset_after_domain_error(function=%s, scenario=%s)\n",
           safe_text(report->function_name),
           safe_text(report->scenario_name));
    context->reset_done = true;
}

/**
 * @brief DomainからControlへ届く状態通知を受け取る。
 */
static void handle_domain_status(
    const domain_status_report_t *report,
    void *user_context)
{
    control_error_context_t *context = (control_error_context_t *)user_context;

    if (report == NULL) {
        return;
    }

    printf("[control-error] status event=%s function=%s scenario=%s sequence=%s step=%s action=%s progress=%zu/%zu error=%d message=%s\n",
           convert_domain_status_event_to_string(report->event),
           safe_text(report->function_name),
           safe_text(report->scenario_name),
           safe_text(report->sequence_name),
           safe_text(report->step_name),
           safe_text(report->action_name),
           report->step_index,
           report->total_step_count,
           report->error_code,
           safe_text(report->message));

    if (report->event == DOMAIN_STATUS_EVENT_ERROR) {
        if (context != NULL) {
            context->error_report_received = true;
        }
        cleanup_after_domain_error(context, report);
        reset_after_domain_error(context, report);
    }
}

/**
 * @brief Function BのエラーScenarioを実行する。
 *
 * @return 期待通りERROR通知、清掃処理、リセット処理まで到達した場合は0。
 */
int main(void)
{
    function_b_manager_t manager;
    function_b_status_t status = FUNCTION_B_STATUS_IDLE;
    control_error_context_t context = {false, false, false};

    printf("[control-error] initialize\n");
    if (!initialize_function_b_manager(&manager, handle_domain_status, &context)) {
        printf("[control-error] initialize failed\n");
        return 1;
    }

    printf("[control-error] prepare error scenario\n");
    if (!prepare_function_b_error(&manager)) {
        printf("[control-error] prepare failed\n");
        return 1;
    }

    printf("[control-error] start\n");
    if (!start_function_b(&manager)) {
        printf("[control-error] start failed\n");
        return 1;
    }

    while (true) {
        if (!process_function_b(&manager)) {
            if (!get_function_b_status(&manager, &status)) {
                return 1;
            }
            break;
        }

        if (!get_function_b_status(&manager, &status)) {
            return 1;
        }

        if ((status == FUNCTION_B_STATUS_COMPLETED) ||
            (status == FUNCTION_B_STATUS_TERMINATED) ||
            (status == FUNCTION_B_STATUS_ERROR)) {
            break;
        }
    }

    if (status != FUNCTION_B_STATUS_ERROR) {
        printf("[control-error] expected ERROR but status=%d\n", status);
        return 1;
    }

    if (!context.error_report_received || !context.cleanup_done || !context.reset_done) {
        printf("[control-error] error handling was not completed\n");
        return 1;
    }

    printf("[control-error] expected error handling completed\n");
    return 0;
}
