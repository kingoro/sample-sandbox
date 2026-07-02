#ifndef DOMAIN_RUNNER_H
#define DOMAIN_RUNNER_H

#include "domain_action_queue.h"
#include "domain_status_dispatcher.h"

#include <stdbool.h>

/**
 * @file domain_runner.h
 * @brief Queueに積まれたActionを順番に実行するRunnerを定義する。
 *
 * Runnerは実行専用の部品です。
 * Scenario、Sequence、Stepを選ぶ責務は持ちません。
 * Unit AやUnit Bの具体的な関数も知りません。
 *
 * Runnerが行うことは、次の4つだけです。
 *
 * 1. startでRUNNING状態になる。
 * 2. processでQueueからActionを1つ取り出す。
 * 3. Actionのexecuteを呼ぶ。
 * 4. Queueが空になったらCOMPLETEDへ遷移する。
 */

/**
 * @brief Runnerの実行状態。
 */
typedef enum {
    /** Runnerは未開始。 */
    DOMAIN_RUNNER_STATUS_IDLE = 0,
    /** RunnerはActionを実行中。 */
    DOMAIN_RUNNER_STATUS_RUNNING,
    /** Runnerは一時停止中。processしてもActionを進めない。 */
    DOMAIN_RUNNER_STATUS_PAUSED,
    /** QueueのActionをすべて実行した。 */
    DOMAIN_RUNNER_STATUS_COMPLETED,
    /** 外部要求により終了した。 */
    DOMAIN_RUNNER_STATUS_TERMINATED,
    /** Action実行失敗などで異常終了した。 */
    DOMAIN_RUNNER_STATUS_ERROR
} domain_runner_status_t;

/**
 * @brief Action Queueを実行するRunner。
 *
 * RunnerはActionの中身を知りません。
 * Queueから取り出したActionの `execute` を呼び出すだけです。
 */
typedef struct {
    domain_action_queue_t *queue;
    domain_status_dispatcher_t *dispatcher;
    domain_runner_status_t status;
    /** 直近に実行したAction。進捗やエラー文脈の通知に使います。 */
    domain_action_t current_action;
    /** 直近Actionがある場合は `true`。 */
    bool has_current_action;
} domain_runner_t;

/**
 * @brief Runnerを初期化する。
 *
 * @param runner 初期化するRunner。
 * @param queue 実行対象のAction Queue。
 * @param dispatcher 状態通知に使うDispatcher。未使用の場合は `NULL`。
 * @return 初期化できた場合は `true`、引数が不正な場合は `false`。
 */
bool initialize_domain_runner(
    domain_runner_t *runner,
    domain_action_queue_t *queue,
    domain_status_dispatcher_t *dispatcher);

/**
 * @brief Runnerを開始する。
 *
 * @param runner 対象Runner。
 * @return 開始できた場合は `true`、失敗した場合は `false`。
 */
bool start_domain_runner(domain_runner_t *runner);

/**
 * @brief Runnerを一時停止する。
 *
 * @param runner 対象Runner。
 * @return 一時停止できた場合は `true`、失敗した場合は `false`。
 */
bool pause_domain_runner(domain_runner_t *runner);

/**
 * @brief 一時停止中のRunnerを再開する。
 *
 * @param runner 対象Runner。
 * @return 再開できた場合は `true`、失敗した場合は `false`。
 */
bool restart_domain_runner(domain_runner_t *runner);

/**
 * @brief Runnerを終了状態にする。
 *
 * @param runner 対象Runner。
 * @return 終了できた場合は `true`、失敗した場合は `false`。
 */
bool terminate_domain_runner(domain_runner_t *runner);

/**
 * @brief Runnerを1ステップ進める。
 *
 * @param runner 対象Runner。
 * @return 処理できた場合は `true`、Action失敗または引数不正の場合は `false`。
 */
bool process_domain_runner(domain_runner_t *runner);

/**
 * @brief Runnerの現在状態を取得する。
 *
 * @param runner 対象Runner。
 * @param status 状態を書き込む出力先。
 * @return 取得できた場合は `true`、引数が不正な場合は `false`。
 */
bool get_domain_runner_status(
    const domain_runner_t *runner,
    domain_runner_status_t *status);

/**
 * @brief Runner状態を表示用文字列へ変換する。
 *
 * @param status 変換する状態。
 * @return 状態名を表す静的文字列。
 */
const char *convert_domain_runner_status_to_string(domain_runner_status_t status);

#endif
