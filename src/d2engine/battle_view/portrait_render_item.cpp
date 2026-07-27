#include "portrait_render_item.hpp"

#include "../app/battle_tuning_state.hpp"
#include "../render/game_texture_cache.hpp"

#include <d2log/log.hpp>

#include <unordered_set>

namespace d2engine {

namespace {
auto kLog = d2log::get("d2.render"); // NOLINT(cert-err58-cpp)
} // namespace

std::optional<PortraitResolution> resolve_portrait_variant(const UnitPortraitEntry& entry,
                                                           PortraitTextureKind preferred_kind) {
    if (preferred_kind == PortraitTextureKind::FaceB) {
        if (entry.has_faceb) {
            return PortraitResolution{.record_name = entry.faceb_record_name,
                                      .kind = PortraitTextureKind::FaceB};
        }
        if (entry.has_face) {
            return PortraitResolution{.record_name = entry.face_record_name,
                                      .kind = PortraitTextureKind::Face};
        }
    } else {
        if (entry.has_face) {
            return PortraitResolution{.record_name = entry.face_record_name,
                                      .kind = PortraitTextureKind::Face};
        }
        if (entry.has_faceb) {
            return PortraitResolution{.record_name = entry.faceb_record_name,
                                      .kind = PortraitTextureKind::FaceB};
        }
    }
    return std::nullopt;
}

std::optional<PortraitRenderItem> build_portrait_render_item(
    const std::string& unit_type, const PortraitManifestIndex& portrait_index,
    GameTextureCache& texture_cache, const std::string& tree_path, const Rect& dest_rect,
    bool flip_x, const std::string& side, PortraitTextureKind preferred_kind) {
    const auto* entry = portrait_index.find_by_unit_type(unit_type);
    if (entry == nullptr) {
        static std::unordered_set<std::string> logged;
        if (logged.emplace(unit_type).second) {
            kLog->warn("portrait_no_manifest_entry unit_type={} tree_path={}", unit_type,
                       tree_path);
        }
        return std::nullopt;
    }

    const auto variant = resolve_portrait_variant(*entry, preferred_kind);
    if (!variant.has_value()) {
        static std::unordered_set<std::string> logged;
        if (logged.emplace(unit_type).second) {
            kLog->warn("portrait_no_face unit_type={} resource_id={} FACE={} FACEB={} tree_path={}",
                       unit_type, entry->resource_unit_id, entry->face_record_name,
                       entry->faceb_record_name, tree_path);
        }
        return std::nullopt;
    }

    const bool   apply_magenta = (variant->kind == PortraitTextureKind::FaceB);
    SDL_Texture* texture =
        texture_cache.get_raw("Imgs/Faces.ff", variant->record_name, apply_magenta);
    if (texture == nullptr) {
        static std::unordered_set<std::string> logged;
        if (logged.emplace(unit_type).second) {
            kLog->warn("portrait_texture_load_failed unit_type={} record={} kind={} tree_path={}",
                       unit_type, variant->record_name, apply_magenta ? "FACEB" : "FACE",
                       tree_path);
        }
        return std::nullopt;
    }

    const bool         is_faceb = apply_magenta;
    PortraitRenderItem item;
    item.texture = texture;
    item.dest_rect = dest_rect;
    item.tree_path = tree_path;
    item.stable_id = tree_path;
    item.layer = 100;
    item.flip_x = flip_x;
    item.flip_y = false;
    item.visual_category = is_faceb ? "PortraitFACEB" : "PortraitFACE";
    item.side = side;
    item.has_texture = true;
    return item;
}

PortraitRenderItem build_portrait_placeholder_item(const std::string& tree_path,
                                                   const Rect& dest_rect, bool flip_x,
                                                   const std::string& side) {
    PortraitRenderItem item;
    item.texture = nullptr;
    item.dest_rect = dest_rect;
    item.tree_path = tree_path;
    item.stable_id = tree_path;
    item.layer = 100;
    item.flip_x = flip_x;
    item.flip_y = false;
    item.visual_category = "PortraitPlaceholder";
    item.side = side;
    item.has_texture = false;
    return item;
}

PortraitRenderItem build_dead_mask_item(bool is_large, GameTextureCache& texture_cache,
                                        const std::string& tree_path, const Rect& dest_rect,
                                        bool flip_x, const std::string& side) {
    const char*  mask_name = is_large ? kDeadMaskLargeName : kDeadMaskSmallName;
    SDL_Texture* texture = texture_cache.get_raw(kDeadMaskContainer, mask_name, false);

    PortraitRenderItem item;
    item.dest_rect = dest_rect;
    item.tree_path = tree_path;
    item.stable_id = tree_path;
    item.layer = 101; // above portrait (layer=100)
    item.flip_x = flip_x;
    item.flip_y = false;
    item.visual_category = is_large ? "PortraitDeadMaskLarge" : "PortraitDeadMaskSmall";
    item.side = side;

    if (texture != nullptr) {
        item.texture = texture;
        item.has_texture = true;
    } else {
        static std::unordered_set<std::string> logged;
        if (logged.emplace(mask_name).second) {
            kLog->warn("dead_portrait_mask_missing is_large={} mask={} container={}", is_large,
                       mask_name, kDeadMaskContainer);
        }
        item.has_texture = false;
    }
    return item;
}

DebugRenderableItem PortraitRenderItem::to_tunable_item() const {
    DebugRenderableItem item;
    item.stable_id = stable_id;
    item.label = tree_path;
    item.tree_path = tree_path;
    item.bounds = dest_rect;
    item.kind = "portrait";
    item.layer = layer;
    item.logical_rect = dest_rect;
    item.screen_rect = dest_rect;
    item.anchor = {.x = dest_rect.x, .y = dest_rect.y};
    item.selectable = true;
    item.visible = true;
    item.render_side = side;
    item.visual_category = visual_category;

    ConfigBinding binding;
    binding.config_file = kBattleScreenConfigPath;
    binding.owner_kind = BindingOwnerKind::TreeLayout;
    binding.tree_path = tree_path;
    binding.role = BindingRole::CombatFrame;
    binding.display_path = std::string("render_tree") + tree_path;
    item.binding = to_tuning_binding(binding);

    return item;
}

} // namespace d2engine
