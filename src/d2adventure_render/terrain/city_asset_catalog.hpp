#pragma once

#include <d2adventure_render/adventure_render_types.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>

namespace d2engine::adventure_render {

enum class CityVisualTier : std::uint8_t {
    Small = 0,
    Medium = 1,
    Large = 2,
};

struct AnimatedCityLayer {
    std::string            container_path;
    std::string            logical_animation;
    int                    canvas_foot_x = 0;
    int                    canvas_foot_y = 0;
    AdventureAnimationData animation;
};

struct CityVisual {
    AnimatedCityLayer                body;
    std::optional<AnimatedCityLayer> shadow;
};

struct CityAssetCatalog {
    std::array<CityVisual, 3> visuals;

    [[nodiscard]] const CityVisual& resolve_for_size(int size) const {
        switch (size) {
        case 1:
            return visuals[static_cast<std::size_t>(CityVisualTier::Small)];
        case 2:
            return visuals[static_cast<std::size_t>(CityVisualTier::Medium)];
        case 3:
        case 4:
        case 5:
            return visuals[static_cast<std::size_t>(CityVisualTier::Large)];
        default:
            throw std::runtime_error("CityAssetCatalog::resolve_for_size unsupported size=" +
                                     std::to_string(size));
        }
    }
};

} // namespace d2engine::adventure_render
