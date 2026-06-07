#!/usr/bin/env bash
set -euo pipefail

reports="${1:-build/reports}"
coverage_toolchain="nightly-2026-06-06"
unit_dir="${reports}/coverage/unit"
integration_dir="${reports}/coverage/integration"

rm -rf "${unit_dir}" "${integration_dir}"
mkdir -p "${unit_dir}" "${integration_dir}"

cargo +"${coverage_toolchain}" llvm-cov clean --workspace
cargo +"${coverage_toolchain}" llvm-cov --workspace --lib --branch \
    --json --summary-only --output-path "${unit_dir}/summary.json"
cargo +"${coverage_toolchain}" llvm-cov --workspace --lib --branch --no-clean \
    --html --output-dir "${unit_dir}"

cargo +"${coverage_toolchain}" llvm-cov clean --workspace
cargo +"${coverage_toolchain}" llvm-cov --workspace --test c_abi --branch \
    --json --summary-only --output-path "${integration_dir}/summary.json"
cargo +"${coverage_toolchain}" llvm-cov --workspace --test c_abi --branch --no-clean \
    --html --output-dir "${integration_dir}"

python3 tools/quality.py check-coverage \
    --unit "${unit_dir}/summary.json" \
    --integration "${integration_dir}/summary.json"

bash tools/c_coverage.sh "${reports}"
