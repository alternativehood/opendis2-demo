#pragma once

#include <map>
#include <string>
#include <string_view>

namespace d2engine::adventure_render {

struct RoadAssetVisual {
    std::string logical_sprite;

    int width = 0;
    int height = 0;
};

struct RoadAssetCatalog {
    std::string container = "Imgs/IsoTerrn.ff";

    std::map<int, RoadAssetVisual> visuals;

    [[nodiscard]] const RoadAssetVisual* find(int index) const {
        auto it = visuals.find(index);
        return it != visuals.end() ? &it->second : nullptr;
    }
};

/// Parse a logical sprite name and extract a road index.
/// Accepts "ROAD%02d00" for index 0..15.
/// Returns the parsed index on success, or -1 for invalid names.
[[nodiscard]] int parse_road_logical_name(std::string_view logical_name);

} // namespace d2engine::adventure_render
