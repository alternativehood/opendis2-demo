#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/_require_make.sh"
require_make_target lint lint-fix
D2ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

patterns=(
  "adventure_terrain_map_compositor"
  "build_terrain_render_plan"
  "prepare_terrain_layers"
  "compose_prepared_terrain_layers"
  "PreparedTerrainLayers"
  "PreparedMaterialMap"
  "TerrainRenderPlan"
)

failed=0

for pattern in "${patterns[@]}"; do
  if grep -RIn --exclude-dir=.git --exclude-dir=build --exclude-dir=cmake-build-* \
      --exclude='guardrail_no_parallel_terrain_renderer.sh' \
      "$pattern" "$D2ROOT/src" "$D2ROOT/tests" "$D2ROOT/CMakeLists.txt" 2>/dev/null; then
    echo "ERROR: forbidden parallel terrain renderer symbol found: $pattern" >&2
    failed=1
  fi
done

if [[ $failed -ne 0 ]]; then
  echo "guardrail_no_parallel_terrain_renderer: FAILED — remove the parallel renderer branch" >&2
  exit 1
fi

echo "guardrail_no_parallel_terrain_renderer: OK"
