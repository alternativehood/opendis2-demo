#pragma once

#include "battle_visual_event.hpp"
#include "battle_visual_step.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace d2engine {

// Scenario-level events carry string unit references instead of UnitInstanceId.
// These are resolved just-in-time by BattleScenarioRuntime during event execution.

struct ScenarioActorSelected {
    std::string previous;
    std::string selected;
};

struct ScenarioTargetSelected {
    std::string previous;
    std::string selected;
};

struct ScenarioAttackStarted {
    AttackInstanceId         attack_id;
    std::string              source;
    std::vector<std::string> targets;
};

struct ScenarioAttackImpactCue {
    AttackInstanceId         attack_id;
    std::string              source;
    std::vector<std::string> targets;
};

struct ScenarioTargetDamaged {
    AttackInstanceId   attack_id;
    std::string        source;
    std::string        target;
    std::optional<int> current_hp;
};

struct ScenarioTargetMissed {
    AttackInstanceId attack_id;
    std::string      target;
};

struct ScenarioTargetResisted {
    AttackInstanceId attack_id;
    std::string      target;
};

struct ScenarioTargetKilled {
    AttackInstanceId attack_id;
    std::string      target;
};

struct ScenarioLifeDrained {
    AttackInstanceId   attack_id;
    std::string        source;
    std::string        target;
    std::optional<int> target_current_hp;
    std::optional<int> source_current_hp;
};

struct ScenarioSourceHealed {
    AttackInstanceId   attack_id;
    std::string        source;
    std::optional<int> current_hp;
};

struct ScenarioUnitHitReceived {
    std::string target;
};

struct ScenarioUnitKilled {
    std::string target;
};

struct ScenarioUnitReviveStarted {
    std::string target;
};

struct ScenarioUnitRevived {
    std::string        target;
    std::optional<int> current_hp;
};

struct ScenarioCastEffectStarted {
    std::string      caster;
    BattleEffectRole effect = BattleEffectRole::Heff;
};

struct ScenarioBattleEffectStarted {
    std::string              source;
    BattleEffectRole         effect = BattleEffectRole::Heff;
    BattleEffectVisualRole   visual_role = BattleEffectVisualRole::TargetDamageFx;
    std::string              target;
    std::vector<std::string> targets;
};

// Lifecycle events
struct ScenarioUnitCreated {
    std::string              alias;
    std::string              unit_type;
    std::string              slot;
    std::optional<int>       hp;
    std::optional<int>       max_hp;
    std::vector<std::string> status;
};

struct ScenarioUnitRetreated {
    std::string unit;
    std::string reason;
};

using BattleScenarioEvent =
    std::variant<ScenarioActorSelected, ScenarioTargetSelected, ScenarioAttackStarted,
                 ScenarioAttackImpactCue, ScenarioTargetDamaged, ScenarioTargetMissed,
                 ScenarioTargetResisted, ScenarioTargetKilled, ScenarioLifeDrained,
                 ScenarioSourceHealed, ScenarioUnitHitReceived, ScenarioUnitKilled,
                 ScenarioUnitReviveStarted, ScenarioUnitRevived, ScenarioCastEffectStarted,
                 ScenarioBattleEffectStarted, ScenarioUnitCreated, ScenarioUnitRetreated>;

struct BattleScenarioEventSchedule {
    std::optional<std::string> event_id;
    std::string                cue = "start";
};

struct BattleScenarioEventEnvelope {
    std::string                 id;
    bool                        optional = false;
    BattleScenarioEventSchedule at;
    BattleScenarioEvent         event;
};

struct BattleScenarioStep {
    std::string                              id;
    BattleVisualStepCompletion               complete = BattleVisualStepCompletion::Immediate;
    std::vector<BattleScenarioEventEnvelope> envelopes;
};

struct BattleScenarioSequence {
    std::string                     id;
    std::vector<BattleScenarioStep> steps;
};

struct BattleScenario {
    int                                 version = 3;
    std::string                         id;
    std::string                         terrain = "HU";
    std::optional<std::string>          autoplay;
    std::vector<BattleScenarioSequence> sequences;
};

} // namespace d2engine
