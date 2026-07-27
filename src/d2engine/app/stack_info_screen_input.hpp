#pragma once

#include "../input/input_event.hpp"

#include <cstdint>
#include <optional>
#include <variant>

namespace d2engine {

struct StackInfoCancel {};

using StackInfoAction = std::variant<StackInfoCancel>;

class StackInfoScreenInputHandler {
public:
    [[nodiscard]] static std::optional<StackInfoAction> handle(const InputEvent& event);
};

} // namespace d2engine
