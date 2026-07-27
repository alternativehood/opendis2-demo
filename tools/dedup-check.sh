#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/_require_make.sh"
require_make_target lint lint-fix
D2ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
REPORT_DIR="${D2ROOT}/build/reports/dedup"

# Prefer PMD CPD (Java), fall back to jscpd (Node)
TOOL=""
if command -v pmd &>/dev/null && pmd cpd --help &>/dev/null 2>&1; then
    TOOL="pmd-cpd"
elif command -v jscpd &>/dev/null; then
    TOOL="jscpd"
else
    echo "ERROR: no dedup tool found." >&2
    echo "  Install PMD CPD: brew install pmd" >&2
    echo "  Install jscpd:   npm install -g jscpd" >&2
    exit 1
fi

mkdir -p "${REPORT_DIR}"

# Thresholds: src/ is stricter (25 lines / 80 tokens), tests/ is lenient (40 lines / 120 tokens)
SRC_MIN_LINES=25
SRC_MIN_TOKENS=80
TEST_MIN_LINES=40
TEST_MIN_TOKENS=120

echo "dedup-check: tool=${TOOL}"

run_jscpd() {
    local min_lines="${1}"
    local min_tokens="${2}"
    local path="${3}"
    local label="${4}"
    local out="${REPORT_DIR}/${label}"
    mkdir -p "${out}"
    jscpd \
        --min-lines "${min_lines}" \
        --min-tokens "${min_tokens}" \
        --format cpp \
        --reporters "consoleFull,json" \
        --output "${out}" \
        --ignore "**/build/**,**/_deps/**,**/vendor/**,**/third_party/**,**/external/**" \
        "${path}"
}

FAILED=0

echo "dedup-check: checking src/ (min ${SRC_MIN_LINES} lines / ${SRC_MIN_TOKENS} tokens)..."
run_jscpd "${SRC_MIN_LINES}" "${SRC_MIN_TOKENS}" "${D2ROOT}/src" "src" 2>&1 || FAILED=1

echo "dedup-check: checking tests/ (min ${TEST_MIN_LINES} lines / ${TEST_MIN_TOKENS} tokens)..."
run_jscpd "${TEST_MIN_LINES}" "${TEST_MIN_TOKENS}" "${D2ROOT}/tests" "tests" 2>&1 || FAILED=1

if [[ ${FAILED} -ne 0 ]]; then
    echo ""
    echo "dedup-check: FAILED — duplicates found above threshold"
    echo "  Report: ${REPORT_DIR}/"
    echo "  Fix duplicates or document exceptions in docs/dedup-policy.md or docs/lint-debt.md"
    exit 1
fi

echo "dedup-check: OK (report in ${REPORT_DIR}/)"
