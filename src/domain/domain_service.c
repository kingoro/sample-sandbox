/**
 * @file domain_service.c
 * @brief Publisherを介した階層Event駆動Workflow実装。
 */
#include "domain_service.h"

#include "domain_event_publisher.h"
#include "domain_workflow.h"
#include "utility_event.h"
#include "utility_log.h"

#include <stdbool.h>
#include <stdlib.h>

/** Feature State Machineの識別子。 */
#define DOMAIN_FEATURE_SOURCE 100U
/** Scenario State Machineの識別子。 */
#define DOMAIN_SCENARIO_SOURCE 200U
/** Sequence State Machineの識別子。 */
#define DOMAIN_SEQUENCE_SOURCE 300U
/** 1周期で配送する最大Event数。 */
#define DOMAIN_DISPATCH_BUDGET 32U

/**
 * 各Runnerで共通利用する状態。
 */
typedef enum {
    /** 開始前状態。 */
    DOMAIN_RUNNER_IDLE = 1,
    /** 子要素を実行中の状態。 */
    DOMAIN_RUNNER_RUNNING,
    /** 全子要素が正常完了した状態。 */
    DOMAIN_RUNNER_COMPLETED,
    /** 継続不能状態。 */
    DOMAIN_RUNNER_ERROR
} domain_runner_state_t;

/**
 * 各Runner State Machineへ入力するEvent。
 */
typedef enum {
    /** Runner開始Event。 */
    DOMAIN_RUNNER_START = 1,
    /** Runner正常完了Event。 */
    DOMAIN_RUNNER_COMPLETE,
    /** RunnerエラーEvent。 */
    DOMAIN_RUNNER_FAIL
} domain_runner_signal_t;

/**
 * Sequence Runnerの実行状態。
 */
typedef struct {
    /** Sequence単位のState Machine。 */
    ut_event_state_machine_t machine;
    /** 実行中のSequence定義。 */
    const domain_sequence_t *definition;
    /** 実行中のStep index。 */
    size_t step_index;
    /** Unitへ発行した要求ID。 */
    uint32_t unit_request_id;
    /** 現在Stepのdeadline。 */
    uint64_t step_deadline_ms;
    /** 現在Stepの完了待ちの場合true。 */
    bool step_running;
    /** timeout Eventを発行済みの場合true。 */
    bool timeout_published;
} domain_sequence_runner_t;

/**
 * Scenario Runnerの実行状態。
 */
typedef struct {
    /** Scenario単位のState Machine。 */
    ut_event_state_machine_t machine;
    /** 実行中のScenario定義。 */
    const domain_scenario_t *definition;
    /** 実行中のSequence index。 */
    size_t sequence_index;
    /** 子Sequence Runner。 */
    domain_sequence_runner_t sequence;
} domain_scenario_runner_t;

/**
 * Domain Service内部状態。
 */
struct domain_service {
    /** Feature単位のState Machine。 */
    ut_event_state_machine_t feature_machine;
    /** 子Scenario Runner。 */
    domain_scenario_runner_t scenario;
    /** 全階層間Eventを配送するPublisher。 */
    domain_event_publisher_t *publisher;
    /** 呼び出し元所有のUnit配列。 */
    unit_mock_t *const *units;
    /** unitsの要素数。 */
    size_t unit_count;
    /** 外部JSONから読み込んだ任意のWorkflow Catalog。 */
    domain_workflow_catalog_t *workflow_catalog;
    /** Controlへ公開するOutput snapshot。 */
    domain_service_output_t output;
    /** Controlから最後に渡された単調増加tick。 */
    uint64_t current_time_ms;
    /** 最初のSequenceを開始済みの場合true。 */
    bool workflow_started;
    /** ControlへFeature Eventを通知するcallback。 */
    domain_service_event_handler_t control_handler;
    /** control_handlerへ渡すcontext。 */
    void *control_context;
};

