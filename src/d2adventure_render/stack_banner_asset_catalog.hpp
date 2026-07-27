#pragma once

#include <d2adventure_render/adventure_render_types.hpp>

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace d2engine::adventure_render {

struct StackBannerAsset {
    std::string         container_path;
    std::string         record_name;
    int                 canvas_width = 0;
    int                 canvas_height = 0;
    int                 canvas_foot_x = 0;
    int                 canvas_foot_y = 0;
    CanvasContentBounds content_bounds;
};

struct StackBannerAssetCatalog {
    std::vector<StackBannerAsset> frames;

    [[nodiscard]] const StackBannerAsset& resolve_banner(int banner_index) const {
        const auto count = frames.size();
        if (banner_index < 0 || static_cast<std::size_t>(banner_index) >= count) {
            throw std::runtime_error(
                "stack_banner_index_out_of_range banner_index=" + std::to_string(banner_index) +
                " frame_count=" + std::to_string(count));
        }
        return frames[static_cast<std::size_t>(banner_index)];
    }
};

} // namespace d2engine::adventure_render
