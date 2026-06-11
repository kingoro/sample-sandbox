.PHONY: help check extended-check test domain-test foundation-test foundation-event-test foundation-log-test foundation-byte-test foundation-retry-test foundation-id-test foundation-thread-pool-test foundation-time-test platform-test platform-linux-test service-test service-time-test foundation-event-examples foundation-log-examples unit-mock-sample domain-service-sample docs rust-docs c-docs c-docs-check header header-check static-analysis domain-static-analysis foundation-static-analysis foundation-log-static-analysis foundation-byte-static-analysis foundation-retry-static-analysis foundation-id-static-analysis foundation-thread-pool-static-analysis foundation-time-static-analysis platform-static-analysis service-static-analysis coverage metrics quality quality-report cppcheck miri fuzz-smoke foundation-fuzz-smoke foundation-event-fuzz-smoke foundation-log-fuzz-smoke foundation-fuzz portable-check mcu-check docker-ready docker-build docker-shell docker-test docker-check docker-extended-check clean

BUILD_DIR ?= build/memory-buffer
FOUNDATION_EVENT_BUILD_DIR ?= build/foundation-event
FOUNDATION_LOG_BUILD_DIR ?= build/foundation-log
FOUNDATION_BYTE_BUILD_DIR ?= build/foundation-byte
FOUNDATION_RETRY_BUILD_DIR ?= build/foundation-retry
FOUNDATION_ID_BUILD_DIR ?= build/foundation-id
FOUNDATION_THREAD_POOL_BUILD_DIR ?= build/foundation-thread-pool
FOUNDATION_TIME_BUILD_DIR ?= build/foundation-time
PLATFORM_LINUX_BUILD_DIR ?= build/platform-linux
SERVICE_TIME_BUILD_DIR ?= build/service-time
DOMAIN_BUILD_DIR ?= build/domain
REPORT_DIR ?= build/reports
PORTABLE_TARGET ?= thumbv7em-none-eabi
NIGHTLY_TOOLCHAIN ?= nightly-2026-06-06
DOXYGEN ?= bash tools/run_doxygen.sh

help:
	@printf '%s\n' \
		'Development targets:' \
		'  test             Rust/C結合テストと全C Foundation単体テスト' \
		'  foundation-test     全C Foundationの単体テスト' \
		'  foundation-event-test Event FoundationのC単体テスト' \
		'  foundation-log-test Log FoundationのC単体テスト' \
		'  foundation-byte-test Byte FoundationのC単体テスト' \
		'  foundation-retry-test Retry FoundationのC単体テスト' \
		'  foundation-id-test ID FoundationのC単体テスト' \
		'  foundation-thread-pool-test Thread Pool FoundationのC単体テスト' \
		'  foundation-time-test Time FoundationのC単体テスト' \
		'  platform-linux-test Linux Time adapterのC単体テスト' \
		'  service-time-test Time ServiceのC単体テスト' \
		'  domain-test      動的Workflow loaderとDomain ServiceのC単体テスト' \
		'  foundation-event-examples Event Foundationの3サンプルを実行' \
		'  foundation-log-examples Log Foundationの3サンプルを実行' \
		'  unit-mock-sample 10個のMock Unit操作をbuildして実行' \
		'  domain-service-sample A機能の階層Workflowをbuildして実行' \
		'  static-analysis  Rust、C利用例、全C Foundationの静的解析' \
		'  domain-static-analysis Domain、Unit、動的定義loaderのGCC静的解析' \
		'  foundation-static-analysis 全C FoundationのGCC静的解析' \
		'  foundation-log-static-analysis Log FoundationのGCC静的解析' \
		'  foundation-byte-static-analysis Byte FoundationのGCC静的解析' \
		'  foundation-retry-static-analysis Retry FoundationのGCC静的解析' \
		'  foundation-id-static-analysis ID FoundationのGCC静的解析' \
		'  foundation-thread-pool-static-analysis Thread Pool FoundationのGCC静的解析' \
		'  foundation-time-static-analysis Time FoundationのGCC静的解析' \
		'  platform-static-analysis Linux platform adapterのGCC静的解析' \
		'  service-static-analysis Time ServiceのGCC静的解析' \
		'  docs             Rustdocと全C API/test仕様書を生成' \
		'  c-docs           Doxygenで全C API/test仕様書を生成' \
		'  c-docs-check     全headerのDocstring契約を検査' \
		'  header           cbindgen header再生成' \
		'  header-check     生成headerのdrift検査' \
		'  coverage         単体・結合branch coverage検査' \
		'  metrics          CC・MIレポート生成' \
		'  quality-report   coverage・metricsのHTML index生成' \
		'  portable-check   32 bit no_std targetへのcross build' \
		'  miri             Miriによる単体テスト' \
		'  fuzz-smoke       libFuzzer短時間検査' \
		'  foundation-fuzz-smoke 全C Foundation操作列をASan/UBSanで検査' \
		'  foundation-log-fuzz-smoke Log Foundation操作列をASan/UBSanで検査' \
		'  foundation-fuzz     Event FoundationをClang libFuzzerで継続探索' \
		'  cppcheck         C利用例のCppcheck' \
		'  check            通常品質ゲート一式' \
		'  extended-check   checkにMiri・fuzzを追加' \
		'  docker-ready     Docker/Compose接続を確認' \
		'  docker-build     Docker開発imageをbuild' \
		'  docker-shell     Docker開発環境のshellを開く' \
		'  docker-test      Docker内でmake testを実行' \
		'  docker-check     Docker内でmake checkを実行' \
		'  docker-extended-check Docker内でmerge前検査を実行' \
		'  clean            通常Cargo・CMake生成物を削除'

