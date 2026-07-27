#!/usr/bin/env bash
# guardrail_ctest_timeout.sh — verify hard 3-second CTest timeout enforcement
#
# Fails if any test registration path bypasses TIMEOUT 3.
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/_require_make.sh"
require_make_target lint lint-fix
ROOT_DIR="$SCRIPT_DIR/.."
ERRORS=0

check_gtest_discover() {
    local file="$1"
    local label="${2:-}"

    # Every gtest_discover_tests must have PROPERTIES TIMEOUT 3.
    while IFS= read -r line; do
        local target
        target=$(echo "$line" | sed -n 's/.*gtest_discover_tests(\([^ )]*\).*/\1/p')
        if [[ -z "$target" ]]; then
            continue
        fi

        # Check that TIMEOUT 3 appears in the properties block
        local props_block
        props_block=$(sed -n "/gtest_discover_tests($target/,/^)/p" "$file")
        if ! echo "$props_block" | grep -q "TIMEOUT 3"; then
            echo "FAIL: gtest_discover_tests($target) in $file lacks TIMEOUT 3"
            ERRORS=$((ERRORS + 1))
        fi
    done < <(grep -n "gtest_discover_tests(" "$file" || true)
}

check_add_test_timeout() {
    local file="$1"

    while IFS= read -r line; do
        local test_name
        test_name=$(echo "$line" | sed -n 's/.*add_test(NAME \([^ )]*\).*/\1/p')
        if [[ -z "$test_name" ]]; then
            continue
        fi

        # Check that set_tests_properties for this test includes TIMEOUT 3
        # We look in a window after add_test for matching set_tests_properties
        local test_line
        test_line=$(echo "$line" | cut -d: -f1)
        local block
        block=$(sed -n "${test_line},+20p" "$file")

        if ! echo "$block" | grep -q "TIMEOUT 3"; then
            echo "FAIL: add_test($test_name) in $file lacks TIMEOUT 3"
            ERRORS=$((ERRORS + 1))
        fi
    done < <(grep -n "add_test(NAME " "$file" || true)
}

check_presets() {
    local file="$1"
    if jq -e '.testPresets[] | select(.execution.timeout != null)' "$file" > /dev/null 2>&1; then
        echo "FAIL: $file has test presets with global timeout (must not override per-test TIMEOUT 3)"
        ERRORS=$((ERRORS + 1))
    fi
}

check_makefile() {
    local file="$1"
    # Makefile must not bypass timeout with direct ctest --timeout
    if grep -q "ctest --timeout" "$file" 2>/dev/null; then
        echo "FAIL: $file uses ctest --timeout (must not override per-test TIMEOUT 3)"
        ERRORS=$((ERRORS + 1))
    fi
    # Makefile must not use --test-timeout
    if grep -q "test-timeout" "$file" 2>/dev/null; then
        echo "FAIL: $file uses --test-timeout (must not override per-test TIMEOUT 3)"
        ERRORS=$((ERRORS + 1))
    fi
    # Makefile ctest invocations must not use --timeout
    if grep -q "ctest.*--timeout" "$file" 2>/dev/null; then
        echo "FAIL: $file ctest invocation bypasses PER_TEST_TIMEOUT guard"
        ERRORS=$((ERRORS + 1))
    fi
}

check_scripts() {
    local dir="$1"
    for script in "$dir"/CI*.sh "$dir"/ci*.sh; do
        [[ -f "$script" ]] || continue
        if grep -q "ctest.*--timeout\|ctest.*--test-timeout" "$script" 2>/dev/null; then
            echo "FAIL: $script overrides per-test timeout with global flag"
            ERRORS=$((ERRORS + 1))
        fi
    done
}

# ── Check CMakeLists.txt ──
check_gtest_discover "$ROOT_DIR/CMakeLists.txt"
check_add_test_timeout "$ROOT_DIR/CMakeLists.txt"

# ── Check presets ──
check_presets "$ROOT_DIR/CMakePresets.json"

# ── Check Makefile ──
check_makefile "$ROOT_DIR/Makefile"

# ── Check CI/test scripts ──
check_scripts "$ROOT_DIR/tools"

if [[ $ERRORS -gt 0 ]]; then
    echo "FAIL: $ERRORS timeout guardrail violation(s) found"
    exit 1
fi

echo "OK: all CTest entries have TIMEOUT 3 enforcement"
