#!/usr/bin/env bash
# guardrail_wa_shoreline_destination.sh
#
# Verifies that WA shoreline operations write to source material (WA) layer,
# not to target material layer.
#
# Invariant:
#   apply_wa_shoreline() must use op.source_material_code for destination
#   layer and must NOT use op.target_material_code for destination.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/_require_make.sh"
require_make_target lint lint-fix
D2ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
FILE="$D2ROOT/src/d2adventure_render/terrain/adventure_terrain_surface.cpp"
FAILED=0

# ---- Check apply_wa_shoreline function body only ----
if grep -q '^void apply_wa_shoreline' "$FILE" 2>/dev/null; then
    # Extract the function body (from opening { to matching closing } at same indent)
    # using sed to find the function start and end by brace depth
    FUNC_START=$(grep -n '^void apply_wa_shoreline' "$FILE" | head -1 | cut -d: -f1)
    if [[ -n "$FUNC_START" ]]; then
        # Find the closing brace at column 0 after function start
        FUNC_END=$(tail -n +"$FUNC_START" "$FILE" | grep -n '^}' | head -1 | cut -d: -f1)
        FUNC_END=$((FUNC_START + FUNC_END - 1))

        # Check op.target_material_code inside the function
        if sed -n "${FUNC_START},${FUNC_END}p" "$FILE" | grep -q 'op\.target_material_code'; then
            echo "ERROR: apply_wa_shoreline must not use op.target_material_code for destination" >&2
            sed -n "${FUNC_START},${FUNC_END}p" "$FILE" | grep -n 'op\.target_material_code'
            FAILED=1
        fi

        # Verify op.source_material_code is used
        if ! sed -n "${FUNC_START},${FUNC_END}p" "$FILE" | grep -q 'op\.source_material_code'; then
            echo "ERROR: apply_wa_shoreline must use op.source_material_code for destination layer" >&2
            FAILED=1
        fi
    fi
fi

# Forbidden: WA operation applying its sprite through MaskBlend instead of ColorKeyOverlay
if grep -q 'family == "WA".*MaskBlend\|MaskBlend.*family == "WA"' "$FILE" 2>/dev/null; then
    echo "ERROR: WA shoreline operations must use ColorKeyOverlay, not MaskBlend" >&2
    FAILED=1
fi

# Forbidden: WA operation inside the WA function writing to non-WA layer
if grep -q 'op\.target_material_code.*wa_shoreline\|wa_shoreline.*op\.target_material_code' "$FILE" 2>/dev/null; then
    echo "ERROR: WA shoreline must not write to target material layer" >&2
    FAILED=1
fi

if [[ $FAILED -ne 0 ]]; then
    echo "guardrail_wa_shoreline_destination: FAILED" >&2
    exit 1
fi

echo "guardrail_wa_shoreline_destination: OK"