check: header-check static-analysis test docs portable-check quality-report

extended-check: check miri fuzz-smoke foundation-fuzz-smoke

header:
	cbindgen --config memory-buffer/cbindgen.toml \
		--crate memory-buffer \
		--output memory-buffer/include/memory_buffer_generated.h

header-check:
	cbindgen --verify \
		--config memory-buffer/cbindgen.toml \
		--crate memory-buffer \
		--output memory-buffer/include/memory_buffer_generated.h

static-analysis: foundation-static-analysis platform-static-analysis service-static-analysis domain-static-analysis
	cargo fmt --all --check
	cargo clippy --workspace --all-targets -- -D warnings
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I memory-buffer/include -c memory-buffer/examples/c_usage.c \
		-o /tmp/memory_buffer_c_usage_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I memory-buffer/include -I foundation/event/include \
		-c memory-buffer/src/memory_buffer_event_adapter.c \
		-o /tmp/memory_buffer_event_adapter_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I memory-buffer/include -I foundation/event/include \
		-c memory-buffer/tests/test_event_buffer_integration.c \
		-o /tmp/memory_buffer_event_integration_analyzed.o

domain-static-analysis:
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I src/domain -I src/unit \
		-I foundation/event/include -I foundation/log/include \
		-c src/domain/domain_workflow_loader.c \
		-o /tmp/domain_workflow_loader_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I src/domain -I src/unit \
		-I foundation/event/include -I foundation/log/include \
		-c src/domain/domain_workflow.c \
		-o /tmp/domain_workflow_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I src/domain -I src/unit \
		-I foundation/event/include -I foundation/log/include \
		-c src/domain/domain_event_publisher.c \
		-o /tmp/domain_event_publisher_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I src/domain -I src/unit \
		-I foundation/event/include -I foundation/log/include \
		-c src/domain/domain_service.c \
		-o /tmp/domain_service_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer -pthread \
		-I src/domain -I src/unit \
		-I foundation/event/include -I foundation/log/include \
		-c src/domain/domain_service_sample.c \
		-o /tmp/domain_service_sample_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer -pthread \
		-I src/domain -I src/unit \
		-I foundation/event/include -I foundation/log/include \
		-c src/domain/tests/test_domain_workflow_loader.c \
		-o /tmp/domain_workflow_loader_test_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer -pthread \
		-I src/unit -I foundation/log/include \
		-c src/unit/unit_mock.c \
		-o /tmp/unit_mock_analyzed.o

