#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>

#include <d2runtime/AdventureTreasure.hpp>

namespace d2engine::adventure_render {

struct StaticTreasureVisual {
    std::string container_path;
    std::string logical_sprite;
    int         canvas_foot_x = 0;
    int         canvas_foot_y = 0;
    int         canvas_width = 0;
    int         canvas_height = 0;
};

struct TreasureAssetCatalog {
    std::array<std::optional<StaticTreasureVisual>, 8> land_visuals;
    std::array<std::optional<StaticTreasureVisual>, 4> water_visuals;

    [[nodiscard]] const StaticTreasureVisual* find(d2runtime::AdventureTreasurePlacement placement,
                                                   int image) const {
        if (image < 0) {
            return nullptr;
        }
        switch (placement) {
        case d2runtime::AdventureTreasurePlacement::Land:
            if (image >= static_cast<int>(land_visuals.size())) {
                return nullptr;
            }
            if (!land_visuals[static_cast<std::size_t>(image)].has_value()) {
                return nullptr;
            }
            return &*land_visuals[static_cast<std::size_t>(image)];
        case d2runtime::AdventureTreasurePlacement::Water:
            if (image >= static_cast<int>(water_visuals.size())) {
                return nullptr;
            }
            if (!water_visuals[static_cast<std::size_t>(image)].has_value()) {
                return nullptr;
            }
            return &*water_visuals[static_cast<std::size_t>(image)];
        }
        return nullptr;
    }

    [[nodiscard]] const StaticTreasureVisual&
    resolve(d2runtime::AdventureTreasurePlacement placement, int image) const {
        const auto* visual = find(placement, image);
        if (visual == nullptr) {
            throw std::runtime_error("TreasureAssetCatalog::resolve unsupported placement/image=" +
                                     std::to_string(static_cast<int>(placement)) + "/" +
                                     std::to_string(image));
        }
        return *visual;
    }
};

} // namespace d2engine::adventure_render
