#pragma once

#include <d2runtime/AdventureSurfacePlacement.hpp>
#include <d2adventure_render/adventure_render_types.hpp>

#include <d2adventure_render/adventure_render_types.hpp>

#include <array>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>

namespace d2engine::adventure_render {

struct StaticRuinVisual {
    std::string         container_path;
    std::string         logical_sprite;
    int                 canvas_foot_x = 0;
    int                 canvas_foot_y = 0;
    int                 canvas_width = 0;
    int                 canvas_height = 0;
    CanvasContentBounds content_bounds;
};

struct AnimatedRuinVisual {
    std::string            container_path;
    std::string            logical_animation;
    int                    canvas_foot_x = 0;
    int                    canvas_foot_y = 0;
    AdventureAnimationData animation;
};

using RuinVisual = std::variant<StaticRuinVisual, AnimatedRuinVisual>;

struct RuinAssetCatalog {
    static constexpr std::size_t kImageCount = 11;

    std::array<std::array<std::optional<RuinVisual>, kImageCount>, 2> visuals;

    [[nodiscard]] const RuinVisual* find(int                                  image,
                                         d2runtime::AdventureSurfacePlacement placement) const {
        if (image < 0 || image >= static_cast<int>(kImageCount)) {
            return nullptr;
        }
        const auto  placement_index = placement == d2runtime::AdventureSurfacePlacement::Water
                                          ? std::size_t{1}
                                          : std::size_t{0};
        const auto& visual = visuals[placement_index][static_cast<std::size_t>(image)];
        return visual.has_value() ? &*visual : nullptr;
    }

    [[nodiscard]] const RuinVisual& resolve(int                                  image,
                                            d2runtime::AdventureSurfacePlacement placement) const {
        const auto* visual = find(image, placement);
        if (visual == nullptr) {
            throw std::runtime_error("RuinAssetCatalog::resolve unsupported image=" +
                                     std::to_string(image));
        }
        return *visual;
    }
};

} // namespace d2engine::adventure_render
