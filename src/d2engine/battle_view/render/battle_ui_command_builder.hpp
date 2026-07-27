#pragma once

#include "../battle_renderer.hpp"

namespace d2engine {

void append_combat_frame(RenderBatch& batch, IBattleTextureProvider& textures,
                         const BattleRenderOptions& options);
void append_unit_groups(RenderBatch& batch, const BattleRenderSnapshot& snapshot,
                        IBattleTextureProvider& textures, const BattleRenderOptions& options);
void append_bottom_portraits(RenderBatch& batch, const BattleRenderSnapshot& snapshot,
                             const BattleRenderOptions& options);
void append_battle_frame_info(RenderBatch& batch, const BattleRenderSnapshot& snapshot,
                              const BattleRenderOptions& options);

} // namespace d2engine
