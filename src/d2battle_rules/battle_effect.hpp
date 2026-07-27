#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace d2battle {

struct PetrifiedEffect {
    std::string source_actor_id;
    std::string source_attack_id;

    std::uint32_t remaining_activation_skips = 1;

    bool operator==(const PetrifiedEffect&) const = default;
};

using BattleUnitEffectState = std::variant<PetrifiedEffect>;

} // namespace d2battle
