#pragma once

#include "battle_scenario_data.hpp"
#include "battle_scenario_runtime.hpp"
#include "battle_visual_event.hpp"
#include "battle_visual_step.hpp"
#include "i_battle_presentation_sink.hpp"

#include <optional>
#include <string>
#include <vector>

namespace d2engine {

// Converts a scenario-level envelope into one or more runtime actions.
// Handles UnitCreated/UnitRetreated lifecycle events,
// and resolves string refs to UnitInstanceId for visual events.
struct BattleScenarioExecutor {
    // Execute a scenario event envelope against the runtime.
    // Returns the resolved BattleVisualEvent if applicable.
    // For lifecycle events, returns std::nullopt (already handled).
    // For visual events, resolves aliases and returns the produced BattleVisualEvent.
    static std::optional<BattleVisualEvent>
    execute_envelope(const BattleScenarioEventEnvelope& envelope, BattleScenarioRuntime& runtime,
                     const std::string& sequence_id = {}, const std::string& step_id = {},
                     const std::string& scenario_path = {});

    // Convert a scenario event (with string refs) to a BattleVisualEvent (with resolved ids).
    // Throws on unknown alias.
    static BattleVisualEvent
    resolve_event(const BattleScenarioEvent& event, BattleScenarioRuntime& runtime,
                  const std::string& envelope_id = {}, const std::string& step_id = {},
                  const std::string& sequence_id = {}, const std::string& scenario_path = {});
};

// Scenario-driven sequence player that processes scenario steps through runtime.
// Works with scenario-level events and forwards resolved visual events to the presentation sink.
class BattleScenarioPlayer {
public:
    void load(BattleScenario scenario, const std::string& scenario_path = {});
    void play(const std::string& sequence_id, std::size_t start_step_index = 0);
    void update(IBattlePresentationSink& presentation, BattleScenarioRuntime& runtime);

    [[nodiscard]] const std::string& current_step_id() const { return current_step_id_; }
    [[nodiscard]] const std::string& current_sequence_id() const { return current_sequence_id_; }
    [[nodiscard]] bool               is_playing() const { return playing_; }
    [[nodiscard]] bool               is_completed() const { return completed_; }

private:
    BattleScenario scenario_;
    std::size_t    sequence_index_ = 0;
    std::size_t    step_index_ = 0;
    bool           playing_ = false;
    bool           completed_ = false;
    bool           step_submitted_ = false;
    std::string    current_step_id_;
    std::string    current_sequence_id_;
    std::string    scenario_path_;
};

} // namespace d2engine
