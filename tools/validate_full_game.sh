#!/usr/bin/env bash
# validate_full_game.sh — run full-game extraction validation outside CTest
#
# This is the exhaustive validation that cannot meet the 3-second CTest limit.
# Run explicitly:  make validate-full-game
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/_require_make.sh"
require_make_target validate-full-game
ROOT_DIR="$SCRIPT_DIR/.."
BUILD_DIR="$ROOT_DIR/build/dev"
TEST_BINARY="$BUILD_DIR/opendis2-dev-full-game-validation"

if [[ ! -f "$TEST_BINARY" ]]; then
    echo "ERROR: full-game validation binary not found: $TEST_BINARY" >&2
    echo "Run 'make build' first." >&2
    exit 1
fi

if [[ -z "${DISCIPLES2_GAME_ROOT:-}" ]]; then
    echo "ERROR: DISCIPLES2_GAME_ROOT is not set." >&2
    echo "Set it to your Disciples II installation directory." >&2
    exit 1
fi

if [[ ! -d "$DISCIPLES2_GAME_ROOT" ]]; then
    echo "ERROR: DISCIPLES2_GAME_ROOT directory not found: $DISCIPLES2_GAME_ROOT" >&2
    exit 1
fi

echo "=== Full-game extraction validation ==="
echo "Game root: $DISCIPLES2_GAME_ROOT"
echo ""

# Run the full-game validation binary directly (outside CTest, no timeout)
OUTPUT=$("$TEST_BINARY" --gtest_filter='ExtractAllGame.FullExtractionProducesValidManifests' \
    --gtest_print_time=1 2>&1) || {
    echo "ERROR: full-game validation test failed"
    echo "$OUTPUT"
    exit 1
}

PASSED=$(echo "$OUTPUT" | grep -cF '[  PASSED  ]' || true)
FAILED=$(echo "$OUTPUT" | grep -cF '[  FAILED  ]' || true)
TOTAL=$((PASSED + FAILED))

if [[ $TOTAL -eq 0 ]]; then
    echo "ERROR: zero tests executed — filter may be broken or test was removed"
    exit 1
fi

if [[ $FAILED -gt 0 ]]; then
    echo "ERROR: $FAILED test(s) failed"
    echo "$OUTPUT"
    exit 1
fi

echo "$OUTPUT"
echo ""
echo "=== Full-game validation complete ($PASSED passed, 0 failed) ==="
