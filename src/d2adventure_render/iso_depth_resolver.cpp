#include "iso_depth_resolver.hpp"

#include <algorithm>
#include <cstddef>

namespace d2engine::adventure_render {

bool IsoDepthResolver::stable_ordering(const PreparedAdventureRenderPrimitive& a,
                                       const PreparedAdventureRenderPrimitive& b) {
    return a.stable_id < b.stable_id;
}

bool IsoDepthResolver::comes_before(const PreparedAdventureRenderPrimitive& a,
                                    const PreparedAdventureRenderPrimitive& b) const {
    const int depth_a = geometry_->iso_depth(a.depth_anchor);
    const int depth_b = geometry_->iso_depth(b.depth_anchor);
    if (depth_a != depth_b)
        return depth_a < depth_b;

    const bool same_anchor = a.depth_anchor == b.depth_anchor;

    if (same_anchor) {
        if (a.level != b.level)
            return world_level_before(a.level, b.level);
        if (a.local_suborder != b.local_suborder)
            return a.local_suborder < b.local_suborder;
    } else {
        if (a.depth_anchor.x != b.depth_anchor.x)
            return a.depth_anchor.x < b.depth_anchor.x;
        if (a.depth_anchor.y != b.depth_anchor.y)
            return a.depth_anchor.y < b.depth_anchor.y;
    }

    return stable_ordering(a, b);
}

std::vector<std::size_t>
IsoDepthResolver::resolve(std::span<const PreparedAdventureRenderPrimitive> primitives) const {
    const auto               n = primitives.size();
    std::vector<std::size_t> indices(n);
    for (std::size_t i = 0; i < n; ++i)
        indices[i] = i;

    std::sort(indices.begin(), indices.end(), [&](std::size_t ai, std::size_t bi) {
        return comes_before(primitives[ai], primitives[bi]);
    });

    return indices;
}

} // namespace d2engine::adventure_render
