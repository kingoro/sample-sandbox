/**
 * @file test_cases.h
 * @brief Event Foundation単体テスト群の実行入口。
 */
#ifndef UTILITY_EVENT_TEST_CASES_H
#define UTILITY_EVENT_TEST_CASES_H

/**
 * Buffer Pool連携の単体テストをすべて実行する。
 *
 * @return 全テスト成功時は0、失敗時は1。
 */
int run_utility_event_buffer_tests(void);

/**
 * Event Contract Registryの単体テストをすべて実行する。
 *
 * @return 全テスト成功時は0、失敗時は1。
 */
int run_utility_event_contract_tests(void);

/**
 * Event Queueの単体テストをすべて実行する。
 *
 * @return 全テスト成功時は0、失敗時は1。
 */
int run_utility_event_queue_tests(void);

/**
 * Event Dispatcherの単体テストをすべて実行する。
 *
 * @return 全テスト成功時は0、失敗時は1。
 */
int run_utility_event_dispatcher_tests(void);

/**
 * Event Executorの単体テストをすべて実行する。
 *
 * @return 全テスト成功時は0、失敗時は1。
 */
int run_utility_event_executor_tests(void);

/**
 * Event Metricsの単体テストをすべて実行する。
 *
 * @return 全テスト成功時は0、失敗時は1。
 */
int run_utility_event_metrics_tests(void);

/**
 * payload値copy Event Publisherの単体テストをすべて実行する。
 *
 * @return 全テスト成功時は0、失敗時は1。
 */
int run_utility_event_publisher_tests(void);

/**
 * Event State Machineの単体テストをすべて実行する。
 *
 * @return 全テスト成功時は0、失敗時は1。
 */
int run_utility_event_state_machine_tests(void);

/**
 * Timer Eventの単体テストをすべて実行する。
 *
 * @return 全テスト成功時は0、失敗時は1。
 */
int run_utility_event_timer_tests(void);

/**
 * Event traceとLog adapterの単体テストをすべて実行する。
 *
 * @return 全テスト成功時は0、失敗時は1。
 */
int run_utility_event_trace_tests(void);

#endif
