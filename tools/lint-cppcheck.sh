#!/usr/bin/env bash
# lint-cppcheck.sh — cppcheck analysis on project source files.
# Fails on cppcheck error findings. Style/performance/portability/warning policy
# is handled by clangd-tidy and separate guardrails.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/_require_make.sh"
require_make_target lint lint-fix
D2ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${D2ROOT}/build/dev"
SUPPRESSIONS="${D2ROOT}/tools/cppcheck-suppressions.txt"

if [[ ! -f "${SUPPRESSIONS}" ]]; then
    echo "ERROR: cppcheck suppressions file not found at ${SUPPRESSIONS}" >&2
    exit 1
fi

if ! command -v cppcheck &>/dev/null; then
    echo "ERROR: cppcheck not found. Install: brew install cppcheck" >&2
    exit 1
fi

COMPILE_DB="${BUILD_DIR}/compile_commands.json"
if [[ ! -f "${COMPILE_DB}" ]]; then
    echo "ERROR: compile_commands.json not found at ${COMPILE_DB}" >&2
    echo "Run 'make build' first to generate the build directory." >&2
    exit 1
fi

echo "lint-cppcheck: running cppcheck error gate..."
TMPOUT=$(mktemp)
trap 'rm -f "${TMPOUT}"' EXIT

set +e
cppcheck \
    --project="${COMPILE_DB}" \
    --std=c++20 \
    --suppressions-list="${SUPPRESSIONS}" \
    --suppress=missingIncludeSystem \
    --suppress=missingInclude \
    --suppress=checkersReport \
    --suppress=unusedFunction \
    --inline-suppr \
    --suppress=*:"${BUILD_DIR}/_deps/*" \
    -i "${BUILD_DIR}/_deps" \
    -i "${D2ROOT}/tests" \
    2>&1 | tee "${TMPOUT}"
CPPCHECK_STATUS=${PIPESTATUS[0]}
set -e

if [[ ${CPPCHECK_STATUS} -ne 0 ]] || grep -E "error:" "${TMPOUT}" | grep -q .; then
    echo ""
    echo "lint-cppcheck: FAILED — cppcheck findings detected" >&2
    exit 1
fi

echo "lint-cppcheck: OK"
