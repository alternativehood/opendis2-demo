#pragma once

#include "stack_inspection.hpp"

#include "../assets/image_asset_key.hpp"
#include "../assets/portrait_manifest_index.hpp"

#include <string>
#include <vector>

namespace d2engine {

class GameDataRegistry;

struct PlannedPortrait {
    ImageAssetKey key;
    int           formation_cell = -1;
    bool          is_large = false;
    bool          is_leader = false;
    std::string   member_instance_id;
    std::string   display_name;
    std::string   layout_path;
    std::string   visual_unit_id;
};

struct StackInfoAssetPlan {
    ImageAssetKey                popup_background;
    std::optional<ImageAssetKey> leader_portrait;
    ImageAssetKey                small_frame; // BORDERUNITSMALL from Imgs/Icons.ff
    ImageAssetKey                large_frame; // BORDERUNITLARGE from Imgs/Icons.ff
    std::vector<ImageAssetKey>   interface_assets;
    std::vector<PlannedPortrait> planned_portraits;
};

StackInfoAssetPlan plan_stack_info_assets(const StackInspectionModel&  model,
                                          const PortraitManifestIndex& portraits,
                                          const GameDataRegistry&      game_data);

} // namespace d2engine
