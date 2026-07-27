#!/usr/bin/env bash
# dead-code-check.sh — cppcheck unusedFunction analysis on whole src+tests tree
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/_require_make.sh"
require_make_target lint lint-fix
D2ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
SUPPRESSIONS="${D2ROOT}/tools/cppcheck-suppressions.txt"

if [[ ! -f "${SUPPRESSIONS}" ]]; then
    echo "ERROR: cppcheck suppressions file not found at ${SUPPRESSIONS}" >&2
    exit 1
fi

if ! command -v cppcheck &>/dev/null; then
    echo "ERROR: cppcheck not found. Install: brew install cppcheck" >&2
    exit 1
fi

echo "dead-code-check: running cppcheck unusedFunction (full project)..."
TMPOUT=$(mktemp)
trap 'rm -f "${TMPOUT}"' EXIT

ROOTS=("${D2ROOT}/src" "${D2ROOT}/tests")
if [[ -n "${OPENDIS2_DEAD_CODE_ROOTS:-}" ]]; then
    read -r -a ROOTS <<< "${OPENDIS2_DEAD_CODE_ROOTS}"
fi

INCLUDES=("-I${D2ROOT}/src" "-I${D2ROOT}/tests")
if [[ -n "${OPENDIS2_DEAD_CODE_INCLUDES:-}" ]]; then
    read -r -a EXTRA_INCLUDES <<< "${OPENDIS2_DEAD_CODE_INCLUDES}"
    INCLUDES+=("${EXTRA_INCLUDES[@]}")
fi

# Intentionally no --error-exitcode; we check output manually so set -e
# doesn't kill us on benign style/info findings.
cppcheck \
    --quiet \
    --enable=unusedFunction \
    --std=c++20 \
    -D'TEST(a,b)=void test_##a##_##b()' \
    -D'TEST_F(a,b)=void test_##a##_##b()' \
    -D'TEST_P(a,b)=void test_##a##_##b()' \
    -D'TYPED_TEST(a,b)=void test_##a##_##b()' \
    "${INCLUDES[@]}" \
    --suppressions-list="${SUPPRESSIONS}" \
    --suppress=missingIncludeSystem \
    --suppress=missingInclude \
    --suppress=unmatchedSuppression \
    --suppress=unusedFunction:tests/* \
    --inline-suppr \
    "${ROOTS[@]}" \
    >"${TMPOUT}" 2>&1

if grep -E "\[(unusedFunction|unusedPrivateFunction)\]" "${TMPOUT}" >/dev/null; then
    echo ""
    grep -E "\[(unusedFunction|unusedPrivateFunction)\]" "${TMPOUT}" | sed 's/ style: / error: /g'
    echo ""
    echo "dead-code-check: FAILED — unusedFunction found." >&2
    echo "Before suppressing anything:" >&2
    echo "  - verify call sites;" >&2
    echo "  - move test-only helpers to test support;" >&2
    echo "  - delete real dead code;" >&2
    echo "  - fix analysis scope for legitimate cross-TU usage." >&2
    exit 1
fi

echo "dead-code-check: OK (no unused functions found)"