/** Runner共通状態定義。 */
static const ut_event_state_t runner_states[] = {
    {DOMAIN_RUNNER_IDLE, "IDLE", NULL, NULL, NULL},
    {DOMAIN_RUNNER_RUNNING, "RUNNING", NULL, NULL, NULL},
    {DOMAIN_RUNNER_COMPLETED, "COMPLETED", NULL, NULL, NULL},
    {DOMAIN_RUNNER_ERROR, "ERROR", NULL, NULL, NULL},
};

/** Runner共通遷移定義。 */
static const ut_event_state_transition_t runner_transitions[] = {
    {
        DOMAIN_RUNNER_IDLE,
        DOMAIN_RUNNER_START,
        DOMAIN_RUNNER_RUNNING,
        NULL,
        NULL,
        NULL,
        0U,
        DOMAIN_RUNNER_START,
    },
    {
        DOMAIN_RUNNER_RUNNING,
        DOMAIN_RUNNER_COMPLETE,
        DOMAIN_RUNNER_COMPLETED,
        NULL,
        NULL,
        NULL,
        0U,
        DOMAIN_RUNNER_COMPLETE,
    },
    {
        DOMAIN_RUNNER_RUNNING,
        DOMAIN_RUNNER_FAIL,
        DOMAIN_RUNNER_ERROR,
        NULL,
        NULL,
        NULL,
        0U,
        DOMAIN_RUNNER_FAIL,
    },
};

/**
 * Runner State Machineを初期化する。
 *
 * @param machine 初期化対象。
 * @param source Runner階層の識別子。
 * @return 成功時true。
 */
static bool runner_initialize(
    ut_event_state_machine_t *machine,
    uint32_t source)
{
    return ut_event_state_machine_init(
        machine,
        runner_states,
        sizeof(runner_states) / sizeof(runner_states[0]),
        runner_transitions,
        sizeof(runner_transitions) / sizeof(runner_transitions[0]),
        DOMAIN_RUNNER_IDLE,
        source,
        NULL) == UT_EVENT_OK;
}

/**
 * Runner State Machineへsignalを入力する。
 *
 * @param machine 入力先。
 * @param signal domain_runner_signal_t。
 * @return 遷移成功時true。
 */
static bool runner_signal(
    ut_event_state_machine_t *machine,
    domain_runner_signal_t signal)
{
    const ut_event_t event = {
        .id = (uint32_t)signal,
        .source = 0U,
        .payload = NULL,
        .payload_size = 0U,
    };

    return ut_event_state_machine_dispatch(machine, &event, NULL)
        == UT_EVENT_OK;
}

/**
 * Runnerの現在状態を確認する。
 *
 * @param machine 確認対象。
 * @param state 比較する状態。
 * @return 一致時true。
 */
static bool runner_is(
    const ut_event_state_machine_t *machine,
    domain_runner_state_t state)
{
    return ut_event_state_machine_current_state(machine) == (uint32_t)state;
}

/**
 * 現在位置を含むDomain Event payloadを作る。
 *
 * @param service 実行中Service。
 * @param error Eventへ設定するエラー。
 * @return 値payload。
 */
static domain_event_payload_t domain_current_payload(
    const domain_service_t *service,
    domain_error_t error)
{
    return (domain_event_payload_t){
        .feature_request_id = service->output.request_id,
        .unit_request_id = service->scenario.sequence.unit_request_id,
        .scenario_id = service->output.scenario_id,
        .sequence_index = service->output.sequence_index,
        .step_index = service->output.step_index,
        .unit_number = 0U,
        .error = (uint32_t)error,
    };
}

/**
 * Domain EventをPublisherへ発行する。
 *
 * @param service 発行元Service。
 * @param event_id Domain Event ID。
 * @param source 発行元階層。
 * @param payload 発行payload。
 * @return 発行成功時true。
 */
