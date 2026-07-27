#include "battle_anchor_resolver.hpp"

#include "battle_render_tree_contract.hpp"

#include <algorithm>
#include <stdexcept>

namespace d2engine::BattleAnchorResolver {
namespace {

// Resolve slot anchor position from render_tree only.
// Requires that config was already validated — missing required battlefield nodes
// cause a hard failure rather than silent {0,0}.
[[nodiscard]] Vec2 resolve_slot_anchor(const TreeLayout& tree_layout, BattleSlotCoord coord) {
    const std::string slot_path = battlefield_slot_tree_path(coord);
    const std::string unit_path = battlefield_unit_tree_path(coord);
    if (!tree_layout.has_node(slot_path)) {
        throw std::runtime_error("resolve_slot_anchor: missing required render_tree node: " +
                                 slot_path);
    }
    if (!tree_layout.has_node(unit_path)) {
        throw std::runtime_error("resolve_slot_anchor: missing required render_tree node: " +
                                 unit_path);
    }
    const Rect composed = tree_layout.compose(unit_path);
    return {.x = composed.x, .y = composed.y};
}

// Average positions for a side (and optionally a single lane).
// Iterates all 3 lanes × {Back, Front} — CENTER excluded, semantic coords only.
[[nodiscard]] Vec2 average_slots(const BattleRenderSnapshot& snapshot,
                                 const TreeLayout& tree_layout, BattleSide side, int lane = -1) {
    Vec2  sum;
    float count = 0.0f;

    // First: live entities on this side.
    for (const auto& entity : snapshot.entities) {
        if (entity.coord.side != side || (lane >= 0 && entity.coord.lane != lane))
            continue;
        if (entity.life_state == LifeVisualState::Dead)
            continue;
        const Vec2 position = resolve_slot_anchor(tree_layout, entity.coord);
        sum.x += position.x;
        sum.y += position.y;
        count += 1.0f;
    }
    if (count > 0.0f)
        return {.x = sum.x / count, .y = sum.y / count};

    // Fallback: average all FRONT/BACK slots for the side (semantic coords, no C++ table).
    for (int l = 0; l < 3; ++l) {
        if (lane >= 0 && l != lane)
            continue;
        for (BattleDepth depth : {BattleDepth::Back, BattleDepth::Front}) {
            const BattleSlotCoord coord{.side = side, .lane = l, .depth = depth};
            const Vec2            position = resolve_slot_anchor(tree_layout, coord);
            sum.x += position.x;
            sum.y += position.y;
            count += 1.0f;
        }
    }
    return count > 0.0f ? Vec2{.x = sum.x / count, .y = sum.y / count} : Vec2{};
}

[[nodiscard]] BattleSide opposite(BattleSide side) {
    return side == BattleSide::Attacker ? BattleSide::Defender : BattleSide::Attacker;
}

} // namespace

Vec2 resolve(AnchorPolicy policy, const SnapshotEntity& entity,
             const BattleRenderSnapshot& snapshot, const TreeLayout& tree_layout,
             const LayoutMetrics& metrics) {
    const Vec2 slot = resolve_slot_anchor(tree_layout, entity.coord);
    Vec2       result = slot;
    switch (policy) {
    case AnchorPolicy::UnitFoot:
    case AnchorPolicy::UnitCanvasFoot:
        break;
    case AnchorPolicy::TeamCentroid:
        result = average_slots(snapshot, tree_layout, entity.coord.side);
        break;
    case AnchorPolicy::OppositeTeamCentroid:
        result = average_slots(snapshot, tree_layout, opposite(entity.coord.side));
        break;
    case AnchorPolicy::LaneMidpoint:
        result = average_slots(snapshot, tree_layout, entity.coord.side, entity.coord.lane);
        break;
    case AnchorPolicy::OppositeLaneMidpoint:
        result =
            average_slots(snapshot, tree_layout, opposite(entity.coord.side), entity.coord.lane);
        break;
    case AnchorPolicy::BattlefieldReferenceRect: {
        const Rect& bf = metrics.battlefield_rect;
        result = {.x = bf.x + bf.w * 0.5f, .y = bf.y + bf.h * 0.5f};
        break;
    }
    case AnchorPolicy::ScreenRect:
        result = {.x = metrics.ref_w * 0.5f, .y = metrics.ref_h * 0.5f};
        break;
    case AnchorPolicy::WorldPoint:
        result.x += entity.position_offset.x;
        result.y += entity.position_offset.y;
        break;
    }
    return result;
}

Rect compute_destination_rect(Vec2 anchor, const SnapshotTrack& track, float texture_width,
                              float texture_height, float magnitude, float scale_x, float scale_y,
                              Vec2 offset) {
    const bool  has_foot = track.canvas_foot_x != 0 || track.canvas_foot_y != 0;
    const float native_w = has_foot && track.native_canvas_w > 0
                               ? static_cast<float>(track.native_canvas_w)
                               : texture_width;
    const float native_h = has_foot && track.native_canvas_h > 0
                               ? static_cast<float>(track.native_canvas_h)
                               : texture_height;
    const float width = native_w * magnitude * scale_x;
    const float height = native_h * magnitude * scale_y;
    const float anchor_x = anchor.x + offset.x;
    const float anchor_y = anchor.y + offset.y;
    if (!has_foot) {
        return {.x = anchor_x - (width / 2.0f), .y = anchor_y - height, .w = width, .h = height};
    }
    return {.x = anchor_x - (static_cast<float>(track.canvas_foot_x) * magnitude * scale_x),
            .y = anchor_y - (static_cast<float>(track.canvas_foot_y) * magnitude * scale_y),
            .w = width,
            .h = height};
}

} // namespace d2engine::BattleAnchorResolver