foundation-static-analysis: foundation-log-static-analysis foundation-byte-static-analysis foundation-retry-static-analysis foundation-id-static-analysis foundation-thread-pool-static-analysis foundation-time-static-analysis
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I foundation/event/include -c foundation/event/src/utility_event_buffer.c \
		-o /tmp/utility_event_buffer_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I foundation/event/include -c foundation/event/src/utility_event_contract.c \
		-o /tmp/utility_event_contract_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I foundation/event/include -c foundation/event/src/utility_event_queue.c \
		-o /tmp/utility_event_queue_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I foundation/event/include -c foundation/event/src/utility_event_dispatcher.c \
		-o /tmp/utility_event_dispatcher_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I foundation/event/include -c foundation/event/src/utility_event_executor.c \
		-o /tmp/utility_event_executor_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I foundation/event/include -c foundation/event/src/utility_event_metrics.c \
		-o /tmp/utility_event_metrics_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I foundation/event/include -c foundation/event/src/utility_event_publisher.c \
		-o /tmp/utility_event_publisher_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I foundation/event/include -c foundation/event/src/utility_event_state_machine.c \
		-o /tmp/utility_event_state_machine_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I foundation/event/include -c foundation/event/src/utility_event_timer.c \
		-o /tmp/utility_event_timer_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I foundation/event/include -c foundation/event/src/utility_event_trace.c \
		-o /tmp/utility_event_trace_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I foundation/event/include -I foundation/log/include \
		-c foundation/event/src/utility_event_trace_log.c \
		-o /tmp/utility_event_trace_log_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I foundation/event/include -I foundation/log/include -I foundation/event/tests \
		-c foundation/event/tests/test_utility_event_buffer.c \
		-o /tmp/utility_event_buffer_test_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I foundation/event/include -I foundation/log/include -I foundation/event/tests \
		-c foundation/event/tests/test_utility_event_contract.c \
		-o /tmp/utility_event_contract_test_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I foundation/event/include -I foundation/log/include -I foundation/event/tests \
		-c foundation/event/tests/test_utility_event_queue.c \
		-o /tmp/utility_event_queue_test_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I foundation/event/include -I foundation/log/include -I foundation/event/tests \
		-c foundation/event/tests/test_utility_event_dispatcher.c \
		-o /tmp/utility_event_dispatcher_test_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I foundation/event/include -I foundation/log/include -I foundation/event/tests \
		-c foundation/event/tests/test_utility_event_executor.c \
		-o /tmp/utility_event_executor_test_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I foundation/event/include -I foundation/log/include -I foundation/event/tests \
		-c foundation/event/tests/test_utility_event_metrics.c \
		-o /tmp/utility_event_metrics_test_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I foundation/event/include -I foundation/log/include -I foundation/event/tests \
		-c foundation/event/tests/test_utility_event_publisher.c \
		-o /tmp/utility_event_publisher_test_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I foundation/event/include -I foundation/log/include -I foundation/event/tests \
		-c foundation/event/tests/test_utility_event_state_machine.c \
		-o /tmp/utility_event_state_machine_test_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I foundation/event/include -I foundation/log/include -I foundation/event/tests \
		-c foundation/event/tests/test_utility_event_timer.c \
		-o /tmp/utility_event_timer_test_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I foundation/event/include -I foundation/log/include -I foundation/event/tests \
		-c foundation/event/tests/test_utility_event_trace.c \
		-o /tmp/utility_event_trace_test_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I foundation/event/include -I foundation/log/include -I foundation/event/tests \
		-c foundation/event/tests/test_utility_event_integration.c \
		-o /tmp/utility_event_integration_test_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I foundation/event/include -I foundation/log/include -I foundation/event/tests \
		-c foundation/event/tests/test_utility_event_main.c \
		-o /tmp/utility_event_main_test_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I foundation/event/include -I foundation/log/include \
		-c foundation/event/examples/state_machine_dispatch.c \
		-o /tmp/utility_event_state_machine_example_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I foundation/event/include -I foundation/log/include \
		-c foundation/event/examples/scheduled_executor.c \
		-o /tmp/utility_event_executor_example_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I foundation/event/include -I foundation/log/include \
		-c foundation/event/examples/payload_lifecycle.c \
		-o /tmp/utility_event_payload_example_analyzed.o