static bool domain_publish(
    domain_service_t *service,
    uint32_t event_id,
    uint32_t source,
    const domain_event_payload_t *payload)
{
    const bool succeeded = domain_event_publisher_publish(
        service->publisher,
        event_id,
        source,
        payload) == UT_EVENT_OK;

    if (!succeeded) {
        service->output.status = DOMAIN_STATUS_ERROR;
        service->output.error = DOMAIN_ERROR_INTERNAL;
        UT_LOG_ERROR("DOMAIN", "publisher queue rejected event=%u", event_id);
    }
    return succeeded;
}

/**
 * SequenceエラーEventを発行する。
 *
 * @param service 発行元Service。
 * @param error エラー内容。
 */
static void sequence_publish_error(
    domain_service_t *service,
    domain_error_t error)
{
    domain_sequence_runner_t *sequence = &service->scenario.sequence;
    domain_event_payload_t payload = domain_current_payload(service, error);

    if (runner_is(&sequence->machine, DOMAIN_RUNNER_RUNNING)) {
        (void)runner_signal(&sequence->machine, DOMAIN_RUNNER_FAIL);
    }
    sequence->step_running = false;
    (void)domain_publish(
        service,
        DOMAIN_EVENT_SEQUENCE_ERROR,
        DOMAIN_SEQUENCE_SOURCE,
        &payload);
}

/**
 * 現在StepをUnitへ入力する。
 *
 * @param service 実行中Service。
 * @param now_ms 現在時刻。
 */
static void sequence_start_step(
    domain_service_t *service,
    uint64_t now_ms)
{
    domain_sequence_runner_t *sequence = &service->scenario.sequence;
    const domain_step_t *step =
        &sequence->definition->steps[sequence->step_index];
    const uint32_t request_id =
        service->output.request_id
        + (uint32_t)service->output.completed_step_count
        + 1U;
    const unit_mock_input_t input = {
        .command = step->command,
        .request_id = request_id,
    };

    if (step->unit_index >= service->unit_count
        || service->units[step->unit_index] == NULL) {
        sequence_publish_error(service, DOMAIN_ERROR_INTERNAL);
    } else if (unit_mock_write_input(
            service->units[step->unit_index],
            &input) != UNIT_MOCK_INPUT_ACCEPTED) {
        sequence_publish_error(service, DOMAIN_ERROR_UNIT_REJECTED);
    } else {
        sequence->unit_request_id = request_id;
        sequence->step_deadline_ms = now_ms + step->timeout_ms;
        sequence->step_running = true;
        sequence->timeout_published = false;
        service->output.step_index = sequence->step_index;
        UT_LOG_INFO(
            "DOMAIN",
            "start scenario=%u sequence=%zu step=%zu unit=%zu",
            service->output.scenario_id,
            service->output.sequence_index,
            service->output.step_index,
            step->unit_index + 1U);
    }
}

/**
 * 現在ScenarioのSequenceを開始する。
 *
 * @param service 実行中Service。
 * @param now_ms 現在時刻。
 */
static void scenario_start_sequence(
    domain_service_t *service,
    uint64_t now_ms)
{
    domain_scenario_runner_t *scenario = &service->scenario;
    domain_sequence_runner_t *sequence = &scenario->sequence;

    if (!runner_initialize(&sequence->machine, DOMAIN_SEQUENCE_SOURCE)) {
        sequence_publish_error(service, DOMAIN_ERROR_INTERNAL);
    } else {
        sequence->definition =
            &scenario->definition->sequences[scenario->sequence_index];
        sequence->step_index = 0U;
        sequence->step_running = false;
        service->output.sequence_index = scenario->sequence_index;
        if (!runner_signal(&sequence->machine, DOMAIN_RUNNER_START)) {
            sequence_publish_error(service, DOMAIN_ERROR_INTERNAL);
        } else {
            UT_LOG_INFO(
                "DOMAIN",
                "start sequence=%s",
                sequence->definition->name);
            sequence_start_step(service, now_ms);
        }
    }
}

