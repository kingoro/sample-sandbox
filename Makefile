.PHONY: help check extended-check test docs rust-docs c-docs c-docs-check \
	header header-check static-analysis coverage metrics quality-report \
	cppcheck miri fuzz-smoke portable-check clean

BUILD_DIR ?= build/memory-buffer
REPORT_DIR ?= build/reports
PORTABLE_TARGET ?= thumbv7em-none-eabi
NIGHTLY_TOOLCHAIN ?= nightly-2026-06-06
DOXYGEN ?= bash tools/run_doxygen.sh

help:
	@printf '%s\n' \
		'Development targets:' \
		'  test             Rust testとC API結合test' \
		'  static-analysis  rustfmt、Clippy、GCC静的解析' \
		'  docs             RustdocとDoxygen仕様書を生成' \
		'  header-check     cbindgen生成headerのdrift検査' \
		'  coverage         単体・結合branch coverage検査' \
		'  metrics          CC・MIレポート生成' \
		'  portable-check   32 bit no_std targetへのcross build' \
		'  check            通常品質ゲート一式' \
		'  extended-check   checkにMiri・fuzzを追加'

check: header-check static-analysis test docs portable-check coverage metrics

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

test:
	cargo test --workspace
	cmake -S memory-buffer -B $(BUILD_DIR)
	cmake --build $(BUILD_DIR)
	ctest --test-dir $(BUILD_DIR) --output-on-failure

docs: rust-docs c-docs

rust-docs:
	cargo doc --workspace --no-deps

c-docs: c-docs-check
	mkdir -p build/docs/c-api
	$(DOXYGEN) Doxyfile

c-docs-check:
	python3 tools/check_c_docs.py memory-buffer/include/*.h

coverage:
	bash tools/coverage.sh $(REPORT_DIR)

metrics:
	python3 tools/quality.py metrics \
		--output $(REPORT_DIR)/metrics/index.html \
		memory-buffer/src/buffer.rs memory-buffer/examples/c_usage.c

quality-report: coverage metrics c-docs
	@printf '%s\n' \
		'Coverage: $(REPORT_DIR)/coverage/' \
		'Metrics:  $(REPORT_DIR)/metrics/index.html' \
		'C API:    build/docs/c-api/html/index.html'

portable-check:
	cargo build -p memory-buffer --release --no-default-features \
		--target $(PORTABLE_TARGET)

miri:
	cargo +$(NIGHTLY_TOOLCHAIN) miri test --lib

fuzz-smoke:
	ASAN_OPTIONS=detect_leaks=0 cargo +$(NIGHTLY_TOOLCHAIN) fuzz run operation_sequence \
		--fuzz-dir memory-buffer/fuzz -- -runs=2000 -max_len=4096

clean:
	cargo clean
	cmake -E remove_directory $(BUILD_DIR)
