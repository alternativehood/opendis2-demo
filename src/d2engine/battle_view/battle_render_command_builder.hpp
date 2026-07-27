#pragma once

#include "battle_renderer.hpp"

namespace d2engine {

class BattleRenderCommandBuilder {
public:
    [[nodiscard]] static RenderBatch build(const BattleRenderSnapshot& snapshot,
                                           IBattleTextureProvider&     textures,
                                           const BattleRenderOptions&  options = {});
};

} // namespace d2engine
