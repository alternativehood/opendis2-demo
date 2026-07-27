#!/usr/bin/env bash
# guardrail_terrain_missing_asset_logging.sh
#
# Verifies that terrain border asset preparation always calls the common logger
# when encountering a missing exact border record, and never silently falls back
# to a different variant or skips without logging.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/_require_make.sh"
require_make_target lint lint-fix
D2ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
FILE="$D2ROOT/src/d2adventure_render/terrain/adventure_terrain_surface.cpp"
FAILED=0

if [[ ! -f "$FILE" ]]; then
    echo "ERROR: $FILE not found" >&2
    exit 1
fi

# 1. prepare_border_assets exists
if ! grep -q '^void prepare_border_assets' "$FILE" 2>/dev/null; then
    echo "ERROR: prepare_border_assets function not found in $FILE" >&2
    exit 1
fi

# 2. MissingTerrainAssetLogState is defined and used somewhere in the file
if ! grep -q 'MissingTerrainAssetLogState' "$FILE" 2>/dev/null; then
    echo "ERROR: MissingTerrainAssetLogState must be used in terrain renderer" >&2
    FAILED=1
fi

# 3. prepare_border_assets creates MissingTerrainAssetLogState locally (one per preparation)
PREPARE_START=$(grep -n '^void prepare_border_assets' "$FILE" 2>/dev/null | head -1 | cut -d: -f1 || true)
if [[ -n "$PREPARE_START" ]]; then
    PREPARE_END=$(tail -n +"$PREPARE_START" "$FILE" | grep -n '^}' | head -1 | cut -d: -f1 || true)
    if [[ -n "$PREPARE_END" ]]; then
        PREPARE_END=$((PREPARE_START + PREPARE_END - 1))
        PREPARE_BODY=$(sed -n "${PREPARE_START},${PREPARE_END}p" "$FILE" 2>/dev/null || true)

        if ! echo "$PREPARE_BODY" | grep -q 'MissingTerrainAssetLogState'; then
            echo "ERROR: prepare_border_assets must create MissingTerrainAssetLogState locally for deduplicated logging" >&2
            FAILED=1
        fi
    fi
fi

# 4. log_missing_terrain_border_asset_once is called in the file
if ! grep -q 'log_missing_terrain_border_asset_once' "$FILE" 2>/dev/null; then
    echo "ERROR: log_missing_terrain_border_asset_once must be called in terrain renderer" >&2
    FAILED=1
fi

# 5. log_missing_terrain_border_asset_once uses D2_LOG_WARN
if ! grep -A30 '^void log_missing_terrain_border_asset_once' "$FILE" 2>/dev/null | grep -q 'D2_LOG_WARN'; then
    echo "ERROR: log_missing_terrain_border_asset_once must use D2_LOG_WARN" >&2
    FAILED=1
fi

# 6. prepare_border_assets does NOT contain fallback variant selection
if [[ -n "$PREPARE_START" ]] && [[ -n "$PREPARE_END" ]]; then
    PREPARE_BODY=$(sed -n "${PREPARE_START},${PREPARE_END}p" "$FILE" 2>/dev/null || true)

    if echo "$PREPARE_BODY" | grep -qi 'fallback\|actual_record\|nearest.*variant\|use_variant'; then
        echo "ERROR: prepare_border_assets must not contain fallback variant selection (use_variant/actual_record/fallback)" >&2
        FAILED=1
    fi

    # Count missing-record checks vs log_missing calls — each missing asset path needs a log call
    MISSING_CHECKS=$(echo "$PREPARE_BODY" | grep -c 'buf == nullptr\|buf->rgba.empty()' 2>/dev/null || true)
    LOG_CALLS=$(echo "$PREPARE_BODY" | grep -c 'log_missing_terrain_border_asset_once' 2>/dev/null || true)
    if [[ "$MISSING_CHECKS" -gt "$LOG_CALLS" ]]; then
        echo "ERROR: prepare_border_assets has ${MISSING_CHECKS} missing-record check(s) but only ${LOG_CALLS} log_missing_terrain_border_asset_once call(s)" >&2
        FAILED=1
    fi
fi

# 7. No raw prints for missing terrain assets (snprintf/sprintf are not raw prints)
if grep -E '\b(std::cout|std::cerr|\bprintf\b|\bfprintf\b)' "$FILE" 2>/dev/null | grep -qv 'snprintf\|sprintf'; then
    if grep -B2 -A2 -E '\b(std::cout|std::cerr|\bprintf\b|\bfprintf\b)' "$FILE" 2>/dev/null | grep -iq 'border\|terrain.*asset\|missing.*asset'; then
        echo "ERROR: raw print (cout/cerr/printf) must not be used for missing terrain asset logging; use D2_LOG_WARN" >&2
        FAILED=1
    fi
fi

# 8. Production full-map preparation must pass through prepare_border_assets
PREPARE_FULL_START=$(grep -n '^PreparedAdventureTerrainMap AdventureTerrainSurfaceComposer::prepare_full_map' "$FILE" 2>/dev/null | head -1 | cut -d: -f1 || true)
if [[ -n "$PREPARE_FULL_START" ]]; then
    PREPARE_FULL_END=$(tail -n +"$PREPARE_FULL_START" "$FILE" | grep -n '^}' | head -1 | cut -d: -f1 || true)
    if [[ -n "$PREPARE_FULL_END" ]]; then
        PREPARE_FULL_END=$((PREPARE_FULL_START + PREPARE_FULL_END - 1))
        if ! sed -n "${PREPARE_FULL_START},${PREPARE_FULL_END}p" "$FILE" 2>/dev/null | grep -q 'prepare_border_assets'; then
            echo "ERROR: prepare_full_map must call prepare_border_assets for production full-map preparation" >&2
            FAILED=1
        fi
    fi
fi

if [[ $FAILED -ne 0 ]]; then
    echo "guardrail_terrain_missing_asset_logging: FAILED" >&2
    exit 1
fi

echo "guardrail_terrain_missing_asset_logging: OK"
