#include "domain_sequence.h"

#include <stdio.h>

/**
 * @file domain_sequence.c
 * @brief SequenceをAction Queueへ展開する実装。
 *
 * SequenceはStepの配列です。
 * このファイルでは、配列の先頭から順番にStepをActionへ変換してQueueへ積みます。
 */

/**
 * @copydoc enqueue_domain_sequence_actions
 */
bool enqueue_domain_sequence_actions(
    domain_action_queue_t *queue,
    const domain_sequence_t *sequence,
    const char *scenario_name,
    size_t first_step_index,
    size_t total_step_count)
{
    size_t index;

    if ((queue == NULL) || (sequence == NULL) ||
        (sequence->steps == NULL) || (sequence->step_count == 0U)) {
        return false;
    }

    printf("[domain] enqueue_domain_sequence_actions(name=%s, step_count=%zu)\n",
           (sequence->name != NULL) ? sequence->name : "UNKNOWN",
           sequence->step_count);

    /*
     * Sequence内のStep順序を保ったままQueueへ積みます。
     * ここで順番を変えないことが、Sequenceの責務として大事です。
     */
    for (index = 0U; index < sequence->step_count; index++) {
        const size_t step_index = first_step_index + index;

        if (!enqueue_domain_step_action(
                queue,
                &sequence->steps[index],
                scenario_name,
                sequence->name,
                step_index,
                total_step_count)) {
            return false;
        }
    }

    return true;
}