foundation-log-static-analysis:
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I foundation/log/include -c foundation/log/src/utility_logger.c \
		-o /tmp/utility_logger_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I foundation/log/include -c foundation/log/src/utility_log_console.c \
		-o /tmp/utility_log_console_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I foundation/log/include -I foundation/log/tests \
		-c foundation/log/tests/test_utility_logger.c \
		-o /tmp/utility_logger_test_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I foundation/log/include -I foundation/log/tests \
		-c foundation/log/tests/test_utility_log_console.c \
		-o /tmp/utility_log_console_test_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I foundation/log/include -I foundation/log/tests \
		-c foundation/log/tests/test_utility_log_main.c \
		-o /tmp/utility_log_main_test_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I foundation/log/include \
		-c foundation/log/examples/basic_default_logger.c \
		-o /tmp/utility_log_basic_example_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I foundation/log/include \
		-c foundation/log/examples/ring_maintenance.c \
		-o /tmp/utility_log_ring_example_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer -pthread \
		-I foundation/log/include \
		-c foundation/log/examples/thread_safe_logger.c \
		-o /tmp/utility_log_threads_example_analyzed.o

foundation-byte-static-analysis:
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I foundation/byte/include -c foundation/byte/src/utility_byte_reader.c \
		-o /tmp/utility_byte_reader_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I foundation/byte/include -c foundation/byte/src/utility_byte_writer.c \
		-o /tmp/utility_byte_writer_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I foundation/byte/include -I foundation/byte/tests \
		-c foundation/byte/tests/test_utility_byte.c \
		-o /tmp/utility_byte_test_analyzed.o

foundation-retry-static-analysis:
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I foundation/retry/include -c foundation/retry/src/utility_retry.c \
		-o /tmp/utility_retry_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I foundation/retry/include -I foundation/retry/tests \
		-c foundation/retry/tests/test_utility_retry.c \
		-o /tmp/utility_retry_test_analyzed.o

foundation-id-static-analysis:
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I foundation/id/include -c foundation/id/src/utility_id.c \
		-o /tmp/utility_id_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I foundation/id/include -I foundation/id/tests \
		-c foundation/id/tests/test_utility_id.c \
		-o /tmp/utility_id_test_analyzed.o

foundation-thread-pool-static-analysis:
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer -pthread \
		-I foundation/thread_pool/include \
		-c foundation/thread_pool/src/utility_thread_pool.c \
		-o /tmp/utility_thread_pool_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer -pthread \
		-I foundation/thread_pool/include -I foundation/thread_pool/tests \
		-c foundation/thread_pool/tests/test_utility_thread_pool.c \
		-o /tmp/utility_thread_pool_test_analyzed.o

foundation-time-static-analysis:
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I foundation/time/include \
		-c foundation/time/src/utility_time.c \
		-o /tmp/utility_time_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I foundation/time/include \
		-c foundation/time/src/utility_time_rfc3339.c \
		-o /tmp/utility_time_rfc3339_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I foundation/time/include \
		-c foundation/time/tests/test_utility_time.c \
		-o /tmp/utility_time_test_analyzed.o

platform-static-analysis:
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-D_POSIX_C_SOURCE=200809L \
		-I foundation/time/include -I platform/linux/include \
		-c platform/linux/src/platform_linux_time.c \
		-o /tmp/platform_linux_time_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-D_POSIX_C_SOURCE=200809L \
		-I foundation/time/include -I platform/linux/include \
		-c platform/linux/tests/test_platform_linux_time.c \
		-o /tmp/platform_linux_time_test_analyzed.o

service-static-analysis:
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I foundation/time/include -I service/time/include \
		-c service/time/src/time_service.c \
		-o /tmp/time_service_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I foundation/time/include -I service/time/include \
		-c service/time/tests/test_time_service.c \
		-o /tmp/time_service_test_analyzed.o

