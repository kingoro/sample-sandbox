/**
 * @file utility_event_contract.c
 * @brief Event Contract Registryの実装。
 */
#include "utility_event_contract.h"

#include "utility_event_buffer.h"

/**
 * Registryの最低限の内部不変条件を検査する。
 *
 * @param registry 検査するRegistry。
 * @return 利用可能なら真、それ以外は偽。
 */
static int registry_is_valid(
    const ut_event_contract_registry_t *registry)
{
    return (registry != NULL) && (registry->contracts != NULL) &&
        (registry->count > 0u);
}

/**
 * payload所有方式が定義済み値か検査する。
 *
 * @param ownership 検査する所有方式。
 * @return 定義済みなら真、それ以外は偽。
 */
static int ownership_is_valid(ut_event_payload_ownership_t ownership)
{
    return (ownership == UT_EVENT_PAYLOAD_NONE) ||
        (ownership == UT_EVENT_PAYLOAD_BORROWED) ||
        (ownership == UT_EVENT_PAYLOAD_BUFFER_REF);
}

/**
 * 1件のContract定義が自己矛盾していないか検査する。
 *
 * @param contract 検査するContract。
 * @return 有効なら真、それ以外は偽。
 */
static int contract_is_valid(const ut_event_contract_t *contract)
{
    if ((contract == NULL) || (contract->event_id == UT_EVENT_ID_ANY) ||
        !ownership_is_valid(contract->ownership) ||
        (contract->payload_min_size > contract->payload_max_size)) {
        return 0;
    }
    if (contract->ownership == UT_EVENT_PAYLOAD_NONE) {
        return (contract->payload_min_size == 0u) &&
            (contract->payload_max_size == 0u);
    }
    if (contract->ownership == UT_EVENT_PAYLOAD_BUFFER_REF) {
        return (contract->payload_min_size == sizeof(ut_event_buffer_ref_t)) &&
            (contract->payload_max_size == sizeof(ut_event_buffer_ref_t));
    }
    return 1;
}

ut_event_result_t ut_event_contract_registry_init(
    ut_event_contract_registry_t *registry,
    const ut_event_contract_t *contracts,
    size_t count)
{
    size_t index;
    size_t other;

    if ((registry == NULL) || (contracts == NULL) || (count == 0u) ||
        (count > (SIZE_MAX / sizeof(contracts[0])))) {
        return UT_EVENT_INVALID_ARGUMENT;
    }

    for (index = 0u; index < count; index++) {
        if (!contract_is_valid(&contracts[index])) {
            return UT_EVENT_INVALID_ARGUMENT;
        }
        for (other = index + 1u; other < count; other++) {
            if (contracts[index].event_id == contracts[other].event_id) {
                return UT_EVENT_ALREADY_EXISTS;
            }
        }
    }

    registry->contracts = contracts;
    registry->count = count;
    return UT_EVENT_OK;
}

ut_event_result_t ut_event_contract_find(
    const ut_event_contract_registry_t *registry,
    uint32_t event_id,
    const ut_event_contract_t **out_contract)
{
    size_t index;

    if (!registry_is_valid(registry) || (out_contract == NULL)) {
        return UT_EVENT_INVALID_ARGUMENT;
    }

    for (index = 0u; index < registry->count; index++) {
        if (registry->contracts[index].event_id == event_id) {
            *out_contract = &registry->contracts[index];
            return UT_EVENT_OK;
        }
    }
    return UT_EVENT_NOT_FOUND;
}

ut_event_result_t ut_event_contract_validate(
    const ut_event_contract_registry_t *registry,
    const ut_event_t *event)
{
    const ut_event_contract_t *contract;
    ut_event_result_t result;

    if (event == NULL) {
        return UT_EVENT_INVALID_ARGUMENT;
    }

    result = ut_event_contract_find(registry, event->id, &contract);
    if (result != UT_EVENT_OK) {
        return result;
    }
    if (((event->payload == NULL) && (event->payload_size != 0u)) ||
        ((event->payload != NULL) && (event->payload_size == 0u)) ||
        (event->payload_size < contract->payload_min_size) ||
        (event->payload_size > contract->payload_max_size)) {
        return UT_EVENT_INVALID_ARGUMENT;
    }
    return UT_EVENT_OK;
}
