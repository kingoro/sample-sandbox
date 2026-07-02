#include "function_a_manager.h"

#include "domain_scenario.h"

#include <stdio.h>

/**
 * @file function_a_manager.c
 * @brief Function Aの管理処理。
 *
 * Function A Managerは、prepareでScenarioを選択してAction Queueを組み立てます。
 * start後の実行はRunnerへ任せます。
 *
 * このファイルは「管理する役割」の例です。
 *
 * Runnerは実行だけを担当します。
 * それに対してFunction A Managerは、Controlからの要求を受けて、
 * どのScenarioを使うか、いつRunnerを開始するか、pause/restart/terminateをどう扱うかを管理します。
 *
 * 読む順番:
 *
 * 1. `g_function_a_prepare_steps` と `g_function_a_execute_steps` でStepの並びを見る。
 * 2. `g_function_a_sequences` でStepがSequenceにまとまる様子を見る。
 * 3. `g_function_a_scenario` でSequenceがScenarioにまとまる様子を見る。
 * 4. `prepare_function_a()` でScenarioがAction Queueへ展開される流れを見る。
 * 5. `start_function_a()` 以降でRunnerへ実行を任せる流れを見る。
 */

/**
 * @brief Function Aの準備Sequenceに含めるStep。
 *
 * この配列の順番が、そのままStepの実行順になります。
 * ここではUnit Aを初期化し、開き、移動し、閉じる流れにしています。
 */
static const domain_step_t g_function_a_prepare_steps[] = {
    {"initialize Unit A", DOMAIN_STEP_UNIT_A, DOMAIN_STEP_OPERATION_INITIALIZE, 0},
    {"open Unit A", DOMAIN_STEP_UNIT_A, DOMAIN_STEP_OPERATION_OPEN, 0},
    {"move Unit A to 100", DOMAIN_STEP_UNIT_A, DOMAIN_STEP_OPERATION_MOVE, 100},
    {"close Unit A", DOMAIN_STEP_UNIT_A, DOMAIN_STEP_OPERATION_CLOSE, 0},
};

/**
 * @brief Function Aの実行Sequenceに含めるStep。
 *
 * Unit Bの操作と、最後にUnit A/Bを終了するStepを並べています。
 * サンプルなので業務的な意味は薄く、順番に実行される様子を見るための定義です。
 */
static const domain_step_t g_function_a_execute_steps[] = {
    {"initialize Unit B", DOMAIN_STEP_UNIT_B, DOMAIN_STEP_OPERATION_INITIALIZE, 0},
    {"open Unit B", DOMAIN_STEP_UNIT_B, DOMAIN_STEP_OPERATION_OPEN, 0},
    {"move Unit B to 200", DOMAIN_STEP_UNIT_B, DOMAIN_STEP_OPERATION_MOVE, 200},
    {"close Unit B", DOMAIN_STEP_UNIT_B, DOMAIN_STEP_OPERATION_CLOSE, 0},
    {"get Unit A status", DOMAIN_STEP_UNIT_A, DOMAIN_STEP_OPERATION_GET_STATUS, 0},
    {"terminate Unit A", DOMAIN_STEP_UNIT_A, DOMAIN_STEP_OPERATION_TERMINATE, 0},
    {"terminate Unit B", DOMAIN_STEP_UNIT_B, DOMAIN_STEP_OPERATION_TERMINATE, 0},
};

/**
 * @brief Function Aで使うSequence一覧。
 *
 * SequenceはStep配列をまとめたものです。
 * この配列の順番が、Scenario内でのSequence実行順になります。
 */
static const domain_sequence_t g_function_a_sequences[] = {
    {"Function A prepare sequence",
     g_function_a_prepare_steps,
     sizeof(g_function_a_prepare_steps) / sizeof(g_function_a_prepare_steps[0])},
    {"Function A execute sequence",
     g_function_a_execute_steps,
     sizeof(g_function_a_execute_steps) / sizeof(g_function_a_execute_steps[0])},
};

/**
 * @brief Function Aで使うScenario。
 *
 * ScenarioはSequence配列をまとめたものです。
 * prepare時には、このScenarioがAction Queueへ展開されます。
 */
static const domain_scenario_t g_function_a_scenario = {
    "Function A standard scenario",
    g_function_a_sequences,
    sizeof(g_function_a_sequences) / sizeof(g_function_a_sequences[0]),
};

/**
 * @brief Runner状態をFunction Aの状態へ反映する。
 *
 * Runnerは汎用的な実行器なので、Runnerの状態をそのまま外へ出すのではなく、
 * Function Aとしての状態へ写し替えます。
 */
static bool update_function_a_status_from_runner(function_a_manager_t *manager);

/**
 * @copydoc initialize_function_a_manager
 *
 * Manager初期化では、Queue、Dispatcher、Runnerをまとめて初期化します。
 */
bool initialize_function_a_manager(
    function_a_manager_t *manager,
    domain_status_handler_fn handler,
    void *user_context)
{
    if (manager == NULL) {
        return false;
    }

    /*
     * prepare時にActionを積む入れ物です。
     * ManagerがQueueを所有します。
     */
    if (!initialize_domain_action_queue(&manager->queue)) {
        return false;
    }

    /*
     * Control相当へ状態変化を通知するための口です。
     * handlerがNULLなら通知なしでも動きます。
     */
    if (!initialize_domain_status_dispatcher(&manager->dispatcher, handler, user_context)) {
        return false;
    }

    (void)set_domain_status_dispatcher_function_name(&manager->dispatcher, "Function A");

    /*
     * RunnerはQueueを参照して実行します。
     * Runner自身はQueueを作りません。
     */
    if (!initialize_domain_runner(&manager->runner, &manager->queue, &manager->dispatcher)) {
        return false;
    }

    manager->status = FUNCTION_A_STATUS_IDLE;
    return true;
}