cppcheck:
	cppcheck --enable=warning,style,performance,portability \
		--error-exitcode=1 --std=c11 --suppress=missingIncludeSystem \
		-I memory-buffer/include memory-buffer/examples/c_usage.c \
		memory-buffer/src/memory_buffer_event_adapter.c \
		memory-buffer/tests/test_event_buffer_integration.c \
		-I foundation/event/include -I foundation/log/include \
		foundation/event/src/utility_event_buffer.c \
		foundation/event/src/utility_event_contract.c \
		foundation/event/src/utility_event_queue.c \
		foundation/event/src/utility_event_dispatcher.c \
		foundation/event/src/utility_event_executor.c \
		foundation/event/src/utility_event_metrics.c \
		foundation/event/src/utility_event_publisher.c \
		foundation/event/src/utility_event_state_machine.c \
		foundation/event/src/utility_event_timer.c \
		foundation/event/src/utility_event_trace.c \
		foundation/event/src/utility_event_trace_log.c \
		-I foundation/event/tests foundation/event/tests/test_utility_event_buffer.c \
		foundation/event/tests/test_utility_event_contract.c \
		foundation/event/tests/test_utility_event_queue.c \
		foundation/event/tests/test_utility_event_dispatcher.c \
		foundation/event/tests/test_utility_event_executor.c \
		foundation/event/tests/test_utility_event_metrics.c \
		foundation/event/tests/test_utility_event_publisher.c \
		foundation/event/tests/test_utility_event_state_machine.c \
		foundation/event/tests/test_utility_event_timer.c \
		foundation/event/tests/test_utility_event_trace.c \
		foundation/event/tests/test_utility_event_integration.c \
		foundation/event/tests/test_utility_event_main.c \
		foundation/event/examples/state_machine_dispatch.c \
		foundation/event/examples/scheduled_executor.c \
		foundation/event/examples/payload_lifecycle.c \
		-I foundation/log/include foundation/log/src/utility_logger.c \
		foundation/log/src/utility_log_console.c \
		-I foundation/log/tests foundation/log/tests/test_utility_logger.c \
		foundation/log/tests/test_utility_log_console.c \
		foundation/log/tests/test_utility_log_main.c \
		foundation/log/examples/basic_default_logger.c \
		foundation/log/examples/ring_maintenance.c \
		foundation/log/examples/thread_safe_logger.c \
		-I foundation/byte/include foundation/byte/src/utility_byte_reader.c \
		foundation/byte/src/utility_byte_writer.c \
		-I foundation/byte/tests foundation/byte/tests/test_utility_byte.c \
		-I foundation/retry/include foundation/retry/src/utility_retry.c \
		-I foundation/retry/tests foundation/retry/tests/test_utility_retry.c \
		-I foundation/id/include foundation/id/src/utility_id.c \
		-I foundation/id/tests foundation/id/tests/test_utility_id.c \
		-I foundation/thread_pool/include \
		foundation/thread_pool/src/utility_thread_pool.c \
		-I foundation/thread_pool/tests \
		foundation/thread_pool/tests/test_utility_thread_pool.c \
		-I foundation/time/include \
		foundation/time/src/utility_time.c \
		foundation/time/src/utility_time_rfc3339.c \
		foundation/time/tests/test_utility_time.c \
		-D_POSIX_C_SOURCE=200809L -I platform/linux/include \
		platform/linux/src/platform_linux_time.c \
		platform/linux/tests/test_platform_linux_time.c \
		-I service/time/include \
		service/time/src/time_service.c \
		service/time/tests/test_time_service.c \
		-I src/domain -I src/unit \
		src/domain/domain_workflow_loader.c \
		src/domain/domain_workflow.c \
		src/domain/domain_event_publisher.c \
		src/domain/domain_service.c \
		src/domain/domain_service_sample.c \
		src/domain/tests/test_domain_workflow_loader.c \
		src/unit/unit_mock.c \
		src/unit/unit_mock_sample.c

miri:
	cargo +$(NIGHTLY_TOOLCHAIN) miri test --lib

fuzz-smoke:
	ASAN_OPTIONS=detect_leaks=0 cargo +$(NIGHTLY_TOOLCHAIN) fuzz run operation_sequence \
		--fuzz-dir fuzz -- -runs=2000 -max_len=4096

foundation-fuzz-smoke: foundation-event-fuzz-smoke foundation-log-fuzz-smoke

foundation-event-fuzz-smoke:
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror \
		-fsanitize=address,undefined -fno-omit-frame-pointer \
		-I foundation/event/include -I foundation/log/include \
		foundation/event/src/utility_event_buffer.c \
		foundation/event/src/utility_event_contract.c \
		foundation/event/src/utility_event_queue.c \
		foundation/event/src/utility_event_dispatcher.c \
		foundation/event/src/utility_event_executor.c \
		foundation/event/src/utility_event_metrics.c \
		foundation/event/src/utility_event_publisher.c \
		foundation/event/src/utility_event_state_machine.c \
		foundation/event/src/utility_event_timer.c \
		foundation/event/src/utility_event_trace.c \
		foundation/event/fuzz/fuzz_event_operations.c \
		foundation/event/fuzz/fuzz_smoke_main.c \
		-o /tmp/utility_event_fuzz_smoke
	ASAN_OPTIONS=detect_leaks=0 /tmp/utility_event_fuzz_smoke

