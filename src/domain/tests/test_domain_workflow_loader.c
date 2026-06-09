/**
 * @file test_domain_workflow_loader.c
 * @brief 動的Workflow JSON loaderとService reload境界の単体テスト。
 */
#include "domain_service.h"
#include "domain_workflow.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/** Test fixture directory。 */
#ifndef DOMAIN_TEST_FIXTURE_DIR
#define DOMAIN_TEST_FIXTURE_DIR "src/domain/tests/fixtures"
#endif

/** Testで生成するMock Unit数。 */
#define DOMAIN_TEST_UNIT_COUNT 10U

/** 条件が偽ならtestを失敗させる。 */
#define CHECK(condition)                                                      \
    do {                                                                      \
        if (!(condition)) {                                                   \
            return 1;                                                         \
        }                                                                     \
    } while (0)

/**
 * fixture filenameからpathを作る。
 *
 * @param filename fixture filename。
 * @param output path格納先。
 * @param capacity output容量。
 * @return pathを格納できた場合true。
 */
static bool fixture_path(
    const char *filename,
    char *output,
    size_t capacity)
{
    const char *prefix = DOMAIN_TEST_FIXTURE_DIR "/";
    const size_t required =
        strlen(prefix) + strlen(filename) + 1U;

    if (required > capacity) {
        return false;
    }
    (void)strcpy(output, prefix);
    (void)strcat(output, filename);
    return true;
}

/**
 * valid JSONをC modelへ変換できることを確認する。
 *
 * @return 成功時0。
 */
static int test_load_and_find(void)
{
    char path[256];
    domain_workflow_catalog_t *catalog = NULL;

    CHECK(fixture_path("valid_workflows.json", path, sizeof(path)));
    CHECK(domain_workflow_catalog_load_json_file(
        path, DOMAIN_TEST_UNIT_COUNT, &catalog) == DOMAIN_WORKFLOW_LOAD_OK);
    CHECK(catalog != NULL);
    CHECK(catalog->entry_count == 1U);

    const domain_scenario_t *scenario =
        domain_workflow_catalog_find(catalog, 1U, 7U);

    CHECK(scenario != NULL);
    CHECK(scenario->id == 7001U);
    CHECK(strcmp(scenario->name, "loader-test") == 0);
    CHECK(scenario->sequence_count == 2U);
    CHECK(scenario->total_step_count == 3U);
    CHECK(strcmp(scenario->sequences[0].name, "prepare") == 0);
    CHECK(scenario->sequences[0].steps[0].unit_index == 1U);
    CHECK(scenario->sequences[1].steps[1].unit_index == 5U);
    CHECK(scenario->sequences[1].steps[1].timeout_ms == 300U);
    CHECK(domain_workflow_catalog_find(catalog, 1U, 8U) == NULL);

    domain_workflow_catalog_destroy(catalog);
    return 0;
}

/**
 * JSON構文とWorkflow schema違反を拒否することを確認する。
 *
 * @return 成功時0。
 */
static int test_invalid_definitions(void)
{
    char path[256];
    domain_workflow_catalog_t *catalog = NULL;

    CHECK(fixture_path("invalid_json.json", path, sizeof(path)));
    CHECK(domain_workflow_catalog_load_json_file(
        path, DOMAIN_TEST_UNIT_COUNT, &catalog)
        == DOMAIN_WORKFLOW_LOAD_PARSE_ERROR);
    CHECK(catalog == NULL);

    CHECK(fixture_path("invalid_unit.json", path, sizeof(path)));
    CHECK(domain_workflow_catalog_load_json_file(
        path, DOMAIN_TEST_UNIT_COUNT, &catalog)
        == DOMAIN_WORKFLOW_LOAD_SCHEMA_ERROR);
    CHECK(catalog == NULL);

    CHECK(fixture_path("duplicate_workflow.json", path, sizeof(path)));
    CHECK(domain_workflow_catalog_load_json_file(
        path, DOMAIN_TEST_UNIT_COUNT, &catalog)
        == DOMAIN_WORKFLOW_LOAD_SCHEMA_ERROR);
    CHECK(catalog == NULL);
    return 0;
}

/**
 * 不正引数と存在しないファイルを拒否することを確認する。
 *
 * @return 成功時0。
 */
static int test_load_errors(void)
{
    domain_workflow_catalog_t *catalog = NULL;

    CHECK(domain_workflow_catalog_load_json_file(
        NULL, DOMAIN_TEST_UNIT_COUNT, &catalog)
        == DOMAIN_WORKFLOW_LOAD_INVALID_ARGUMENT);
    CHECK(domain_workflow_catalog_load_json_file(
        "", DOMAIN_TEST_UNIT_COUNT, &catalog)
        == DOMAIN_WORKFLOW_LOAD_INVALID_ARGUMENT);
    CHECK(domain_workflow_catalog_load_json_file(
        "not-found.json", DOMAIN_TEST_UNIT_COUNT, &catalog)
        == DOMAIN_WORKFLOW_LOAD_IO_ERROR);
    CHECK(domain_workflow_catalog_find(NULL, 1U, 1U) == NULL);
    domain_workflow_catalog_destroy(NULL);
    return 0;
}

/**
 * Service reloadが失敗時に旧Catalogを維持し、実行中は拒否することを確認する。
 *
 * @return 成功時0。
 */
static int test_service_reload_boundary(void)
{
    char valid_path[256];
    char invalid_path[256];
    unit_mock_t *units[DOMAIN_TEST_UNIT_COUNT] = {NULL};
    bool units_created = true;

    for (uint8_t index = 0U;
         index < DOMAIN_TEST_UNIT_COUNT && units_created;
         ++index) {
        units[index] = unit_mock_create((uint8_t)(index + 1U));
        units_created = units[index] != NULL;
    }
    CHECK(units_created);
    domain_service_t *service =
        domain_service_create(units, DOMAIN_TEST_UNIT_COUNT);

    CHECK(service != NULL);
    CHECK(fixture_path(
        "valid_workflows.json", valid_path, sizeof(valid_path)));
    CHECK(fixture_path(
        "invalid_unit.json", invalid_path, sizeof(invalid_path)));
    CHECK(domain_service_load_workflows_json(service, valid_path)
        == DOMAIN_WORKFLOW_LOAD_OK);
    CHECK(domain_service_load_workflows_json(service, invalid_path)
        == DOMAIN_WORKFLOW_LOAD_SCHEMA_ERROR);

    const domain_service_input_t input = {
        .feature = DOMAIN_FEATURE_A,
        .condition = 7U,
        .request_id = 99U,
    };

    CHECK(domain_service_write_input(service, &input)
        == DOMAIN_INPUT_ACCEPTED);
    CHECK(domain_service_load_workflows_json(service, valid_path)
        == DOMAIN_WORKFLOW_LOAD_BUSY);

    domain_service_destroy(service);
    for (size_t index = 0U; index < DOMAIN_TEST_UNIT_COUNT; ++index) {
        unit_mock_destroy(units[index]);
    }
    return 0;
}

/**
 * Domain Workflow loader test入口。
 *
 * @return 全test成功時0。
 */
int main(void)
{
    CHECK(test_load_and_find() == 0);
    CHECK(test_invalid_definitions() == 0);
    CHECK(test_load_errors() == 0);
    CHECK(test_service_reload_boundary() == 0);
    return 0;
}
