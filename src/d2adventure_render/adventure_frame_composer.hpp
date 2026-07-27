#pragma once

#include <d2adventure_render/adventure_render_types.hpp>

#include <span>
#include <vector>

namespace d2engine::adventure_render {

class IsoDepthResolver;

// Ordered sequence of world primitives (static + dynamic merged).
// All primitives use the same IsoDepthResolver::comes_before ordering.
struct AdventureFrameWorldStream {
    std::vector<PreparedAdventureRenderPrimitive> primitives;

    [[nodiscard]] bool        empty() const { return primitives.empty(); }
    [[nodiscard]] std::size_t size() const { return primitives.size(); }
};

// Composes static prepared world + transient dynamic primitives into
// one correctly-ordered frame world stream.
[[nodiscard]] AdventureFrameWorldStream
compose_frame_world(std::span<const PreparedAdventureRenderPrimitive> static_world,
                    std::span<const PreparedAdventureRenderPrimitive> dynamic_primitives,
                    const IsoDepthResolver&                           resolver);

} // namespace d2engine::adventure_render
