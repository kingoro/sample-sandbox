/**
 * @file domain_workflow.h
 * @brief Domain Serviceが実行するScenario、Sequence、Stepの宣言モデル。
 *
 * 本headerの型は実行状態を持たない。実行順序とUnitへの命令だけを表現し、
 * Runnerが同じ定義を繰り返し利用できる。
 */
#ifndef DOMAIN_WORKFLOW_H
#define DOMAIN_WORKFLOW_H

#include "unit_mock.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 1つのUnit命令を表すStep定義。
 */
typedef struct {
    /** Domain Serviceへ登録されたUnitの0始まりindex。 */
    size_t unit_index;
    /** UnitのInputへ書き込む命令。 */
    unit_mock_command_t command;
    /** Step完了を待つ最大時間。単位はミリ秒。 */
    uint64_t timeout_ms;
} domain_step_t;

/**
 * 順番に実行するStep群を表すSequence定義。
 */
typedef struct {
    /** 診断用Sequence名。定義の利用中は有効に保つ。 */
    const char *name;
    /** 実行順に並べたStep配列。 */
    const domain_step_t *steps;
    /** stepsの要素数。 */
    size_t step_count;
} domain_sequence_t;

/**
 * 順番に実行するSequence群を表すScenario定義。
 */
typedef struct {
    /** Scenarioを識別するID。 */
    uint32_t id;
    /** 診断用Scenario名。定義の利用中は有効に保つ。 */
    const char *name;
    /** 実行順に並べたSequence配列。 */
    const domain_sequence_t *sequences;
    /** sequencesの要素数。 */
    size_t sequence_count;
    /** Scenario全体に含まれるStep数。 */
    size_t total_step_count;
} domain_scenario_t;

/**
 * A機能の実行条件に対応するScenarioを取得する。
 *
 * @param condition Controlから渡された実行条件。1または2を指定する。
 * @return 静的寿命を持つScenario。未対応条件ではNULL。
 */
const domain_scenario_t *domain_workflow_feature_a(uint32_t condition);

#ifdef __cplusplus
}
#endif

#endif
