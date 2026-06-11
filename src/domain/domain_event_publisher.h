/**
 * @file domain_event_publisher.h
 * @brief Domain階層間とControlへのEvent配送を統一するPublisher。
 *
 * PublisherはEvent payloadを固定長Queue内で値所有し、Unit workerからの発行と
 * Control thread上の配送を直列化する。完了条件の判断は各Runnerが担当する。
 */
#ifndef DOMAIN_EVENT_PUBLISHER_H
#define DOMAIN_EVENT_PUBLISHER_H

#include "unit_mock.h"
#include "utility_event.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Unit正常完了Event。 */
#define DOMAIN_EVENT_UNIT_COMPLETED 2001U
/** UnitエラーEvent。 */
#define DOMAIN_EVENT_UNIT_ERROR 2002U
/** Step timeout Event。 */
#define DOMAIN_EVENT_STEP_TIMEOUT 2003U
/** Sequence正常完了Event。 */
#define DOMAIN_EVENT_SEQUENCE_COMPLETED 2101U
/** SequenceエラーEvent。 */
#define DOMAIN_EVENT_SEQUENCE_ERROR 2102U
/** Scenario正常完了Event。 */
#define DOMAIN_EVENT_SCENARIO_COMPLETED 2201U
/** ScenarioエラーEvent。 */
#define DOMAIN_EVENT_SCENARIO_ERROR 2202U
/** Feature正常完了Event。 */
#define DOMAIN_EVENT_FEATURE_COMPLETED 2301U
/** FeatureエラーEvent。 */
#define DOMAIN_EVENT_FEATURE_ERROR 2302U

/**
 * Domain Event間で受け渡す値payload。
 */
typedef struct {
    /** Controlが発行した機能要求ID。 */
    uint32_t feature_request_id;
    /** Unitへ発行したStep要求ID。 */
    uint32_t unit_request_id;
    /** Scenario ID。 */
    uint32_t scenario_id;
    /** 0始まりのSequence index。 */
    size_t sequence_index;
    /** 0始まりのStep index。 */
    size_t step_index;
    /** 1始まりのUnit番号。 */
    uint8_t unit_number;
    /** Domain Serviceが定義するエラー値。 */
    uint32_t error;
} domain_event_payload_t;

/**
 * Domain Event Publisherの内部状態を隠蔽する型。
 */
typedef struct domain_event_publisher domain_event_publisher_t;

/**
 * 固定長QueueとDispatcherを持つPublisherを生成する。
 *
 * @return 生成したPublisher。生成失敗時はNULL。
 *
 * 戻り値の所有権は呼び出し元へ移る。複数threadからpublishできるが、
 * dispatchは単一threadから呼ぶ。
 */
domain_event_publisher_t *domain_event_publisher_create(void);

/**
 * Publisherを破棄する。
 *
 * @param publisher 破棄するPublisher。NULLも許容する。
 *
 * dispatch中またはpublish中に呼び出してはならない。
 */
void domain_event_publisher_destroy(domain_event_publisher_t *publisher);

/**
 * Event購読handlerを登録する。
 *
 * @param publisher 登録先Publisher。
 * @param event_id 購読するDomain Event ID。
 * @param handler Event受信handler。
 * @param context handlerへ渡すcontext。
 * @return Event Foundationのresult code。
 */
ut_event_result_t domain_event_publisher_subscribe(domain_event_publisher_t *publisher, uint32_t event_id, ut_event_handler_t handler, void *context);

/**
 * Domain EventをQueueへ発行する。
 *
 * @param publisher 発行先Publisher。
 * @param event_id 発行するDomain Event ID。
 * @param source Event発行元ID。
 * @param payload Queue内へ値copyするpayload。
 * @return Event Foundationのresult code。
 *
 * payloadの所有権は移動せず、関数から戻った後に呼び出し元で破棄できる。
 */
ut_event_result_t domain_event_publisher_publish(domain_event_publisher_t *publisher, uint32_t event_id, uint32_t source, const domain_event_payload_t *payload);

/**
 * Unit結果通知callbackとしてDomain Eventを発行する。
 *
 * @param result Unit workerから通知された処理結果。
 * @param context domain_event_publisher_tへのポインタ。
 *
 * 成功結果はDOMAIN_EVENT_UNIT_COMPLETED、エラー結果は
 * DOMAIN_EVENT_UNIT_ERRORへ変換する。
 */
void domain_event_publisher_on_unit_result(const unit_mock_result_t *result, void *context);

/**
 * Queue内Eventを最大budget件配送する。
 *
 * @param publisher 配送するPublisher。
 * @param budget 1回で配送する最大件数。0は指定できない。
 * @param out_dispatched 配送したEvent件数の任意格納先。
 * @return UT_EVENT_OK、UT_EVENT_BUSY、UT_EVENT_INVALID_ARGUMENT、
 * または最初のDispatcher error。
 *
 * handlerが新しいEventをpublishした場合、budget内なら同じ呼び出しで続けて配送する。
 */
ut_event_result_t domain_event_publisher_dispatch(domain_event_publisher_t *publisher, size_t budget, size_t *out_dispatched);

#ifdef __cplusplus
}
#endif

#endif
