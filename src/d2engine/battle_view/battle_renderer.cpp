#include "battle_renderer.hpp"

#include "battle_render_command_builder.hpp"

#include <algorithm>

namespace d2engine {

RenderBatch BattleRenderer::build_render_batch(const BattleRenderSnapshot& snapshot,
                                               IBattleTextureProvider&     textures,
                                               const BattleRenderOptions&  options) {
    return BattleRenderCommandBuilder::build(snapshot, textures, options);
}

RenderLayer battle_render_layer(BattleRenderPass pass) {
    const auto order = BattleRenderer::pass_order();
    const auto it = std::ranges::find(order, pass);
    return {.order = it == order.end() ? 0 : static_cast<int>(it - order.begin())};
}

} // namespace d2engine