/**
 * Unit完了EventをSequence Runnerで処理する。
 *
 * @param event Unit完了Event。
 * @param context domain_service_t。
 */
static void sequence_on_unit_completed(
    const ut_event_t *event,
    void *context)
{
    domain_service_t *service = context;
    domain_sequence_runner_t *sequence = &service->scenario.sequence;
    const domain_event_payload_t *payload = event->payload;

    if (service->output.status != DOMAIN_STATUS_RUNNING
        || !sequence->step_running
        || payload->unit_request_id != sequence->unit_request_id) {
        return;
    }

    sequence->step_running = false;
    service->output.completed_step_count++;
    sequence->step_index++;
    if (sequence->step_index >= sequence->definition->step_count) {
        domain_event_payload_t completed =
            domain_current_payload(service, DOMAIN_ERROR_NONE);

        if (!runner_signal(&sequence->machine, DOMAIN_RUNNER_COMPLETE)) {
            sequence_publish_error(service, DOMAIN_ERROR_INTERNAL);
        } else {
            UT_LOG_INFO(
                "DOMAIN",
                "publish sequence completed index=%zu",
                service->output.sequence_index);
            (void)domain_publish(
                service,
                DOMAIN_EVENT_SEQUENCE_COMPLETED,
                DOMAIN_SEQUENCE_SOURCE,
                &completed);
        }
    } else {
        sequence_start_step(service, service->current_time_ms);
    }
}

/**
 * UnitエラーまたはStep timeoutをSequence Runnerで処理する。
 *
 * @param event Unitエラーまたはtimeout Event。
 * @param context domain_service_t。
 */
static void sequence_on_failure(
    const ut_event_t *event,
    void *context)
{
    domain_service_t *service = context;
    domain_sequence_runner_t *sequence = &service->scenario.sequence;
    const domain_event_payload_t *payload = event->payload;

    if (service->output.status == DOMAIN_STATUS_RUNNING
        && sequence->step_running
        && (event->id == DOMAIN_EVENT_STEP_TIMEOUT
            || payload->unit_request_id == sequence->unit_request_id)) {
        const domain_error_t error =
            event->id == DOMAIN_EVENT_STEP_TIMEOUT
            ? DOMAIN_ERROR_STEP_TIMEOUT
            : DOMAIN_ERROR_UNIT_FAILED;
        sequence_publish_error(service, error);
    }
}

/**
 * Sequence完了EventをScenario Runnerで処理する。
 *
 * @param event Sequence完了Event。
 * @param context domain_service_t。
 */
static void scenario_on_sequence_completed(
    const ut_event_t *event,
    void *context)
{
    domain_service_t *service = context;
    domain_scenario_runner_t *scenario = &service->scenario;
    domain_event_payload_t payload;

    (void)event;
    scenario->sequence_index++;
    if (scenario->sequence_index >= scenario->definition->sequence_count) {
        payload = domain_current_payload(service, DOMAIN_ERROR_NONE);
        if (!runner_signal(&scenario->machine, DOMAIN_RUNNER_COMPLETE)) {
            payload.error = DOMAIN_ERROR_INTERNAL;
            (void)domain_publish(
                service,
                DOMAIN_EVENT_SCENARIO_ERROR,
                DOMAIN_SCENARIO_SOURCE,
                &payload);
        } else {
            UT_LOG_INFO(
                "DOMAIN",
                "publish scenario completed id=%u",
                service->output.scenario_id);
            (void)domain_publish(
                service,
                DOMAIN_EVENT_SCENARIO_COMPLETED,
                DOMAIN_SCENARIO_SOURCE,
                &payload);
        }
    } else {
        scenario_start_sequence(service, service->current_time_ms);
    }
}

/**
 * SequenceエラーEventをScenario Runnerで処理する。
 *
 * @param event SequenceエラーEvent。
 * @param context domain_service_t。
 */
