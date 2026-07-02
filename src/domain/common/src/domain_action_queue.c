#include "domain_action_queue.h"

#include <stdio.h>

/**
 * @file domain_action_queue.c
 * @brief 固定長Action Queueの実装。
 *
 * QueueはActionの実行順を保持します。
 * pushした順にpopされるため、Scenario/Sequence/Stepで決めた順番をRunnerへ渡せます。
 *
 * このサンプルでは、malloc/freeを使わず固定長配列で実装しています。
 * メモリ管理の話に寄せすぎず、Queueの役割に集中できるようにするためです。
 */

/**
 * @copydoc initialize_domain_action_queue
 */
bool initialize_domain_action_queue(domain_action_queue_t *queue)
{
    if (queue == NULL) {
        return false;
    }

    queue->head = 0U;
    queue->tail = 0U;
    queue->count = 0U;
    return true;
}

/**
 * @copydoc clear_domain_action_queue
 *
 * 実装は初期化と同じです。
 * head、tail、countを0に戻すことでQueueを空にします。
 */
bool clear_domain_action_queue(domain_action_queue_t *queue)
{
    return initialize_domain_action_queue(queue);
}

/**
 * @copydoc push_domain_action_queue
 */
bool push_domain_action_queue(domain_action_queue_t *queue, const domain_action_t *action)
{
    if ((queue == NULL) || (action == NULL) ||
        (queue->count >= DOMAIN_ACTION_QUEUE_CAPACITY)) {
        printf("[domain] push_domain_action_queue() -> false\n");
        return false;
    }

    /*
     * tail位置へActionをコピーし、tailを次へ進めます。
     * 固定長配列をリングバッファとして使うため、末尾まで行ったら0へ戻します。
     */
    queue->actions[queue->tail] = *action;
    queue->tail = (queue->tail + 1U) % DOMAIN_ACTION_QUEUE_CAPACITY;
    queue->count++;

    printf("[domain] push_domain_action_queue(name=%s, count=%zu) -> true\n",
           (action->name != NULL) ? action->name : "UNKNOWN",
           queue->count);
    return true;
}

/**
 * @copydoc pop_domain_action_queue
 */
bool pop_domain_action_queue(domain_action_queue_t *queue, domain_action_t *action)
{
    if ((queue == NULL) || (action == NULL) || (queue->count == 0U)) {
        return false;
    }

    /*
     * head位置のActionを取り出し、headを次へ進めます。
     * pushと同じく、末尾まで行ったら0へ戻ります。
     */
    *action = queue->actions[queue->head];
    queue->head = (queue->head + 1U) % DOMAIN_ACTION_QUEUE_CAPACITY;
    queue->count--;
    return true;
}

/**
 * @copydoc get_domain_action_queue_count
 */
size_t get_domain_action_queue_count(const domain_action_queue_t *queue)
{
    if (queue == NULL) {
        return 0U;
    }

    return queue->count;
}
