/**
 * @file test_cases.h
 * @brief Log Utility単体テスト群の実行入口。
 */
#ifndef UTILITY_LOG_TEST_CASES_H
#define UTILITY_LOG_TEST_CASES_H

/**
 * LoggerとRAM Ringの単体テストをすべて実行する。
 *
 * @return 全テスト成功時は0、失敗時は1。
 */
int run_utility_logger_tests(void);

/**
 * Console adapterの単体テストをすべて実行する。
 *
 * @return 全テスト成功時は0、失敗時は1。
 */
int run_utility_log_console_tests(void);

#endif
