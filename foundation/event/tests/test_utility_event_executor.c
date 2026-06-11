/**
 * @file test_utility_event_executor.c
 * @brief Event Executorの単体テスト。
 */
#include "utility_event_executor.h"

#include "test_support.h"

/** Executor handler呼出しを観測する単体テストcontext。 */
typedef struct executor_test_context {
    /** handler呼出回数。 */
    size_t call_count;
    /** 最後に受信したEvent ID。 */
    uint32_t last_event_id;
    /** 再帰実行を試すExecutor。 */
    ut_event_executor_t *executor;
    /** handler内で試した再帰実行結果。 */
    ut_event_result_t recursive_result;
} executor_test_context_t;

/**
 * Event IDと呼出回数をcontextへ記録する。
 *
 * @param event 配送されたEvent。
 * @param user_context executor_test_context_tへのpointer。
 */
static void record_event(const ut_event_t *event, void *user_context)
{
    executor_test_context_t *context =
        (executor_test_context_t *)user_context;

    context->call_count++;
    context->last_event_id = event->id;
}

/**
 * handler内からExecutorを再帰実行し、拒否結果を記録する。
 *
 * @param event 配送されたEvent。
 * @param user_context executor_test_context_tへのpointer。
 */
static void try_recursive_run(
    const ut_event_t *event,
    void *user_context)
{
    executor_test_context_t *context =
        (executor_test_context_t *)user_context;

    (void)event;
    context->recursive_result =
        ut_event_executor_run_once(context->executor, 0u, 1u, NULL);
}

/**
 * TimerとContractを接続しない基本Executor fixtureを初期化する。
 *
 * @param executor 初期化するExecutor。
 * @param queue 初期化するQueue。
 * @param queue_storage 4件のQueue storage。
 * @param dispatcher 初期化するDispatcher。
 * @param subscription_storage 2件のsubscription storage。
 * @param metrics 初期化するMetrics。
 * @return 成功時0、失敗時1。
 */
static int initialize_executor(
    ut_event_executor_t *executor,
    ut_event_queue_t *queue,
    ut_event_t *queue_storage,
    ut_event_dispatcher_t *dispatcher,
    ut_event_subscription_t *subscription_storage,
    ut_event_metrics_t *metrics)
{
    CHECK(ut_event_queue_init(queue, queue_storage, 4u) == UT_EVENT_OK);
    CHECK(ut_event_dispatcher_init(
        dispatcher, subscription_storage, 2u) == UT_EVENT_OK);
    CHECK(ut_event_metrics_init(metrics) == UT_EVENT_OK);
    CHECK(ut_event_executor_init(
        executor, queue, dispatcher, NULL, NULL, metrics) == UT_EVENT_OK);
    return 0;
}

/**
 * budget停止、通常配送、未購読EventとMetricsを確認する。
 *
 * @return 成功時0、失敗時1。
 */
static int test_budget_dispatch_and_unhandled(void)
{
    ut_event_t queue_storage[4];
    ut_event_subscription_t subscription_storage[2];
    ut_event_queue_t queue;
    ut_event_dispatcher_t dispatcher;
    ut_event_metrics_t metrics;
    ut_event_executor_t executor;
    ut_event_executor_report_t report;
    executor_test_context_t context = {0};
    const ut_event_t handled = {1u, 0u, NULL, 0u};
    const ut_event_t unhandled = {2u, 0u, NULL, 0u};

    CHECK(initialize_executor(
        &executor, &queue, queue_storage, &dispatcher,
        subscription_storage, &metrics) == 0);
    CHECK(ut_event_subscribe(
        &dispatcher, 1u, record_event, &context) == UT_EVENT_OK);
    CHECK(ut_event_queue_push(&queue, &handled) == UT_EVENT_OK);
    CHECK(ut_event_queue_push(&queue, &unhandled) == UT_EVENT_OK);

    CHECK(ut_event_executor_run_once(
        &executor, 0u, 1u, &report) == UT_EVENT_OK);
    CHECK(report.dispatched_count == 1u);
    CHECK(report.budget_exhausted == 1u);
    CHECK(ut_event_queue_count(&queue) == 1u);

    CHECK(ut_event_executor_run_once(
        &executor, 0u, 4u, &report) == UT_EVENT_OK);
    CHECK(report.unhandled_count == 1u);
    CHECK(context.call_count == 1u);
    CHECK(metrics.dequeued_count == 2u);
    CHECK(metrics.dispatched_count == 1u);
    CHECK(metrics.unhandled_count == 1u);
    CHECK(metrics.budget_exhausted_count == 1u);
    CHECK(metrics.queue_high_watermark == 2u);
    return 0;
}

/**
 * Contract違反拒否とhandlerからのExecutor再入拒否を確認する。
 *
 * @return 成功時0、失敗時1。
 */
