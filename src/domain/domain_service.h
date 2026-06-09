/**
 * @file domain_service.h
 * @brief 機能単位の非同期Workflowを管理するDomain Service。
 *
 * ControlはInputで機能と条件を指定し、周期的にprocessを呼び、Outputだけを監視する。
 * Scenario、Sequence、Stepの進行判断はDomain Service内部で完結する。
 */
#ifndef DOMAIN_SERVICE_H
#define DOMAIN_SERVICE_H

#include "unit_mock.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Domain Serviceが提供する機能。
 */
typedef enum {
    /** 機能未指定。 */
    DOMAIN_FEATURE_NONE = 0,
    /** 複雑なWorkflowを持つA機能。 */
    DOMAIN_FEATURE_A,
    /** 将来追加する複雑なB機能。 */
    DOMAIN_FEATURE_B,
    /** セルフチェックを行うC機能。 */
    DOMAIN_FEATURE_C,
    /** 設定を行うD機能。 */
    DOMAIN_FEATURE_D,
    /** 単一操作を行うE機能。 */
    DOMAIN_FEATURE_E
} domain_feature_t;

/**
 * Domain Serviceの機能単位状態。
 */
typedef enum {
    /** 実行要求を待つ状態。 */
    DOMAIN_STATUS_IDLE = 0,
    /** Scenarioを実行している状態。 */
    DOMAIN_STATUS_RUNNING,
    /** Scenarioが正常完了した状態。 */
    DOMAIN_STATUS_COMPLETED,
    /** Scenarioがエラー終了した状態。 */
    DOMAIN_STATUS_ERROR
} domain_status_t;

/**
 * Domain Serviceが上位へ返すエラー。
 */
typedef enum {
    /** エラーなし。 */
    DOMAIN_ERROR_NONE = 0,
    /** Inputで指定された機能または条件を提供していない。 */
    DOMAIN_ERROR_UNSUPPORTED_REQUEST,
    /** UnitがInputを受け付けなかった。 */
    DOMAIN_ERROR_UNIT_REJECTED,
    /** Unitがエラー終了した。 */
    DOMAIN_ERROR_UNIT_FAILED,
    /** Stepがtimeoutした。 */
    DOMAIN_ERROR_STEP_TIMEOUT,
    /** Domain内部の状態遷移に失敗した。 */
    DOMAIN_ERROR_INTERNAL
} domain_error_t;

/**
 * Input受付結果。
 */
typedef enum {
    /** Inputを受け付けた。 */
    DOMAIN_INPUT_ACCEPTED = 0,
    /** 別の機能を実行中のためInputを受け付けなかった。 */
    DOMAIN_INPUT_BUSY,
    /** 引数またはInput内容が無効だった。 */
    DOMAIN_INPUT_INVALID_ARGUMENT,
    /** 指定された機能または条件を提供していない。 */
    DOMAIN_INPUT_UNSUPPORTED
} domain_input_result_t;

/**
 * ControlからDomain Serviceへ渡すInput。
 */
typedef struct {
    /** 実行する機能。 */
    domain_feature_t feature;
    /** Scenario選択に使用する機能固有条件。 */
    uint32_t condition;
    /** 要求と完了を対応付けるControl発行ID。 */
    uint32_t request_id;
} domain_service_input_t;

/**
 * Domain ServiceからControlへ返すOutput。
 */
typedef struct {
    /** 機能単位の現在状態。 */
    domain_status_t status;
    /** 実行中、または最後に完了した機能。 */
    domain_feature_t feature;
    /** 実行中、または最後に完了した要求ID。 */
    uint32_t request_id;
    /** 選択されたScenario ID。 */
    uint32_t scenario_id;
    /** 実行中、または最後に完了したSequenceの0始まりindex。 */
    size_t sequence_index;
    /** 実行中、または最後に完了したStepの0始まりindex。 */
    size_t step_index;
    /** Scenario全体で完了したStep数。 */
    size_t completed_step_count;
    /** Scenario全体のStep数。 */
    size_t total_step_count;
    /** 機能単位で上位へ通知するエラー。 */
    domain_error_t error;
} domain_service_output_t;

/**
 * Domain Serviceの内部状態を隠蔽する型。
 */
typedef struct domain_service domain_service_t;

/**
 * Feature終端EventをControlへ通知するcallback。
 *
 * @param output Feature完了またはエラーを反映したOutput snapshot。
 * @param context 利用側が登録したcontext。
 *
 * outputはcallback中のみ有効であり、保持する場合は値を複製する。
 */
typedef void (*domain_service_event_handler_t)(const domain_service_output_t *output, void *context);

/**
 * Unit群を使用するDomain Serviceを生成する。
 *
 * @param units Domainが操作するUnitポインタ配列。
 * @param unit_count unitsの要素数。サンプルWorkflowでは10以上必要。
 * @return 生成したService。引数不正または生成失敗時はNULL。
 *
 * ServiceはUnitを所有しない。unitsと各UnitはService破棄後まで有効に保つ。
 * 戻り値の所有権は呼び出し元へ移る。
 */
domain_service_t *domain_service_create(unit_mock_t *const *units, size_t unit_count);

/**
 * Domain Serviceを破棄する。
 *
 * @param service domain_service_create()が返したService。NULLも許容する。
 *
 * Unitは破棄しない。実行中に呼び出す場合、Unit処理は継続するため、
 * Controlは通常終端状態を確認してから破棄する。
 */
void domain_service_destroy(domain_service_t *service);

/**
 * Feature終端EventのControl向け通知先を設定する。
 *
 * @param service 設定するService。
 * @param handler Feature完了・エラー時にPublisherから呼ばれるcallback。
 * @param context handlerへ渡すcontext。
 * @return 設定成功時DOMAIN_INPUT_ACCEPTED、引数不正時
 * DOMAIN_INPUT_INVALID_ARGUMENT。
 */
domain_input_result_t domain_service_set_event_handler(domain_service_t *service, domain_service_event_handler_t handler, void *context);

/**
 * 機能実行InputをDomain Serviceへ書き込む。
 *
 * @param service Inputを書き込むService。
 * @param input 書き込むInput。値を複製しポインタを保持しない。
 * @return Input受付結果。
 *
 * この関数はWorkflowを初期化するがUnit完了を待たずに戻る。
 */
domain_input_result_t domain_service_write_input(domain_service_t *service, const domain_service_input_t *input);

/**
 * Domain Serviceを1周期進める。
 *
 * @param service 進行させるService。
 * @param now_ms Controlが提供する単調増加ミリ秒tick。
 *
 * timeout Eventを必要に応じて発行し、PublisherのQueueをDispatcherへ配送する。
 * Unit Outputのポーリングとsleepは行わない。
 */
void domain_service_process(domain_service_t *service, uint64_t now_ms);

/**
 * Domain ServiceのOutput snapshotを読み取る。
 *
 * @param service Outputを読み取るService。
 * @param output 読み取った値の格納先。
 * @return 読み取り成功時DOMAIN_INPUT_ACCEPTED、引数不正時
 * DOMAIN_INPUT_INVALID_ARGUMENT。
 */
domain_input_result_t domain_service_read_output(const domain_service_t *service, domain_service_output_t *output);

#ifdef __cplusplus
}
#endif

#endif
