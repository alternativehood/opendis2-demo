#pragma once

#include "GameCommand.hpp"
#include "GameEvent.hpp"
#include "AdventureMovementState.hpp"
#include "AdventureUnitMovementProfileCatalog.hpp"

#include <d2adventure_rules/AdventureRoutePreview.hpp>

#include <d2runtime/AdventureWorldState.hpp>

#include <string>
#include <optional>
#include <string_view>
#include <vector>

namespace d2game {

struct WorldInspectSummary {
    std::string scenario_id;
    std::string scenario_name;
    int         map_width = 0;
    int         map_height = 0;
    int         terrain_tiles = 0;
    std::size_t semantic_objects = 0;
    std::size_t runtime_objects = 0;
    std::size_t build_warnings = 0;
    std::size_t build_errors = 0;
};

class GameSession {
public:
    explicit GameSession(d2runtime::AdventureWorldState world, std::size_t build_warnings = 0,
                         std::size_t                         build_errors = 0,
                         AdventureUnitMovementProfileCatalog movement_profiles = {});

    GameCommandResult handle_command(const GameCommand& cmd);

    [[nodiscard]] const d2runtime::AdventureWorldState& world() const { return world_; }
    [[nodiscard]] const AdventureMovementState&         adventure_movement_state() const {
        return adventure_movement_state_;
    }
    [[nodiscard]] std::optional<d2adventure::AdventureRoutePreview> adventure_route_preview() const;
    [[nodiscard]] AdventureMovementDebugSnapshot adventure_movement_debug_snapshot() const;
    [[nodiscard]] WorldInspectSummary            inspect() const;

private:
    [[nodiscard]] GameCommandResult handle(const GameQuitCommand&);
    [[nodiscard]] GameCommandResult handle(const GameNoOpCommand&);
    [[nodiscard]] GameCommandResult handle(const GameInspectWorldCommand&);
    [[nodiscard]] GameCommandResult handle(const GameAdvanceFrameCommand&);
    [[nodiscard]] GameCommandResult handle(const GameSelectAdventureStackCommand&);
    [[nodiscard]] GameCommandResult handle(const GameClearAdventureSelectionCommand&);
    [[nodiscard]] GameCommandResult handle(const GamePlanAdventureMovementCommand&);
    [[nodiscard]] GameCommandResult handle(const GameConfirmAdventureMovementCommand&);
    [[nodiscard]] GameCommandResult handle(const GameAdvanceAdventureMovementStepCommand&);
    [[nodiscard]] GameCommandResult
    handle(const GameDebugResetSelectedAdventureMovementPointsCommand&);
    [[nodiscard]] GameCommandResult
    handle(const GameDebugGrantSelectedAdventureFreeMovementPointsCommand&);

    [[nodiscard]] d2runtime::AdventureStack*       mutable_stack(std::string_view id);
    [[nodiscard]] const d2runtime::AdventureStack* selected_stack() const;
    [[nodiscard]] const d2runtime::AdventureUnitInstance*
    leader_instance(const d2runtime::AdventureStack& stack) const;
    [[nodiscard]] std::optional<d2adventure::AdventureMovementProfile>
    movement_profile(const d2runtime::AdventureStack& stack) const;
    [[nodiscard]] std::optional<d2adventure::AdventureRoute>
    find_route(const d2runtime::AdventureStack&      stack,
               d2adventure::AdventureMovementProfile profile, d2runtime::MapCellCoord destination,
               GameCommandResult& result, AdventureMovementAction action) const;
    [[nodiscard]] AdventurePlannedMovement
         planned_movement(const d2runtime::AdventureStack&      stack,
                          d2adventure::AdventureMovementProfile profile,
                          d2adventure::AdventureRoute           route) const;
    void reset_idle();
    void reset_selected();
    void interrupt(GameCommandResult& result, AdventureMovementInterruptionReason reason,
                   d2adventure::AdventureMovementBlockReason block_reason =
                       d2adventure::AdventureMovementBlockReason::None);
    [[nodiscard]] std::optional<int> initial_movement_points(std::string_view stack_id) const;
    void                             refresh_planned_movement_after_debug_change();
    [[nodiscard]] GameCommandResult
    change_selected_movement_points(AdventureMovementDebugChange change);

    struct InitialAdventureStackMovementPoints {
        std::string stack_id;
        int         movement_points = 0;
    };

    d2runtime::AdventureWorldState                   world_;
    std::size_t                                      build_warnings_ = 0;
    std::size_t                                      build_errors_ = 0;
    AdventureUnitMovementProfileCatalog              movement_profiles_;
    AdventureMovementState                           adventure_movement_state_;
    std::vector<InitialAdventureStackMovementPoints> initial_stack_movement_points_;
};

} // namespace d2game