static void scenario_on_sequence_error(
    const ut_event_t *event,
    void *context)
{
    domain_service_t *service = context;
    const domain_event_payload_t *payload = event->payload;

    if (runner_is(&service->scenario.machine, DOMAIN_RUNNER_RUNNING)) {
        (void)runner_signal(&service->scenario.machine, DOMAIN_RUNNER_FAIL);
    }
    (void)domain_publish(
        service,
        DOMAIN_EVENT_SCENARIO_ERROR,
        DOMAIN_SCENARIO_SOURCE,
        payload);
}

/**
 * Scenario完了EventをFeature Runnerで処理する。
 *
 * @param event Scenario完了Event。
 * @param context domain_service_t。
 */
static void feature_on_scenario_completed(
    const ut_event_t *event,
    void *context)
{
    domain_service_t *service = context;
    const domain_event_payload_t *payload = event->payload;

    if (!runner_signal(&service->feature_machine, DOMAIN_RUNNER_COMPLETE)) {
        domain_event_payload_t failed = *payload;

        failed.error = DOMAIN_ERROR_INTERNAL;
        (void)domain_publish(
            service,
            DOMAIN_EVENT_FEATURE_ERROR,
            DOMAIN_FEATURE_SOURCE,
            &failed);
    } else {
        UT_LOG_INFO(
            "DOMAIN",
            "publish feature completed request=%u",
            service->output.request_id);
        (void)domain_publish(
            service,
            DOMAIN_EVENT_FEATURE_COMPLETED,
            DOMAIN_FEATURE_SOURCE,
            payload);
    }
}

/**
 * ScenarioエラーEventをFeature Runnerで処理する。
 *
 * @param event ScenarioエラーEvent。
 * @param context domain_service_t。
 */
static void feature_on_scenario_error(
    const ut_event_t *event,
    void *context)
{
    domain_service_t *service = context;

    if (runner_is(&service->feature_machine, DOMAIN_RUNNER_RUNNING)) {
        (void)runner_signal(&service->feature_machine, DOMAIN_RUNNER_FAIL);
    }
    (void)domain_publish(
        service,
        DOMAIN_EVENT_FEATURE_ERROR,
        DOMAIN_FEATURE_SOURCE,
        event->payload);
}

/**
 * Feature終端EventをControl向けOutputへ反映する。
 *
 * @param event Feature完了またはエラーEvent。
 * @param context domain_service_t。
 */
static void control_on_feature_event(
    const ut_event_t *event,
    void *context)
{
    domain_service_t *service = context;
    const domain_event_payload_t *payload = event->payload;

    service->output.status =
        event->id == DOMAIN_EVENT_FEATURE_COMPLETED
        ? DOMAIN_STATUS_COMPLETED
        : DOMAIN_STATUS_ERROR;
    service->output.error = (domain_error_t)payload->error;
    UT_LOG_INFO(
        "DOMAIN",
        "feature terminal request=%u status=%d error=%d",
        service->output.request_id,
        service->output.status,
        service->output.error);
    if (service->control_handler != NULL) {
        service->control_handler(&service->output, service->control_context);
    }
}

/**
 * Publisherへ階層間購読を登録する。
 *
 * @param service 登録対象Service。
 * @return 全登録成功時true。
 */
