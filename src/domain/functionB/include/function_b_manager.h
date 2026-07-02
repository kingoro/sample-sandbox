#ifndef FUNCTION_B_MANAGER_H
#define FUNCTION_B_MANAGER_H

#include "domain_action_queue.h"
#include "domain_runner.h"
#include "domain_status_dispatcher.h"

#include <stdbool.h>

/**
 * @file function_b_manager.h
 * @brief Function Bのprepare/start/pause/restart/terminateを管理する。
 *
 * Function B Managerは、Controlから見える機能単位の入口です。
 *
 * Function Aと同じ共通Runner、Action Queue、Scenario、Sequence、Stepの仕組みを使います。
 * 違いは、Function B固有のScenario定義と、そこに並べるStepの内容です。
 */

/**
 * @brief Function Bの状態。
 */
typedef enum {
    /** まだprepareされていない状態。 */
    FUNCTION_B_STATUS_IDLE = 0,
    /** prepare済みで、Action Queueが構築されている状態。 */
    FUNCTION_B_STATUS_PREPARED,
    /** RunnerがActionを実行中の状態。 */
    FUNCTION_B_STATUS_RUNNING,
    /** Runnerが一時停止中の状態。 */
    FUNCTION_B_STATUS_PAUSED,
    /** RunnerがすべてのActionを実行し終えた状態。 */
    FUNCTION_B_STATUS_COMPLETED,
    /** terminate要求により終了した状態。 */
    FUNCTION_B_STATUS_TERMINATED,
    /** prepareまたは実行中に失敗した状態。 */
    FUNCTION_B_STATUS_ERROR
} function_b_status_t;

/**
 * @brief Function Bの管理オブジェクト。
 *
 * Function Managerは、Scenarioを選び、Action Queueを組み立て、Runnerを操作します。
 */
typedef struct {
    /** prepare時にScenarioから展開されたActionを保持するQueue。 */
    domain_action_queue_t queue;
    /** RunnerやManagerの状態変化をControl相当へ通知するDispatcher。 */
    domain_status_dispatcher_t dispatcher;
    /** Action Queueを実行するRunner。 */
    domain_runner_t runner;
    /** Function Bとして外側へ見せる状態。 */
    function_b_status_t status;
} function_b_manager_t;

/**
 * @brief Function B Managerを初期化する。
 *
 * @param manager 初期化するManager。
 * @param handler Control相当へ状態を通知するコールバック。未使用の場合は `NULL`。
 * @param user_context コールバックへ渡す任意の情報。
 * @return 初期化できた場合は `true`、引数が不正な場合は `false`。
 */
bool initialize_function_b_manager(
    function_b_manager_t *manager,
    domain_status_handler_fn handler,
    void *user_context);

/**
 * @brief Function Bの実行準備を行う。
 *
 * Scenario、Sequence、Stepの組み合わせを選び、Action Queueへ積みます。
 *
 * @param manager 対象Manager。
 * @return 準備できた場合は `true`、失敗した場合は `false`。
 */
bool prepare_function_b(function_b_manager_t *manager);

/**
 * @brief Function Bのエラー確認用Scenarioで実行準備を行う。
 *
 * 負の移動先を指定するStepを含むScenarioをAction Queueへ積みます。
 * RunnerがUnitの `false` を検出し、ERROR通知をControlへ返す流れを確認するためのAPIです。
 *
 * @param manager 対象Manager。
 * @return 準備できた場合は `true`、失敗した場合は `false`。
 */
bool prepare_function_b_error(function_b_manager_t *manager);

/**
 * @brief Function Bを開始する。
 *
 * @param manager 対象Manager。
 * @return 開始できた場合は `true`、失敗した場合は `false`。
 */
bool start_function_b(function_b_manager_t *manager);

/**
 * @brief Function Bを一時停止する。
 *
 * @param manager 対象Manager。
 * @return 一時停止できた場合は `true`、失敗した場合は `false`。
 */
bool pause_function_b(function_b_manager_t *manager);

/**
 * @brief Function Bを再開する。
 *
 * @param manager 対象Manager。
 * @return 再開できた場合は `true`、失敗した場合は `false`。
 */
bool restart_function_b(function_b_manager_t *manager);

/**
 * @brief Function Bを終了する。
 *
 * @param manager 対象Manager。
 * @return 終了できた場合は `true`、失敗した場合は `false`。
 */
bool terminate_function_b(function_b_manager_t *manager);

/**
 * @brief Function Bを1ステップ分進める。
 *
 * @param manager 対象Manager。
 * @return 処理できた場合は `true`、失敗した場合は `false`。
 */
bool process_function_b(function_b_manager_t *manager);

/**
 * @brief Function Bの状態を取得する。
 *
 * @param manager 対象Manager。
 * @param status 状態を書き込む出力先。
 * @return 取得できた場合は `true`、引数が不正な場合は `false`。
 */
bool get_function_b_status(
    const function_b_manager_t *manager,
    function_b_status_t *status);

#endif
