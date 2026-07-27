#pragma once

// ── IsoDepthResolver ────────────────────────────────────────────────────
//
// Orders world-phase primitives by spatial isometric depth.
//
// Priority (highest first, i.e., drawn last / on top):
//   1. IsoDepth (increasing = closer to camera)
//   2. WorldRenderLevel — only for the EXACT SAME spatial anchor
//   3. Deterministic spatial fallback — for same scalar depth but different anchors
//   4. StableRenderId — deterministic tie-break
//
// Key invariant:
//   ISO DEPTH has priority over WorldRenderLevel.
//   A primitive that is spatially in front always draws after one behind,
//   regardless of their WorldRenderLevel values.
//
// WorldRenderLevel is LOCAL:
//   It applies only when two primitives share the exact same IsoDepthAnchor.
//   It does NOT order an entire isometric diagonal.
//   Forest@(10,0) and Unit@(0,10) both have IsoDepth=10 but different anchors;
//   WorldRenderLevel does NOT apply — spatial fallback does.
//
// comes_before() is the single authoritative comparator.
// Both resolve() and external frame composers MUST use it.

#include "adventure_render_types.hpp"
#include "map_geometry.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace d2engine::adventure_render {

class IsoDepthResolver {
public:
    explicit IsoDepthResolver(const AdventureMapGeometry& geometry) : geometry_(&geometry) {}

    [[nodiscard]] bool comes_before(const PreparedAdventureRenderPrimitive& a,
                                    const PreparedAdventureRenderPrimitive& b) const;

    [[nodiscard]] std::vector<std::size_t>
    resolve(std::span<const PreparedAdventureRenderPrimitive> primitives) const;

    [[nodiscard]] static bool stable_ordering(const PreparedAdventureRenderPrimitive& a,
                                              const PreparedAdventureRenderPrimitive& b);

private:
    const AdventureMapGeometry* geometry_;
};

} // namespace d2engine::adventure_render