static int test_contract_rejection_and_reentry(void)
{
    ut_event_t queue_storage[4];
    ut_event_subscription_t subscription_storage[2];
    ut_event_queue_t queue;
    ut_event_dispatcher_t dispatcher;
    ut_event_metrics_t metrics;
    ut_event_executor_t executor;
    ut_event_executor_report_t report;
    executor_test_context_t context = {0};
    const ut_event_contract_t contracts[] = {
        {1u, "empty", 0u, 0u, UT_EVENT_PAYLOAD_NONE}
    };
    ut_event_contract_registry_t registry;
    const uint8_t payload = 1u;
    const ut_event_t invalid = {1u, 0u, &payload, sizeof(payload)};
    const ut_event_t valid = {1u, 0u, NULL, 0u};

    CHECK(initialize_executor(
        &executor, &queue, queue_storage, &dispatcher,
        subscription_storage, &metrics) == 0);
    CHECK(ut_event_contract_registry_init(
        &registry, contracts, 1u) == UT_EVENT_OK);
    executor.contract_registry = &registry;
    context.executor = &executor;
    CHECK(ut_event_subscribe(
        &dispatcher, 1u, try_recursive_run, &context) == UT_EVENT_OK);
    CHECK(ut_event_queue_push(&queue, &invalid) == UT_EVENT_OK);
    CHECK(ut_event_queue_push(&queue, &valid) == UT_EVENT_OK);

    CHECK(ut_event_executor_run_once(
        &executor, 0u, 4u, &report) == UT_EVENT_OK);
    CHECK(report.rejected_count == 1u);
    CHECK(report.dispatched_count == 1u);
    CHECK(context.recursive_result == UT_EVENT_BUSY);
    CHECK(metrics.rejected_count == 1u);
    return 0;
}

/**
 * Timer発行のQueue満杯再試行と不正引数を確認する。
 *
 * @return 成功時0、失敗時1。
 */
static int test_timer_full_and_invalid_arguments(void)
{
    ut_event_t queue_storage[1];
    ut_event_subscription_t subscription_storage[1];
    ut_event_timer_slot_t timer_storage[1];
    ut_event_queue_t queue;
    ut_event_dispatcher_t dispatcher;
    ut_event_timer_scheduler_t scheduler;
    ut_event_metrics_t metrics;
    ut_event_executor_t executor;
    ut_event_executor_report_t report;
    executor_test_context_t context = {0};
    const ut_event_t queued = {1u, 0u, NULL, 0u};
    const ut_event_t timed = {2u, 0u, NULL, 0u};

    CHECK(ut_event_queue_init(&queue, queue_storage, 1u) == UT_EVENT_OK);
    CHECK(ut_event_dispatcher_init(
        &dispatcher, subscription_storage, 1u) == UT_EVENT_OK);
    CHECK(ut_event_timer_scheduler_init(
        &scheduler, timer_storage, 1u) == UT_EVENT_OK);
    CHECK(ut_event_metrics_init(&metrics) == UT_EVENT_OK);
    CHECK(ut_event_executor_init(
        &executor, &queue, &dispatcher, &scheduler, NULL, &metrics) ==
        UT_EVENT_OK);
    CHECK(ut_event_subscribe(
        &dispatcher, UT_EVENT_ID_ANY, record_event, &context) ==
        UT_EVENT_OK);
    CHECK(ut_event_queue_push(&queue, &queued) == UT_EVENT_OK);
    CHECK(ut_event_timer_start(
        &scheduler, 1u, &timed, 0u, 0u, 0u) == UT_EVENT_OK);

    CHECK(ut_event_executor_run_once(
        &executor, 0u, 1u, &report) == UT_EVENT_FULL);
    CHECK(report.timer_result == UT_EVENT_FULL);
    CHECK(context.last_event_id == 1u);
    CHECK(metrics.timer_blocked_count == 1u);
    CHECK(ut_event_timer_count(&scheduler) == 1u);

    CHECK(ut_event_executor_run_once(
        &executor, 0u, 1u, &report) == UT_EVENT_OK);
    CHECK(report.timer_emitted_count == 1u);
    CHECK(context.last_event_id == 2u);
    CHECK(metrics.timer_emitted_count == 1u);

    CHECK(ut_event_executor_init(
        NULL, &queue, &dispatcher, NULL, NULL, NULL) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_executor_init(
        &executor, NULL, &dispatcher, NULL, NULL, NULL) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_executor_run_once(NULL, 0u, 1u, NULL) ==
        UT_EVENT_INVALID_ARGUMENT);
    CHECK(ut_event_executor_run_once(&executor, 0u, 0u, NULL) ==
        UT_EVENT_INVALID_ARGUMENT);
    return 0;
}

int run_utility_event_executor_tests(void)
{
    CHECK(test_budget_dispatch_and_unhandled() == 0);
    CHECK(test_contract_rejection_and_reentry() == 0);
    CHECK(test_timer_full_and_invalid_arguments() == 0);
    return 0;
}
