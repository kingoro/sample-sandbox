/**
 * @file utility_event_contract.h
 * @brief Event IDとpayload契約を検証する読み取り専用Registry API。
 */
#ifndef UTILITY_EVENT_CONTRACT_H
#define UTILITY_EVENT_CONTRACT_H

#include "utility_event_result.h"
#include "utility_event_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Event payload所有方式の固定幅型。 */
typedef uint32_t ut_event_payload_ownership_t;

/** Event payloadの所有方式。 */
enum {
    /** payloadを持たない。 */
    UT_EVENT_PAYLOAD_NONE = 0,
    /** 呼出側が寿命を管理する非所有参照。 */
    UT_EVENT_PAYLOAD_BORROWED = 1,
    /** ut_event_buffer_ref_tを指すBuffer Pool参照。 */
    UT_EVENT_PAYLOAD_BUFFER_REF = 2u
};

/** 1種類のEventに対応するpayload契約。 */
typedef struct ut_event_contract {
    /** Application固有Event ID。UT_EVENT_ID_ANYは指定できない。 */
    uint32_t event_id;
    /** 診断表示用の静的文字列。不要ならNULL。 */
    const char *name;
    /** 許容するpayloadの最小byte数。 */
    size_t payload_min_size;
    /** 許容するpayloadの最大byte数。 */
    size_t payload_max_size;
    /** payloadの所有方式。 */
    ut_event_payload_ownership_t ownership;
} ut_event_contract_t;

/**
 * 呼出側所有の不変Contract tableを参照するRegistry。
 *
 * tableはRegistry利用終了まで有効に保つ。Registryはtableを変更せず、heapやlockを
 * 使用しない。同じRegistryの並行読み取りは可能だが、table自体を変更してはならない。
 */
typedef struct ut_event_contract_registry {
    /** 呼出側が提供するContract table。 */
    const ut_event_contract_t *contracts;
    /** table内のContract件数。 */
    size_t count;
} ut_event_contract_registry_t;

/**
 * Contract Registryを初期化し、ID重複とsize条件を検査する。
 *
 * UT_EVENT_PAYLOAD_NONEでは最小・最大sizeがともに0でなければならない。
 * UT_EVENT_PAYLOAD_BUFFER_REFでは最小・最大sizeがut_event_buffer_ref_tのsizeと
 * 一致しなければならない。
 *
 * @param registry 初期化するRegistry。
 * @param contracts 利用終了まで有効なContract table。
 * @param count table内のContract件数。
 * @return UT_EVENT_OK、UT_EVENT_ALREADY_EXISTS、UT_EVENT_INVALID_ARGUMENT。
 */
ut_event_result_t ut_event_contract_registry_init(
    ut_event_contract_registry_t *registry,
    const ut_event_contract_t *contracts,
    size_t count);

/**
 * Event IDに対応するContractを取得する。
 *
 * @param registry 初期化済みRegistry。
 * @param event_id 検索するEvent ID。
 * @param out_contract 見つかったContract pointerの格納先。
 * @return UT_EVENT_OK、UT_EVENT_NOT_FOUND、UT_EVENT_INVALID_ARGUMENT。
 */
ut_event_result_t ut_event_contract_find(
    const ut_event_contract_registry_t *registry,
    uint32_t event_id,
    const ut_event_contract_t **out_contract);

/**
 * EventのID、payload pointer、payload sizeが登録契約に一致するか検証する。
 *
 * @param registry 初期化済みRegistry。
 * @param event 検証するEvent。
 * @return UT_EVENT_OK、UT_EVENT_NOT_FOUND、UT_EVENT_INVALID_ARGUMENT。
 */
ut_event_result_t ut_event_contract_validate(
    const ut_event_contract_registry_t *registry,
    const ut_event_t *event);

#ifdef __cplusplus
}
#endif

#endif
