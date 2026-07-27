#pragma once

#include <string>
#include <string_view>

namespace d2engine::AnimationNames {

inline std::string build(const std::string& unit_type, const std::string& role_suffix, int variant,
                         char direction) {
    return "G000" + unit_type + role_suffix + std::to_string(variant) + direction + "00";
}

} // namespace d2engine::AnimationNames

namespace d2engine::AnimationRoles {

inline constexpr std::string_view IDLE = "IDLEA";
inline constexpr std::string_view HIT = "HHITA";
inline constexpr std::string_view DEATH = "STILA";
inline constexpr std::string_view ATTACK = "HMOVA";
inline constexpr std::string_view HEFF = "HEFFA";
inline constexpr std::string_view TUCH = "TUCHA";

} // namespace d2engine::AnimationRoles
