#include "function_b_manager.h"

#include "domain_scenario.h"

#include <stdio.h>

/**
 * @file function_b_manager.c
 * @brief Function Bの管理処理。
 *
 * Function B Managerは、Function B固有のScenarioを選び、Action Queueを組み立てます。
 * start後の実行は共通Runnerへ任せます。
 *
 * Function Aとの違い:
 *
 * - Function AはUnit A/Bを中心に使います。
 * - Function BはUnit C/Dを中心に使います。
 * - Manager、Runner、Queueの形は同じです。
 *
 * このように、機能ごとにフォルダを分けると、共通部品を使い回しながら、
 * 機能固有のScenarioだけを差し替える構造が見えやすくなります。
 */

/**
 * @brief Function Bの準備Sequenceに含めるStep。
 *
 * この配列の順番が、そのままStepの実行順になります。
 * Function Bでは、まずUnit Cを準備する流れにしています。
 */
static const domain_step_t g_function_b_prepare_steps[] = {
    {"initialize Unit C", DOMAIN_STEP_UNIT_C, DOMAIN_STEP_OPERATION_INITIALIZE, 0},
    {"open Unit C", DOMAIN_STEP_UNIT_C, DOMAIN_STEP_OPERATION_OPEN, 0},
    {"move Unit C to 300", DOMAIN_STEP_UNIT_C, DOMAIN_STEP_OPERATION_MOVE, 300},
    {"close Unit C", DOMAIN_STEP_UNIT_C, DOMAIN_STEP_OPERATION_CLOSE, 0},
};

/**
 * @brief Function Bの実行Sequenceに含めるStep。
 *
 * Unit Dを動かし、最後にUnit C/Dを終了するStepを並べています。
 */
static const domain_step_t g_function_b_execute_steps[] = {
    {"initialize Unit D", DOMAIN_STEP_UNIT_D, DOMAIN_STEP_OPERATION_INITIALIZE, 0},
    {"open Unit D", DOMAIN_STEP_UNIT_D, DOMAIN_STEP_OPERATION_OPEN, 0},
    {"move Unit D to 400", DOMAIN_STEP_UNIT_D, DOMAIN_STEP_OPERATION_MOVE, 400},
    {"close Unit D", DOMAIN_STEP_UNIT_D, DOMAIN_STEP_OPERATION_CLOSE, 0},
    {"get Unit C status", DOMAIN_STEP_UNIT_C, DOMAIN_STEP_OPERATION_GET_STATUS, 0},
    {"terminate Unit C", DOMAIN_STEP_UNIT_C, DOMAIN_STEP_OPERATION_TERMINATE, 0},
    {"terminate Unit D", DOMAIN_STEP_UNIT_D, DOMAIN_STEP_OPERATION_TERMINATE, 0},
};

/**
 * @brief Function Bで使うSequence一覧。
 *
 * SequenceはStep配列をまとめたものです。
 * この配列の順番が、Scenario内でのSequence実行順になります。
 */
static const domain_sequence_t g_function_b_sequences[] = {
    {"Function B prepare sequence",
     g_function_b_prepare_steps,
     sizeof(g_function_b_prepare_steps) / sizeof(g_function_b_prepare_steps[0])},
    {"Function B execute sequence",
     g_function_b_execute_steps,
     sizeof(g_function_b_execute_steps) / sizeof(g_function_b_execute_steps[0])},
};

/**
 * @brief Function Bで使うScenario。
 *
 * ScenarioはSequence配列をまとめたものです。
 * prepare時には、このScenarioがAction Queueへ展開されます。
 */
static const domain_scenario_t g_function_b_scenario = {
    "Function B standard scenario",
    g_function_b_sequences,
    sizeof(g_function_b_sequences) / sizeof(g_function_b_sequences[0]),
};

