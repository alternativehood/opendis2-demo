#include "road_asset_catalog_builder.hpp"

#include "ff_asset_store.hpp"

#include <d2adventure_render/terrain/road_asset_catalog.hpp>

#include <cctype>
#include <cstdlib>
#include <string>

namespace d2engine {

using adventure_render::RoadAssetCatalog;
using adventure_render::RoadAssetVisual;

int adventure_render::parse_road_logical_name(std::string_view logical_name) {
    constexpr std::string_view kPrefix = "ROAD";

    if (!logical_name.starts_with(kPrefix))
        return -1;

    const auto rest = logical_name.substr(kPrefix.size());

    // Must be exactly 4 characters: 2 digit index + "00"
    if (rest.size() != 4)
        return -1;

    if (!std::isdigit(static_cast<unsigned char>(rest[0])) ||
        !std::isdigit(static_cast<unsigned char>(rest[1])))
        return -1;

    if (rest[2] != '0' || rest[3] != '0')
        return -1;

    char buf[3] = {};
    buf[0] = rest[0];
    buf[1] = rest[1];

    const int index = std::atoi(buf);

    if (index < 0 || index > 15)
        return -1;

    return index;
}

RoadAssetCatalog build_road_asset_catalog(const FfAssetStore& store) {
    RoadAssetCatalog catalog;

    std::vector<std::string> logical_sprites;
    try {
        logical_sprites = store.sprites_in(catalog.container);
    } catch (const std::exception&) {
        throw;
    }

    for (const auto& name : logical_sprites) {
        const int index = adventure_render::parse_road_logical_name(name);
        if (index < 0)
            continue;

        RoadAssetVisual visual;
        visual.logical_sprite = name;

        try {
            const auto meta = store.sprite_metadata(catalog.container, name);
            visual.width = meta.canvas_width;
            visual.height = meta.canvas_height;
        } catch (const std::exception&) {
            throw;
        }

        catalog.visuals.emplace(index, std::move(visual));
    }

    return catalog;
}

} // namespace d2engine
