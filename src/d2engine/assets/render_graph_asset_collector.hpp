#pragma once

#include "image_asset_key.hpp"

#include <d2adventure_render/render_graph.hpp>

#include <vector>

namespace d2engine {

/// Collect unique ImageAssetKeys from all drawable prepared render phases.
/// Scans ground_overlay, world, world_overlay, fog, and ui_overlay.
/// For animated primitives, collects every animation frame.
/// Ignores primitives with empty container_path or record name.
/// Returns deterministic output ordering with deduplicated keys.
[[nodiscard]] std::vector<ImageAssetKey>
collect_adventure_render_asset_keys(const adventure_render::PreparedAdventureRenderGraph& graph);

/// Collect only assets needed for initial (non-blocking) startup:
/// every static primitive asset and exactly the first frame of every
/// animated primitive. Used by Adventure startup as the critical set.
[[nodiscard]] std::vector<ImageAssetKey>
collect_adventure_initial_asset_keys(const adventure_render::PreparedAdventureRenderGraph& graph);

/// Collect animation frames beyond frame 0, deduplicated against the
/// initial set. These remaining animation assets are mandatory before the
/// map enters the Running phase.
[[nodiscard]] std::vector<ImageAssetKey> collect_adventure_remaining_animation_asset_keys(
    const adventure_render::PreparedAdventureRenderGraph& graph);

} // namespace d2engine
