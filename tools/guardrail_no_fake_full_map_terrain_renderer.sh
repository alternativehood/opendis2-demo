#!/usr/bin/env bash
# guardrail_no_fake_full_map_terrain_renderer.sh
#
# Forbids reintroduction of old per-tile / fake full-map terrain renderer patterns
# that were removed during the transition to correct native-rectangular Ground.ff
# tiling with deterministic variant selection.
#
# Checks both:
#   src/d2adventure_render/terrain/adventure_terrain_surface.cpp  — terrain renderer internals
#   src/opendis2_terrain_preview/terrain_preview_image.cpp  — production Assets entrypoint

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/_require_make.sh"
require_make_target lint lint-fix
D2ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
FAILED=0

check_forbidden() {
    local file="$1"
    local pattern="$2"
    local label="$3"
    if [[ ! -f "$file" ]]; then
        return 0
    fi
    local matches
    matches=$(grep -n "$pattern" "$file" 2>/dev/null || true)
    if [[ -n "$matches" ]]; then
        echo "ERROR [${label}]: forbidden pattern '${pattern}' found in ${file}" >&2
        echo "$matches" >&2
        FAILED=1
    fi
}

SRC_FILE="$D2ROOT/src/d2adventure_render/terrain/adventure_terrain_surface.cpp"
PREVIEW_FILE="$D2ROOT/src/opendis2_terrain_preview/terrain_preview_image.cpp"

# ── Checks on adventure_terrain_surface.cpp (d2adventure_render/terrain/) ────

# 1a. Old two-pass draw_assets in terrain renderer
check_forbidden "$SRC_FILE" 'draw_assets(true, false)' 'old_assets_two_pass'
check_forbidden "$SRC_FILE" 'draw_assets(false, true)' 'old_assets_two_pass'

# 2a. Old wallpaper Ground fill: sample_x = px + state.min_world_x style
check_forbidden "$SRC_FILE" 'sample_x =.*state\.min_world_x' 'old_wallpaper_ground'
check_forbidden "$SRC_FILE" 'sample_y =.*state\.min_world_y' 'old_wallpaper_ground'

# 3a. Diamond-stamped Ground RGB field inside any ground build function
#     (diamond.pixel_coords used in build_coverage_maps is legitimate)
if grep -q 'state\.diamond\.pixel_coords' "$SRC_FILE" 2>/dev/null; then
    for func in build_ground_patches prepare_full_map; do
        FUNC_LINE=$(grep -n "^void ${func}" "$SRC_FILE" 2>/dev/null | head -1 | cut -d: -f1 || true)
        if [[ -n "$FUNC_LINE" ]]; then
            END_LINE=$(tail -n +"$FUNC_LINE" "$SRC_FILE" | grep -n '^}' | head -1 | cut -d: -f1 || true)
            if [[ -n "$END_LINE" ]]; then
                END_LINE=$((FUNC_LINE + END_LINE - 1))
                if sed -n "${FUNC_LINE},${END_LINE}p" "$SRC_FILE" 2>/dev/null | grep -q 'state\.diamond\.pixel_coords'; then
                    echo "ERROR [old_diamond_stamp_ground]: state.diamond.pixel_coords must not appear in ${func}" >&2
                    FAILED=1
                fi
            fi
        fi
    done
fi
if grep -q 'terrain_surface_diamond_contains' "$SRC_FILE" 2>/dev/null; then
    for func in build_ground_patches prepare_full_map; do
        FUNC_LINE=$(grep -n "^void ${func}" "$SRC_FILE" 2>/dev/null | head -1 | cut -d: -f1 || true)
        if [[ -n "$FUNC_LINE" ]]; then
            END_LINE=$(tail -n +"$FUNC_LINE" "$SRC_FILE" | grep -n '^}' | head -1 | cut -d: -f1 || true)
            if [[ -n "$END_LINE" ]]; then
                END_LINE=$((FUNC_LINE + END_LINE - 1))
                if sed -n "${FUNC_LINE},${END_LINE}p" "$SRC_FILE" 2>/dev/null | grep -q 'terrain_surface_diamond_contains'; then
                    echo "ERROR: terrain_surface_diamond_contains must not appear in ${func}" >&2
                    FAILED=1
                fi
            fi
        fi
    done
