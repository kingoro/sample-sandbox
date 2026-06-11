/**
 * @file test_utility_event_main.c
 * @brief Event Foundation C単体テスト実行program。
 */
#include "test_cases.h"
#include "test_support.h"

/**
 * QueueとDispatcherの全単体テストを実行する。
 *
 * @return 全テスト成功時は0、失敗時は1。
 */
int main(void)
{
    CHECK(run_utility_event_buffer_tests() == 0);
    CHECK(run_utility_event_contract_tests() == 0);
    CHECK(run_utility_event_queue_tests() == 0);
    CHECK(run_utility_event_dispatcher_tests() == 0);
    CHECK(run_utility_event_executor_tests() == 0);
    CHECK(run_utility_event_metrics_tests() == 0);
    CHECK(run_utility_event_publisher_tests() == 0);
    CHECK(run_utility_event_state_machine_tests() == 0);
    CHECK(run_utility_event_timer_tests() == 0);
    CHECK(run_utility_event_trace_tests() == 0);
    return 0;
}
