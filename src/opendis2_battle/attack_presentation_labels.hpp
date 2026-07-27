#pragma once

#include <d2engine/assets/attack_def.hpp>

#include <string_view>

namespace d2battle_terminal {

[[nodiscard]] std::string_view attack_class_label(d2engine::AttackClass value);

[[nodiscard]] std::string_view attack_reach_label(d2engine::AttackReach value);

} // namespace d2battle_terminal
