#include "domain_runner.h"

#include <stdio.h>

/**
 * @file domain_runner.c
 * @brief Action Queueを順番に実行するRunnerの実装。
 *
 * Runnerは「実行するだけ」の部品です。
 *
 * 混乱しやすい点:
 *
 * - RunnerはScenarioを選びません。
 * - RunnerはSequenceを組み立てません。
 * - RunnerはStepがUnit AなのかUnit Bなのかを知りません。
 * - RunnerはQueueからActionを取り出して `execute_domain_action()` を呼ぶだけです。
 *
 * この分け方にすると、ワークフローを変えるときはManagerやScenario側を見ればよく、
 * 実行ループを変えるときだけRunnerを見ればよくなります。
 */

static void fill_report_from_action(
    domain_status_report_t *report,
    domain_status_event_t event,
    const domain_action_t *action,
    int error_code,
    const char *message);

/**
 * @copydoc initialize_domain_runner
 *
 * RunnerはQueueを所有しません。
 * 外から渡されたQueueを参照して実行します。
 */
bool initialize_domain_runner(
    domain_runner_t *runner,
    domain_action_queue_t *queue,
    domain_status_dispatcher_t *dispatcher)
{
    if ((runner == NULL) || (queue == NULL)) {
        return false;
    }

    runner->queue = queue;
    runner->dispatcher = dispatcher;
    runner->status = DOMAIN_RUNNER_STATUS_IDLE;
    runner->has_current_action = false;
    return true;
}

/**
 * @copydoc start_domain_runner
 *
 * QueueにActionがない状態でstartしても実行するものがないため失敗にします。
 */
bool start_domain_runner(domain_runner_t *runner)
{
    if ((runner == NULL) || (runner->queue == NULL) ||
        (get_domain_action_queue_count(runner->queue) == 0U)) {
        return false;
    }

    runner->status = DOMAIN_RUNNER_STATUS_RUNNING;
    (void)dispatch_domain_status(
        runner->dispatcher,
        DOMAIN_STATUS_EVENT_STARTED,
        "Runner started");
    return true;
}

/**
 * @copydoc pause_domain_runner
 *
 * PAUSEDになると、`process_domain_runner()` を呼んでもQueueは進みません。
 */
bool pause_domain_runner(domain_runner_t *runner)
{
    if ((runner == NULL) || (runner->status != DOMAIN_RUNNER_STATUS_RUNNING)) {
        return false;
    }

    runner->status = DOMAIN_RUNNER_STATUS_PAUSED;
    (void)dispatch_domain_status(
        runner->dispatcher,
        DOMAIN_STATUS_EVENT_PAUSED,
        "Runner paused");
    return true;
}

/**
 * @copydoc restart_domain_runner
 *
 * restartは、PAUSEDからRUNNINGへ戻す操作です。
 * Queueの中身はpause前の続きから実行されます。
 */
bool restart_domain_runner(domain_runner_t *runner)
{
    if ((runner == NULL) || (runner->status != DOMAIN_RUNNER_STATUS_PAUSED)) {
        return false;
    }

    runner->status = DOMAIN_RUNNER_STATUS_RUNNING;
    (void)dispatch_domain_status(
        runner->dispatcher,
        DOMAIN_STATUS_EVENT_RESTARTED,
        "Runner restarted");
    return true;
}

/**
 * @copydoc terminate_domain_runner
 *
 * terminateは、残っているActionを実行せずにRunnerを終了状態へ移します。
 */
bool terminate_domain_runner(domain_runner_t *runner)
{
    if (runner == NULL) {
        return false;
    }

    runner->status = DOMAIN_RUNNER_STATUS_TERMINATED;
    (void)dispatch_domain_status(
        runner->dispatcher,
        DOMAIN_STATUS_EVENT_TERMINATED,
        "Runner terminated");
    return true;
}

/**
 * @copydoc process_domain_runner
 *
 * 1回のprocessでActionを1つだけ実行します。
 * これにより、Control側は次のような制御ができます。
 *
 * - 毎周期1つずつ進める
 * - 途中でpauseを挟む
 * - 途中でterminateする
 * - 進行状態を外へ通知する
 */
