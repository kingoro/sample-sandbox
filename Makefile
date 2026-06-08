.PHONY: help check extended-check test utility-test utility-event-test utility-log-test docs rust-docs c-docs c-docs-check header header-check static-analysis utility-static-analysis utility-log-static-analysis coverage metrics quality quality-report cppcheck miri fuzz-smoke utility-fuzz-smoke utility-event-fuzz-smoke utility-log-fuzz-smoke utility-fuzz portable-check mcu-check docker-ready docker-build docker-shell docker-test docker-check docker-extended-check clean

BUILD_DIR ?= build/memory-buffer
UTILITY_BUILD_DIR ?= build/utility-event
UTILITY_LOG_BUILD_DIR ?= build/utility-log
REPORT_DIR ?= build/reports
PORTABLE_TARGET ?= thumbv7em-none-eabi
NIGHTLY_TOOLCHAIN ?= nightly-2026-06-06
DOXYGEN ?= bash tools/run_doxygen.sh

help:
	@printf '%s\n' \
		'Development targets:' \
		'  test             Rust/C結合テストと全C Utility単体テスト' \
		'  utility-test     全C Utilityの単体テスト' \
		'  utility-event-test Event UtilityのC単体テスト' \
		'  utility-log-test Log UtilityのC単体テスト' \
		'  static-analysis  Rust、C利用例、全C Utilityの静的解析' \
		'  utility-static-analysis 全C UtilityのGCC静的解析' \
		'  utility-log-static-analysis Log UtilityのGCC静的解析' \
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
		'  utility-fuzz-smoke 全C Utility操作列をASan/UBSanで検査' \
		'  utility-log-fuzz-smoke Log Utility操作列をASan/UBSanで検査' \
		'  utility-fuzz     Event UtilityをClang libFuzzerで継続探索' \
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

extended-check: check miri fuzz-smoke utility-fuzz-smoke

header:
	cbindgen --config memory-buffer/cbindgen.toml \
		--crate memory-buffer \
		--output memory-buffer/include/memory_buffer_generated.h

header-check:
	cbindgen --verify \
		--config memory-buffer/cbindgen.toml \
		--crate memory-buffer \
		--output memory-buffer/include/memory_buffer_generated.h

static-analysis: utility-static-analysis
	cargo fmt --all --check
	cargo clippy --workspace --all-targets -- -D warnings
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I memory-buffer/include -c memory-buffer/examples/c_usage.c \
		-o /tmp/memory_buffer_c_usage_analyzed.o

utility-static-analysis: utility-log-static-analysis
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I Utility/event/include -c Utility/event/src/utility_event_queue.c \
		-o /tmp/utility_event_queue_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I Utility/event/include -c Utility/event/src/utility_event_dispatcher.c \
		-o /tmp/utility_event_dispatcher_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I Utility/event/include -c Utility/event/src/utility_event_state_machine.c \
		-o /tmp/utility_event_state_machine_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I Utility/event/include -c Utility/event/src/utility_event_timer.c \
		-o /tmp/utility_event_timer_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I Utility/event/include -c Utility/event/src/utility_event_trace.c \
		-o /tmp/utility_event_trace_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I Utility/event/include -I Utility/log/include \
		-c Utility/event/src/utility_event_trace_log.c \
		-o /tmp/utility_event_trace_log_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I Utility/event/include -I Utility/log/include -I Utility/event/tests \
		-c Utility/event/tests/test_utility_event_queue.c \
		-o /tmp/utility_event_queue_test_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I Utility/event/include -I Utility/log/include -I Utility/event/tests \
		-c Utility/event/tests/test_utility_event_dispatcher.c \
		-o /tmp/utility_event_dispatcher_test_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I Utility/event/include -I Utility/log/include -I Utility/event/tests \
		-c Utility/event/tests/test_utility_event_state_machine.c \
		-o /tmp/utility_event_state_machine_test_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I Utility/event/include -I Utility/log/include -I Utility/event/tests \
		-c Utility/event/tests/test_utility_event_timer.c \
		-o /tmp/utility_event_timer_test_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I Utility/event/include -I Utility/log/include -I Utility/event/tests \
		-c Utility/event/tests/test_utility_event_trace.c \
		-o /tmp/utility_event_trace_test_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I Utility/event/include -I Utility/log/include -I Utility/event/tests \
		-c Utility/event/tests/test_utility_event_integration.c \
		-o /tmp/utility_event_integration_test_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I Utility/event/include -I Utility/log/include -I Utility/event/tests \
		-c Utility/event/tests/test_utility_event_main.c \
		-o /tmp/utility_event_main_test_analyzed.o

utility-log-static-analysis:
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I Utility/log/include -c Utility/log/src/utility_logger.c \
		-o /tmp/utility_logger_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I Utility/log/include -c Utility/log/src/utility_log_console.c \
		-o /tmp/utility_log_console_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I Utility/log/include -I Utility/log/tests \
		-c Utility/log/tests/test_utility_logger.c \
		-o /tmp/utility_logger_test_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I Utility/log/include -I Utility/log/tests \
		-c Utility/log/tests/test_utility_log_console.c \
		-o /tmp/utility_log_console_test_analyzed.o
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I Utility/log/include -I Utility/log/tests \
		-c Utility/log/tests/test_utility_log_main.c \
		-o /tmp/utility_log_main_test_analyzed.o

