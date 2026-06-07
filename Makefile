.PHONY: help check extended-check test docs header header-check static-analysis coverage metrics quality quality-report cppcheck miri fuzz-smoke mcu-check clean

BUILD_DIR ?= build/memory-buffer
REPORT_DIR ?= build/reports
MCU_TARGET ?= thumbv7em-none-eabi
NIGHTLY_TOOLCHAIN ?= nightly-2026-06-06

help:
	@printf '%s\n' \
		'Development targets:' \
		'  test             Rust testとCMake/CTest結合テスト' \
		'  static-analysis  rustfmt、Clippy、GCC -fanalyzer' \
		'  docs             Rustdoc生成' \
		'  header           cbindgen header再生成' \
		'  header-check     生成headerのdrift検査' \
		'  coverage         単体・結合branch coverage検査' \
		'  metrics          CC・MIレポート生成' \
		'  quality-report   coverage・metricsのHTML index生成' \
		'  mcu-check        MCU target向けno_std cross build' \
		'  miri             Miriによる単体テスト' \
		'  fuzz-smoke       libFuzzer短時間検査' \
		'  cppcheck         C利用例のCppcheck' \
		'  check            通常品質ゲート一式' \
		'  extended-check   checkにMiri・fuzzを追加' \
		'  clean            通常Cargo・CMake生成物を削除'

check: header-check static-analysis test docs mcu-check quality-report

extended-check: check miri fuzz-smoke

header:
	cbindgen --config memory-buffer/cbindgen.toml \
		--crate memory-buffer \
		--output memory-buffer/include/memory_buffer_generated.h

header-check:
	cbindgen --verify \
		--config memory-buffer/cbindgen.toml \
		--crate memory-buffer \
		--output memory-buffer/include/memory_buffer_generated.h

static-analysis:
	cargo fmt --all --check
	cargo clippy --workspace --all-targets -- -D warnings
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer \
		-I memory-buffer/include -c memory-buffer/examples/c_usage.c \
		-o /tmp/memory_buffer_c_usage_analyzed.o

cppcheck:
	cppcheck --enable=warning,style,performance,portability \
		--error-exitcode=1 --std=c11 --suppress=missingIncludeSystem \
		-I memory-buffer/include memory-buffer/examples/c_usage.c

miri:
	cargo +$(NIGHTLY_TOOLCHAIN) miri test --lib

fuzz-smoke:
	ASAN_OPTIONS=detect_leaks=0 cargo +$(NIGHTLY_TOOLCHAIN) fuzz run operation_sequence \
		--fuzz-dir fuzz -- -runs=2000 -max_len=4096

mcu-check:
	cargo build -p memory-buffer --release --no-default-features \
		--target $(MCU_TARGET)

coverage:
	bash tools/coverage.sh $(REPORT_DIR)

metrics:
	python3 tools/quality.py metrics \
		--output $(REPORT_DIR)/metrics/index.html \
		memory-buffer/src/buffer.rs memory-buffer/examples/c_usage.c

quality: coverage metrics

quality-report: quality
	python3 tools/quality.py index \
		--output $(REPORT_DIR)/index.html \
		--unit $(REPORT_DIR)/coverage/unit/summary.json \
		--integration $(REPORT_DIR)/coverage/integration/summary.json
	@printf '品質レポート: %s/index.html\n' "$(REPORT_DIR)"

test:
	cargo test --workspace
	cmake -S memory-buffer -B $(BUILD_DIR)
	cmake --build $(BUILD_DIR)
	ctest --test-dir $(BUILD_DIR) --output-on-failure

docs:
	cargo doc --workspace --no-deps

clean:
	cargo clean
	cmake -E remove_directory $(BUILD_DIR)
