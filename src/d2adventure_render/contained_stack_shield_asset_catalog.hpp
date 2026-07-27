#pragma once

#include <d2runtime/AdventureWorldState.hpp>

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace d2engine::adventure_render {

struct ContainedStackShieldAsset {
    std::string container_path;
    std::string outer_logical_name;
    std::string sprite_name;
    int         canvas_width = 0;
    int         canvas_height = 0;
    int         canvas_foot_x = 0;
    int         canvas_foot_y = 0;
};

struct ContainedStackShieldAssetCatalog {
    std::vector<ContainedStackShieldAsset> assets;

    [[nodiscard]] const ContainedStackShieldAsset&
    resolve(std::string_view race_id, d2runtime::AdventureSettlementKind settlement_kind) const;
};

} // namespace d2engine::adventure_render
