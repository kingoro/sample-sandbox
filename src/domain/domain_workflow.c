/**
 * @file domain_workflow.c
 * @brief A機能で選択可能なサンプルWorkflow定義。
 */
#include "domain_workflow.h"

/** 各サンプルStepのtimeout。 */
#define DOMAIN_SAMPLE_STEP_TIMEOUT_MS 2000U

/** 標準Scenarioの前半Sequence。 */
static const domain_step_t feature_a_standard_first_steps[] = {
    {0U, UNIT_MOCK_COMMAND_EXECUTE, DOMAIN_SAMPLE_STEP_TIMEOUT_MS},
    {1U, UNIT_MOCK_COMMAND_EXECUTE, DOMAIN_SAMPLE_STEP_TIMEOUT_MS},
    {2U, UNIT_MOCK_COMMAND_EXECUTE, DOMAIN_SAMPLE_STEP_TIMEOUT_MS},
    {3U, UNIT_MOCK_COMMAND_EXECUTE, DOMAIN_SAMPLE_STEP_TIMEOUT_MS},
    {4U, UNIT_MOCK_COMMAND_EXECUTE, DOMAIN_SAMPLE_STEP_TIMEOUT_MS},
};

/** 標準Scenarioの後半Sequence。 */
static const domain_step_t feature_a_standard_second_steps[] = {
    {5U, UNIT_MOCK_COMMAND_EXECUTE, DOMAIN_SAMPLE_STEP_TIMEOUT_MS},
    {6U, UNIT_MOCK_COMMAND_EXECUTE, DOMAIN_SAMPLE_STEP_TIMEOUT_MS},
    {7U, UNIT_MOCK_COMMAND_EXECUTE, DOMAIN_SAMPLE_STEP_TIMEOUT_MS},
    {8U, UNIT_MOCK_COMMAND_EXECUTE, DOMAIN_SAMPLE_STEP_TIMEOUT_MS},
    {9U, UNIT_MOCK_COMMAND_EXECUTE, DOMAIN_SAMPLE_STEP_TIMEOUT_MS},
};

/** 標準Scenarioを構成するSequence群。 */
static const domain_sequence_t feature_a_standard_sequences[] = {
    {
        "standard-first",
        feature_a_standard_first_steps,
        sizeof(feature_a_standard_first_steps)
            / sizeof(feature_a_standard_first_steps[0]),
    },
    {
        "standard-second",
        feature_a_standard_second_steps,
        sizeof(feature_a_standard_second_steps)
            / sizeof(feature_a_standard_second_steps[0]),
    },
};

/** 条件2で選択する逆順Scenarioの前半Sequence。 */
static const domain_step_t feature_a_reverse_first_steps[] = {
    {9U, UNIT_MOCK_COMMAND_EXECUTE, DOMAIN_SAMPLE_STEP_TIMEOUT_MS},
    {8U, UNIT_MOCK_COMMAND_EXECUTE, DOMAIN_SAMPLE_STEP_TIMEOUT_MS},
    {7U, UNIT_MOCK_COMMAND_EXECUTE, DOMAIN_SAMPLE_STEP_TIMEOUT_MS},
    {6U, UNIT_MOCK_COMMAND_EXECUTE, DOMAIN_SAMPLE_STEP_TIMEOUT_MS},
    {5U, UNIT_MOCK_COMMAND_EXECUTE, DOMAIN_SAMPLE_STEP_TIMEOUT_MS},
};

/** 条件2で選択する逆順Scenarioの後半Sequence。 */
static const domain_step_t feature_a_reverse_second_steps[] = {
    {4U, UNIT_MOCK_COMMAND_EXECUTE, DOMAIN_SAMPLE_STEP_TIMEOUT_MS},
    {3U, UNIT_MOCK_COMMAND_EXECUTE, DOMAIN_SAMPLE_STEP_TIMEOUT_MS},
    {2U, UNIT_MOCK_COMMAND_EXECUTE, DOMAIN_SAMPLE_STEP_TIMEOUT_MS},
    {1U, UNIT_MOCK_COMMAND_EXECUTE, DOMAIN_SAMPLE_STEP_TIMEOUT_MS},
    {0U, UNIT_MOCK_COMMAND_EXECUTE, DOMAIN_SAMPLE_STEP_TIMEOUT_MS},
};

/** 逆順Scenarioを構成するSequence群。 */
static const domain_sequence_t feature_a_reverse_sequences[] = {
    {
        "reverse-first",
        feature_a_reverse_first_steps,
        sizeof(feature_a_reverse_first_steps)
            / sizeof(feature_a_reverse_first_steps[0]),
    },
    {
        "reverse-second",
        feature_a_reverse_second_steps,
        sizeof(feature_a_reverse_second_steps)
            / sizeof(feature_a_reverse_second_steps[0]),
    },
};

/** 条件1で選択するA機能Scenario。 */
static const domain_scenario_t feature_a_standard = {
    1U,
    "feature-a-standard",
    feature_a_standard_sequences,
    sizeof(feature_a_standard_sequences)
        / sizeof(feature_a_standard_sequences[0]),
    10U,
};

/** 条件2で選択するA機能Scenario。 */
static const domain_scenario_t feature_a_reverse = {
    2U,
    "feature-a-reverse",
    feature_a_reverse_sequences,
    sizeof(feature_a_reverse_sequences)
        / sizeof(feature_a_reverse_sequences[0]),
    10U,
};

const domain_scenario_t *domain_workflow_feature_a(uint32_t condition)
{
    const domain_scenario_t *scenario = NULL;

    if (condition == 1U) {
        scenario = &feature_a_standard;
    } else if (condition == 2U) {
        scenario = &feature_a_reverse;
    }
    return scenario;
}
