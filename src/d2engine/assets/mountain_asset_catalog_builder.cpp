#include "mountain_asset_catalog_builder.hpp"

#include "ff_asset_store.hpp"

#include <d2adventure_render/terrain/mountain_asset_catalog.hpp>

#include <cctype>
#include <cstdlib>
#include <string>

namespace d2engine {

namespace {

// Only the corpus-proven neutral mountain family is intentionally registered.
// MOMDW*, bare MOM*, and MOM_DW* are not yet sufficiently proven.
// An unsupported race or family must produce a diagnostic, not a fallback.
constexpr std::string_view kMountainPrefix = "MOMNE";

bool is_neutral_mountain_sprite(std::string_view logical_name) {
    if (!logical_name.starts_with(kMountainPrefix))
        return false;

    auto rest = logical_name.substr(kMountainPrefix.size());
    if (rest.size() != 4)
        return false;

    return std::ranges::all_of(rest, [](char c) {
        return static_cast<bool>(std::isdigit(static_cast<unsigned char>(c)));
    });
}

bool parse_mountain_sprite(std::string_view name, int& out_size_x, int& out_size_y,
                           int& out_image) {
    if (!is_neutral_mountain_sprite(name))
        return false;

    const auto rest = name.substr(kMountainPrefix.size());
    // rest is exactly 4 decimal digits: SS II
    char size_buf[3] = {};
    size_buf[0] = rest[0];
    size_buf[1] = rest[1];

    char image_buf[3] = {};
    image_buf[0] = rest[2];
    image_buf[1] = rest[3];

    const int size = std::atoi(size_buf);
    const int image = std::atoi(image_buf);

    out_size_x = size;
    out_size_y = size;
    out_image = image;
    return true;
}

} // namespace

adventure_render::MountainAssetCatalog build_mountain_asset_catalog(const FfAssetStore& store) {
    adventure_render::MountainAssetCatalog catalog;

    std::vector<std::string> logical_sprites;
    try {
        logical_sprites = store.sprites_in(catalog.container);
    } catch (const std::exception&) {
        return catalog;
    }

    for (const auto& name : logical_sprites) {
        int sx = 0;
        int sy = 0;
        int img = 0;
        if (!parse_mountain_sprite(name, sx, sy, img))
            continue;

        adventure_render::MountainAssetVisual visual;
        visual.logical_sprite = name;

        try {
            const auto meta = store.sprite_metadata(catalog.container, name);
            visual.width = meta.canvas_width;
            visual.height = meta.canvas_height;
            visual.canvas_foot_x = meta.canvas_foot_x;
            visual.canvas_foot_y = meta.canvas_foot_y;
        } catch (const std::exception&) {
            continue;
        }

        adventure_render::MountainAssetKey key;
        key.race = 4;
        key.size_x = sx;
        key.size_y = sy;
        key.image = img;

        catalog.visuals.emplace(key, std::move(visual));
    }

    return catalog;
}

} // namespace d2engine
