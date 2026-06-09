/**
 * @file test_utility_event_contract.c
 * @brief Event Contract Registryの単体テスト。
 */
#include "utility_event_contract.h"

#include "utility_event_buffer.h"
#include "test_support.h"

/**
 * 登録、検索、正常Eventと違反Eventの検証結果を確認する。
 *
 * @return 成功時0、失敗時1。
 */
static int test_registry_and_validation(void)
{
    const uint32_t payload = 42u;
    const ut_event_contract_t contracts[] = {
        {1u, "empty", 0u, 0u, UT_EVENT_PAYLOAD_NONE},
        {
            2u,
            "value",
            sizeof(payload),
            sizeof(payload),
            UT_EVENT_PAYLOAD_BORROWED
        },
        {
            3u,
            "buffer",
            sizeof(ut_event_buffer_ref_t),
            sizeof(ut_event_buffer_ref_t),
            UT_EVENT_PAYLOAD_BUFFER_REF
        }
    };
    ut_event_contract_registry_t registry;
    const ut_event_contract_t *found = NULL;
    const ut_event_t empty_event = {1u, 0u, NULL, 0u};
    const ut_event_t value_event = {
        2u, 0u, &payload, sizeof(payload)
    };
    const ut_event_t unknown_event = {99u, 0u, NULL, 0u};
    const ut_event_t bad_pointer = {2u, 0u, NULL, sizeof(payload)};
    const ut_event_t bad_size = {2u, 0u, &payload, 1u};

    CHECK(ut_event_contract_registry_init(
        &registry, contracts, 3u) == UT_EVENT_OK);
    CHECK(ut_event_contract_find(&registry, 2u, &found) == UT_EVENT_OK);
    CHECK(found == &contracts[1]);
    CHECK(ut_event_contract_find(&registry, 9u, &found) ==
        UT_EVENT_NOT_FOUND);
    CHECK(ut_event_contract_validate(&registry, &empty_event) ==
        UT_EVENT_OK);
    CHECK(ut_event_contract_validate(&registry, &value_event) ==
        UT_EVENT_OK);
    CHECK(ut_event_contract_validate(&registry, &unknown_event) ==
        UT_EVENT_NOT_FOUND);
    CHECK(ut_event_contract_validate(&registry, &bad_pointer) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_contract_validate(&registry, &bad_size) ==
        UT_EVENT_INVALID_ARGUMENT);
    return 0;
}

/**
 * ID重複、予約ID、矛盾size、未定義所有方式を拒否することを確認する。
 *
 * @return 成功時0、失敗時1。
 */
static int test_invalid_registry_definitions(void)
{
    const ut_event_contract_t duplicate[] = {
        {1u, "one", 0u, 0u, UT_EVENT_PAYLOAD_NONE},
        {1u, "duplicate", 0u, 0u, UT_EVENT_PAYLOAD_NONE}
    };
    const ut_event_contract_t any_id[] = {
        {UT_EVENT_ID_ANY, "any", 0u, 0u, UT_EVENT_PAYLOAD_NONE}
    };
    const ut_event_contract_t bad_none[] = {
        {1u, "none", 0u, 1u, UT_EVENT_PAYLOAD_NONE}
    };
    const ut_event_contract_t bad_range[] = {
        {1u, "range", 2u, 1u, UT_EVENT_PAYLOAD_BORROWED}
    };
    const ut_event_contract_t bad_buffer[] = {
        {1u, "buffer", 1u, 1u, UT_EVENT_PAYLOAD_BUFFER_REF}
    };
    const ut_event_contract_t bad_ownership[] = {
        {1u, "bad", 0u, 0u, (ut_event_payload_ownership_t)99}
    };
    ut_event_contract_registry_t registry;

    CHECK(ut_event_contract_registry_init(&registry, duplicate, 2u) ==
        UT_EVENT_ALREADY_EXISTS);
    CHECK(ut_event_contract_registry_init(&registry, any_id, 1u) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_contract_registry_init(&registry, bad_none, 1u) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_contract_registry_init(&registry, bad_range, 1u) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_contract_registry_init(&registry, bad_buffer, 1u) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_contract_registry_init(&registry, bad_ownership, 1u) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_contract_registry_init(NULL, duplicate, 2u) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_contract_registry_init(&registry, NULL, 2u) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_contract_registry_init(&registry, duplicate, 0u) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_contract_find(NULL, 1u, NULL) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_contract_validate(NULL, NULL) ==
        UT_EVENT_INVALID_ARGUMENT);
    return 0;
}

int run_utility_event_contract_tests(void)
{
    CHECK(test_registry_and_validation() == 0);
    CHECK(test_invalid_registry_definitions() == 0);
    return 0;
}