/**
 * @copydoc prepare_function_a
 *
 * prepareは「実行するActionの列を作る」段階です。
 * まだUnitは動きません。
 */
bool prepare_function_a(function_a_manager_t *manager)
{
    if (manager == NULL) {
        return false;
    }

    /*
     * 再prepareに備えて、前回のActionを消します。
     */
    if (!clear_domain_action_queue(&manager->queue)) {
        manager->status = FUNCTION_A_STATUS_ERROR;
        return false;
    }

    printf("[domain] prepare_function_a()\n");

    /*
     * Scenario -> Sequence -> Step -> Action の順で展開されます。
     * ここでQueueにActionが積まれますが、実行はまだ行いません。
     */
    if (!enqueue_domain_scenario_actions(&manager->queue, &g_function_a_scenario)) {
        manager->status = FUNCTION_A_STATUS_ERROR;
        (void)dispatch_domain_status(
            &manager->dispatcher,
            DOMAIN_STATUS_EVENT_ERROR,
            "Function A prepare failed");
        return false;
    }

    /*
     * prepareが終わったので、start可能な状態にします。
     * RunnerもQueueの先頭から実行できるように初期化し直します。
     */
    manager->status = FUNCTION_A_STATUS_PREPARED;
    (void)initialize_domain_runner(&manager->runner, &manager->queue, &manager->dispatcher);
    (void)dispatch_domain_status(
        &manager->dispatcher,
        DOMAIN_STATUS_EVENT_PREPARED,
        "Function A prepared");
    return true;
}

/**
 * @copydoc start_function_a
 *
 * startはRunnerをRUNNINGへ移すだけです。
 * 実際にActionを1つ進めるのは `process_function_a()` です。
 */
bool start_function_a(function_a_manager_t *manager)
{
    if ((manager == NULL) || (manager->status != FUNCTION_A_STATUS_PREPARED)) {
        return false;
    }

    if (!start_domain_runner(&manager->runner)) {
        manager->status = FUNCTION_A_STATUS_ERROR;
        return false;
    }

    manager->status = FUNCTION_A_STATUS_RUNNING;
    return true;
}

/**
 * @copydoc pause_function_a
 *
 * pause後は、Queueに残っているActionはそのまま保持されます。
 */
bool pause_function_a(function_a_manager_t *manager)
{
    if ((manager == NULL) || (manager->status != FUNCTION_A_STATUS_RUNNING)) {
        return false;
    }

    if (!pause_domain_runner(&manager->runner)) {
        return false;
    }

    manager->status = FUNCTION_A_STATUS_PAUSED;
    return true;
}

/**
 * @copydoc restart_function_a
 *
 * restartすると、pause前に残っていたActionの続きから実行します。
 */
bool restart_function_a(function_a_manager_t *manager)
{
    if ((manager == NULL) || (manager->status != FUNCTION_A_STATUS_PAUSED)) {
        return false;
    }

    if (!restart_domain_runner(&manager->runner)) {
        return false;
    }

    manager->status = FUNCTION_A_STATUS_RUNNING;
    return true;
}

/**
 * @copydoc terminate_function_a
 *
 * terminateは残りActionを実行せずに終了扱いにします。
 */
bool terminate_function_a(function_a_manager_t *manager)
{
    if (manager == NULL) {
        return false;
    }

    if (!terminate_domain_runner(&manager->runner)) {
        return false;
    }

    manager->status = FUNCTION_A_STATUS_TERMINATED;
    return true;
}

/**
 * @copydoc process_function_a
 *
 * Function AのprocessはRunnerへ処理を委譲します。
 * Manager自身はActionを実行しません。
 */
bool process_function_a(function_a_manager_t *manager)
{
    if (manager == NULL) {
        return false;
    }

    /*
     * Runnerが1Action分進めます。
     * 失敗した場合はFunction AとしてもERRORにします。
     */
    if (!process_domain_runner(&manager->runner)) {
        manager->status = FUNCTION_A_STATUS_ERROR;
        return false;
    }

    return update_function_a_status_from_runner(manager);
}

/**
 * @copydoc get_function_a_status
 */
bool get_function_a_status(
    const function_a_manager_t *manager,
    function_a_status_t *status)
{
    if ((manager == NULL) || (status == NULL)) {
        return false;
    }

    *status = manager->status;
    return true;
}

static bool update_function_a_status_from_runner(function_a_manager_t *manager)
{
    domain_runner_status_t runner_status;

    /*
     * Runnerの状態を読み取り、Function Aの状態へ変換します。
     * 外側のControlはFunction A単位の状態だけを見ればよい、という設計です。
     */
    if (!get_domain_runner_status(&manager->runner, &runner_status)) {
        return false;
    }

    switch (runner_status) {
    case DOMAIN_RUNNER_STATUS_RUNNING:
        manager->status = FUNCTION_A_STATUS_RUNNING;
        break;
    case DOMAIN_RUNNER_STATUS_PAUSED:
        manager->status = FUNCTION_A_STATUS_PAUSED;
        break;
    case DOMAIN_RUNNER_STATUS_COMPLETED:
        manager->status = FUNCTION_A_STATUS_COMPLETED;
        break;
    case DOMAIN_RUNNER_STATUS_TERMINATED:
        manager->status = FUNCTION_A_STATUS_TERMINATED;
        break;
    case DOMAIN_RUNNER_STATUS_ERROR:
        manager->status = FUNCTION_A_STATUS_ERROR;
        break;
    default:
        break;
    }

    return true;
}
