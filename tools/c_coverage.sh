#!/usr/bin/env bash
set -euo pipefail

reports="${1:-build/reports}"
repo_root="$(pwd)"
output_dir="${reports}/coverage/utility-c"
build_dir="${output_dir}/build"
raw_dir="${output_dir}/raw"

rm -rf "${output_dir}"
mkdir -p "${build_dir}" "${raw_dir}"

common_flags=(
    -std=c11
    -Wall
    -Wextra
    -Wpedantic
    -Werror
    -O0
    -g
    --coverage
    -I Utility/event/include
    -I Utility/event/tests
    -I Utility/log/include
    -I Utility/log/tests
    -I Utility/byte/include
    -I Utility/byte/tests
    -I Utility/retry/include
    -I Utility/retry/tests
    -I Utility/id/include
    -I Utility/id/tests
    -I Utility/thread_pool/include
    -I Utility/thread_pool/tests
    -pthread
)

event_sources=(
    Utility/event/src/utility_event_buffer.c
    Utility/event/src/utility_event_contract.c
    Utility/event/src/utility_event_queue.c
    Utility/event/src/utility_event_dispatcher.c
    Utility/event/src/utility_event_executor.c
    Utility/event/src/utility_event_metrics.c
    Utility/event/src/utility_event_publisher.c
    Utility/event/src/utility_event_state_machine.c
    Utility/event/src/utility_event_timer.c
    Utility/event/src/utility_event_trace.c
    Utility/event/src/utility_event_trace_log.c
    Utility/log/src/utility_logger.c
    Utility/log/src/utility_log_console.c
    Utility/event/tests/test_utility_event_buffer.c
    Utility/event/tests/test_utility_event_contract.c
    Utility/event/tests/test_utility_event_queue.c
    Utility/event/tests/test_utility_event_dispatcher.c
    Utility/event/tests/test_utility_event_executor.c
    Utility/event/tests/test_utility_event_metrics.c
    Utility/event/tests/test_utility_event_publisher.c
    Utility/event/tests/test_utility_event_state_machine.c
    Utility/event/tests/test_utility_event_timer.c
    Utility/event/tests/test_utility_event_trace.c
    Utility/event/tests/test_utility_event_main.c
)

log_sources=(
    Utility/log/src/utility_logger.c
    Utility/log/src/utility_log_console.c
    Utility/log/tests/test_utility_logger.c
    Utility/log/tests/test_utility_log_console.c
    Utility/log/tests/test_utility_log_main.c
)

byte_sources=(
    Utility/byte/src/utility_byte_reader.c
    Utility/byte/src/utility_byte_writer.c
    Utility/byte/tests/test_utility_byte.c
)

retry_sources=(
    Utility/retry/src/utility_retry.c
    Utility/retry/tests/test_utility_retry.c
)

id_sources=(
    Utility/id/src/utility_id.c
    Utility/id/tests/test_utility_id.c
)

thread_pool_sources=(
    Utility/thread_pool/src/utility_thread_pool.c
    Utility/thread_pool/tests/test_utility_thread_pool.c
)

event_integration_sources=(
    Utility/event/src/utility_event_buffer.c
    Utility/event/src/utility_event_contract.c
    Utility/event/src/utility_event_queue.c
    Utility/event/src/utility_event_dispatcher.c
    Utility/event/src/utility_event_executor.c
    Utility/event/src/utility_event_metrics.c
    Utility/event/src/utility_event_publisher.c
    Utility/event/src/utility_event_state_machine.c
    Utility/event/src/utility_event_timer.c
    Utility/event/src/utility_event_trace.c
    Utility/event/src/utility_event_trace_log.c
    Utility/log/src/utility_logger.c
    Utility/log/src/utility_log_console.c
    Utility/event/tests/test_utility_event_integration.c
)

compile_sources()
{
    local prefix="$1"
    shift
    local source
    for source in "$@"; do
        local object="${build_dir}/${prefix}_$(basename "${source%.c}").o"
        "${CC:-cc}" "${common_flags[@]}" -c "${source}" -o "${object}"
        printf '%s\n' "${object}"
    done
}

