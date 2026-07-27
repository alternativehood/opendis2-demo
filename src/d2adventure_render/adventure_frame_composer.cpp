#include "adventure_frame_composer.hpp"
#include "iso_depth_resolver.hpp"

#include <algorithm>
#include <span>

namespace d2engine::adventure_render {

AdventureFrameWorldStream
compose_frame_world(std::span<const PreparedAdventureRenderPrimitive> static_world,
                    std::span<const PreparedAdventureRenderPrimitive> dynamic_primitives,
                    const IsoDepthResolver&                           resolver) {
    AdventureFrameWorldStream result;

    if (dynamic_primitives.empty()) {
        result.primitives.assign(static_world.begin(), static_world.end());
        return result;
    }

    // Sort dynamic primitives by the same authoritative comparator
    std::vector<PreparedAdventureRenderPrimitive> sorted_dynamic(dynamic_primitives.begin(),
                                                                 dynamic_primitives.end());
    std::sort(sorted_dynamic.begin(), sorted_dynamic.end(),
              [&](const auto& a, const auto& b) { return resolver.comes_before(a, b); });

    // Merge static (already sorted) + sorted dynamic using the same comparator
    result.primitives.reserve(static_world.size() + sorted_dynamic.size());
    std::size_t si = 0;
    std::size_t di = 0;
    while (si < static_world.size() && di < sorted_dynamic.size()) {
        if (resolver.comes_before(static_world[si], sorted_dynamic[di])) {
            result.primitives.push_back(static_world[si]);
            ++si;
        } else {
            result.primitives.push_back(std::move(sorted_dynamic[di]));
            ++di;
        }
    }
    while (si < static_world.size())
        result.primitives.push_back(static_world[si++]);
    while (di < sorted_dynamic.size())
        result.primitives.push_back(std::move(sorted_dynamic[di++]));

    return result;
}

} // namespace d2engine::adventure_render
