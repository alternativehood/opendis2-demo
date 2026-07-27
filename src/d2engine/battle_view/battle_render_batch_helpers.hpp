#pragma once

#include "../render/render_batch.hpp"

#include <cstddef>
#include <utility>

namespace d2engine {

[[nodiscard]] inline std::size_t append_tunable_item(RenderBatch& batch, TunableRenderItem item) {
    batch.tunable_items.push_back(std::move(item));
    return batch.tunable_items.size() - 1;
}

inline void append_tunable_command(RenderBatch& batch, RenderCommand command,
                                   TunableRenderItem item) {
    command.tunable_item_index = append_tunable_item(batch, std::move(item));
    batch.commands.push_back(std::move(command));
}

} // namespace d2engine