static bool domain_subscribe(domain_service_t *service)
{
    return domain_event_publisher_subscribe(
            service->publisher,
            DOMAIN_EVENT_UNIT_COMPLETED,
            sequence_on_unit_completed,
            service) == UT_EVENT_OK
        && domain_event_publisher_subscribe(
            service->publisher,
            DOMAIN_EVENT_UNIT_ERROR,
            sequence_on_failure,
            service) == UT_EVENT_OK
        && domain_event_publisher_subscribe(
            service->publisher,
            DOMAIN_EVENT_STEP_TIMEOUT,
            sequence_on_failure,
            service) == UT_EVENT_OK
        && domain_event_publisher_subscribe(
            service->publisher,
            DOMAIN_EVENT_SEQUENCE_COMPLETED,
            scenario_on_sequence_completed,
            service) == UT_EVENT_OK
        && domain_event_publisher_subscribe(
            service->publisher,
            DOMAIN_EVENT_SEQUENCE_ERROR,
            scenario_on_sequence_error,
            service) == UT_EVENT_OK
        && domain_event_publisher_subscribe(
            service->publisher,
            DOMAIN_EVENT_SCENARIO_COMPLETED,
            feature_on_scenario_completed,
            service) == UT_EVENT_OK
        && domain_event_publisher_subscribe(
            service->publisher,
            DOMAIN_EVENT_SCENARIO_ERROR,
            feature_on_scenario_error,
            service) == UT_EVENT_OK
        && domain_event_publisher_subscribe(
            service->publisher,
            DOMAIN_EVENT_FEATURE_COMPLETED,
            control_on_feature_event,
            service) == UT_EVENT_OK
        && domain_event_publisher_subscribe(
            service->publisher,
            DOMAIN_EVENT_FEATURE_ERROR,
            control_on_feature_event,
            service) == UT_EVENT_OK;
}

/**
 * 全Runnerを新しい要求用に初期化する。
 *
 * @param service 初期化対象。
 * @param scenario 実行Scenario。
 * @return 成功時true。
 */
static bool domain_prepare(
    domain_service_t *service,
    const domain_scenario_t *scenario)
{
    bool succeeded = runner_initialize(
        &service->feature_machine,
        DOMAIN_FEATURE_SOURCE);

    if (succeeded) {
        succeeded = runner_initialize(
            &service->scenario.machine,
            DOMAIN_SCENARIO_SOURCE);
    }
    if (succeeded) {
        service->scenario.definition = scenario;
        service->scenario.sequence_index = 0U;
        service->scenario.sequence.definition = NULL;
        succeeded = runner_signal(
            &service->feature_machine,
            DOMAIN_RUNNER_START);
    }
    if (succeeded) {
        succeeded = runner_signal(
            &service->scenario.machine,
            DOMAIN_RUNNER_START);
    }
    return succeeded;
}

domain_service_t *domain_service_create(
    unit_mock_t *const *units,
    size_t unit_count)
{
    domain_service_t *service;
    const unit_mock_result_output_t result_output_template = {
        .handler = domain_event_publisher_on_unit_result,
        .context = NULL,
    };

    if (units == NULL || unit_count < 10U) {
        return NULL;
    }
    service = calloc(1U, sizeof(*service));
    if (service == NULL) {
        return NULL;
    }
    service->publisher = domain_event_publisher_create();
    service->units = units;
    service->unit_count = unit_count;
    service->output.status = DOMAIN_STATUS_IDLE;

    bool succeeded = service->publisher != NULL
        && domain_subscribe(service);
    for (size_t index = 0U; index < unit_count && succeeded; ++index) {
        unit_mock_result_output_t result_output = result_output_template;

        result_output.context = service->publisher;
        succeeded = units[index] != NULL
            && unit_mock_set_result_output(units[index], &result_output)
                == UNIT_MOCK_INPUT_ACCEPTED;
    }
    if (!succeeded) {
        domain_event_publisher_destroy(service->publisher);
        free(service);
        service = NULL;
    }
    return service;
}

void domain_service_destroy(domain_service_t *service)
{
    if (service != NULL) {
        domain_event_publisher_destroy(service->publisher);
        domain_workflow_catalog_destroy(service->workflow_catalog);
        free(service);
    }
}