/**
 * @brief Function Bのエラー確認用Sequenceに含めるStep。
 *
 * 3番目のStepでUnit Cへ負の移動先を渡します。
 * Unit Cはこの値を不正として `false` を返し、RunnerはそこでERRORへ遷移します。
 * 後続Stepは、ERROR時には実行されないことを確認するために置いています。
 */
static const domain_step_t g_function_b_error_steps[] = {
    {"initialize Unit C", DOMAIN_STEP_UNIT_C, DOMAIN_STEP_OPERATION_INITIALIZE, 0},
    {"open Unit C", DOMAIN_STEP_UNIT_C, DOMAIN_STEP_OPERATION_OPEN, 0},
    {"move Unit C to invalid position", DOMAIN_STEP_UNIT_C, DOMAIN_STEP_OPERATION_MOVE, -1},
    {"close Unit C after error", DOMAIN_STEP_UNIT_C, DOMAIN_STEP_OPERATION_CLOSE, 0},
    {"terminate Unit C after error", DOMAIN_STEP_UNIT_C, DOMAIN_STEP_OPERATION_TERMINATE, 0},
};

/**
 * @brief Function Bのエラー確認用Sequence一覧。
 */
static const domain_sequence_t g_function_b_error_sequences[] = {
    {"Function B error sequence",
     g_function_b_error_steps,
     sizeof(g_function_b_error_steps) / sizeof(g_function_b_error_steps[0])},
};

/**
 * @brief Function Bのエラー確認用Scenario。
 */
static const domain_scenario_t g_function_b_error_scenario = {
    "Function B error scenario",
    g_function_b_error_sequences,
    sizeof(g_function_b_error_sequences) / sizeof(g_function_b_error_sequences[0]),
};

/**
 * @brief Runner状態をFunction Bの状態へ反映する。
 *
 * Runnerは汎用的な実行器なので、Runnerの状態をそのまま外へ出すのではなく、
 * Function Bとしての状態へ写し替えます。
 */
static bool update_function_b_status_from_runner(function_b_manager_t *manager);

/**
 * @brief 指定されたScenarioをAction Queueへ展開する。
 *
 * 通常Scenarioとエラー確認用Scenarioでprepare処理の形を揃えるための共通処理です。
 */
static bool prepare_function_b_scenario(
    function_b_manager_t *manager,
    const domain_scenario_t *scenario,
    const char *prepared_message);

/**
 * @copydoc initialize_function_b_manager
 */
bool initialize_function_b_manager(
    function_b_manager_t *manager,
    domain_status_handler_fn handler,
    void *user_context)
{
    if (manager == NULL) {
        return false;
    }

    if (!initialize_domain_action_queue(&manager->queue)) {
        return false;
    }

    if (!initialize_domain_status_dispatcher(&manager->dispatcher, handler, user_context)) {
        return false;
    }

    (void)set_domain_status_dispatcher_function_name(&manager->dispatcher, "Function B");

    if (!initialize_domain_runner(&manager->runner, &manager->queue, &manager->dispatcher)) {
        return false;
    }

    manager->status = FUNCTION_B_STATUS_IDLE;
    return true;
}

/**
 * @copydoc prepare_function_b
 *
 * prepareは「実行するActionの列を作る」段階です。
 * まだUnitは動きません。
 */
bool prepare_function_b(function_b_manager_t *manager)
{
    return prepare_function_b_scenario(
        manager,
        &g_function_b_scenario,
        "Function B prepared");
}

/**
 * @copydoc prepare_function_b_error
 */
bool prepare_function_b_error(function_b_manager_t *manager)
{
    return prepare_function_b_scenario(
        manager,
        &g_function_b_error_scenario,
        "Function B error scenario prepared");
}

