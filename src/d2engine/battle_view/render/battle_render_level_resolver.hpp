#pragma once

#include "../battle_renderer.hpp"

namespace d2engine {

[[nodiscard]] BattleRenderPass draw_pass_for(TrackRenderLayer layer);
[[nodiscard]] int compute_level(const SnapshotTrack& track, const BattleRenderOptions& options,
                                const SnapshotEntity& entity);
[[nodiscard]] TrackRenderLayer leveled_layer(TrackRenderLayer layer, int level);

} // namespace d2engine