foundation-log-fuzz-smoke:
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror \
		-fsanitize=address,undefined -fno-omit-frame-pointer \
		-I foundation/log/include \
		foundation/log/src/utility_logger.c \
		foundation/log/src/utility_log_console.c \
		foundation/log/fuzz/fuzz_log_operations.c \
		foundation/log/fuzz/fuzz_smoke_main.c \
		-o /tmp/utility_log_fuzz_smoke
	ASAN_OPTIONS=detect_leaks=0 /tmp/utility_log_fuzz_smoke

foundation-fuzz:
	clang -std=c11 -Wall -Wextra -Wpedantic -Werror \
		-fsanitize=fuzzer,address,undefined \
		-I foundation/event/include -I foundation/log/include \
		foundation/event/src/utility_event_buffer.c \
		foundation/event/src/utility_event_contract.c \
		foundation/event/src/utility_event_queue.c \
		foundation/event/src/utility_event_dispatcher.c \
		foundation/event/src/utility_event_executor.c \
		foundation/event/src/utility_event_metrics.c \
		foundation/event/src/utility_event_publisher.c \
		foundation/event/src/utility_event_state_machine.c \
		foundation/event/src/utility_event_timer.c \
		foundation/event/src/utility_event_trace.c \
		foundation/event/fuzz/fuzz_event_operations.c \
		-o /tmp/utility_event_fuzz
	ASAN_OPTIONS=detect_leaks=0 /tmp/utility_event_fuzz \
		-max_len=4096 -artifact_prefix=foundation/event/fuzz/artifacts/
portable-check:
	cargo build -p memory-buffer --release --no-default-features \
		--target $(PORTABLE_TARGET)

mcu-check: portable-check

coverage:
	bash tools/coverage.sh $(REPORT_DIR)

metrics:
	python3 tools/quality.py metrics \
		--output $(REPORT_DIR)/metrics/index.html \
		memory-buffer/src/buffer.rs memory-buffer/examples/c_usage.c \
		memory-buffer/src/memory_buffer_event_adapter.c \
		foundation/event/src/utility_event_buffer.c \
		foundation/event/src/utility_event_contract.c \
		foundation/event/src/utility_event_queue.c \
		foundation/event/src/utility_event_dispatcher.c \
		foundation/event/src/utility_event_executor.c \
		foundation/event/src/utility_event_metrics.c \
		foundation/event/src/utility_event_publisher.c \
		foundation/event/src/utility_event_state_machine.c \
		foundation/event/src/utility_event_timer.c \
		foundation/event/src/utility_event_trace.c \
		foundation/event/src/utility_event_trace_log.c \
		foundation/log/src/utility_logger.c \
		foundation/log/src/utility_log_console.c \
		foundation/byte/src/utility_byte_reader.c \
		foundation/byte/src/utility_byte_writer.c \
		foundation/retry/src/utility_retry.c \
		foundation/id/src/utility_id.c \
		foundation/thread_pool/src/utility_thread_pool.c \
		foundation/time/src/utility_time.c \
		foundation/time/src/utility_time_rfc3339.c \
		platform/linux/src/platform_linux_time.c \
		service/time/src/time_service.c \
		src/domain/domain_workflow_loader.c \
		src/domain/domain_workflow.c \
		src/domain/domain_event_publisher.c \
		src/domain/domain_service.c \
		src/unit/unit_mock.c

quality: coverage metrics

quality-report: quality c-docs
	python3 tools/quality.py index \
		--output $(REPORT_DIR)/index.html \
		--unit $(REPORT_DIR)/coverage/unit/summary.json \
		--integration $(REPORT_DIR)/coverage/integration/summary.json \
		--c-unit $(REPORT_DIR)/coverage/foundation-c/unit/summary.json \
		--c-integration \
			$(REPORT_DIR)/coverage/foundation-c/integration/summary.json
	@printf '品質レポート: %s/index.html\n' "$(REPORT_DIR)"

test: foundation-test platform-test service-test domain-test
	cargo test --workspace
	cmake -S memory-buffer -B $(BUILD_DIR)
	cmake --build $(BUILD_DIR)
	ctest --test-dir $(BUILD_DIR) --output-on-failure

foundation-test: foundation-event-test foundation-log-test foundation-byte-test foundation-retry-test foundation-id-test foundation-thread-pool-test foundation-time-test

