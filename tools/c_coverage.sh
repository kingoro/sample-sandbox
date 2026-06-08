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
)

event_sources=(
    Utility/event/src/utility_event_queue.c
    Utility/event/src/utility_event_dispatcher.c
    Utility/event/src/utility_event_state_machine.c
    Utility/event/src/utility_event_trace.c
    Utility/event/src/utility_event_trace_log.c
    Utility/log/src/utility_logger.c
    Utility/log/src/utility_log_console.c
    Utility/event/tests/test_utility_event_queue.c
    Utility/event/tests/test_utility_event_dispatcher.c
    Utility/event/tests/test_utility_event_state_machine.c
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

event_integration_sources=(
    Utility/event/src/utility_event_queue.c
    Utility/event/src/utility_event_dispatcher.c
    Utility/event/src/utility_event_state_machine.c
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
mapfile -t event_integration_objects < <(
    compile_sources event_integration "${event_integration_sources[@]}"
)

"${CC:-cc}" --coverage "${event_objects[@]}" \
    -o "${build_dir}/utility_event_tests"
"${CC:-cc}" --coverage "${log_objects[@]}" \
    -o "${build_dir}/utility_log_tests"
"${CC:-cc}" --coverage "${event_integration_objects[@]}" \
    -o "${build_dir}/utility_event_integration_tests"

test_status="passed"
if ! "${build_dir}/utility_event_tests"; then
    test_status="failed"
fi
if ! "${build_dir}/utility_log_tests"; then
    test_status="failed"
fi
if ! "${build_dir}/utility_event_integration_tests"; then
    test_status="failed"
fi

production_sources=(
    Utility/event/src/utility_event_queue.c
    Utility/event/src/utility_event_dispatcher.c
    Utility/event/src/utility_event_state_machine.c
    Utility/event/src/utility_event_trace.c
    Utility/event/src/utility_event_trace_log.c
    Utility/log/src/utility_logger.c
    Utility/log/src/utility_log_console.c
)

for source in "${production_sources[@]}"; do
    prefix="event"
    if [[ "${source}" == Utility/log/* ]]; then
        prefix="log"
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
