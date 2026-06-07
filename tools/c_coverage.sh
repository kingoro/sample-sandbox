#!/usr/bin/env bash
set -euo pipefail

reports="${1:-build/reports}"
repo_root="$(pwd)"
output_dir="${reports}/coverage/event-c"
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
)

sources=(
    Utility/event/src/utility_event_queue.c
    Utility/event/src/utility_event_dispatcher.c
    Utility/event/tests/test_utility_event_queue.c
    Utility/event/tests/test_utility_event_dispatcher.c
    Utility/event/tests/test_utility_event_main.c
)

objects=()
for source in "${sources[@]}"; do
    object="${build_dir}/$(basename "${source%.c}").o"
    "${CC:-cc}" "${common_flags[@]}" -c "${source}" -o "${object}"
    objects+=("${object}")
done

"${CC:-cc}" --coverage "${objects[@]}" -o "${build_dir}/utility_event_tests"

test_status="passed"
if ! "${build_dir}/utility_event_tests"; then
    test_status="failed"
fi

for source in Utility/event/src/utility_event_queue.c \
    Utility/event/src/utility_event_dispatcher.c; do
    (
        cd "${raw_dir}"
        gcov --json-format --branch-probabilities --branch-counts \
            -o "$(realpath "${repo_root}/${build_dir}")" \
            "$(realpath "${repo_root}/${source}")"
    )
done

python3 tools/quality.py c-coverage \
    --output "${output_dir}/summary.json" \
    --html "${output_dir}/html/index.html" \
    --test-status "${test_status}" \
    "${raw_dir}"/*.gcov.json.gz

test "${test_status}" = "passed"
