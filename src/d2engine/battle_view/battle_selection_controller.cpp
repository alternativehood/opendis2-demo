#include "battle_selection_controller.hpp"

namespace d2engine {

std::optional<UnitInstanceId>
BattleSelectionController::select_next(std::optional<UnitInstanceId>         current,
                                       std::span<const BattleSelectableUnit> units) {
    if (units.empty()) {
        return current;
    }

    std::size_t current_idx = units.size() - 1u;
    if (current.has_value()) {
        for (std::size_t i = 0; i < units.size(); ++i) {
            if (units[i].id == *current) {
                current_idx = i;
                break;
            }
        }
    }

    for (std::size_t i = 1; i <= units.size(); ++i) {
        const std::size_t candidate = (current_idx + i) % units.size();
        if (units[candidate].selectable) {
            return units[candidate].id;
        }
    }
    return current;
}

bool BattleSelectionController::is_selectable(UnitInstanceId                        id,
                                              std::span<const BattleSelectableUnit> units) {
    for (const auto& unit : units) {
        if (unit.id == id) {
            return unit.selectable;
        }
    }
    return false;
}

BattleSelectionChange
BattleSelectionController::select_next_actor(BattleSelectionModel&                 model,
                                             std::span<const BattleSelectableUnit> units) {
    BattleSelectionChange change;
    const auto            previous_actor = model.actor;
    model.actor = select_next(model.actor, units);
    if (previous_actor != model.actor && previous_actor.has_value() && model.actor.has_value()) {
        change.actor = ActorSelected{.previous = *previous_actor, .selected = *model.actor};
    }

    if (!model.target.has_value() || !is_selectable(*model.target, units)) {
        const auto previous_target = model.target;
        model.target = select_next(std::nullopt, units);
        if (previous_target != model.target && previous_target.has_value() &&
            model.target.has_value()) {
            change.target = TargetSelected{.previous = *previous_target, .selected = *model.target};
        }
    }
    return change;
}

std::optional<TargetSelected>
BattleSelectionController::select_next_target(BattleSelectionModel&                 model,
                                              std::span<const BattleSelectableUnit> units) {
    const auto previous = model.target;
    model.target = select_next(model.target, units);
    if (previous != model.target && previous.has_value() && model.target.has_value()) {
        return TargetSelected{.previous = *previous, .selected = *model.target};
    }
    return std::nullopt;
}

} // namespace d2engine
