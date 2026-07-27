#pragma once

#include "anchor_policy.hpp"
#include "battle_render_snapshot.hpp"
#include "layout_types.hpp"
#include "../render/rect.hpp"
#include "../render/render_tree.hpp"
#include "../render/vec2.hpp"

namespace d2engine::BattleAnchorResolver {

[[nodiscard]] Vec2 resolve(AnchorPolicy policy, const SnapshotEntity& entity,
                           const BattleRenderSnapshot& snapshot, const TreeLayout& tree_layout,
                           const LayoutMetrics& metrics);

[[nodiscard]] Rect compute_destination_rect(Vec2 anchor, const SnapshotTrack& track,
                                            float texture_width, float texture_height,
                                            float magnitude = 1.0f, float scale_x = 1.0f,
                                            float scale_y = 1.0f, Vec2 offset = {});

} // namespace d2engine::BattleAnchorResolver
