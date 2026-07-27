#!/usr/bin/env bash
# check_no_raw_prints.sh — fail if any diagnostic raw prints remain in src/
# Allowed exceptions:
#   - std::snprintf (buffer building, not logging)
#   - commented-out lines
#   - std::cout in commands_list.cpp, commands_research.cpp, main.cpp (intentional CLI output)
#   - "Enter scenario name/id:" interactive prompt
#   - test files

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SRC_DIR="$SCRIPT_DIR/../src"

FOUND=$(grep -rn \
  "std::cerr\|std::fprintf\|std::printf\|\bfprintf\b\|\bputs\b\|std::puts\|\bSDL_Log\b" \
  "$SRC_DIR" \
  --include="*.cpp" --include="*.hpp" \
  | grep -v "std::snprintf\|snprintf" \
  | grep -v '^\s*//' \
  | grep -v "/tests/" \
  | grep -v "test_" \
  | grep -v "commands_list\.cpp\|commands_sg_inspect\.cpp\|headless_frontend\.cpp\|opendis2_battle/main\.cpp\|opendis2_battle/terminal_view\.cpp" \
  | grep -v "terrain_preview/main\.cpp" \
  || true)

# Also check std::cout outside of the intentional output files
FOUND_COUT=$(grep -rn "std::cout" \
  "$SRC_DIR" \
  --include="*.cpp" --include="*.hpp" \
  | grep -v "commands_list\.cpp\|commands_research\.cpp\|commands_sg_inspect\.cpp\|main\.cpp\|headless_frontend\.cpp\|opendis2_battle/main\.cpp\|opendis2_battle/terminal_view\.cpp\|sweep_runner\.cpp\|opendis2_battle_sweep/main\.cpp" \
  | grep -v "Enter scenario name/id" \
  | grep -v '^\s*//' \
  | grep -v "/tests/" \
  | grep -v "test_" \
  || true)

if [[ -n "$FOUND" || -n "$FOUND_COUT" ]]; then
  echo "ERROR: raw diagnostic prints found in src/" >&2
  [[ -n "$FOUND" ]] && echo "$FOUND" >&2
  [[ -n "$FOUND_COUT" ]] && echo "$FOUND_COUT" >&2
  exit 1
fi

echo "OK: no raw diagnostic prints found."
