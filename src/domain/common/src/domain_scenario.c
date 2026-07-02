#include "domain_scenario.h"

#include <stdio.h>

/**
 * @file domain_scenario.c
 * @brief ScenarioをAction Queueへ展開する実装。
 *
 * ScenarioはSequenceの配列です。
 * このファイルでは、Scenarioが持つSequenceを先頭から順番にAction Queueへ展開します。
 */

/**
 * @copydoc enqueue_domain_scenario_actions
 */
bool enqueue_domain_scenario_actions(
    domain_action_queue_t *queue,
    const domain_scenario_t *scenario)
{
    size_t index;
    size_t next_step_index;
    size_t total_step_count;

    if ((queue == NULL) || (scenario == NULL) ||
        (scenario->sequences == NULL) || (scenario->sequence_count == 0U)) {
        return false;
    }

    if (!count_domain_scenario_steps(scenario, &total_step_count)) {
        return false;
    }

    printf("[domain] enqueue_domain_scenario_actions(name=%s, sequence_count=%zu)\n",
           (scenario->name != NULL) ? scenario->name : "UNKNOWN",
           scenario->sequence_count);

    /*
     * Scenario内のSequence順序を保ったままQueueへ積みます。
     * Sequenceの中身は `enqueue_domain_sequence_actions()` がさらにStepへ展開します。
     */
    next_step_index = 1U;
    for (index = 0U; index < scenario->sequence_count; index++) {
        if (!enqueue_domain_sequence_actions(
                queue,
                &scenario->sequences[index],
                scenario->name,
                next_step_index,
                total_step_count)) {
            return false;
        }

        next_step_index += scenario->sequences[index].step_count;
    }

    return true;
}

bool count_domain_scenario_steps(
    const domain_scenario_t *scenario,
    size_t *total_step_count)
{
    size_t index;
    size_t total;

    if ((scenario == NULL) || (scenario->sequences == NULL) ||
        (scenario->sequence_count == 0U) || (total_step_count == NULL)) {
        return false;
    }

    total = 0U;
    for (index = 0U; index < scenario->sequence_count; index++) {
        total += scenario->sequences[index].step_count;
    }

    *total_step_count = total;
    return true;
}