domain_workflow_load_result_t domain_service_load_workflows_json(
    domain_service_t *service,
    const char *path)
{
    domain_workflow_catalog_t *loaded_catalog = NULL;

    if (service == NULL || path == NULL) {
        return DOMAIN_WORKFLOW_LOAD_INVALID_ARGUMENT;
    }
    if (service->output.status == DOMAIN_STATUS_RUNNING) {
        return DOMAIN_WORKFLOW_LOAD_BUSY;
    }
    const domain_workflow_load_result_t result =
        domain_workflow_catalog_load_json_file(
            path,
            service->unit_count,
            &loaded_catalog);

    if (result == DOMAIN_WORKFLOW_LOAD_OK) {
        domain_workflow_catalog_t *old_catalog =
            service->workflow_catalog;

        service->scenario.definition = NULL;
        service->scenario.sequence.definition = NULL;
        service->workflow_catalog = loaded_catalog;
        domain_workflow_catalog_destroy(old_catalog);
    }
    return result;
}

domain_input_result_t domain_service_set_event_handler(
    domain_service_t *service,
    domain_service_event_handler_t handler,
    void *context)
{
    if (service == NULL || handler == NULL) {
        return DOMAIN_INPUT_INVALID_ARGUMENT;
    }
    service->control_handler = handler;
    service->control_context = context;
    return DOMAIN_INPUT_ACCEPTED;
}

domain_input_result_t domain_service_write_input(
    domain_service_t *service,
    const domain_service_input_t *input)
{
    const domain_scenario_t *scenario;

    if (service == NULL || input == NULL
        || input->feature == DOMAIN_FEATURE_NONE) {
        return DOMAIN_INPUT_INVALID_ARGUMENT;
    }
    if (service->output.status == DOMAIN_STATUS_RUNNING) {
        return DOMAIN_INPUT_BUSY;
    }
    if (input->feature != DOMAIN_FEATURE_A) {
        return DOMAIN_INPUT_UNSUPPORTED;
    }
    scenario = service->workflow_catalog == NULL
        ? domain_workflow_feature_a(input->condition)
        : domain_workflow_catalog_find(
            service->workflow_catalog,
            (uint32_t)input->feature,
            input->condition);
    if (scenario == NULL) {
        return DOMAIN_INPUT_UNSUPPORTED;
    }
    if (!domain_prepare(service, scenario)) {
        return DOMAIN_INPUT_INVALID_ARGUMENT;
    }

    service->output = (domain_service_output_t){
        .status = DOMAIN_STATUS_RUNNING,
        .feature = input->feature,
        .request_id = input->request_id,
        .scenario_id = scenario->id,
        .sequence_index = 0U,
        .step_index = 0U,
        .completed_step_count = 0U,
        .total_step_count = scenario->total_step_count,
        .error = DOMAIN_ERROR_NONE,
    };
    service->current_time_ms = 0U;
    service->workflow_started = false;
    return DOMAIN_INPUT_ACCEPTED;
}

void domain_service_process(domain_service_t *service, uint64_t now_ms)
{
    if (service == NULL) {
        return;
    }
    service->current_time_ms = now_ms;
    if (service->output.status == DOMAIN_STATUS_RUNNING) {
        domain_sequence_runner_t *sequence = &service->scenario.sequence;

        if (!service->workflow_started) {
            service->workflow_started = true;
            scenario_start_sequence(service, now_ms);
        } else if (sequence->step_running && !sequence->timeout_published
            && now_ms >= sequence->step_deadline_ms) {
            domain_event_payload_t payload =
                domain_current_payload(service, DOMAIN_ERROR_STEP_TIMEOUT);

            sequence->timeout_published = true;
            (void)domain_publish(
                service,
                DOMAIN_EVENT_STEP_TIMEOUT,
                DOMAIN_SEQUENCE_SOURCE,
                &payload);
        }
    }
    (void)domain_event_publisher_dispatch(
        service->publisher,
        DOMAIN_DISPATCH_BUDGET,
        NULL);
}

domain_input_result_t domain_service_read_output(
    const domain_service_t *service,
    domain_service_output_t *output)
{
    if (service == NULL || output == NULL) {
        return DOMAIN_INPUT_INVALID_ARGUMENT;
    }
    *output = service->output;
    return DOMAIN_INPUT_ACCEPTED;
}