bool process_domain_runner(domain_runner_t *runner)
{
    domain_action_t action;
    domain_status_report_t report;

    if ((runner == NULL) || (runner->queue == NULL)) {
        return false;
    }

    /*
     * PAUSED中は正常状態です。
     * エラーではなく「今は進めない」という扱いなのでtrueを返します。
     */
    if (runner->status == DOMAIN_RUNNER_STATUS_PAUSED) {
        printf("[domain] process_domain_runner(status=PAUSED)\n");
        return true;
    }

    /*
     * RUNNING以外ではActionを実行しません。
     * IDLEやCOMPLETEDでprocessが呼ばれても、サンプルでは何もしない設計です。
     */
    if (runner->status != DOMAIN_RUNNER_STATUS_RUNNING) {
        return true;
    }

    /*
     * Queueが空なら全Actionが完了したという意味です。
     * Runner状態をCOMPLETEDへ変え、Control相当へ通知します。
     */
    if (!pop_domain_action_queue(runner->queue, &action)) {
        runner->status = DOMAIN_RUNNER_STATUS_COMPLETED;
        (void)dispatch_domain_status(
            runner->dispatcher,
            DOMAIN_STATUS_EVENT_COMPLETED,
            "Runner completed");
        return true;
    }

    runner->current_action = action;
    runner->has_current_action = true;

    /*
     * RunnerはActionのexecuteだけを呼びます。
     * ここではUnit関数を直接呼ばないことが重要です。
     */
    if (!execute_domain_action(&action)) {
        runner->status = DOMAIN_RUNNER_STATUS_ERROR;
        fill_report_from_action(
            &report,
            DOMAIN_STATUS_EVENT_ERROR,
            &action,
            -1,
            "Action failed");
        (void)dispatch_domain_status_report(runner->dispatcher, &report);
        return false;
    }

    /*
     * Actionが1つ終わったことを通知します。
     * Control側はこの通知を使ってログ表示や進捗表示ができます。
     */
    fill_report_from_action(
        &report,
        DOMAIN_STATUS_EVENT_ACTION_COMPLETED,
        &action,
        0,
        "Action completed");
    (void)dispatch_domain_status_report(runner->dispatcher, &report);

    /*
     * 今実行したActionが最後だった場合、この時点でCOMPLETEDへ遷移します。
     */
    if (get_domain_action_queue_count(runner->queue) == 0U) {
        runner->status = DOMAIN_RUNNER_STATUS_COMPLETED;
        (void)dispatch_domain_status(
            runner->dispatcher,
            DOMAIN_STATUS_EVENT_COMPLETED,
            "Runner completed");
    }

    return true;
}

static void fill_report_from_action(
    domain_status_report_t *report,
    domain_status_event_t event,
    const domain_action_t *action,
    int error_code,
    const char *message)
{
    if (report == NULL) {
        return;
    }

    report->event = event;
    report->function_name = NULL;
    report->scenario_name = (action != NULL) ? action->scenario_name : NULL;
    report->sequence_name = (action != NULL) ? action->sequence_name : NULL;
    report->step_name = (action != NULL) ? action->step_name : NULL;
    report->action_name = (action != NULL) ? action->name : NULL;
    report->step_index = (action != NULL) ? action->step_index : 0U;
    report->total_step_count = (action != NULL) ? action->total_step_count : 0U;
    report->error_code = error_code;
    report->message = message;
}

/**
 * @copydoc get_domain_runner_status
 */
bool get_domain_runner_status(
    const domain_runner_t *runner,
    domain_runner_status_t *status)
{
    if ((runner == NULL) || (status == NULL)) {
        return false;
    }

    *status = runner->status;
    return true;
}

/**
 * @copydoc convert_domain_runner_status_to_string
 */
const char *convert_domain_runner_status_to_string(domain_runner_status_t status)
{
    switch (status) {
    case DOMAIN_RUNNER_STATUS_IDLE:
        return "IDLE";
    case DOMAIN_RUNNER_STATUS_RUNNING:
        return "RUNNING";
    case DOMAIN_RUNNER_STATUS_PAUSED:
        return "PAUSED";
    case DOMAIN_RUNNER_STATUS_COMPLETED:
        return "COMPLETED";
    case DOMAIN_RUNNER_STATUS_TERMINATED:
        return "TERMINATED";
    case DOMAIN_RUNNER_STATUS_ERROR:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}
