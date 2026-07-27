#pragma once

#include <compare>
#include <map>
#include <string>
#include <string_view>

namespace d2engine::adventure_render {

struct MountainAssetKey {
    int race = 0;
    int size_x = 0;
    int size_y = 0;
    int image = 0;

    auto operator<=>(const MountainAssetKey&) const = default;
};

struct MountainAssetVisual {
    std::string logical_sprite;

    int width = 0;
    int height = 0;

    int canvas_foot_x = 0;
    int canvas_foot_y = 0;
};

struct MountainAssetCatalog {
    std::string container = "Imgs/IsoTerrn.ff";

    std::map<MountainAssetKey, MountainAssetVisual> visuals;

    [[nodiscard]] const MountainAssetVisual* find(int race, int size_x, int size_y,
                                                  int image) const {
        MountainAssetKey key{race, size_x, size_y, image};
        auto             it = visuals.find(key);
        return it != visuals.end() ? &it->second : nullptr;
    }
};

} // namespace d2engine::adventure_render
