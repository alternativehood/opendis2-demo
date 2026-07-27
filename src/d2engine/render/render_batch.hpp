#pragma once

#include "render_command.hpp"
#include "render_tuning.hpp"

#include <vector>

namespace d2engine {

struct RenderBatch {
    std::vector<RenderCommand>     commands;
    std::vector<TunableRenderItem> tunable_items;
};

} // namespace d2engine
