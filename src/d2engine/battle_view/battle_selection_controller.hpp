#pragma once

#include "battle_visual_event.hpp"

#include <optional>
#include <span>

namespace d2engine {

struct BattleSelectionModel {
    std::optional<UnitInstanceId> actor;
    std::optional<UnitInstanceId> target;
};

struct BattleSelectableUnit {
    UnitInstanceId id;
    bool           selectable = false;
};

struct BattleSelectionChange {
    std::optional<ActorSelected>  actor;
    std::optional<TargetSelected> target;
};

class BattleSelectionController {
public:
    [[nodiscard]] static BattleSelectionChange
    select_next_actor(BattleSelectionModel& model, std::span<const BattleSelectableUnit> units);
    [[nodiscard]] static std::optional<TargetSelected>
    select_next_target(BattleSelectionModel& model, std::span<const BattleSelectableUnit> units);

private:
    [[nodiscard]] static std::optional<UnitInstanceId>
    select_next(std::optional<UnitInstanceId> current, std::span<const BattleSelectableUnit> units);
    [[nodiscard]] static bool is_selectable(UnitInstanceId                        id,
                                            std::span<const BattleSelectableUnit> units);
};

} // namespace d2engine
