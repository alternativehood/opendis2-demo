#include "battle_render_level_resolver.hpp"

#include "battle_render_binding_helpers.hpp"
#include "../battle_slot.hpp"

#include <d2log/log.hpp>

#include <algorithm>
#include <string>
#include <unordered_set>

namespace d2engine {
namespace {

auto kLog = d2log::get("d2.render"); // NOLINT(cert-err58-cpp)

} // namespace

[[nodiscard]] BattleRenderPass draw_pass_for(TrackRenderLayer layer) {
    switch (layer) {
    case TrackRenderLayer::Background:
        return BattleRenderPass::Background;
    case TrackRenderLayer::Ground:
        return BattleRenderPass::GroundEffects;
    case TrackRenderLayer::Shadow:
        return BattleRenderPass::Shadows;
    case TrackRenderLayer::Base:
        return BattleRenderPass::UnitsFront;
    case TrackRenderLayer::Effect:
    case TrackRenderLayer::Overlay:
        return BattleRenderPass::Effects;
    case TrackRenderLayer::Marker:
        return BattleRenderPass::Markers;
    case TrackRenderLayer::Frame:
        return BattleRenderPass::CombatFrame;
    case TrackRenderLayer::Debug:
        return BattleRenderPass::Debug;
    }
    return BattleRenderPass::Debug;
}
[[nodiscard]] int compute_level(const SnapshotTrack& track, const BattleRenderOptions& options,
                                const SnapshotEntity& entity) {
    // Slot-based position level is the base for all tracks anchored to a specific unit.
    const auto pos_it = options.position_levels.find(slot_coord_to_string(entity.coord));
    const int  slot_base = (pos_it != options.position_levels.end()) ? pos_it->second : 0;

    if (track.kind == TrackKind::Base) {
        // Additive: slot_base + role(common+side) + builtin_bias(slot) + layer(common+side).
        const int base =
            (pos_it != options.position_levels.end()) ? pos_it->second : options.unit_level;
        const char* side = entity_side(entity);
        int         level = base;
        const auto  role_b = role_binding_for(track, entity);
        if (role_b.has_value()) {
            level += compose_placements(options.placements, *role_b, side).level;
        }

        // Built-in layer order bias for S1/A1/A2 Base layered tracks.
        // This is an immutable engine constant — never serialized to config.
        // Saved config level = user delta above this bias; bias is transparent to serialization.
        int builtin_bias = 0;
        if (track.layer_slot.has_value()) {
            switch (*track.layer_slot) {
            case LayerSlot::S1:
                builtin_bias = -1;
                break;
            case LayerSlot::A1:
                builtin_bias = 0;
                break;
            case LayerSlot::A2:
                builtin_bias = +2;
                break;
            }
            level += builtin_bias;
        }

        const auto def_b = layer_default_binding_for(track);
        if (def_b.has_value()) {
            level += compose_placements(options.placements, *def_b, side).level;
        }

        const auto layer_b = layer_binding_for(track, entity);
        if (layer_b.has_value()) {
            level += compose_placements(options.placements, *layer_b, side).level;
        }

        // Diagnostic: log once per session per (sequence_name, slot) combination.
        if (track.layer_slot.has_value() && !track.sequence_name.empty()) {
            static std::unordered_set<std::string> s_seen;
            const char* slot_name = (*track.layer_slot == LayerSlot::S1)   ? "S1"
                                    : (*track.layer_slot == LayerSlot::A2) ? "A2"
                                                                           : "A1";
            if (s_seen.insert(track.sequence_name + slot_name).second) {
                int role_c = 0;
                int role_s_v = 0;
                int def_c = 0;
                int def_s_v = 0;
                int layer_c = 0;
                int layer_s_v = 0;
                if (role_b.has_value()) {
                    auto it = options.placements.find(role_b->key());
                    if (it != options.placements.end())
                        role_c = it->second.level;
                    ConfigBinding sb = *role_b;
                    sb.side = side;
                    it = options.placements.find(sb.key());
                    if (it != options.placements.end())
                        role_s_v = it->second.level;
                }
                if (def_b.has_value()) {
                    auto it = options.placements.find(def_b->key());
                    if (it != options.placements.end())
                        def_c = it->second.level;
                    ConfigBinding sb = *def_b;
                    sb.side = side;
                    it = options.placements.find(sb.key());
                    if (it != options.placements.end())
                        def_s_v = it->second.level;
                }
                if (layer_b.has_value()) {
                    auto it = options.placements.find(layer_b->key());
                    if (it != options.placements.end())
                        layer_c = it->second.level;
                    ConfigBinding sb = *layer_b;
                    sb.side = side;
                    it = options.placements.find(sb.key());
                    if (it != options.placements.end())
                        layer_s_v = it->second.level;
                }
                kLog->debug("layer_sort seq={} slot={} base={} role={}/{} bias={} def={}/{} "
                            "layer={}/{} final={}",
                            track.sequence_name, slot_name, base, role_c, role_s_v, builtin_bias,
                            def_c, def_s_v, layer_c, layer_s_v, level);
            }
        }

        return level;
    }
    if (track.kind == TrackKind::DeathBody) {
        // Corpse: slot_base (killed unit's slot) + lifecycle corpse delta.
        int        level = options.lifecycle_level;
        const auto sprite_binding = sprite_binding_for(track);
        if (sprite_binding.has_value()) {
            const auto sit = options.placements.find(sprite_binding->key());
            if (sit != options.placements.end()) {
                level += sit->second.level;
            }
        }
        const auto binding = binding_for(track, entity);
        if (binding.has_value()) {
            const auto pit = options.placements.find(binding->key());
            if (pit != options.placements.end()) {
                level += pit->second.level;
            }
        }
        return slot_base + level;
    }
    if (is_unit_attached_marker(track.kind)) {
        int        level = slot_base;
        const auto binding = sprite_binding_for(track);
        if (binding.has_value()) {
            const auto pit = options.placements.find(binding->key());
            if (pit != options.placements.end()) {
                level += pit->second.level;
            }
        }
        return level;
    }
    if (track.kind == TrackKind::CastFx || track.kind == TrackKind::HitFx ||
        track.kind == TrackKind::Effect || track.kind == TrackKind::Aura) {
        // Source-anchored effects (BindingRole::Source) inherit the source unit's slot level.
        // Team/global overlays (BindingRole::TargetTeam, Global) have no position component.
        const bool source_anchored = track.effect_role == BindingRole::Source;
        const int  effect_base = source_anchored ? slot_base : 0;
        // Default level from battle_effect_default_profiles.<category>.<side>
        int default_level = 0;
        if (track.visual_role.has_value()) {
            const auto edb = effect_default_binding_for(track);
            if (edb.has_value()) {
                default_level =
                    compose_placements(options.placements, *edb, effect_lookup_side(track, entity))
                        .level;
            }
        }
        const auto binding = binding_for(track, entity);
        if (binding.has_value()) {
            const auto pit = options.placements.find(binding->key());
            if (pit != options.placements.end()) {
                return effect_base + default_level + pit->second.level;
            }
        }
        return effect_base + default_level + options.effect_level;
    }
    return 0;
}

[[nodiscard]] TrackRenderLayer leveled_layer(TrackRenderLayer layer, int level) {
    const int min_layer = static_cast<int>(TrackRenderLayer::Background);
    const int max_layer = static_cast<int>(TrackRenderLayer::Frame);
    return static_cast<TrackRenderLayer>(
        std::clamp(static_cast<int>(layer) + level, min_layer, max_layer));
}

} // namespace d2engine
