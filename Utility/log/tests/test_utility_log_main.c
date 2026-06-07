/**
 * @file test_utility_log_main.c
 * @brief Log Utility C単体テスト実行program。
 */
#include "test_cases.h"
#include "test_support.h"

/**
 * LoggerとConsole adapterの全単体テストを実行する。
 *
 * @return 全テスト成功時は0、失敗時は1。
 */
int main(void)
{
    CHECK(run_utility_logger_tests() == 0);
    CHECK(run_utility_log_console_tests() == 0);
    return 0;
}
