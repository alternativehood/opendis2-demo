#pragma once

#include <d2adventure_render/adventure_render_types.hpp>
#include <d2adventure_render/terrain/landmark_asset_id.hpp>

#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>

namespace d2engine::adventure_render {

struct StaticLandmarkVisual {
    std::string container_path;
    std::string logical_sprite;

    int width = 0;
    int height = 0;

    int canvas_foot_x = 0;
    int canvas_foot_y = 0;
};

struct AnimatedLandmarkVisual {
    std::string container_path;
    std::string logical_animation;

    int canvas_foot_x = 0;
    int canvas_foot_y = 0;

    AdventureAnimationData animation_data;
};

using LandmarkVisual = std::variant<StaticLandmarkVisual, AnimatedLandmarkVisual>;

struct LandmarkAssetCatalog {
    std::unordered_map<std::string, LandmarkVisual> visuals;

    [[nodiscard]] const LandmarkVisual* find(std::string_view type_id) const {
        const auto key = canonical_landmark_type_id(type_id);
        auto       it = visuals.find(key);
        return it != visuals.end() ? &it->second : nullptr;
    }
};

} // namespace d2engine::adventure_render
