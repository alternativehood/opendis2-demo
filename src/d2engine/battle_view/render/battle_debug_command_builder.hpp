#pragma once

#include "../battle_renderer.hpp"

namespace d2engine {

void append_background_container(RenderBatch& batch, const BattleRenderOptions& options);
void append_tree_layout_items(RenderBatch& batch, const BattleRenderOptions& options);

} // namespace d2engine
