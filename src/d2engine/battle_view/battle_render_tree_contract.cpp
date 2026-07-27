#include "battle_render_tree_contract.hpp"

#include <cstdio>
#include <stdexcept>
#include <vector>

namespace d2engine {

std::string battlefield_slot_tree_path(BattleSlotCoord coord) {
    const char  side = (coord.side == BattleSide::Attacker) ? 'a' : 'd';
    const char* depth = "center";
    if (coord.depth == BattleDepth::Front) {
        depth = "front";
    } else if (coord.depth == BattleDepth::Back) {
        depth = "back";
    }
    char buf[48];
    std::snprintf(buf, sizeof(buf), "/battlefield/slot_%c_%s_%d", side, depth, coord.lane);
    return std::string{buf};
}

std::string battlefield_unit_tree_path(BattleSlotCoord coord) {
    return battlefield_slot_tree_path(coord) + "/unit";
}

std::vector<std::string> battle_render_tree_diagnostics(const RenderTree& tree) {
    std::vector<std::string> result;
    for (const auto& [path, node] : tree.entries()) {
        const bool tolerated_ui_offset =
            path.rfind("/ui/", 0) == 0 && node.x >= -4.0f && node.y >= -4.0f;
        if (node.x < 0.0f && path != "/background/battle" && !tolerated_ui_offset) {
            result.push_back("render_tree local x < 0: " + path);
        }
        if (node.y < 0.0f && !tolerated_ui_offset) {
            result.push_back("render_tree local y < 0: " + path);
        }
    }
    return result;
}

void validate_required_battle_nodes(const RenderTree& tree) {
    auto issues = tree.diagnostics();
    auto battle_issues = battle_render_tree_diagnostics(tree);
    issues.insert(issues.end(), battle_issues.begin(), battle_issues.end());
    if (!issues.empty()) {
        throw std::runtime_error(issues.front());
    }
    for (const BattleSlotCoord coord : kBattlefieldLayoutCoords) {
        const std::string slot_path = battlefield_slot_tree_path(coord);
        if (!tree.has_node(slot_path)) {
            throw std::runtime_error("missing required render_tree node: " + slot_path);
        }
        const std::string unit_path = battlefield_unit_tree_path(coord);
        if (!tree.has_node(unit_path)) {
            throw std::runtime_error("missing required render_tree node: " + unit_path);
        }
    }
}

} // namespace d2engine