cppcheck:
	cppcheck --enable=warning,style,performance,portability \
		--error-exitcode=1 --std=c11 --suppress=missingIncludeSystem \
		-I memory-buffer/include memory-buffer/examples/c_usage.c \
		-I Utility/event/include -I Utility/log/include \
		Utility/event/src/utility_event_queue.c \
		Utility/event/src/utility_event_dispatcher.c \
		Utility/event/src/utility_event_state_machine.c \
		Utility/event/src/utility_event_timer.c \
		Utility/event/src/utility_event_trace.c \
		Utility/event/src/utility_event_trace_log.c \
		-I Utility/event/tests Utility/event/tests/test_utility_event_queue.c \
		Utility/event/tests/test_utility_event_dispatcher.c \
		Utility/event/tests/test_utility_event_state_machine.c \
		Utility/event/tests/test_utility_event_timer.c \
		Utility/event/tests/test_utility_event_trace.c \
		Utility/event/tests/test_utility_event_integration.c \
		Utility/event/tests/test_utility_event_main.c \
		-I Utility/log/include Utility/log/src/utility_logger.c \
		Utility/log/src/utility_log_console.c \
		-I Utility/log/tests Utility/log/tests/test_utility_logger.c \
		Utility/log/tests/test_utility_log_console.c \
		Utility/log/tests/test_utility_log_main.c

miri:
	cargo +$(NIGHTLY_TOOLCHAIN) miri test --lib

fuzz-smoke:
	ASAN_OPTIONS=detect_leaks=0 cargo +$(NIGHTLY_TOOLCHAIN) fuzz run operation_sequence \
		--fuzz-dir fuzz -- -runs=2000 -max_len=4096

utility-fuzz-smoke: utility-event-fuzz-smoke utility-log-fuzz-smoke

utility-event-fuzz-smoke:
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror \
		-fsanitize=address,undefined -fno-omit-frame-pointer \
		-I Utility/event/include -I Utility/log/include \
		Utility/event/src/utility_event_queue.c \
		Utility/event/src/utility_event_dispatcher.c \
		Utility/event/src/utility_event_state_machine.c \
		Utility/event/src/utility_event_timer.c \
		Utility/event/src/utility_event_trace.c \
		Utility/event/fuzz/fuzz_event_operations.c \
		Utility/event/fuzz/fuzz_smoke_main.c \
		-o /tmp/utility_event_fuzz_smoke
	ASAN_OPTIONS=detect_leaks=0 /tmp/utility_event_fuzz_smoke

utility-log-fuzz-smoke:
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror \
		-fsanitize=address,undefined -fno-omit-frame-pointer \
		-I Utility/log/include \
		Utility/log/src/utility_logger.c \
		Utility/log/src/utility_log_console.c \
		Utility/log/fuzz/fuzz_log_operations.c \
		Utility/log/fuzz/fuzz_smoke_main.c \
		-o /tmp/utility_log_fuzz_smoke
	ASAN_OPTIONS=detect_leaks=0 /tmp/utility_log_fuzz_smoke

utility-fuzz:
	clang -std=c11 -Wall -Wextra -Wpedantic -Werror \
		-fsanitize=fuzzer,address,undefined \
		-I Utility/event/include -I Utility/log/include \
		Utility/event/src/utility_event_queue.c \
		Utility/event/src/utility_event_dispatcher.c \
		Utility/event/src/utility_event_state_machine.c \
		Utility/event/src/utility_event_timer.c \
		Utility/event/src/utility_event_trace.c \
		Utility/event/fuzz/fuzz_event_operations.c \
		-o /tmp/utility_event_fuzz
	ASAN_OPTIONS=detect_leaks=0 /tmp/utility_event_fuzz \
		-max_len=4096 -artifact_prefix=Utility/event/fuzz/artifacts/
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
		Utility/event/src/utility_event_queue.c \
		Utility/event/src/utility_event_dispatcher.c \
		Utility/event/src/utility_event_state_machine.c \
		Utility/event/src/utility_event_timer.c \
		Utility/event/src/utility_event_trace.c \
		Utility/event/src/utility_event_trace_log.c \
		Utility/log/src/utility_logger.c \
		Utility/log/src/utility_log_console.c

quality: coverage metrics

quality-report: quality c-docs
	python3 tools/quality.py index \
		--output $(REPORT_DIR)/index.html \
		--unit $(REPORT_DIR)/coverage/unit/summary.json \
		--integration $(REPORT_DIR)/coverage/integration/summary.json \
		--c-coverage $(REPORT_DIR)/coverage/utility-c/summary.json
	@printf '品質レポート: %s/index.html\n' "$(REPORT_DIR)"

test: utility-test
	cargo test --workspace
	cmake -S memory-buffer -B $(BUILD_DIR)
	cmake --build $(BUILD_DIR)
	ctest --test-dir $(BUILD_DIR) --output-on-failure

utility-test: utility-event-test utility-log-test

utility-event-test:
	cmake -S Utility/event -B $(UTILITY_BUILD_DIR)
	cmake --build $(UTILITY_BUILD_DIR)
	ctest --test-dir $(UTILITY_BUILD_DIR) --output-on-failure

utility-log-test:
	cmake -S Utility/log -B $(UTILITY_LOG_BUILD_DIR)
	cmake --build $(UTILITY_LOG_BUILD_DIR)
	ctest --test-dir $(UTILITY_LOG_BUILD_DIR) --output-on-failure

docs: rust-docs c-docs

rust-docs:
	cargo doc --workspace --no-deps

c-docs: c-docs-check
	mkdir -p build/docs/c-api
	$(DOXYGEN) Doxyfile

c-docs-check:
	python3 tools/check_c_docs.py \
		memory-buffer/include/*.h \
		Utility/event/include/*.h \
		Utility/event/tests/*.h \
		Utility/log/include/*.h \
		Utility/log/tests/*.h

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
	cmake -E remove_directory $(UTILITY_BUILD_DIR)
	cmake -E remove_directory $(UTILITY_LOG_BUILD_DIR)
