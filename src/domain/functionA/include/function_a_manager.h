#ifndef FUNCTION_A_MANAGER_H
#define FUNCTION_A_MANAGER_H

#include "domain_action_queue.h"
#include "domain_runner.h"
#include "domain_status_dispatcher.h"

#include <stdbool.h>

/**
 * @file function_a_manager.h
 * @brief Function Aのprepare/start/pause/restart/terminateを管理する。
 *
 * Function Managerは、Controlから見える機能単位の入口です。
 *
 * ControlはScenarioやSequenceの細かい構造を知りません。
 * Controlは `prepare_function_a()` や `start_function_a()` を呼びます。
 * その中でFunction ManagerがScenarioを選び、Action Queueを作り、Runnerを操作します。
 */

/**
 * @brief Function Aの状態。
 */
typedef enum {
    /** まだprepareされていない状態。 */
    FUNCTION_A_STATUS_IDLE = 0,
    /** prepare済みで、Action Queueが構築されている状態。 */
    FUNCTION_A_STATUS_PREPARED,
    /** RunnerがActionを実行中の状態。 */
    FUNCTION_A_STATUS_RUNNING,
    /** Runnerが一時停止中の状態。 */
    FUNCTION_A_STATUS_PAUSED,
    /** RunnerがすべてのActionを実行し終えた状態。 */
    FUNCTION_A_STATUS_COMPLETED,
    /** terminate要求により終了した状態。 */
    FUNCTION_A_STATUS_TERMINATED,
    /** prepareまたは実行中に失敗した状態。 */
    FUNCTION_A_STATUS_ERROR
} function_a_status_t;

/**
 * @brief Function Aの管理オブジェクト。
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
    /** Function Aとして外側へ見せる状態。 */
    function_a_status_t status;
} function_a_manager_t;

/**
 * @brief Function A Managerを初期化する。
 *
 * @param manager 初期化するManager。
 * @param handler Control相当へ状態を通知するコールバック。未使用の場合は `NULL`。
 * @param user_context コールバックへ渡す任意の情報。
 * @return 初期化できた場合は `true`、引数が不正な場合は `false`。
 */
bool initialize_function_a_manager(
    function_a_manager_t *manager,
    domain_status_handler_fn handler,
    void *user_context);

/**
 * @brief Function Aの実行準備を行う。
 *
 * Scenario、Sequence、Stepの組み合わせを選び、Action Queueへ積みます。
 *
 * @param manager 対象Manager。
 * @return 準備できた場合は `true`、失敗した場合は `false`。
 */
bool prepare_function_a(function_a_manager_t *manager);

/**
 * @brief Function Aを開始する。
 *
 * @param manager 対象Manager。
 * @return 開始できた場合は `true`、失敗した場合は `false`。
 */
bool start_function_a(function_a_manager_t *manager);

/**
 * @brief Function Aを一時停止する。
 *
 * @param manager 対象Manager。
 * @return 一時停止できた場合は `true`、失敗した場合は `false`。
 */
bool pause_function_a(function_a_manager_t *manager);

/**
 * @brief Function Aを再開する。
 *
 * @param manager 対象Manager。
 * @return 再開できた場合は `true`、失敗した場合は `false`。
 */
bool restart_function_a(function_a_manager_t *manager);

/**
 * @brief Function Aを終了する。
 *
 * @param manager 対象Manager。
 * @return 終了できた場合は `true`、失敗した場合は `false`。
 */
bool terminate_function_a(function_a_manager_t *manager);

/**
 * @brief Function Aを1ステップ分進める。
 *
 * @param manager 対象Manager。
 * @return 処理できた場合は `true`、失敗した場合は `false`。
 */
bool process_function_a(function_a_manager_t *manager);

/**
 * @brief Function Aの状態を取得する。
 *
 * @param manager 対象Manager。
 * @param status 状態を書き込む出力先。
 * @return 取得できた場合は `true`、引数が不正な場合は `false`。
 */
bool get_function_a_status(
    const function_a_manager_t *manager,
    function_a_status_t *status);

#endif
