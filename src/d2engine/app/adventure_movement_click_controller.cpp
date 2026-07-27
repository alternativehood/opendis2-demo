#include "adventure_movement_click_controller.hpp"

namespace d2engine {

std::optional<d2game::GameCommandResult>
AdventureMovementClickController::handle_left_click(d2game::GameSession&                session,
                                                    const AdventureMovementClickTarget& target) {
    if (target.stack_id) {
        return session.handle_command(d2game::GameSelectAdventureStackCommand{*target.stack_id});
    }
    if (target.cell && session.adventure_movement_state().selected_stack_id) {
        const auto& state = session.adventure_movement_state();
        if (state.mode == d2game::AdventureInteractionMode::RoutePlanned &&
            state.planned.has_value() && *target.cell == state.planned->route.destination) {
            return session.handle_command(d2game::GameConfirmAdventureMovementCommand{});
        }
        return session.handle_command(d2game::GamePlanAdventureMovementCommand{*target.cell});
    }
    return std::nullopt;
}

} // namespace d2engine
