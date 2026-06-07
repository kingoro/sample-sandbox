/**
 * @file test_cases.h
 * @brief Event Utility単体テスト群の実行入口。
 */
#ifndef UTILITY_EVENT_TEST_CASES_H
#define UTILITY_EVENT_TEST_CASES_H

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

#endif
