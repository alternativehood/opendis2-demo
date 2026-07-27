#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/_require_make.sh"
require_make_target lint lint-fix

TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT

run_dead_code() {
    local root="$1"
    MAKELEVEL=1 \
        OPENDIS2_MAKE_TARGET="${OPENDIS2_MAKE_TARGET:-lint}" \
        OPENDIS2_MAKE_GUARD="${OPENDIS2_MAKE_GUARD}" \
        OPENDIS2_DEAD_CODE_ROOTS="${root}/src ${root}/tests" \
        OPENDIS2_DEAD_CODE_INCLUDES="-I${root}/src -I${root}/tests" \
        "${SCRIPT_DIR}/dead-code-check.sh"
}

case_b="${TMP}/header_inline"
mkdir -p "${case_b}/src" "${case_b}/tests"
cat > "${case_b}/src/inline_helper.hpp" <<'CPP'
#pragma once
inline int inline_helper() {
    return 7;
}
CPP
cat > "${case_b}/tests/use_inline.cpp" <<'CPP'
#include "inline_helper.hpp"
int main() {
    return inline_helper();
}
CPP
run_dead_code "${case_b}" > "${case_b}/out.txt" 2>&1

case_c="${TMP}/zero_calls"
mkdir -p "${case_c}/src" "${case_c}/tests"
cat > "${case_c}/src/zero_calls.cpp" <<'CPP'
int declared_dead();
int declared_dead() {
    return 9;
}
CPP
if run_dead_code "${case_c}" > "${case_c}/out.txt" 2>&1; then
    echo "ERROR: dead-code selftest failed to detect declaration+definition zero-call function" >&2
    exit 1
fi
if ! grep -q "declared_dead" "${case_c}/out.txt"; then
    echo "ERROR: dead-code selftest did not report declared_dead" >&2
    cat "${case_c}/out.txt" >&2
    exit 1
fi

echo "guardrail_dead_code_checker_selftest: OK"
