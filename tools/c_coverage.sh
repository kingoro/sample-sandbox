#!/usr/bin/env bash
set -euo pipefail

reports="${1:-build/reports}"
repo_root="$(pwd)"
output_dir="${reports}/coverage/foundation-c"
unit_dir="${output_dir}/unit"
integration_dir="${output_dir}/integration"
unit_build="${unit_dir}/build"
unit_raw="${unit_dir}/raw"
integration_build="${integration_dir}/build"
integration_raw="${integration_dir}/raw"

rm -rf "${output_dir}"
mkdir -p \
    "${unit_build}" "${unit_raw}" \
    "${integration_build}" "${integration_raw}"

common_flags=(
    -std=c11
    -Wall
    -Wextra
    -Wpedantic
    -Werror
    -O0
    -g
    --coverage
    -I foundation/event/include
    -I foundation/event/tests
    -I foundation/log/include
    -I foundation/log/tests
    -I foundation/byte/include
    -I foundation/byte/tests
    -I foundation/retry/include
    -I foundation/retry/tests
    -I foundation/id/include
    -I foundation/id/tests
    -I foundation/thread_pool/include
    -I foundation/thread_pool/tests
    -I foundation/time/include
    -I platform/linux/include
    -I platform/linux/src
    -I service/time/include
    -pthread
)

event_sources=(
    foundation/event/src/utility_event_buffer.c
    foundation/event/src/utility_event_contract.c
    foundation/event/src/utility_event_queue.c
    foundation/event/src/utility_event_dispatcher.c
    foundation/event/src/utility_event_executor.c
    foundation/event/src/utility_event_metrics.c
    foundation/event/src/utility_event_publisher.c
    foundation/event/src/utility_event_state_machine.c
    foundation/event/src/utility_event_timer.c
    foundation/event/src/utility_event_trace.c
    foundation/event/src/utility_event_trace_log.c
    foundation/log/src/utility_logger.c
    foundation/log/src/utility_log_console.c
    foundation/event/tests/test_utility_event_buffer.c
    foundation/event/tests/test_utility_event_contract.c
    foundation/event/tests/test_utility_event_queue.c
    foundation/event/tests/test_utility_event_dispatcher.c
    foundation/event/tests/test_utility_event_executor.c
    foundation/event/tests/test_utility_event_metrics.c
    foundation/event/tests/test_utility_event_publisher.c
    foundation/event/tests/test_utility_event_state_machine.c
    foundation/event/tests/test_utility_event_timer.c
    foundation/event/tests/test_utility_event_trace.c
    foundation/event/tests/test_utility_event_main.c
)

log_sources=(
    foundation/log/src/utility_logger.c
    foundation/log/src/utility_log_console.c
    foundation/log/tests/test_utility_logger.c
    foundation/log/tests/test_utility_log_console.c
    foundation/log/tests/test_utility_log_main.c
)

byte_sources=(
    foundation/byte/src/utility_byte_reader.c
    foundation/byte/src/utility_byte_writer.c
    foundation/byte/tests/test_utility_byte.c
)

retry_sources=(
    foundation/retry/src/utility_retry.c
    foundation/retry/tests/test_utility_retry.c
)

id_sources=(
    foundation/id/src/utility_id.c
    foundation/id/tests/test_utility_id.c
)

thread_pool_sources=(
    foundation/thread_pool/src/utility_thread_pool.c
    foundation/thread_pool/tests/test_utility_thread_pool.c
)

time_sources=(
    foundation/time/src/utility_time.c
    foundation/time/src/utility_time_rfc3339.c
    foundation/time/tests/test_utility_time.c
)

platform_linux_sources=(
    platform/linux/src/platform_linux_time.c
    foundation/time/src/utility_time.c
    platform/linux/tests/test_platform_linux_time.c
)

service_time_sources=(
    service/time/src/time_service.c
    foundation/time/src/utility_time.c
    service/time/tests/test_time_service.c
)

event_integration_sources=(
    foundation/event/src/utility_event_contract.c
    foundation/event/src/utility_event_queue.c
    foundation/event/src/utility_event_dispatcher.c
    foundation/event/src/utility_event_executor.c
    foundation/event/src/utility_event_metrics.c
    foundation/event/src/utility_event_state_machine.c
    foundation/event/src/utility_event_timer.c
    foundation/event/src/utility_event_trace.c
    foundation/event/src/utility_event_trace_log.c
    foundation/log/src/utility_logger.c
    foundation/event/tests/test_utility_event_integration.c
)

event_integration_coverage_sources=(
    foundation/event/src/utility_event_contract.c
    foundation/event/src/utility_event_queue.c
    foundation/event/src/utility_event_dispatcher.c
    foundation/event/src/utility_event_executor.c
    foundation/event/src/utility_event_metrics.c
    foundation/event/src/utility_event_state_machine.c
    foundation/event/src/utility_event_timer.c
    foundation/event/src/utility_event_trace.c
    foundation/event/src/utility_event_trace_log.c
    foundation/log/src/utility_logger.c
)

compile_sources()
{
    local build_dir="$1"
    local prefix="$2"
    shift 2
    local source
    for source in "$@"; do
        local object="${build_dir}/${prefix}_$(basename "${source%.c}").o"
        "${CC:-cc}" "${common_flags[@]}" -c "${source}" -o "${object}"
        printf '%s\n' "${object}"
    done
}

