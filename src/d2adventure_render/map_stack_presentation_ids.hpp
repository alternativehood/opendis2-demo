#pragma once

#include "adventure_render_types.hpp"

#include <string>
#include <string_view>

namespace d2engine::adventure_render {

struct MapStackPresentationRenderIds {
    StableRenderId body;
    StableRenderId shadow;
    StableRenderId banner;
};

[[nodiscard]] inline MapStackPresentationRenderIds
map_stack_presentation_render_ids(std::string_view stack_id) {
    return {stable_render_id("Stack:" + std::string(stack_id)),
            stable_render_id("StackShadow:" + std::string(stack_id)),
            stable_render_id("StackBanner:" + std::string(stack_id))};
}

} // namespace d2engine::adventure_render
