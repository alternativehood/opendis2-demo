#include "stack_info_asset_plan.hpp"

#include "../assets/canonical_containers.hpp"
#include "../assets/game_data_registry.hpp"
#include "../battle_view/portrait_render_item.hpp"

#include <d2log/log.hpp>

#include <algorithm>
#include <array>

namespace d2engine {

namespace {

auto kLog = d2log::get("d2.stack_info"); // NOLINT

std::string display_name_for(const UnitInspectionModel& member) {
    if (member.instance_resolved && !member.custom_name.empty()) {
        return member.custom_name;
    }
    if (member.definition_resolved && member.definition.has_value()) {
        return member.definition->name;
    }
    return member.type_id;
}

bool has_faces_portrait(const std::string& unit_id, const PortraitManifestIndex& portraits) {
    const auto* entry = portraits.find_by_unit_type(unit_id);
    if (entry == nullptr)
        return false;
    return entry->has_face || entry->has_faceb;
}

struct VisualResolution {
    std::string              visual_unit_id;
    std::vector<std::string> candidates;
};

VisualResolution resolve_formation_visual(const GameDataRegistry&      game_data,
                                          const std::string&           concrete_type_id,
                                          const PortraitManifestIndex& portraits) {
    VisualResolution result;

    auto checker = [&](const std::string& unit_id) -> bool {
        result.candidates.push_back(unit_id);
        return has_faces_portrait(unit_id, portraits);
    };

    result.visual_unit_id = game_data.resolve_visual_type(concrete_type_id, checker);
    return result;
}

} // namespace

StackInfoAssetPlan plan_stack_info_assets(const StackInspectionModel&  model,
                                          const PortraitManifestIndex& portraits,
                                          const GameDataRegistry&      game_data) {
    StackInfoAssetPlan plan;

    //     Popup parchment background — verified Interf.ff OPT-indexed logical sprite
    plan.popup_background = {std::string(kInterfContainer), "_PG0500IX",
                             ImageAssetKind::ComposedSprite, ImagePostprocess::None};
    plan.interface_assets.push_back(plan.popup_background);

    // Verified formation slot frames from Imgs/Icons.ff
    plan.small_frame = {std::string(kIconsContainer), "BORDERUNITSMALL.PNG", ImageAssetKind::RawPng,
                        ImagePostprocess::MagentaKey};
    plan.large_frame = {std::string(kIconsContainer), "BORDERUNITLARGE.PNG", ImageAssetKind::RawPng,
                        ImagePostprocess::MagentaKey};
    plan.interface_assets.push_back(plan.small_frame);
    plan.interface_assets.push_back(plan.large_frame);

    // Leader portrait from Imgs/Events.ff via logical composed sprite
    for (const auto& member : model.members) {
        if (!member.is_leader)
            continue;

        // Find visual unit type via BASE_UNIT chain.
        // Events.ff uses the same logical sprite naming as Faces.ff normalization.
        auto leader_vis = resolve_formation_visual(game_data, member.type_id, portraits);

        kLog->info("leader_portrait_resolve leader={} concrete={} visual={} candidates=[{}]",
                   member.instance_id, member.type_id, leader_vis.visual_unit_id, [&]() {
                       std::string s;
                       for (const auto& c : leader_vis.candidates) {
                           if (!s.empty())
                               s += "->";
                           s += c;
                       }
                       return s;
                   }());

        // Events.ff logical sprite: search for a logical sprite matching the visual unit ID.
        // The naming in Events.ff follows the normalized G000UU#### pattern.
        plan.leader_portrait = {std::string(kEventsContainer), leader_vis.visual_unit_id,
                                ImageAssetKind::ComposedSprite, ImagePostprocess::MagentaKey};
        plan.interface_assets.push_back(*plan.leader_portrait);
        break;
    }

    // Formation portraits from Faces.ff, ordered by formation cell.
    // Build a per-cell index of which member occupies each formation cell.
    std::array<const UnitInspectionModel*, 6> cell_to_member = {};
    for (const auto& member : model.members) {
        if (member.formation_cells.empty())
            continue;
        const int cell = member.formation_cells[0];
        if (cell >= 0 && cell < 6) {
            cell_to_member[static_cast<std::size_t>(cell)] = &member;
        }
    }

    for (std::size_t cell = 0; cell < 6; ++cell) {
        const auto* member = cell_to_member[cell];
        if (member == nullptr)
            continue;

        PlannedPortrait pp;
        pp.formation_cell = static_cast<int>(cell);
        pp.member_instance_id = member->instance_id;
        pp.is_leader = member->is_leader;

        if (member->definition_resolved && member->definition.has_value()) {
            pp.is_large = !member->definition->size_small;
        }

        pp.display_name = display_name_for(*member);

        // Use DLG-derived layout paths
        if (pp.is_large) {
            const auto row = pp.formation_cell / 2;
            pp.layout_path = "/stack_info/formation/large_row_" + std::to_string(row);
        } else {
            pp.layout_path = "/stack_info/formation/slot_" + std::to_string(pp.formation_cell);
        }

        if (!member->type_id.empty()) {
            auto vis = resolve_formation_visual(game_data, member->type_id, portraits);
            pp.visual_unit_id = vis.visual_unit_id;

            const auto* portrait_entry = portraits.find_by_unit_type(pp.visual_unit_id);
            if (portrait_entry != nullptr) {
                if (!portrait_entry->has_face && !portrait_entry->has_faceb) {
                    kLog->warn("formation_portrait_no_variants member={} type={} visual={} "
                               "has_face=false has_faceb=false",
                               member->instance_id, member->type_id, pp.visual_unit_id);
                } else {
                    auto resolution =
                        resolve_portrait_variant(*portrait_entry, PortraitTextureKind::Face);
                    if (resolution.has_value()) {
                        const auto postprocess = (resolution->kind == PortraitTextureKind::FaceB)
                                                     ? ImagePostprocess::MagentaKey
                                                     : ImagePostprocess::None;
                        pp.key = {std::string(kFacesContainer), resolution->record_name,
                                  ImageAssetKind::RawPng, postprocess};
                        plan.interface_assets.push_back(pp.key);
                    }
                }
            } else {
                kLog->warn("formation_portrait_missing member={} type={} visual={}",
                           member->instance_id, member->type_id, pp.visual_unit_id);
            }
        }

        plan.planned_portraits.push_back(std::move(pp));
    }

    return plan;
}

} // namespace d2engine
