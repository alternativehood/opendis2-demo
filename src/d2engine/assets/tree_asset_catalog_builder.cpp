#include "tree_asset_catalog_builder.hpp"

#include "ff_asset_store.hpp"

#include <d2adventure_render/terrain/tree_asset_catalog.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <string>

namespace d2engine {

namespace {

constexpr std::array<std::string_view, 6> kTreeFamilies = {"DWF", "ELF", "HEF",
                                                           "HUF", "NEF", "UNF"};

bool is_tree_sprite(std::string_view family_prefix, std::string_view logical_name) {
    if (!logical_name.starts_with(family_prefix))
        return false;
    auto rest = logical_name.substr(family_prefix.size());
    return !rest.empty() && std::ranges::all_of(rest, [](char c) {
        return static_cast<bool>(std::isdigit(static_cast<unsigned char>(c)));
    });
}

} // namespace

// cppcheck-suppress unusedFunction
adventure_render::TreeAssetCatalog build_tree_asset_catalog(const FfAssetStore& store) {
    adventure_render::TreeAssetCatalog catalog;

    std::vector<std::string> logical_sprites;
    try {
        logical_sprites = store.sprites_in("Imgs/IsoTerrn.ff");
    } catch (const std::exception&) {
        return catalog;
    }

    for (const auto& prefix : kTreeFamilies) {
        adventure_render::TreeFamilyCatalog family;
        family.family_prefix = prefix;

        for (const auto& name : logical_sprites) {
            if (is_tree_sprite(prefix, name))
                family.logical_sprites.push_back(name);
        }

        std::ranges::sort(family.logical_sprites);
        catalog.families.emplace(std::string(prefix), std::move(family));
    }

    return catalog;
}

} // namespace d2engine
