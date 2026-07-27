#pragma once

#include "../battle_renderer.hpp"

namespace d2engine {

void append_render_queue(RenderBatch& batch, const BattleRenderSnapshot& snapshot,
                         IBattleTextureProvider& textures, const BattleRenderOptions& options);

} // namespace d2engine