foundation-event-test:
	cmake -S foundation/event -B $(FOUNDATION_EVENT_BUILD_DIR)
	cmake --build $(FOUNDATION_EVENT_BUILD_DIR)
	ctest --test-dir $(FOUNDATION_EVENT_BUILD_DIR) --output-on-failure

foundation-log-test:
	cmake -S foundation/log -B $(FOUNDATION_LOG_BUILD_DIR)
	cmake --build $(FOUNDATION_LOG_BUILD_DIR)
	ctest --test-dir $(FOUNDATION_LOG_BUILD_DIR) --output-on-failure

foundation-byte-test:
	cmake -S foundation/byte -B $(FOUNDATION_BYTE_BUILD_DIR)
	cmake --build $(FOUNDATION_BYTE_BUILD_DIR)
	ctest --test-dir $(FOUNDATION_BYTE_BUILD_DIR) --output-on-failure

foundation-retry-test:
	cmake -S foundation/retry -B $(FOUNDATION_RETRY_BUILD_DIR)
	cmake --build $(FOUNDATION_RETRY_BUILD_DIR)
	ctest --test-dir $(FOUNDATION_RETRY_BUILD_DIR) --output-on-failure

foundation-id-test:
	cmake -S foundation/id -B $(FOUNDATION_ID_BUILD_DIR)
	cmake --build $(FOUNDATION_ID_BUILD_DIR)
	ctest --test-dir $(FOUNDATION_ID_BUILD_DIR) --output-on-failure

foundation-thread-pool-test:
	cmake -S foundation/thread_pool -B $(FOUNDATION_THREAD_POOL_BUILD_DIR)
	cmake --build $(FOUNDATION_THREAD_POOL_BUILD_DIR)
	ctest --test-dir $(FOUNDATION_THREAD_POOL_BUILD_DIR) --output-on-failure

foundation-time-test:
	cmake -S foundation/time -B $(FOUNDATION_TIME_BUILD_DIR)
	cmake --build $(FOUNDATION_TIME_BUILD_DIR)
	ctest --test-dir $(FOUNDATION_TIME_BUILD_DIR) --output-on-failure

platform-test: platform-linux-test

platform-linux-test:
	cmake -S platform/linux -B $(PLATFORM_LINUX_BUILD_DIR)
	cmake --build $(PLATFORM_LINUX_BUILD_DIR)
	ctest --test-dir $(PLATFORM_LINUX_BUILD_DIR) --output-on-failure

service-test: service-time-test

service-time-test:
	cmake -S service/time -B $(SERVICE_TIME_BUILD_DIR)
	cmake --build $(SERVICE_TIME_BUILD_DIR)
	ctest --test-dir $(SERVICE_TIME_BUILD_DIR) --output-on-failure

domain-test:
	cmake -S src/domain -B $(DOMAIN_BUILD_DIR)
	cmake --build $(DOMAIN_BUILD_DIR)
	ctest --test-dir $(DOMAIN_BUILD_DIR) --output-on-failure

foundation-event-examples:
	cmake -S foundation/event -B $(FOUNDATION_EVENT_BUILD_DIR)
	cmake --build $(FOUNDATION_EVENT_BUILD_DIR)
	$(FOUNDATION_EVENT_BUILD_DIR)/utility_event_example_state_machine
	$(FOUNDATION_EVENT_BUILD_DIR)/utility_event_example_executor
	$(FOUNDATION_EVENT_BUILD_DIR)/utility_event_example_payload

foundation-log-examples:
	cmake -S foundation/log -B $(FOUNDATION_LOG_BUILD_DIR)
	cmake --build $(FOUNDATION_LOG_BUILD_DIR)
	$(FOUNDATION_LOG_BUILD_DIR)/utility_log_example_basic
	$(FOUNDATION_LOG_BUILD_DIR)/utility_log_example_ring
	$(FOUNDATION_LOG_BUILD_DIR)/utility_log_example_threads

unit-mock-sample:
	mkdir -p build/sample
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror \
		-I src/unit -I foundation/log/include \
		src/unit/unit_mock.c src/unit/unit_mock_sample.c \
		foundation/log/src/utility_logger.c \
		foundation/log/src/utility_log_console.c \
		-pthread -o build/sample/unit_mock_sample
	build/sample/unit_mock_sample