collect_gcov()
{
    local build_dir="$1"
    local raw_dir="$2"
    local prefix="$3"
    shift 3
    local source
    for source in "$@"; do
        local object="${build_dir}/${prefix}_$(basename "${source%.c}").o"
        (
            cd "${raw_dir}"
            gcov --json-format --branch-probabilities --branch-counts \
                -o "$(realpath "${repo_root}/${object}")" \
                "$(realpath "${repo_root}/${source}")"
        )
    done
}

mapfile -t event_objects < <(
    compile_sources "${unit_build}" event "${event_sources[@]}"
)
mapfile -t log_objects < <(
    compile_sources "${unit_build}" log "${log_sources[@]}"
)
mapfile -t byte_objects < <(
    compile_sources "${unit_build}" byte "${byte_sources[@]}"
)
mapfile -t retry_objects < <(
    compile_sources "${unit_build}" retry "${retry_sources[@]}"
)
mapfile -t id_objects < <(
    compile_sources "${unit_build}" id "${id_sources[@]}"
)
mapfile -t thread_pool_objects < <(
    compile_sources "${unit_build}" thread_pool "${thread_pool_sources[@]}"
)
mapfile -t time_objects < <(
    compile_sources "${unit_build}" time "${time_sources[@]}"
)
mapfile -t platform_linux_objects < <(
    compile_sources \
        "${unit_build}" platform_linux "${platform_linux_sources[@]}"
)
mapfile -t service_time_objects < <(
    compile_sources "${unit_build}" service_time "${service_time_sources[@]}"
)

"${CC:-cc}" --coverage "${event_objects[@]}" \
    -o "${unit_build}/utility_event_tests"
"${CC:-cc}" --coverage "${log_objects[@]}" \
    -o "${unit_build}/utility_log_tests"
"${CC:-cc}" --coverage "${byte_objects[@]}" \
    -o "${unit_build}/utility_byte_tests"
"${CC:-cc}" --coverage "${retry_objects[@]}" \
    -o "${unit_build}/utility_retry_tests"
"${CC:-cc}" --coverage "${id_objects[@]}" \
    -o "${unit_build}/utility_id_tests"
"${CC:-cc}" --coverage -pthread "${thread_pool_objects[@]}" \
    -o "${unit_build}/utility_thread_pool_tests"
"${CC:-cc}" --coverage "${time_objects[@]}" \
    -o "${unit_build}/utility_time_tests"
"${CC:-cc}" --coverage "${platform_linux_objects[@]}" \
    -o "${unit_build}/platform_linux_time_tests"
"${CC:-cc}" --coverage "${service_time_objects[@]}" \
    -o "${unit_build}/time_service_tests"

unit_status="passed"
for executable in \
    utility_event_tests utility_log_tests utility_byte_tests \
    utility_retry_tests utility_id_tests utility_thread_pool_tests \
    utility_time_tests platform_linux_time_tests time_service_tests
do
    if ! "${unit_build}/${executable}"; then
        unit_status="failed"
    fi
done

collect_gcov "${unit_build}" "${unit_raw}" event \
    "${event_sources[@]:0:13}"
collect_gcov "${unit_build}" "${unit_raw}" log \
    foundation/log/src/utility_logger.c \
    foundation/log/src/utility_log_console.c
collect_gcov "${unit_build}" "${unit_raw}" byte \
    foundation/byte/src/utility_byte_reader.c \
    foundation/byte/src/utility_byte_writer.c
collect_gcov "${unit_build}" "${unit_raw}" retry \
    foundation/retry/src/utility_retry.c
collect_gcov "${unit_build}" "${unit_raw}" id \
    foundation/id/src/utility_id.c
collect_gcov "${unit_build}" "${unit_raw}" thread_pool \
    foundation/thread_pool/src/utility_thread_pool.c
collect_gcov "${unit_build}" "${unit_raw}" time \
    foundation/time/src/utility_time.c \
    foundation/time/src/utility_time_rfc3339.c
collect_gcov "${unit_build}" "${unit_raw}" platform_linux \
    platform/linux/src/platform_linux_time.c
collect_gcov "${unit_build}" "${unit_raw}" service_time \
    service/time/src/time_service.c

unit_quality_status=0
python3 tools/quality.py c-coverage \
    --kind unit \
    --output "${unit_dir}/summary.json" \
    --html "${unit_dir}/html/index.html" \
    --test-status "${unit_status}" \
    "${unit_raw}"/*.gcov.json.gz || unit_quality_status=$?

mapfile -t event_integration_objects < <(
    compile_sources \
        "${integration_build}" event_integration \
        "${event_integration_sources[@]}"
)
"${CC:-cc}" --coverage "${event_integration_objects[@]}" \
    -o "${integration_build}/utility_event_integration_tests"

integration_status="passed"
if ! "${integration_build}/utility_event_integration_tests"; then
    integration_status="failed"
fi

collect_gcov \
    "${integration_build}" "${integration_raw}" event_integration \
    "${event_integration_coverage_sources[@]}"

integration_quality_status=0
python3 tools/quality.py c-coverage \
    --kind integration \
    --output "${integration_dir}/summary.json" \
    --html "${integration_dir}/html/index.html" \
    --test-status "${integration_status}" \
    "${integration_raw}"/*.gcov.json.gz || integration_quality_status=$?

test "${unit_status}" = "passed"
test "${integration_status}" = "passed"
test "${unit_quality_status}" -eq 0
test "${integration_quality_status}" -eq 0
