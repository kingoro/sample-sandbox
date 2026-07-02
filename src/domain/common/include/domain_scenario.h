#ifndef DOMAIN_SCENARIO_H
#define DOMAIN_SCENARIO_H

#include "domain_sequence.h"

#include <stdbool.h>
#include <stddef.h>

/**
 * @file domain_scenario.h
 * @brief Sequenceの実行順序を決めるScenarioを定義する。
 *
 * Scenarioは、機能や業務として意味のある一連の流れです。
 * たとえば「通常運転」「停止処理」「診断処理」のような単位を想定します。
 *
 * このファイルでは、ScenarioがどのSequenceをどの順番で持つかだけを表現します。
 * Scenario自身はUnitを直接操作しません。
 */

/**
 * @brief Sequenceの並びを表すScenario。
 */
typedef struct {
    /** ログに表示するScenario名。 */
    const char *name;
    /** このScenarioに含まれるSequence配列。 */
    const domain_sequence_t *sequences;
    /** Sequence配列の要素数。 */
    size_t sequence_count;
} domain_scenario_t;

/**
 * @brief Scenario内のSequenceをAction Queueへ順番に追加する。
 *
 * @param queue 追加先Queue。
 * @param scenario Action化するScenario。
 * @return 追加できた場合は `true`、失敗した場合は `false`。
 */
bool enqueue_domain_scenario_actions(
    domain_action_queue_t *queue,
    const domain_scenario_t *scenario);

/**
 * @brief Scenarioに含まれるStep総数を数える。
 *
 * @param scenario 対象Scenario。
 * @param total_step_count Step総数を書き込む出力先。
 * @return 数えられた場合は `true`、引数が不正な場合は `false`。
 */
bool count_domain_scenario_steps(
    const domain_scenario_t *scenario,
    size_t *total_step_count);

#endif
