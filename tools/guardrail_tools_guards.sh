#!/usr/bin/env bash
# guardrail_tools_guards.sh — verify every script in tools/ has a make guard.
#
# Scripts on the allowlist are intentional standalone utilities that
# do not have a corresponding Make target and may be run directly.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/_require_make.sh"
require_make_target lint lint-fix lint-changed lint-changed-fix
FAILED=0

# Scripts that are allowed to run directly (standalone utilities):
#   check_public_repo_clean.sh — CI/pre-push utility, no Make target
#   configure_vcpkg_debug.sh   — setup utility, no Make target
#   unit-info.sh               — info utility, no Make target
#   make-icns.sh               — icon generation utility, no Make target
#   check_no_raw_prints.sh     — dual-use: CTest test + lint sub-check
ALLOWLIST=(
    "check_public_repo_clean.sh"
    "configure_vcpkg_debug.sh"
    "unit-info.sh"
    "make-icns.sh"
    "check_no_raw_prints.sh"
    "_require_make.sh"
    "make_guard.py"
    "verify.sh"                     # custom guard (accepts verify + verify-integration)
)

for f in "${SCRIPT_DIR}"/*.sh "${SCRIPT_DIR}"/*.py; do
    base="$(basename "${f}")"
    [[ "${base}" == "cppcheck-suppressions.txt"* ]] && continue
    [[ "${base}" == "make_guard.py" ]] && continue
    [[ "${base}" == "_require_make.sh" ]] && continue

    # Check if on allowlist
    allowed=false
    for a in "${ALLOWLIST[@]}"; do
        if [[ "${a}" == "${base}" ]]; then
            allowed=true
            break
        fi
    done
    $allowed && continue

    # Check that the file has a require_make_target call
    if ! grep -q 'require_make_target' "${f}" 2>/dev/null; then
        echo "FAIL: ${base} is missing require_make_target guard" >&2
        FAILED=1
    fi
done

for f in "${SCRIPT_DIR}"/*.sh; do
    base="$(basename "${f}")"
    [[ "${base}" == "_require_make.sh" ]] && continue
    if [[ ! -x "${f}" ]]; then
        echo "ERROR: tools/${base} is not executable" >&2
        echo "Run:" >&2
        echo "  chmod +x tools/${base}" >&2
        FAILED=1
    fi
done

if [[ ${FAILED} -ne 0 ]]; then
    echo "guardrail_tools_guards: FAILED — some tools scripts lack make guard" >&2
    exit 1
fi

echo "guardrail_tools_guards: OK"
