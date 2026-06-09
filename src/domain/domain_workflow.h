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
 * 外部定義1件と検索条件を対応付けるWorkflow entry。
 */
typedef struct {
    /** Domain Serviceの機能ID。 */
    uint32_t feature;
    /** InputのScenario選択条件。 */
    uint32_t condition;
    /** 検証済みScenario定義。 */
    domain_scenario_t scenario;
} domain_workflow_entry_t;

/**
 * 外部ファイルから読み込んだWorkflow定義集合。
 *
 * 内部配列を所有する。fieldは参照専用であり、呼出側が変更してはならない。
 */
typedef struct {
    /** 検証済みWorkflow entry配列。 */
    domain_workflow_entry_t *entries;
    /** entriesの要素数。 */
    size_t entry_count;
} domain_workflow_catalog_t;

/**
 * Workflow定義読込み結果。
 */
typedef enum {
    /** 読込みと検証に成功した。 */
    DOMAIN_WORKFLOW_LOAD_OK = 0,
    /** 引数が無効だった。 */
    DOMAIN_WORKFLOW_LOAD_INVALID_ARGUMENT,
    /** ファイルを開けない、または読み取れなかった。 */
    DOMAIN_WORKFLOW_LOAD_IO_ERROR,
    /** JSON構文が無効だった。 */
    DOMAIN_WORKFLOW_LOAD_PARSE_ERROR,
    /** JSONは読めたがWorkflow schemaまたは値が無効だった。 */
    DOMAIN_WORKFLOW_LOAD_SCHEMA_ERROR,
    /** 必要な領域を確保できなかった。 */
    DOMAIN_WORKFLOW_LOAD_NO_MEMORY,
    /** Serviceが実行中のため定義を差し替えられなかった。 */
    DOMAIN_WORKFLOW_LOAD_BUSY
} domain_workflow_load_result_t;

/**
 * A機能の実行条件に対応するScenarioを取得する。
 *
 * @param condition Controlから渡された実行条件。1または2を指定する。
 * @return 静的寿命を持つScenario。未対応条件ではNULL。
 */
const domain_scenario_t *domain_workflow_feature_a(uint32_t condition);

/**
 * @fn domain_workflow_load_result_t domain_workflow_catalog_load_json_file(const char *path, size_t unit_count, domain_workflow_catalog_t **out_catalog);
 * JSONファイルからWorkflow Catalogを生成する。
 *
 * 対応schemaはsrc/domain/examples/workflows.jsonを参照する。読込み時にfeatureと
 * conditionの重複、Scenario ID、Sequence、Step、Unit番号、command、timeoutを
 * 検証する。
 *
 * @param path 読み込むJSONファイルpath。
 * @param unit_count 利用可能なUnit数。Stepのunit番号上限検証に使用する。
 * @param out_catalog 生成したCatalogの格納先。成功時に所有権が呼出側へ移る。
 * @return 読込み結果。
 */
domain_workflow_load_result_t domain_workflow_catalog_load_json_file(const char *path, size_t unit_count, domain_workflow_catalog_t **out_catalog);

/**
 * Workflow Catalogを破棄する。
 *
 * @param catalog load関数が生成したCatalog。NULLも許容する。
 *
 * Catalogから取得したScenario pointerの寿命も終了する。
 */
void domain_workflow_catalog_destroy(domain_workflow_catalog_t *catalog);

/**
 * featureとconditionに一致するScenarioを検索する。
 *
 * @param catalog 検索対象Catalog。
 * @param feature 機能ID。
 * @param condition Scenario選択条件。
 * @return Catalog所有のScenario。未登録または引数不正時はNULL。
 *
 * 戻り値はCatalog破棄まで有効であり、呼出側は変更・解放してはならない。
 */
const domain_scenario_t *domain_workflow_catalog_find(const domain_workflow_catalog_t *catalog, uint32_t feature, uint32_t condition);

#ifdef __cplusplus
}
#endif

#endif
