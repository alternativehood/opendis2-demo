#pragma once

#include <gtest/gtest.h>

#include "d2engine/render/render_batch.hpp"

#include <algorithm>
#include <vector>

namespace d2engine::test {

[[nodiscard]] inline std::vector<const RenderBatch*>& render_batches() {
    static std::vector<const RenderBatch*> batches;
    return batches;
}

[[nodiscard]] inline const std::vector<RenderCommand>& commands_from(const RenderBatch& batch) {
    auto& batches = render_batches();
    if (std::ranges::find(batches, &batch) == batches.end()) {
        batches.push_back(&batch);
    }
    return batch.commands;
}

[[nodiscard]] inline const TunableRenderItem& tunable_item_for(const RenderBatch&   batch,
                                                               const RenderCommand& command) {
    if (!command.tunable_item_index.has_value()) {
        ADD_FAILURE() << "command has no tunable_item_index";
        static const TunableRenderItem fallback;
        return fallback;
    }
    if (*command.tunable_item_index >= batch.tunable_items.size()) {
        ADD_FAILURE() << "command tunable_item_index is out of range";
        static const TunableRenderItem fallback;
        return fallback;
    }
    return batch.tunable_items[*command.tunable_item_index];
}

struct TunableItemRef {
    const TunableRenderItem* item = nullptr;

    [[nodiscard]] bool                     has_value() const noexcept { return item != nullptr; }
    [[nodiscard]] explicit                 operator bool() const noexcept { return has_value(); }
    [[nodiscard]] const TunableRenderItem* operator->() const noexcept { return item; }
    [[nodiscard]] const TunableRenderItem& operator*() const noexcept { return *item; }
};

[[nodiscard]] inline TunableItemRef tunable_item(const RenderCommand& command) {
    if (!command.tunable_item_index.has_value()) {
        return {};
    }
    for (const RenderBatch* batch : render_batches()) {
        const auto* begin = batch->commands.data();
        const auto* end = begin + batch->commands.size();
        if (&command >= begin && &command < end) {
            return {.item = &tunable_item_for(*batch, command)};
        }
    }
    ADD_FAILURE() << "command batch was not registered with commands_from";
    return {};
}

} // namespace d2engine::test
