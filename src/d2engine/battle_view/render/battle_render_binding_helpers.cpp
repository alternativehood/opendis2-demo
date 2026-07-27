#include "battle_render_binding_helpers.hpp"

#include "../../app/battle_tuning_state.hpp"
#include "../animation_role.hpp"
#include "../battle_slot.hpp"

#include <optional>
#include <string>
#include <utility>

namespace d2engine {

[[nodiscard]] bool is_unit_attached_marker(TrackKind kind) {
    return kind == TrackKind::ActorMarker || kind == TrackKind::TargetMarker;
}

// Determine unit animation role from propagated effect_role field.
// Sequence_name inference is fallback only — role is set explicitly by PlayClip.unit_anim_role.
[[nodiscard]] BindingRole unit_animation_role(const SnapshotTrack& track) noexcept {
    switch (track.effect_role) {
    case BindingRole::UnitIdle:
        return BindingRole::UnitIdle;
    case BindingRole::UnitAttack:
        return BindingRole::UnitAttack;
    case BindingRole::UnitHit:
        return BindingRole::UnitHit;
    case BindingRole::UnitBase:
        return BindingRole::UnitBase;
    case BindingRole::Source:
    case BindingRole::Target:
    case BindingRole::TargetTeam:
    case BindingRole::Global:
    case BindingRole::Corpse:
    case BindingRole::DeathFx:
    case BindingRole::ReviveSmall:
    case BindingRole::ReviveLarge:
    case BindingRole::SelectionMarker:
    case BindingRole::TargetMarker:
    case BindingRole::Background:
    case BindingRole::CombatFrame:
    case BindingRole::Unit:
        break;
    }
    // Fallback: sequence_name substring matching when role was not explicitly propagated.
    // This path should not occur in production — bootstrapped tracks have role set.
    if (track.sequence_name.find(AnimationRoles::ATTACK) != std::string::npos)
        return BindingRole::UnitAttack;
    if (track.sequence_name.find(AnimationRoles::HIT) != std::string::npos)
        return BindingRole::UnitHit;
    if (track.sequence_name.find(AnimationRoles::IDLE) != std::string::npos)
        return BindingRole::UnitIdle;
    return BindingRole::UnitBase;
}

// Returns "a" for attacker-side entities, "d" for defender-side.
[[nodiscard]] const char* entity_side(const SnapshotEntity& entity) noexcept {
    return entity.coord.side == BattleSide::Attacker ? "a" : "d";
}
// For spawned effects, returns the side used to look up the default profile.
// TeamAttackOverlayFx and TeamOverlayFx are spawned on the source entity but visually
// target the opposite team (OppositeTeamCentroid anchor), so their default lookup uses the
// opposite side from the source entity.
[[nodiscard]] const char* effect_lookup_side(const SnapshotTrack&  track,
                                             const SnapshotEntity& entity) noexcept {
    if (track.visual_role.has_value()) {
        const auto vr = *track.visual_role;
        if (vr == BattleEffectVisualRole::TeamAttackOverlayFx ||
            vr == BattleEffectVisualRole::TeamOverlayFx) {
            return entity.coord.side == BattleSide::Attacker ? "d" : "a";
        }
    }
    return entity_side(entity);
}

// Compose a common (side="") and optional side-specific entry from the placements map.
// Returns the combined placement: offset additive, scale multiplicative, level additive.
[[nodiscard]] VisualPlacementValue
compose_placements(const std::map<std::string, VisualPlacementValue>& placements,
                   const ConfigBinding& common_binding, const char* side) {
    VisualPlacementValue result{}; // x=0, y=0, scale_x=1, scale_y=1, alpha=1, level=0
    // Common (side="" key)
    const auto cit = placements.find(common_binding.key());
    if (cit != placements.end()) {
        result.x += cit->second.x;
        result.y += cit->second.y;
        result.scale_x *= cit->second.scale_x;
        result.scale_y *= cit->second.scale_y;
        result.alpha *= cit->second.alpha;
        result.level += cit->second.level;
    }
    // Side-specific override
    if (side[0] != '\0') {
        ConfigBinding sb = common_binding;
        sb.side = side;
        const auto sit = placements.find(sb.key());
        if (sit != placements.end()) {
            result.x += sit->second.x;
            result.y += sit->second.y;
            result.scale_x *= sit->second.scale_x;
            result.scale_y *= sit->second.scale_y;
            result.alpha *= sit->second.alpha;
            result.level += sit->second.level;
        }
    }
    return result;
}

// Returns the role-level binding for a Base track (whole-unit role placement, side="").
[[nodiscard]] std::optional<ConfigBinding> role_binding_for(const SnapshotTrack&  track,
                                                            const SnapshotEntity& entity) {
    const std::string& owner =
        entity.animation_unit_type.empty() ? entity.unit_type : entity.animation_unit_type;
    if (!owner.empty()) {
        const BindingRole role = unit_animation_role(track);
        return ConfigBinding{.config_file = kBattleScreenConfigPath,
                             .owner_kind = BindingOwnerKind::UnitVisualProfile,
                             .tree_path = owner,
                             .role = role,
                             .display_path = "unit_visual_profiles." + owner + "." +
                                             std::string(role_json_key(role))};
    }
    // Fallback: depth-only position level binding when unit_type unknown
    const std::string pos = slot_coord_to_string(entity.coord);
    return ConfigBinding{.config_file = kBattleScreenConfigPath,
                         .owner_kind = BindingOwnerKind::PositionLevel,
                         .tree_path = pos,
                         .role = BindingRole::Unit,
                         .display_path = "position_levels." + pos};
}

// Returns the shared layer-default binding (side="") for a layered Base track.
// Keyed under unit_visual_layer_default_profiles.<role>.<slot>.
[[nodiscard]] std::optional<ConfigBinding> layer_default_binding_for(const SnapshotTrack& track) {
    if (!track.layer_slot.has_value())
        return std::nullopt;
    const BindingRole role = unit_animation_role(track);
    const char*       slot_name = (*track.layer_slot == LayerSlot::S1)   ? "s1"
                                  : (*track.layer_slot == LayerSlot::A2) ? "a2"
                                                                         : "a1";
    return ConfigBinding{.config_file = kBattleScreenConfigPath,
                         .owner_kind = BindingOwnerKind::UnitVisualLayerDefaultProfile,
                         .tree_path = slot_name,
                         .role = role,
                         .display_path = std::string("unit_visual_layer_default_profiles.") +
                                         role_json_key(role) + "." + slot_name};
}

// Returns the shared effect-default binding for a spawned effect by visual category.
// B-direction assets (e.g. TUCHA1B) resolve placement by render side (a/d) — never "b".
[[nodiscard]] std::optional<ConfigBinding> effect_default_binding_for(const SnapshotTrack& track) {
    if (!track.visual_role.has_value())
        return std::nullopt;
    const char* owner_id = nullptr;
    BindingRole role = BindingRole::Source;
    switch (*track.visual_role) {
    case BattleEffectVisualRole::SourceAttackOverlayFx:
        owner_id = "source_attack_overlay";
        role = BindingRole::Source;
        break;
    case BattleEffectVisualRole::SourceCastFx:
        owner_id = "source_cast";
        role = BindingRole::Source;
        break;
    case BattleEffectVisualRole::TargetDamageFx:
        owner_id = "target_damage";
        role = BindingRole::Target;
        break;
    case BattleEffectVisualRole::TeamAttackOverlayFx:
        owner_id = "team_attack";
        role = BindingRole::TargetTeam;
        break;
    case BattleEffectVisualRole::TeamOverlayFx:
        owner_id = "target_team";
        role = BindingRole::TargetTeam;
        break;
    case BattleEffectVisualRole::FieldOverlayFx:
        return std::nullopt;
    }
    if (owner_id == nullptr)
        return std::nullopt;
    return ConfigBinding{.config_file = kBattleScreenConfigPath,
                         .owner_kind = BindingOwnerKind::EffectDefaultProfile,
                         .tree_path = owner_id,
                         .role = role,
                         .display_path = std::string("battle_effect_default_profiles.") + owner_id};
}

[[nodiscard]] std::optional<ConfigBinding> sprite_binding_for(const SnapshotTrack& track) {
    if (track.sequence_name.empty()) {
        return std::nullopt;
    }
    return ConfigBinding{.config_file = kBattleScreenConfigPath,
                         .owner_kind = BindingOwnerKind::SpriteProfile,
                         .tree_path = track.sequence_name,
                         .role = BindingRole::Global,
                         .display_path = "sprite_profiles." + track.sequence_name};
}

// Returns the common per-layer binding (side="") for placement resolution.
[[nodiscard]] std::optional<ConfigBinding> layer_binding_for(const SnapshotTrack&  track,
                                                             const SnapshotEntity& entity) {
    if (!track.layer_slot.has_value())
        return std::nullopt;
    const std::string& owner =
        entity.animation_unit_type.empty() ? entity.unit_type : entity.animation_unit_type;
    if (owner.empty())
        return std::nullopt;
    const char*       slot_name = (track.layer_slot == LayerSlot::S1)   ? "s1"
                                  : (track.layer_slot == LayerSlot::A2) ? "a2"
                                                                        : "a1";
    const BindingRole role = unit_animation_role(track);
    const std::string owner_id = owner + ":" + slot_name;
    return ConfigBinding{.config_file = kBattleScreenConfigPath,
                         .owner_kind = BindingOwnerKind::UnitVisualLayerProfile,
                         .tree_path = owner_id,
                         .role = role,
                         // side="" → common key; display_path without side suffix
                         .display_path = "unit_visual_layer_profiles." + owner + "." +
                                         std::string(role_json_key(role)) + "." + slot_name};
}

// Returns the side-specific per-layer binding for debug editing (writes to
// <unit>.<role>.<layer>.<side>).
[[nodiscard]] std::optional<ConfigBinding> layer_side_binding_for(const SnapshotTrack&  track,
                                                                  const SnapshotEntity& entity) {
    auto b = layer_binding_for(track, entity);
    if (!b.has_value())
        return std::nullopt;
    const char* side = entity_side(entity);
    b->side = side;
    b->display_path += std::string(".") + side;
    return b;
}

[[nodiscard]] std::optional<ConfigBinding> binding_for(const SnapshotTrack&  track,
                                                       const SnapshotEntity& entity) {
    if (track.kind == TrackKind::Base) {
        // For layered Base tracks (S1/A1/A2): debug writes to side-specific layer config.
        // The role-level and common-layer bindings are applied separately in make_track_command.
        if (track.layer_slot.has_value()) {
            return layer_side_binding_for(track, entity);
        }
        return role_binding_for(track, entity);
    }
    if (track.kind == TrackKind::DeathBody || track.kind == TrackKind::DeathFx ||
        track.kind == TrackKind::ReviveFx) {
        if (!track.lifecycle_profile_id.empty()) {
            BindingRole role = BindingRole::Corpse;
            if (track.kind == TrackKind::DeathFx) {
                role = BindingRole::DeathFx;
            } else if (track.kind == TrackKind::ReviveFx) {
                role = track.effect_role;
            }
            return ConfigBinding{.config_file = kBattleScreenConfigPath,
                                 .owner_kind = BindingOwnerKind::LifecycleProfile,
                                 .tree_path = track.lifecycle_profile_id,
                                 .role = role,
                                 .display_path = "unit_lifecycle_profiles." +
                                                 track.lifecycle_profile_id + "." +
                                                 std::string(role_json_key(role))};
        }
        return std::nullopt;
    }
    if (is_unit_attached_marker(track.kind)) {
        return sprite_binding_for(track);
    }
    if (track.kind == TrackKind::CastFx || track.kind == TrackKind::HitFx ||
        track.kind == TrackKind::Effect || track.kind == TrackKind::Aura) {
        // For layered bundles, all S1/A1/A2 layers share the driver (bundle) sequence name
        // as owner_id so placement/tuning applies to the whole bundle, not per-layer.
        const std::string& owner =
            track.bundle_sequence_name.empty() ? track.sequence_name : track.bundle_sequence_name;
        if (!owner.empty()) {
            return ConfigBinding{.config_file = kBattleScreenConfigPath,
                                 .owner_kind = BindingOwnerKind::EffectProfile,
                                 .tree_path = owner,
                                 .role = track.effect_role,
                                 .display_path = "battle_effect_profiles." + owner + "." +
                                                 std::string(role_json_key(track.effect_role))};
        }
        return std::nullopt;
    }
    return std::nullopt;
}
[[nodiscard]] DebugRenderableItem make_tree_command_tunable_item(std::string stable_id,
                                                                 std::string tree_path,
                                                                 std::string kind, const Rect& rect,
                                                                 BindingRole role) {
    DebugRenderableItem item;
    item.stable_id = std::move(stable_id);
    item.tree_path = tree_path;
    item.label = item.stable_id;
    item.kind = std::move(kind);
    item.layer = static_cast<int>(TrackRenderLayer::Frame);
    item.logical_rect = rect;
    item.screen_rect = rect;
    item.anchor = {.x = rect.x, .y = rect.y};
    item.selectable = true;
    item.visible = true;
    item.binding = to_tuning_binding(
        ConfigBinding{.config_file = kBattleScreenConfigPath,
                      .owner_kind = BindingOwnerKind::TreeLayout,
                      .tree_path = std::move(tree_path),
                      .role = role,
                      .display_path = std::string("render_tree") + item.tree_path});
    return item;
}

} // namespace d2engine
