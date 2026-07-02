#ifndef DOMAIN_ACTION_H
#define DOMAIN_ACTION_H

#include <stdbool.h>
#include <stddef.h>

/**
 * @file domain_action.h
 * @brief Runnerが実行する抽象Actionを定義する。
 *
 * C言語にはクラスや仮想関数がないため、このサンプルでは関数ポインタを使って
 * オブジェクト指向のCommand Patternに近い形を表現します。
 *
 * 読み方:
 *
 * - Actionは「あとで実行できる命令の箱」です。
 * - Runnerは箱の中身がUnit AなのかUnit Bなのかを知りません。
 * - Runnerは `execute` に入っている関数を呼ぶだけです。
 *
 * こうすると、Runnerの処理をシンプルに保てます。
 */

/**
 * @brief Action実行関数の型。
 *
 * @param context Actionごとの実行情報。
 * @return 実行に成功した場合は `true`、失敗した場合は `false`。
 */
typedef bool (*domain_action_execute_fn)(void *context);

/**
 * @brief Runnerが扱う抽象Action。
 *
 * Runnerは `execute` と `context` だけを見て実行します。
 * そのActionがUnit Aのopenなのか、Unit Bのmoveなのかは知りません。
 */
typedef struct {
    /** ログやデバッグで表示するAction名。 */
    const char *name;
    /** このActionが属するScenario名。 */
    const char *scenario_name;
    /** このActionが属するSequence名。 */
    const char *sequence_name;
    /** このActionが由来するStep名。 */
    const char *step_name;
    /** 1始まりのStep番号。 */
    size_t step_index;
    /** Scenario全体で実行するStep数。 */
    size_t total_step_count;
    /** Actionを実行する関数。 */
    domain_action_execute_fn execute;
    /** 実行関数へ渡すAction固有の情報。 */
    void *context;
} domain_action_t;

/**
 * @brief Actionを実行する。
 *
 * @param action 実行するAction。
 * @return 実行に成功した場合は `true`、Actionが不正または実行失敗の場合は `false`。
 */
bool execute_domain_action(const domain_action_t *action);

#endif
