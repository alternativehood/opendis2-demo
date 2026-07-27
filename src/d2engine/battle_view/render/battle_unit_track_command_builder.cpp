#include "battle_unit_track_command_builder.hpp"

#include "battle_render_binding_helpers.hpp"
#include "battle_render_level_resolver.hpp"
#include "../battle_anchor_resolver.hpp"
#include "../battle_render_batch_helpers.hpp"
#include "../battle_render_tree_contract.hpp"
#include "../battle_slot.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

namespace d2engine {
namespace {

[[nodiscard]] SnapshotTrack reference_canvas_track(const SnapshotTrack& track, float ref_w,
                                                   float ref_h) {
    if (track.native_canvas_w <= 0 || track.native_canvas_h <= 0) {
        return track;
    }
    SnapshotTrack adjusted = track;
    adjusted.canvas_foot_x = static_cast<int>(static_cast<float>(track.canvas_foot_x) *
                                              (ref_w / static_cast<float>(track.native_canvas_w)));
    adjusted.canvas_foot_y = static_cast<int>(static_cast<float>(track.canvas_foot_y) *
                                              (ref_h / static_cast<float>(track.native_canvas_h)));
    adjusted.native_canvas_w = static_cast<int>(ref_w);
    adjusted.native_canvas_h = static_cast<int>(ref_h);
    return adjusted;
}

[[nodiscard]] const char* debug_kind(TrackKind kind) {
    switch (kind) {
    case TrackKind::Base:
    case TrackKind::DeathBody:
    case TrackKind::ActorMarker:
    case TrackKind::TargetMarker:
        return "unit"; // unit-local visuals: body, corpse, selection/target markers
    case TrackKind::DeathFx:
    case TrackKind::ReviveFx:
        return "lifecycle";
    case TrackKind::CastFx:
    case TrackKind::HitFx:
    case TrackKind::Effect:
    case TrackKind::Aura:
        return "effect";
    case TrackKind::Shadow:
    case TrackKind::Hidden:
        return "readonly";
    }
    return "unknown";
}
// Extract asset direction letter from a sequence name (last uppercase A-Z before digits/end).
// HMOVA2A00 → "A", TUCHA1B00 → "B", generic fallback → "".
[[nodiscard]] std::string asset_direction_from_seq(std::string_view name) {
    // Walk backwards looking for a trailing two-digit suffix, then grab the letter before it.
    if (name.size() >= 3) {
        const auto end = name.size();
        if ((std::isdigit(static_cast<unsigned char>(name[end - 1])) != 0) &&
            (std::isdigit(static_cast<unsigned char>(name[end - 2])) != 0)) {
            const char c = name[end - 3];
            if (std::isupper(static_cast<unsigned char>(c)) != 0) {
                return std::string(1, c);
            }
        }
    }
    return {};
}

[[nodiscard]] std::string visual_category_for(const SnapshotTrack& track) {
    if (track.visual_role.has_value()) {
        switch (*track.visual_role) {
        case BattleEffectVisualRole::SourceAttackOverlayFx:
            return "SourceAttackOverlayFx";
        case BattleEffectVisualRole::SourceCastFx:
            return "SourceCastFx";
        case BattleEffectVisualRole::TargetDamageFx:
            return "TargetDamageFx";
        case BattleEffectVisualRole::TeamAttackOverlayFx:
            return "TeamAttackOverlayFx";
        case BattleEffectVisualRole::TeamOverlayFx:
            return "TeamOverlayFx";
        case BattleEffectVisualRole::FieldOverlayFx:
            return "FieldOverlayFx";
        }
    }
    if (track.kind == TrackKind::Base && track.layer_slot.has_value()) {
        const BindingRole role = unit_animation_role(track);
        const char*       r = (role == BindingRole::UnitAttack) ? "Attack"
                              : (role == BindingRole::UnitHit)  ? "Hit"
                              : (role == BindingRole::UnitIdle) ? "Idle"
                              : (role == BindingRole::UnitBase) ? "Death"
                                                                : nullptr;
        if (r != nullptr) {
            const char* s = (*track.layer_slot == LayerSlot::S1)   ? "S1"
                            : (*track.layer_slot == LayerSlot::A2) ? "A2"
                                                                   : "A1";
            return std::string("Unit") + r + s;
        }
    }
    return {};
}

[[nodiscard]] DebugRenderableItem make_tunable_item(const SnapshotEntity& entity,
                                                    const SnapshotTrack& track, const Rect& rect,
                                                    const Vec2& anchor, int level,
                                                    const std::string& tree_path) {
    char id[96];
    if (track.layer_slot.has_value()) {
        const char* slot_name = (track.layer_slot == LayerSlot::S1)   ? "s1"
                                : (track.layer_slot == LayerSlot::A2) ? "a2"
                                                                      : "a1";
        std::snprintf(id, sizeof(id), "unit:%u:track:%u:%s", entity.unit_instance_id.value,
                      track.id.value, slot_name);
    } else {
        std::snprintf(id, sizeof(id), "unit:%u:track:%u", entity.unit_instance_id.value,
                      track.id.value);
    }
    const bool has_anchor = track.canvas_foot_x != 0 || track.canvas_foot_y != 0;
    const bool selectable = track.layer != TrackRenderLayer::Debug &&
                            (track.kind != TrackKind::DeathBody || has_anchor);
    auto       opt_binding = binding_for(track, entity);
    // Floating label: show human-readable path
    std::string label = (opt_binding.has_value() && !opt_binding->display_path.empty())
                            ? opt_binding->display_path
                            : std::string{id};
    if (track.kind == TrackKind::DeathBody) {
        char meta[128];
        std::snprintf(meta, sizeof(meta), " lc:%s foot:(%d,%d) canvas:%dx%d anchor:%s",
                      track.lifecycle_profile_id.c_str(), track.canvas_foot_x, track.canvas_foot_y,
                      track.native_canvas_w, track.native_canvas_h,
                      has_anchor ? "ok" : "missing(read-only)");
        label += meta;
    }
    // Default binding: for Base layered tracks and effect tracks with visual_role
    std::optional<ConfigBinding> default_b;
    // NOLINTNEXTLINE(bugprone-branch-clone)
    if (track.kind == TrackKind::Base && track.layer_slot.has_value()) {
        default_b = layer_default_binding_for(track);
    } else if (track.visual_role.has_value()) {
        default_b = effect_default_binding_for(track);
    }
    const std::string side = entity_side(entity);
    if (default_b.has_value()) {
        // Team effects target opposite side — default_binding display_path reflects actual lookup.
        const char* def_side =
            track.visual_role.has_value() ? effect_lookup_side(track, entity) : side.c_str();
        default_b->side = def_side;
        default_b->display_path += std::string(".") + def_side;
    }
    return {.stable_id = id,
            .label = std::move(label),
            .tree_path = tree_path,
            .bounds = rect,
            .kind = debug_kind(track.kind),
            .layer = static_cast<int>(leveled_layer(track.layer, level)),
            .resource_key = track.container_path + "/" + track.current_frame_name,
            .current_frame_name = track.current_frame_name,
            .logical_rect = rect,
            .screen_rect = rect,
            .anchor = anchor,
            .selectable = selectable,
            .visible = true,
            .binding = opt_binding ? std::optional<TuningBinding>{to_tuning_binding(*opt_binding)}
                                   : std::nullopt,
            .default_binding = default_b
                                   ? std::optional<TuningBinding>{to_tuning_binding(*default_b)}
                                   : std::nullopt,
            .visual_category = visual_category_for(track),
            .render_side = side,
            .asset_direction = asset_direction_from_seq(track.sequence_name)};
}
[[nodiscard]] RenderCommand make_track_command(RenderBatch& batch, const SnapshotEntity& entity,
                                               const SnapshotTrack&        track,
                                               const BattleRenderSnapshot& snapshot,
                                               IBattleTextureProvider&     textures,
                                               const BattleRenderOptions&  options) {
    BackendTextureRef const texture =
        textures.get_texture(track.container_path, track.current_frame_name);
    if (!texture.present()) {
        return {};
    }
    const auto [texture_width, texture_height] = textures.texture_size(texture);
    const Vec2 anchor =
        BattleAnchorResolver::resolve(track.placement.position_anchor, entity, snapshot,
                                      *options.tree_layout, options.layout_metrics);

    const bool native_sprite =
        is_unit_attached_marker(track.kind) || track.kind == TrackKind::DeathBody;
    const SnapshotTrack positioned_track =
        native_sprite ? track
                      : reference_canvas_track(track, options.layout_metrics.ref_w,
                                               options.layout_metrics.ref_h);

    float const magnitude = options.magnitude;
    Vec2        offset = {};
    float       the_sx = options.scale_x;
    float       the_sy = options.scale_y;
    float       the_alpha = 1.0f;

    if (track.kind == TrackKind::Base) {
        // Composition: slot + role(common+side) + layer_default(common+side) +
        // layer_exact(common+side)
        const char* side = entity_side(entity);
        float       role_x = 0.0f;
        float       role_y = 0.0f;
        float       role_sx = 1.0f;
        float       role_sy = 1.0f;
        const auto  rb = role_binding_for(track, entity);
        if (rb.has_value() && rb->owner_kind == BindingOwnerKind::UnitVisualProfile) {
            const auto rp = compose_placements(options.placements, *rb, side);
            role_x = rp.x;
            role_y = rp.y;
            role_sx = rp.scale_x;
            role_sy = rp.scale_y;
        }
        float      def_x = 0.0f;
        float      def_y = 0.0f;
        float      def_sx = 1.0f;
        float      def_sy = 1.0f;
        float      def_alpha = 1.0f;
        const auto db = layer_default_binding_for(track);
        if (db.has_value()) {
            const auto dp = compose_placements(options.placements, *db, side);
            def_x = dp.x;
            def_y = dp.y;
            def_sx = dp.scale_x;
            def_sy = dp.scale_y;
            def_alpha = dp.alpha;
        }
        float      layer_x = 0.0f;
        float      layer_y = 0.0f;
        float      layer_sx = 1.0f;
        float      layer_sy = 1.0f;
        const auto lb = layer_binding_for(track, entity); // common binding, side=""
        if (lb.has_value()) {
            const auto lp = compose_placements(options.placements, *lb, side);
            layer_x = lp.x;
            layer_y = lp.y;
            layer_sx = lp.scale_x;
            layer_sy = lp.scale_y;
        }
        offset.x = entity.position_offset.x + role_x + def_x + layer_x;
        offset.y = entity.position_offset.y + role_y + def_y + layer_y;
        the_sx = options.scale_x * role_sx * def_sx * layer_sx;
        the_sy = options.scale_y * role_sy * def_sy * layer_sy;
        the_alpha *= def_alpha;
    } else {
        const auto typed_binding = binding_for(track, entity);
        const auto per_it = typed_binding ? options.placements.find(typed_binding->key())
                                          : options.placements.end();
        const bool has_per_item = per_it != options.placements.end();

        if (track.visual_role.has_value()) {
            // Spawned effect with resolved visual category: additive composition — default + exact.
            const auto  edb = effect_default_binding_for(track);
            const char* side = effect_lookup_side(track, entity);
            const auto  def_p =
                edb ? compose_placements(options.placements, *edb, side) : VisualPlacementValue{};
            const float ex_x = has_per_item ? per_it->second.x : 0.0f;
            const float ex_y = has_per_item ? per_it->second.y : 0.0f;
            const float ex_sx = has_per_item ? per_it->second.scale_x : 1.0f;
            const float ex_sy = has_per_item ? per_it->second.scale_y : 1.0f;
            offset = {.x = def_p.x + ex_x, .y = def_p.y + ex_y};
            the_sx = def_p.scale_x * ex_sx;
            the_sy = def_p.scale_y * ex_sy;
        } else if (is_unit_attached_marker(track.kind)) {
            const auto sprite_binding = sprite_binding_for(track);
            const auto sprite_value =
                sprite_binding ? compose_placements(options.placements, *sprite_binding, "")
                               : VisualPlacementValue{};
            offset = {.x = entity.position_offset.x + sprite_value.x,
                      .y = entity.position_offset.y + sprite_value.y};
            the_sx = sprite_value.scale_x;
            the_sy = sprite_value.scale_y;
            the_alpha *= sprite_value.alpha;
        } else if (has_per_item) {
            if (track.kind == TrackKind::DeathBody) {
                // Corpse: entity position + global corpse + sprite default + per-unit delta.
                const auto corpse_it = options.placements.find("SceneLayout:scene:Corpse");
                const auto global_value = corpse_it != options.placements.end()
                                              ? corpse_it->second
                                              : VisualPlacementValue{};
                const auto sprite_binding = sprite_binding_for(track);
                const auto sprite_value =
                    sprite_binding ? compose_placements(options.placements, *sprite_binding, "")
                                   : VisualPlacementValue{};
                offset = {.x = entity.position_offset.x + global_value.x + sprite_value.x +
                               per_it->second.x,
                          .y = entity.position_offset.y + global_value.y + sprite_value.y +
                               per_it->second.y};
                the_sx = global_value.scale_x * sprite_value.scale_x * per_it->second.scale_x;
                the_sy = global_value.scale_y * sprite_value.scale_y * per_it->second.scale_y;
                the_alpha *= global_value.alpha * sprite_value.alpha * per_it->second.alpha;
            } else {
                offset = {.x = per_it->second.x, .y = per_it->second.y};
                the_sx = per_it->second.scale_x;
                the_sy = per_it->second.scale_y;
            }
        } else {
            const bool is_effect_layer = track.layer == TrackRenderLayer::Effect ||
                                         track.layer == TrackRenderLayer::Overlay ||
                                         track.layer == TrackRenderLayer::Marker;
            if (is_effect_layer) {
                offset = options.overlay_offset;
            } else if (track.kind == TrackKind::DeathBody) {
                // Global corpse fallback plus sprite default when no per-lifecycle override exists.
                const auto corpse_it = options.placements.find("SceneLayout:scene:Corpse");
                const auto sprite_binding = sprite_binding_for(track);
                const auto sprite_value =
                    sprite_binding ? compose_placements(options.placements, *sprite_binding, "")
                                   : VisualPlacementValue{};
                if (corpse_it != options.placements.end()) {
                    offset = {.x = entity.position_offset.x + corpse_it->second.x + sprite_value.x,
                              .y = entity.position_offset.y + corpse_it->second.y + sprite_value.y};
                    the_sx = corpse_it->second.scale_x * sprite_value.scale_x;
                    the_sy = corpse_it->second.scale_y * sprite_value.scale_y;
                    the_alpha *= corpse_it->second.alpha * sprite_value.alpha;
                } else {
                    offset = {.x = entity.position_offset.x + sprite_value.x,
                              .y = entity.position_offset.y + sprite_value.y};
                    the_sx = sprite_value.scale_x;
                    the_sy = sprite_value.scale_y;
                    the_alpha *= sprite_value.alpha;
                }
            } else {
                offset = entity.position_offset;
            }
        }
    } // end non-Base track branch
    offset.x += track.transform.position_offset.x;
    offset.y += track.transform.position_offset.y;
    const Rect destination = BattleAnchorResolver::compute_destination_rect(
        anchor, positioned_track, texture_width, texture_height, magnitude,
        the_sx * track.transform.scale_x, the_sy * track.transform.scale_y, offset);
    const int              level = compute_level(track, options, entity);
    const TrackRenderLayer effective_layer = leveled_layer(track.placement.pass, level);
    const std::string      unit_tp = battlefield_unit_tree_path(entity.coord);
    RenderCommand          command{.layer = battle_render_layer(draw_pass_for(effective_layer)),
                                   .texture = texture,
                                   .destination = destination,
                                   .alpha = entity.alpha * track.alpha * the_alpha,
                                   .flip_x = entity.flip || track.playback.flip_x,
                                   .flip_y = track.playback.flip_y,
                                   .tree_path = unit_tp};
    command.tunable_item_index = append_tunable_item(
        batch, make_tunable_item(entity, track, destination, anchor, level, unit_tp));
    return command;
}

} // namespace

void append_render_queue(RenderBatch& batch, const BattleRenderSnapshot& snapshot,
                         IBattleTextureProvider& textures, const BattleRenderOptions& options) {
    struct RenderItem {
        TrackRenderLayer pass = TrackRenderLayer::Base;
        float            depth_y = 0.0f;
        int              depth_bias = 0;
        std::uint64_t    stable_order = 0;
        RenderCommand    command;
    };

    std::vector<RenderItem> items;
    std::uint64_t           stable_order = 0;
    for (const auto& entity : snapshot.entities) {
        for (const auto& track : entity.tracks) {
            const std::uint64_t order = stable_order++;
            if (track.visibility != TrackVisibility::Visible || track.current_frame_name.empty()) {
                continue;
            }
            const int              level = compute_level(track, options, entity);
            const TrackRenderLayer effective_layer = leveled_layer(track.placement.pass, level);
            RenderCommand const    command =
                make_track_command(batch, entity, track, snapshot, textures, options);
            if (!command.texture.present()) {
                continue;
            }
            const Vec2 depth_anchor =
                BattleAnchorResolver::resolve(track.placement.depth_anchor, entity, snapshot,
                                              *options.tree_layout, options.layout_metrics);
            items.push_back({.pass = effective_layer,
                             .depth_y = depth_anchor.y,
                             .depth_bias = track.placement.depth_bias,
                             .stable_order = order,
                             .command = command});
        }
    }
    std::ranges::sort(items, {}, [](const RenderItem& item) {
        return std::tuple{item.pass, item.depth_y, item.depth_bias, item.stable_order};
    });
    for (auto& item : items) {
        batch.commands.push_back(std::move(item.command));
    }
}

} // namespace d2engine
