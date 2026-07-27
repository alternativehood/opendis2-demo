#pragma once

#include "../battle_renderer.hpp"

namespace d2engine {

void append_ground_background(RenderBatch& batch, IBattleTextureProvider& textures,
                              const BattleRenderOptions& options);
void append_background(RenderBatch& batch, IBattleTextureProvider& textures,
                       const BattleRenderOptions& options);
void append_background_overlays(RenderBatch& batch, IBattleTextureProvider& textures,
                                const BattleRenderOptions& options);

} // namespace d2engine