fi

# 4a. Ground source coordinates tied to dpx/dpy in ground build functions
check_forbidden "$SRC_FILE" 'dpx.*dpy.*ground\|ground.*dpx.*dpy' 'old_dpx_dpy_ground'

# 5a. Old WA destination bug: op.target_material_code inside apply_wa_shoreline
if grep -q '^void apply_wa_shoreline' "$SRC_FILE" 2>/dev/null; then
    FUNC_START=$(grep -n '^void apply_wa_shoreline' "$SRC_FILE" | head -1 | cut -d: -f1 || true)
    if [[ -n "$FUNC_START" ]]; then
        FUNC_END=$(tail -n +"$FUNC_START" "$SRC_FILE" | grep -n '^}' | head -1 | cut -d: -f1 || true)
        if [[ -n "$FUNC_END" ]]; then
            FUNC_END=$((FUNC_START + FUNC_END - 1))
            if sed -n "${FUNC_START},${FUNC_END}p" "$SRC_FILE" 2>/dev/null | grep -q 'op\.target_material_code'; then
                echo "ERROR [old_wa_target_bug]: apply_wa_shoreline must not use op.target_material_code" >&2
                FAILED=1
            fi
        fi
    fi
fi

# 6a. Production full-map path must exist and must not use per-tile fake full-map
check_forbidden "$SRC_FILE" '^AdventureTerrainSurface.*render_full_map.*input.*options' 'old_per_tile_render_signature'

# ── Checks on terrain_preview_image.cpp (production Assets entrypoint) ──────

# 1b. Old two-pass draw_assets in preview entrypoint
check_forbidden "$PREVIEW_FILE" 'draw_assets(true, false)' 'preview_old_assets_two_pass'
check_forbidden "$PREVIEW_FILE" 'draw_assets(false, true)' 'preview_old_assets_two_pass'

# 2b. compose_tile inside PreviewMode::Assets path (must use render_full_map)
#     compose_tile is allowed only when mode != Assets (per-tile diagnostic modes)
if [[ -f "$PREVIEW_FILE" ]] && grep -q 'PreviewMode::Assets' "$PREVIEW_FILE" 2>/dev/null; then
    if grep -n 'compose_tile' "$PREVIEW_FILE" 2>/dev/null | grep -q 'Assets'; then
        echo "ERROR [preview_compose_tile_assets]: compose_tile must not be called in PreviewMode::Assets path; use render_full_map" >&2
        FAILED=1
    fi
fi

# 3b. owner_id / owner_distance used for Assets terrain composition
check_forbidden "$PREVIEW_FILE" 'owner_id.*Assets\|Assets.*owner_id' 'preview_owner_id_assets'
check_forbidden "$PREVIEW_FILE" 'owner_distance.*Assets\|Assets.*owner_distance' 'preview_owner_distance_assets'

# 4b. render_full_map must be called in Assets path
if [[ -f "$PREVIEW_FILE" ]] && ! grep -q 'render_full_map' "$PREVIEW_FILE" 2>/dev/null; then
    echo "ERROR: render_full_map must be called in terrain_preview_image.cpp for Assets mode" >&2
    FAILED=1
fi

# 5b. prepare_full_map / render_prepared_full_map must exist in production renderer
if ! grep -q '^PreparedAdventureTerrainMap AdventureTerrainSurfaceComposer::prepare_full_map' "$SRC_FILE" 2>/dev/null; then
    echo "ERROR: prepare_full_map must exist as production full-map preparation entrypoint" >&2
    FAILED=1
fi
if ! grep -q '^AdventureTerrainSurface AdventureTerrainSurfaceComposer::render_prepared_full_map' "$SRC_FILE" 2>/dev/null; then
    echo "ERROR: render_prepared_full_map must exist as production full-map render entrypoint" >&2
    FAILED=1
fi

if [[ $FAILED -ne 0 ]]; then
    echo "guardrail_no_fake_full_map_terrain_renderer: FAILED" >&2
    exit 1
fi

echo "guardrail_no_fake_full_map_terrain_renderer: OK"
