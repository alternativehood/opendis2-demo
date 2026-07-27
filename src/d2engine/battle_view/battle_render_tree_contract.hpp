#pragma once

#include "battle_slot.hpp"
#include "../render/render_tree.hpp"

#include <array>
#include <string>
#include <vector>

namespace d2engine {

inline constexpr int kBattlefieldLayoutSlotCount = 18;

inline constexpr std::array<BattleSlotCoord, kBattlefieldLayoutSlotCount> kBattlefieldLayoutCoords =
    {{
        {.side = BattleSide::Attacker, .lane = 0, .depth = BattleDepth::Back},
        {.side = BattleSide::Attacker, .lane = 0, .depth = BattleDepth::Front},
        {.side = BattleSide::Attacker, .lane = 1, .depth = BattleDepth::Back},
        {.side = BattleSide::Attacker, .lane = 1, .depth = BattleDepth::Front},
        {.side = BattleSide::Attacker, .lane = 2, .depth = BattleDepth::Back},
        {.side = BattleSide::Attacker, .lane = 2, .depth = BattleDepth::Front},
        {.side = BattleSide::Defender, .lane = 0, .depth = BattleDepth::Back},
        {.side = BattleSide::Defender, .lane = 0, .depth = BattleDepth::Front},
        {.side = BattleSide::Defender, .lane = 1, .depth = BattleDepth::Back},
        {.side = BattleSide::Defender, .lane = 1, .depth = BattleDepth::Front},
        {.side = BattleSide::Defender, .lane = 2, .depth = BattleDepth::Back},
        {.side = BattleSide::Defender, .lane = 2, .depth = BattleDepth::Front},
        {.side = BattleSide::Attacker, .lane = 0, .depth = BattleDepth::Center},
        {.side = BattleSide::Attacker, .lane = 1, .depth = BattleDepth::Center},
        {.side = BattleSide::Attacker, .lane = 2, .depth = BattleDepth::Center},
        {.side = BattleSide::Defender, .lane = 0, .depth = BattleDepth::Center},
        {.side = BattleSide::Defender, .lane = 1, .depth = BattleDepth::Center},
        {.side = BattleSide::Defender, .lane = 2, .depth = BattleDepth::Center},
    }};

[[nodiscard]] std::string              battlefield_slot_tree_path(BattleSlotCoord coord);
[[nodiscard]] std::string              battlefield_unit_tree_path(BattleSlotCoord coord);
[[nodiscard]] std::vector<std::string> battle_render_tree_diagnostics(const RenderTree& tree);
void                                   validate_required_battle_nodes(const RenderTree& tree);

} // namespace d2engine