static bool prepare_function_b_scenario(
    function_b_manager_t *manager,
    const domain_scenario_t *scenario,
    const char *prepared_message)
{
    if (manager == NULL) {
        return false;
    }

    if (!clear_domain_action_queue(&manager->queue)) {
        manager->status = FUNCTION_B_STATUS_ERROR;
        return false;
    }

    printf("[domain] prepare_function_b()\n");

    if (!enqueue_domain_scenario_actions(&manager->queue, scenario)) {
        manager->status = FUNCTION_B_STATUS_ERROR;
        (void)dispatch_domain_status(
            &manager->dispatcher,
            DOMAIN_STATUS_EVENT_ERROR,
            "Function B prepare failed");
        return false;
    }

    manager->status = FUNCTION_B_STATUS_PREPARED;
    (void)initialize_domain_runner(&manager->runner, &manager->queue, &manager->dispatcher);
    (void)dispatch_domain_status(
        &manager->dispatcher,
        DOMAIN_STATUS_EVENT_PREPARED,
        prepared_message);
    return true;
}

/**
 * @copydoc start_function_b
 */
bool start_function_b(function_b_manager_t *manager)
{
    if ((manager == NULL) || (manager->status != FUNCTION_B_STATUS_PREPARED)) {
        return false;
    }

    if (!start_domain_runner(&manager->runner)) {
        manager->status = FUNCTION_B_STATUS_ERROR;
        return false;
    }

    manager->status = FUNCTION_B_STATUS_RUNNING;
    return true;
}

/**
 * @copydoc pause_function_b
 */
bool pause_function_b(function_b_manager_t *manager)
{
    if ((manager == NULL) || (manager->status != FUNCTION_B_STATUS_RUNNING)) {
        return false;
    }

    if (!pause_domain_runner(&manager->runner)) {
        return false;
    }

    manager->status = FUNCTION_B_STATUS_PAUSED;
    return true;
}

/**
 * @copydoc restart_function_b
 */
bool restart_function_b(function_b_manager_t *manager)
{
    if ((manager == NULL) || (manager->status != FUNCTION_B_STATUS_PAUSED)) {
        return false;
    }

    if (!restart_domain_runner(&manager->runner)) {
        return false;
    }

    manager->status = FUNCTION_B_STATUS_RUNNING;
    return true;
}

/**
 * @copydoc terminate_function_b
 */
bool terminate_function_b(function_b_manager_t *manager)
{
    if (manager == NULL) {
        return false;
    }

    if (!terminate_domain_runner(&manager->runner)) {
        return false;
    }

    manager->status = FUNCTION_B_STATUS_TERMINATED;
    return true;
}

/**
 * @copydoc process_function_b
 */
bool process_function_b(function_b_manager_t *manager)
{
    if (manager == NULL) {
        return false;
    }

    if (!process_domain_runner(&manager->runner)) {
        manager->status = FUNCTION_B_STATUS_ERROR;
        return false;
    }

    return update_function_b_status_from_runner(manager);
}

/**
 * @copydoc get_function_b_status
 */
bool get_function_b_status(
    const function_b_manager_t *manager,
    function_b_status_t *status)
{
    if ((manager == NULL) || (status == NULL)) {
        return false;
    }

    *status = manager->status;
    return true;
}

static bool update_function_b_status_from_runner(function_b_manager_t *manager)
{
    domain_runner_status_t runner_status;

    if (!get_domain_runner_status(&manager->runner, &runner_status)) {
        return false;
    }

    switch (runner_status) {
    case DOMAIN_RUNNER_STATUS_RUNNING:
        manager->status = FUNCTION_B_STATUS_RUNNING;
        break;
    case DOMAIN_RUNNER_STATUS_PAUSED:
        manager->status = FUNCTION_B_STATUS_PAUSED;
        break;
    case DOMAIN_RUNNER_STATUS_COMPLETED:
        manager->status = FUNCTION_B_STATUS_COMPLETED;
        break;
    case DOMAIN_RUNNER_STATUS_TERMINATED:
        manager->status = FUNCTION_B_STATUS_TERMINATED;
        break;
    case DOMAIN_RUNNER_STATUS_ERROR:
        manager->status = FUNCTION_B_STATUS_ERROR;
        break;
    default:
        break;
    }

    return true;
}
