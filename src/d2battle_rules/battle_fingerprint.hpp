#pragma once

#include "battle_state.hpp"

#include <string>

namespace d2battle {

[[nodiscard]] std::string compute_fingerprint(const BattleState& state);

} // namespace d2battle
