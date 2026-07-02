#include "domain_action.h"

#include <stdio.h>

/**
 * @file domain_action.c
 * @brief 抽象Actionの共通実行処理。
 *
 * Actionは、Runnerから見た「実行できるもの」です。
 * Unit操作かどうか、どのStep由来か、といった詳細はActionの中に隠します。
 */

/**
 * @copydoc execute_domain_action
 *
 * この関数はActionの入口です。
 * NULLチェックをしたあと、Actionに登録されたexecute関数を呼びます。
 */
bool execute_domain_action(const domain_action_t *action)
{
    if ((action == NULL) || (action->execute == NULL)) {
        printf("[domain] execute_domain_action(action=NULL or execute=NULL) -> false\n");
        return false;
    }

    printf("[domain] execute_domain_action(name=%s)\n",
           (action->name != NULL) ? action->name : "UNKNOWN");

    /*
     * ここがCommand Patternの実行点です。
     * 実際に何が起きるかは、Actionに入っているexecute関数が決めます。
     */
    return action->execute(action->context);
}
