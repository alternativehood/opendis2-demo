#pragma once

#include <d2game/GameSession.hpp>

#include <optional>
#include <string>

namespace d2engine {

struct AdventureMovementClickTarget {
    std::optional<std::string>             stack_id;
    std::optional<d2runtime::MapCellCoord> cell;
};

class AdventureMovementClickController {
public:
    [[nodiscard]] static std::optional<d2game::GameCommandResult>
    handle_left_click(d2game::GameSession& session, const AdventureMovementClickTarget& target);
};

} // namespace d2engine
