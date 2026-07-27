#include "runtime_asset_context.hpp"

#include <algorithm>
#include <stdexcept>

namespace d2engine {

RuntimeAssetContext::RuntimeAssetContext(const std::filesystem::path& asset_root) {
    try {
        database_ =
            std::make_unique<d2asset::AssetDatabase>(d2asset::AssetDatabase::open(asset_root));
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to load asset package: ") + e.what());
    }

    if (database_->atlas_sheets().empty()) {
        throw std::runtime_error("Asset package contains no atlases");
    }
    const auto& assets = database_->manifest().assets();
    const bool  has_animations = std::ranges::any_of(assets, [](const d2asset::AssetRecord& r) {
        return r.type == d2asset::AssetType::Animation;
    });
    if (!has_animations) {
        throw std::runtime_error("Asset package contains no animations");
    }
}

} // namespace d2engine
