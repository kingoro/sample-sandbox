#!/usr/bin/env bash
set -euo pipefail

if command -v doxygen >/dev/null 2>&1; then
    exec doxygen "$@"
fi

local_root="${HOME}/.local/share/doxygen"
local_binary="${local_root}/usr/bin/doxygen"
local_library_path="${local_root}/usr/lib/x86_64-linux-gnu"

if [[ -x "${local_binary}" ]]; then
    export LD_LIBRARY_PATH="${local_library_path}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
    exec "${local_binary}" "$@"
fi

printf '%s\n' \
    "ERROR: doxygenが見つかりません。" \
    "Ubuntu/WSL: sudo apt-get install doxygen" \
    "Docker: make docker-check" >&2
exit 127
