#pragma once

#include "battle_effect.hpp"
#include "battle_types.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace d2battle {

struct BattleUnitState {
    std::string id;
    std::string type_id;

    int serialized_level = 0;

    std::vector<std::string> modifier_ids;

    int         creation = 0;
    std::string name;

    std::uint8_t                transformed = 0;
    std::optional<std::uint8_t> dynamic_level;

    int current_hp = 0;
    int xp = 0;

    BattleSide side = BattleSide::Party1;
    int        member_index = 0;
    int        formation_cell = -1;

    bool alive = true;

    std::vector<BattleUnitEffectState> effects;

    bool operator==(const BattleUnitState&) const = default;
};

} // namespace d2battle
