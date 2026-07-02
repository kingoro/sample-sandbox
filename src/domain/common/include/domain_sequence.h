#ifndef DOMAIN_SEQUENCE_H
#define DOMAIN_SEQUENCE_H

#include "domain_step.h"

#include <stdbool.h>
#include <stddef.h>

/**
 * @file domain_sequence.h
 * @brief Stepの実行順序を決めるSequenceを定義する。
 *
 * Sequenceは、Stepを順番に並べたものです。
 * たとえば「初期化手順」や「実行手順」のように、意味のある手順のまとまりを表します。
 *
 * このファイルでは、SequenceがどのStepをどの順番で持つかだけを表現します。
 * 実際にUnitを動かす処理は `domain_step.c`、順番に実行する処理は `domain_runner.c` が担当します。
 */

/**
 * @brief Stepの並びを表すSequence。
 */
typedef struct {
    /** ログに表示するSequence名。 */
    const char *name;
    /** このSequenceに含まれるStep配列。 */
    const domain_step_t *steps;
    /** Step配列の要素数。 */
    size_t step_count;
} domain_sequence_t;

/**
 * @brief Sequence内のStepをAction Queueへ順番に追加する。
 *
 * @param queue 追加先Queue。
 * @param sequence Action化するSequence。
 * @return 追加できた場合は `true`、失敗した場合は `false`。
 */
bool enqueue_domain_sequence_actions(
    domain_action_queue_t *queue,
    const domain_sequence_t *sequence,
    const char *scenario_name,
    size_t first_step_index,
    size_t total_step_count);

#endif