mapfile -t event_objects < <(compile_sources event "${event_sources[@]}")
mapfile -t log_objects < <(compile_sources log "${log_sources[@]}")
mapfile -t byte_objects < <(compile_sources byte "${byte_sources[@]}")
mapfile -t retry_objects < <(compile_sources retry "${retry_sources[@]}")
mapfile -t id_objects < <(compile_sources id "${id_sources[@]}")
mapfile -t thread_pool_objects < <(
    compile_sources thread_pool "${thread_pool_sources[@]}"
)
mapfile -t event_integration_objects < <(
    compile_sources event_integration "${event_integration_sources[@]}"
)

"${CC:-cc}" --coverage "${event_objects[@]}" \
    -o "${build_dir}/utility_event_tests"
"${CC:-cc}" --coverage "${log_objects[@]}" \
    -o "${build_dir}/utility_log_tests"
"${CC:-cc}" --coverage "${byte_objects[@]}" \
    -o "${build_dir}/utility_byte_tests"
"${CC:-cc}" --coverage "${retry_objects[@]}" \
    -o "${build_dir}/utility_retry_tests"
"${CC:-cc}" --coverage "${id_objects[@]}" \
    -o "${build_dir}/utility_id_tests"
"${CC:-cc}" --coverage -pthread "${thread_pool_objects[@]}" \
    -o "${build_dir}/utility_thread_pool_tests"
"${CC:-cc}" --coverage "${event_integration_objects[@]}" \
    -o "${build_dir}/utility_event_integration_tests"

test_status="passed"
if ! "${build_dir}/utility_event_tests"; then
    test_status="failed"
fi
if ! "${build_dir}/utility_log_tests"; then
    test_status="failed"
fi
if ! "${build_dir}/utility_byte_tests"; then
    test_status="failed"
fi
if ! "${build_dir}/utility_retry_tests"; then
    test_status="failed"
fi
if ! "${build_dir}/utility_id_tests"; then
    test_status="failed"
fi
if ! "${build_dir}/utility_thread_pool_tests"; then
    test_status="failed"
fi
if ! "${build_dir}/utility_event_integration_tests"; then
    test_status="failed"
fi

production_sources=(
    Utility/event/src/utility_event_buffer.c
    Utility/event/src/utility_event_contract.c
    Utility/event/src/utility_event_queue.c
    Utility/event/src/utility_event_dispatcher.c
    Utility/event/src/utility_event_executor.c
    Utility/event/src/utility_event_metrics.c
    Utility/event/src/utility_event_publisher.c
    Utility/event/src/utility_event_state_machine.c
    Utility/event/src/utility_event_timer.c
    Utility/event/src/utility_event_trace.c
    Utility/event/src/utility_event_trace_log.c
    Utility/log/src/utility_logger.c
    Utility/log/src/utility_log_console.c
    Utility/byte/src/utility_byte_reader.c
    Utility/byte/src/utility_byte_writer.c
    Utility/retry/src/utility_retry.c
    Utility/id/src/utility_id.c
    Utility/thread_pool/src/utility_thread_pool.c
)

for source in "${production_sources[@]}"; do
    prefix="event"
    if [[ "${source}" == Utility/log/* ]]; then
        prefix="log"
    elif [[ "${source}" == Utility/byte/* ]]; then
        prefix="byte"
    elif [[ "${source}" == Utility/retry/* ]]; then
        prefix="retry"
    elif [[ "${source}" == Utility/id/* ]]; then
        prefix="id"
    elif [[ "${source}" == Utility/thread_pool/* ]]; then
        prefix="thread_pool"
    fi
    object="${build_dir}/${prefix}_$(basename "${source%.c}").o"
    (
        cd "${raw_dir}"
        gcov --json-format --branch-probabilities --branch-counts \
            -o "$(realpath "${repo_root}/${object}")" \
            "$(realpath "${repo_root}/${source}")"
    )
done

python3 tools/quality.py c-coverage \
    --output "${output_dir}/summary.json" \
    --html "${output_dir}/html/index.html" \
    --test-status "${test_status}" \
    "${raw_dir}"/*.gcov.json.gz

test "${test_status}" = "passed"
