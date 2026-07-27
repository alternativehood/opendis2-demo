#pragma once

#include <cstdint>

namespace d2engine {

inline constexpr int kMinDrawLevel = -8;
inline constexpr int kMaxDrawLevel = 8;

enum class DebugTuningEditKind : std::uint8_t {
    MoveUp,
    MoveDown,
    MoveLeft,
    MoveRight,
    WidthDecrease,
    WidthIncrease,
    HeightIncrease,
    HeightDecrease,
    ScaleBothDecrease,
    ScaleBothIncrease,
    LevelIncrease,
    LevelDecrease,
    ParameterIncrease,
    ParameterDecrease,
};

struct DebugTuningEditAction {
    DebugTuningEditKind kind;
    float               step = 1.0f;
};

} // namespace d2engine
