#include "battle_render_command_builder.hpp"

#include "battle_render_batch_helpers.hpp"
#include "render/battle_background_command_builder.hpp"
#include "render/battle_debug_command_builder.hpp"
#include "render/battle_ui_command_builder.hpp"
#include "render/battle_unit_track_command_builder.hpp"

#include <algorithm>

namespace d2engine {

RenderBatch BattleRenderCommandBuilder::build(const BattleRenderSnapshot& snapshot,
                                              IBattleTextureProvider&     textures,
                                              const BattleRenderOptions&  options) {
    RenderBatch batch;
    if (options.tree_layout != nullptr) {
        append_render_queue(batch, snapshot, textures, options);
    }
    append_ground_background(batch, textures, options);
    append_background(batch, textures, options);
    append_background_overlays(batch, textures, options);
    if (options.debug_enabled) {
        append_background_container(batch, options);
    }
    append_combat_frame(batch, textures, options);
    append_unit_groups(batch, snapshot, textures, options);
    append_bottom_portraits(batch, snapshot, options);
    append_battle_frame_info(batch, snapshot, options);
    if (options.debug_enabled) {
        append_tree_layout_items(batch, options);
    }
    std::ranges::stable_sort(batch.commands, {},
                             [](const RenderCommand& command) { return command.layer.order; });
    if (!options.solo_filter.empty()) {
        RenderBatch filtered;
        for (RenderCommand command : batch.commands) {
            if (!command.tunable_item_index.has_value() ||
                *command.tunable_item_index >= batch.tunable_items.size()) {
                continue;
            }
            const auto& item = batch.tunable_items[*command.tunable_item_index];
            if (item.stable_id != options.solo_filter) {
                continue;
            }
            command.tunable_item_index = append_tunable_item(filtered, item);
            filtered.commands.push_back(std::move(command));
        }
        return filtered;
    }
    return batch;
}

} // namespace d2engine
