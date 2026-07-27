#!/usr/bin/env bash
set -u
# No -e: we track failures manually across parallel+sections.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/_require_make.sh"
require_make_target lint lint-fix
FAILED=0

run_bg() {
    local label="$1"
    shift
    echo "=== lint: ${label} ==="
    "$@"
}

run_heavy() {
    local label="$1"
    shift
    echo "=== lint: ${label} ==="
    "$@" &
}

echo ""
echo "═══ lint: bootstrap guardrails ═══"
echo ""

run_bg "guardrail: tools guards" "${SCRIPT_DIR}/guardrail_tools_guards.sh" || (( FAILED++ ))
if [[ ${FAILED} -ne 0 ]]; then
    echo ""
    echo "=== lint: ${FAILED} FAILURE(S) ===" >&2
    exit 1
fi

echo ""
echo "═══ lint: heavy checks (parallel) ═══"
echo ""

pids=()
run_heavy "clangd-tidy"        "${SCRIPT_DIR}/lint-tidy-check.sh"
pids+=($!)
run_heavy "format-check"       "${SCRIPT_DIR}/lint-format-check.sh"
pids+=($!)
run_heavy "cppcheck"           "${SCRIPT_DIR}/lint-cppcheck.sh"
pids+=($!)
run_heavy "dead-code"          "${SCRIPT_DIR}/dead-code-check.sh"
pids+=($!)

for pid in "${pids[@]}"; do
    wait "${pid}" || (( FAILED++ ))
done

echo ""
echo "═══ lint: dedup & guardrails (sequential) ═══"
echo ""

run_bg "dedup"                               "${SCRIPT_DIR}/dedup-check.sh"                                  || (( FAILED++ ))
run_bg "guardrail: clangd-tidy policy"        "${SCRIPT_DIR}/guardrail_clangd_tidy_policy.sh"                 || (( FAILED++ ))
run_bg "guardrail: dead-code checker selftest" "${SCRIPT_DIR}/guardrail_dead_code_checker_selftest.sh"        || (( FAILED++ ))
run_bg "guardrail: cppcheck suppressions"     "${SCRIPT_DIR}/guardrail_cppcheck_suppressions.sh"               || (( FAILED++ ))
run_bg "guardrail: WA shoreline destination"  "${SCRIPT_DIR}/guardrail_wa_shoreline_destination.sh"            || (( FAILED++ ))
run_bg "guardrail: no fake full-map terrain"  "${SCRIPT_DIR}/guardrail_no_fake_full_map_terrain_renderer.sh"   || (( FAILED++ ))
run_bg "guardrail: no parallel terrain"       "${SCRIPT_DIR}/guardrail_no_parallel_terrain_renderer.sh"        || (( FAILED++ ))
run_bg "guardrail: terrain missing logging"   "${SCRIPT_DIR}/guardrail_terrain_missing_asset_logging.sh"       || (( FAILED++ ))
run_bg "guardrail: filesystem paths"          "${SCRIPT_DIR}/guardrail_filesystem_paths.sh"                    || (( FAILED++ ))
run_bg "battle-architecture-boundaries"       python3 "${SCRIPT_DIR}/check_battle_architecture_boundaries.py"  || (( FAILED++ ))
run_bg "guardrail: no d2scenario in gamedata" python3 "${SCRIPT_DIR}/../tests/tools/guardrail_no_d2scenario_in_gamedata.py" src/d2gamedata || (( FAILED++ ))
run_bg "guardrail: CLI no DBF scanning"       python3 "${SCRIPT_DIR}/../tests/tools/guardrail_cli_no_dbf_scanning.py" src/cli       || (( FAILED++ ))
run_bg "guardrail: executable version"         python3 "${SCRIPT_DIR}/../tests/tools/guardrail_executable_version.py"               || (( FAILED++ ))
run_bg "no-raw-prints"                        "${SCRIPT_DIR}/check_no_raw_prints.sh"                          || (( FAILED++ ))
run_bg "guardrail: CTest timeout"             "${SCRIPT_DIR}/guardrail_ctest_timeout.sh"                       || (( FAILED++ ))
run_bg "guardrail: UI layout"                  "${SCRIPT_DIR}/guardrail_ui_layout.sh"                            || (( FAILED++ ))
run_bg "guardrail: UI layout selftest"         "${SCRIPT_DIR}/guardrail_ui_layout_selftest.sh"                  || (( FAILED++ ))
run_bg "guardrail: stack semantics"            python3 "${SCRIPT_DIR}/guardrail_stack_semantics.py"               || (( FAILED++ ))
run_bg "guardrail: stack semantics selftest"   python3 "${SCRIPT_DIR}/guardrail_stack_semantics_selftest.py"      || (( FAILED++ ))

echo ""
if [[ ${FAILED} -eq 0 ]]; then
    echo "=== lint: ALL PASSED ==="
else
    echo "=== lint: ${FAILED} FAILURE(S) ===" >&2
    exit 1
fi
