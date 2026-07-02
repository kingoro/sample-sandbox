#ifndef DOMAIN_STEP_H
#define DOMAIN_STEP_H

#include "domain_action_queue.h"

#include <stdbool.h>
#include <stddef.h>

/**
 * @file domain_step.h
 * @brief Scenario階層の最小指示命令であるStepを定義する。
 *
 * 読み方:
 *
 * - `Scenario` は大きな作業全体です。
 * - `Sequence` はScenarioの中にある手順のまとまりです。
 * - `Step` はUnitへ渡す最小の命令です。
 *
 * このファイルでは、その一番小さい命令であるStepだけを扱います。
 * StepはUnitの関数を直接呼びません。
 * StepをAction Queueへ積み、Runnerが後で順番に実行できる形へ変換します。
 */

/**
 * @brief Stepが操作するUnit。
 *
 * ここではUnit AからUnit Eまでを列挙しています。
 * 実プロジェクトでは、センサ、モーター、バルブなどの部品に対応する想定です。
 */
typedef enum {
    /** Unit Aを操作するStep。 */
    DOMAIN_STEP_UNIT_A = 0,
    /** Unit Bを操作するStep。 */
    DOMAIN_STEP_UNIT_B,
    /** Unit Cを操作するStep。 */
    DOMAIN_STEP_UNIT_C,
    /** Unit Dを操作するStep。 */
    DOMAIN_STEP_UNIT_D,
    /** Unit Eを操作するStep。 */
    DOMAIN_STEP_UNIT_E
} domain_step_unit_t;

/**
 * @brief StepがUnitへ依頼する操作。
 *
 * ここで定義している操作は、Unit層の公開APIと対応します。
 * たとえば `DOMAIN_STEP_OPERATION_OPEN` は、最終的に `open_unit_a()` のような
 * Unit関数呼び出しへ変換されます。
 */
typedef enum {
    /** Unitを初期化する操作。 */
    DOMAIN_STEP_OPERATION_INITIALIZE = 0,
    /** Unitを終了する操作。 */
    DOMAIN_STEP_OPERATION_TERMINATE,
    /** Unitの状態を取得する操作。 */
    DOMAIN_STEP_OPERATION_GET_STATUS,
    /** Unitを移動させる操作。 */
    DOMAIN_STEP_OPERATION_MOVE,
    /** Unitを開く操作。 */
    DOMAIN_STEP_OPERATION_OPEN,
    /** Unitを閉じる操作。 */
    DOMAIN_STEP_OPERATION_CLOSE
} domain_step_operation_t;

/**
 * @brief 最小の指示命令。
 *
 * Stepは「どのUnitへ、どの操作を、どの値で依頼するか」を表します。
 */
typedef struct {
    /** ログに表示するStep名。実行順序を追いやすくするために使います。 */
    const char *name;
    /** 操作対象のUnit。 */
    domain_step_unit_t unit;
    /** Unitへ依頼する操作。 */
    domain_step_operation_t operation;
    /** moveなど、追加の整数値が必要な操作で使う値。 */
    int value;
} domain_step_t;

/**
 * @brief Stepを抽象Actionへ変換してQueueへ追加する。
 *
 * @param queue 追加先Queue。
 * @param step Action化するStep。
 * @return 追加できた場合は `true`、失敗した場合は `false`。
 */
bool enqueue_domain_step_action(
    domain_action_queue_t *queue,
    const domain_step_t *step,
    const char *scenario_name,
    const char *sequence_name,
    size_t step_index,
    size_t total_step_count);

#endif