domain-service-sample:
	mkdir -p build/sample
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror \
		-I src/domain -I src/unit \
		-I foundation/event/include -I foundation/log/include \
		src/domain/domain_event_publisher.c \
		src/domain/domain_workflow.c \
		src/domain/domain_workflow_loader.c \
		src/domain/domain_service.c \
		src/domain/domain_service_sample.c \
		src/unit/unit_mock.c \
		foundation/event/src/utility_event_publisher.c \
		foundation/event/src/utility_event_queue.c \
		foundation/event/src/utility_event_dispatcher.c \
		foundation/event/src/utility_event_state_machine.c \
		foundation/event/src/utility_event_trace.c \
		foundation/log/src/utility_logger.c \
		foundation/log/src/utility_log_console.c \
		-pthread -o build/sample/domain_service_sample
	build/sample/domain_service_sample

docs: rust-docs c-docs

rust-docs:
	cargo doc --workspace --no-deps

c-docs: c-docs-check
	mkdir -p build/docs/c-api
	$(DOXYGEN) Doxyfile

c-docs-check:
	python3 tools/check_c_docs.py \
		memory-buffer/include/*.h \
		foundation/event/include/*.h \
		foundation/event/tests/*.h \
		foundation/log/include/*.h \
		foundation/log/tests/*.h \
		foundation/byte/include/*.h \
		foundation/byte/tests/*.h \
		foundation/retry/include/*.h \
		foundation/retry/tests/*.h \
		foundation/id/include/*.h \
		foundation/id/tests/*.h \
		foundation/thread_pool/include/*.h \
		foundation/thread_pool/src/*.h \
		foundation/thread_pool/tests/*.h \
		foundation/time/include/*.h \
		platform/linux/include/*.h \
		platform/linux/src/*.h \
		service/time/include/*.h \
		src/unit/*.h \
		src/domain/*.h

docker-ready:
	@docker version >/dev/null 2>&1 || { \
		printf '%s\n' \
			'ERROR: Docker Engineへ接続できません。' \
			'WSL2の場合:' \
			'  1. WindowsでDocker Desktopを起動' \
			'  2. Settings > Resources > WSL Integrationを開く' \
			'  3. Ubuntuを有効化してApply & restart' \
			'  4. WSL shellを開き直してmake docker-readyを実行'; \
		exit 1; \
	}
	@docker compose version >/dev/null 2>&1 || { \
		printf '%s\n' \
			'ERROR: Docker Compose v2を利用できません。' \
			'`docker compose version`が成功する環境を用意してください。'; \
		exit 1; \
	}
	@printf 'Docker開発環境を利用できます。\n'

docker-build: docker-ready
	LOCAL_UID=$$(id -u) LOCAL_GID=$$(id -g) docker compose build dev

docker-shell: docker-ready
	LOCAL_UID=$$(id -u) LOCAL_GID=$$(id -g) docker compose run --rm dev bash

docker-test: docker-ready
	LOCAL_UID=$$(id -u) LOCAL_GID=$$(id -g) docker compose run --rm dev make test

docker-check: docker-ready
	LOCAL_UID=$$(id -u) LOCAL_GID=$$(id -g) docker compose run --rm dev make check

docker-extended-check: docker-ready
	LOCAL_UID=$$(id -u) LOCAL_GID=$$(id -g) docker compose run --rm dev \
		make extended-check cppcheck

clean:
	cargo clean
	cmake -E remove_directory $(BUILD_DIR)
	cmake -E remove_directory $(FOUNDATION_EVENT_BUILD_DIR)
	cmake -E remove_directory $(FOUNDATION_LOG_BUILD_DIR)
	cmake -E remove_directory $(FOUNDATION_BYTE_BUILD_DIR)
	cmake -E remove_directory $(FOUNDATION_RETRY_BUILD_DIR)
	cmake -E remove_directory $(FOUNDATION_ID_BUILD_DIR)
	cmake -E remove_directory $(FOUNDATION_THREAD_POOL_BUILD_DIR)
	cmake -E remove_directory $(FOUNDATION_TIME_BUILD_DIR)
	cmake -E remove_directory $(PLATFORM_LINUX_BUILD_DIR)
	cmake -E remove_directory $(SERVICE_TIME_BUILD_DIR)
	cmake -E remove_directory $(DOMAIN_BUILD_DIR)
