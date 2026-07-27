#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace d2engine::adventure_render {

struct TreeFamilyCatalog {
    std::string              family_prefix;
    std::vector<std::string> logical_sprites;

    [[nodiscard]] bool        empty() const { return logical_sprites.empty(); }
    [[nodiscard]] std::size_t size() const { return logical_sprites.size(); }
};

struct TreeAssetCatalog {
    std::string                              container = "Imgs/IsoTerrn.ff";
    std::map<std::string, TreeFamilyCatalog> families;

    [[nodiscard]] const TreeFamilyCatalog* find_family(std::string_view prefix) const {
        std::string key(prefix);
        auto        it = families.find(key);
        return it != families.end() ? &it->second : nullptr;
    }
};

} // namespace d2engine::adventure_render
