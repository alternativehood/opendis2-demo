#include "treasure_asset_catalog_builder.hpp"

#include "ff_asset_store.hpp"

#include <array>
#include <stdexcept>
#include <string>
#include <utility>

namespace d2engine {

adventure_render::TreasureAssetCatalog build_treasure_asset_catalog(const FfAssetStore& store) {
    adventure_render::TreasureAssetCatalog catalog;

    struct TreasureEntry {
        int         image;
        const char* logical_sprite;
    };

    constexpr std::array<TreasureEntry, 8> kLandEntries = {{{0, "G000BG0000100"},
                                                            {1, "G000BG0000101"},
                                                            {2, "G000BG0000102"},
                                                            {3, "G000BG0000103"},
                                                            {4, "G000BG0000104"},
                                                            {5, "G000BG0000105"},
                                                            {6, "G000BG0000106"},
                                                            {7, "G000BG0000107"}}};
    constexpr std::array<TreasureEntry, 4> kWaterEntries = {
        {{0, "G000BG0000000"}, {1, "G000BG0000001"}, {2, "G000BG0000002"}, {3, "G000BG0000003"}}};

    auto add_visual = [&](auto& visuals, const TreasureEntry& entry) {
        const auto full_path = std::string("Imgs/IsoCmon.ff/") + entry.logical_sprite;

        d2engine::FfAssetStore::SpriteMetadata meta{};
        try {
            meta = store.sprite_metadata("Imgs/IsoCmon.ff", entry.logical_sprite);
        } catch (const std::exception& e) {
            throw std::runtime_error("build_treasure_asset_catalog: failed to resolve " +
                                     full_path + ": " + e.what());
        }
        if (meta.canvas_width <= 0 || meta.canvas_height <= 0 || meta.canvas_foot_x < 0 ||
            meta.canvas_foot_y < 0) {
            throw std::runtime_error("build_treasure_asset_catalog: invalid metadata for " +
                                     full_path + " canvas=(" + std::to_string(meta.canvas_width) +
                                     "x" + std::to_string(meta.canvas_height) + ") foot=(" +
                                     std::to_string(meta.canvas_foot_x) + "," +
                                     std::to_string(meta.canvas_foot_y) + ")");
        }

        adventure_render::StaticTreasureVisual visual;
        visual.container_path = "Imgs/IsoCmon.ff";
        visual.logical_sprite = entry.logical_sprite;
        visual.canvas_foot_x = meta.canvas_foot_x;
        visual.canvas_foot_y = meta.canvas_foot_y;
        visual.canvas_width = meta.canvas_width;
        visual.canvas_height = meta.canvas_height;
        visuals[static_cast<std::size_t>(entry.image)] = std::move(visual);
    };

    for (const auto& entry : kLandEntries) {
        add_visual(catalog.land_visuals, entry);
    }
    for (const auto& entry : kWaterEntries) {
        add_visual(catalog.water_visuals, entry);
    }

    return catalog;
}

} // namespace d2engine
