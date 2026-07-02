#ifndef DOMAIN_ACTION_QUEUE_H
#define DOMAIN_ACTION_QUEUE_H

#include "domain_action.h"

#include <stdbool.h>
#include <stddef.h>

/**
 * @file domain_action_queue.h
 * @brief Runnerへ渡すAction Queueを定義する。
 *
 * Queueは、Actionを実行順に並べるための入れ物です。
 * Function Managerがprepare時にActionを積み、Runnerがstart後に先頭から順番に取り出します。
 */

/**
 * @brief このサンプルでQueueへ積めるAction数。
 */
#define DOMAIN_ACTION_QUEUE_CAPACITY 64U

/**
 * @brief 固定長リングバッファのAction Queue。
 *
 * サンプルを読みやすくするため、動的メモリ確保は使いません。
 * 実プロジェクトで可変長にしたい場合も、最初はこの固定長実装を理解してから拡張します。
 */
typedef struct {
    domain_action_t actions[DOMAIN_ACTION_QUEUE_CAPACITY];
    size_t head;
    size_t tail;
    size_t count;
} domain_action_queue_t;

/**
 * @brief Action Queueを初期化する。
 *
 * @param queue 初期化するQueue。
 * @return 初期化できた場合は `true`、引数が不正な場合は `false`。
 */
bool initialize_domain_action_queue(domain_action_queue_t *queue);

/**
 * @brief Action Queueを空にする。
 *
 * @param queue 対象Queue。
 * @return 成功した場合は `true`、引数が不正な場合は `false`。
 */
bool clear_domain_action_queue(domain_action_queue_t *queue);

/**
 * @brief ActionをQueueへ追加する。
 *
 * @param queue 追加先Queue。
 * @param action 追加するAction。
 * @return 追加できた場合は `true`、Queue満杯または引数不正の場合は `false`。
 */
bool push_domain_action_queue(domain_action_queue_t *queue, const domain_action_t *action);

/**
 * @brief QueueからActionを取り出す。
 *
 * @param queue 取り出し元Queue。
 * @param action 取り出したActionを書き込む出力先。
 * @return 取り出せた場合は `true`、Queue空または引数不正の場合は `false`。
 */
bool pop_domain_action_queue(domain_action_queue_t *queue, domain_action_t *action);

/**
 * @brief Queueに積まれているAction数を取得する。
 *
 * @param queue 対象Queue。
 * @return Queue内のAction数。引数が不正な場合は0。
 */
size_t get_domain_action_queue_count(const domain_action_queue_t *queue);

#endif
