#include "battle_render_tree_helpers.hpp"

#include <algorithm>
#include <string>

namespace d2engine {

[[nodiscard]] std::string unit_group_slot_path(BattleSlotCoord coord) {
    const char* panel = coord.side == BattleSide::Attacker ? "left_unit_group" : "right_unit_group";
    const char* depth = coord.depth == BattleDepth::Front  ? "front"
                        : coord.depth == BattleDepth::Back ? "back"
                                                           : "center";
    return "/ui/" + std::string(panel) + "/" + std::to_string(coord.lane) + "_" + depth;
}

[[nodiscard]] std::string hp_text(int current_hp, int max_hp) {
    return std::to_string(current_hp) + "/" + std::to_string(max_hp);
}

[[nodiscard]] Rect union_rect(const Rect& a, const Rect& b) {
    if (a.w <= 0.0f && a.h <= 0.0f) {
        return b;
    }
    if (b.w <= 0.0f && b.h <= 0.0f) {
        return a;
    }
    const float x1 = std::min(a.x, b.x);
    const float y1 = std::min(a.y, b.y);
    const float x2 = std::max(a.x + a.w, b.x + b.w);
    const float y2 = std::max(a.y + a.h, b.y + b.h);
    return {.x = x1, .y = y1, .w = x2 - x1, .h = y2 - y1};
}

[[nodiscard]] float missing_hp_ratio(int current_hp, int max_hp) {
    if (max_hp <= 0) {
        return 0.0f;
    }
    return 1.0f -
           std::clamp(static_cast<float>(current_hp) / static_cast<float>(max_hp), 0.0f, 1.0f);
}

void apply_text_style(RenderCommand& command, const TreeLayout& tree, const std::string& path,
                      const std::string& font_face) {
    if (const auto node = tree.node(path); node.has_value()) {
        command.text_color = node->color;
        command.font_size = node->font_size;
    }
    command.game_font_text = true;
    command.font_face = font_face;
}

[[nodiscard]] Rect portrait_damage_basis(const SnapshotEntity& entity, const TreeLayout& tree,
                                         const std::string& portrait_path) {
    if (!entity.is_large || entity.coord.depth != BattleDepth::Center) {
        return tree.compose(portrait_path);
    }
    BattleSlotCoord back = entity.coord;
    back.depth = BattleDepth::Back;
    BattleSlotCoord front = entity.coord;
    front.depth = BattleDepth::Front;
    return union_rect(tree.compose(unit_group_slot_path(back) + "/portrait"),
                      tree.compose(unit_group_slot_path(front) + "/portrait"));
}
const SnapshotEntity* find_entity_by_unit(const BattleRenderSnapshot& snapshot, UnitInstanceId id) {
    if (id.value == 0) {
        return nullptr;
    }
    const auto it = std::ranges::find_if(snapshot.entities, [id](const SnapshotEntity& entity) {
        return entity.unit_instance_id == id;
    });
    return it != snapshot.entities.end() ? &*it : nullptr;
}

} // namespace d2engine
