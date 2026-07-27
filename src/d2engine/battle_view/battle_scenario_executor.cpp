#include "battle_scenario_executor.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>
#include <variant>

namespace d2engine {

std::optional<BattleVisualEvent> BattleScenarioExecutor::execute_envelope(
    const BattleScenarioEventEnvelope& envelope, BattleScenarioRuntime& runtime,
    const std::string& sequence_id, const std::string& step_id, const std::string& scenario_path) {
    if (std::holds_alternative<ScenarioUnitCreated>(envelope.event)) {
        runtime.apply_unit_created(std::get<ScenarioUnitCreated>(envelope.event), envelope.id,
                                   step_id, sequence_id, scenario_path);
        return std::nullopt;
    }
    if (std::holds_alternative<ScenarioUnitRetreated>(envelope.event)) {
        runtime.apply_unit_retreated(std::get<ScenarioUnitRetreated>(envelope.event), envelope.id,
                                     step_id, sequence_id, scenario_path);
        return std::nullopt;
    }
    return resolve_event(envelope.event, runtime, envelope.id, step_id, sequence_id, scenario_path);
}

BattleVisualEvent BattleScenarioExecutor::resolve_event(const BattleScenarioEvent& event,
                                                        BattleScenarioRuntime&     runtime,
                                                        const std::string&         envelope_id,
                                                        const std::string&         step_id,
                                                        const std::string&         sequence_id,
                                                        const std::string&         scenario_path) {
    auto resolve = [&](const std::string& alias) -> UnitInstanceId {
        return runtime.resolve(alias, envelope_id, step_id, sequence_id, scenario_path);
    };
    auto validate_hp = [&](UnitInstanceId id, int hp, const std::string& alias,
                           const std::string& event_type, int min_hp = 0) {
        runtime.validate_unit_hp(id, hp, alias, event_type, min_hp);
    };
    auto resolve_targets = [&](const std::vector<std::string>& aliases) -> TargetSet {
        auto ids =
            runtime.resolve_targets(aliases, envelope_id, step_id, sequence_id, scenario_path);
        return TargetSet::unit_list(std::move(ids));
    };

    return std::visit(
        [&](const auto& e) -> BattleVisualEvent {
            using T = std::decay_t<decltype(e)>;

            if constexpr (std::is_same_v<T, ScenarioActorSelected>) {
                return ActorSelected{.previous = resolve(e.previous),
                                     .selected = resolve(e.selected)};
            } else if constexpr (std::is_same_v<T, ScenarioTargetSelected>) {
                return TargetSelected{.previous = resolve(e.previous),
                                      .selected = resolve(e.selected)};
            } else if constexpr (std::is_same_v<T, ScenarioAttackStarted>) {
                return AttackStarted{.attack_id = e.attack_id,
                                     .source = resolve(e.source),
                                     .targets = resolve_targets(e.targets)};
            } else if constexpr (std::is_same_v<T, ScenarioAttackImpactCue>) {
                return AttackImpactCue{.attack_id = e.attack_id,
                                       .source = resolve(e.source),
                                       .targets = resolve_targets(e.targets)};
            } else if constexpr (std::is_same_v<T, ScenarioTargetDamaged>) {
                const UnitInstanceId target = resolve(e.target);
                if (e.current_hp.has_value()) {
                    validate_hp(target, *e.current_hp, e.target, "TargetDamaged");
                }
                return TargetDamaged{.attack_id = e.attack_id,
                                     .source = resolve(e.source),
                                     .target = target,
                                     .current_hp = e.current_hp};
            } else if constexpr (std::is_same_v<T, ScenarioTargetMissed>) {
                return TargetMissed{.attack_id = e.attack_id, .target = resolve(e.target)};
            } else if constexpr (std::is_same_v<T, ScenarioTargetResisted>) {
                return TargetResisted{.attack_id = e.attack_id, .target = resolve(e.target)};
            } else if constexpr (std::is_same_v<T, ScenarioTargetKilled>) {
                const UnitInstanceId target = resolve(e.target);
                return TargetKilled{.attack_id = e.attack_id, .target = target};
            } else if constexpr (std::is_same_v<T, ScenarioLifeDrained>) {
                const UnitInstanceId source = resolve(e.source);
                const UnitInstanceId target = resolve(e.target);
                if (e.target_current_hp.has_value()) {
                    validate_hp(target, *e.target_current_hp, e.target, "LifeDrained");
                }
                if (e.source_current_hp.has_value()) {
                    validate_hp(source, *e.source_current_hp, e.source, "LifeDrained");
                }
                return LifeDrained{.attack_id = e.attack_id,
                                   .source = source,
                                   .target = target,
                                   .target_current_hp = e.target_current_hp,
                                   .source_current_hp = e.source_current_hp};
            } else if constexpr (std::is_same_v<T, ScenarioSourceHealed>) {
                const UnitInstanceId source = resolve(e.source);
                if (e.current_hp.has_value()) {
                    validate_hp(source, *e.current_hp, e.source, "SourceHealed");
                }
                return SourceHealed{
                    .attack_id = e.attack_id, .source = source, .current_hp = e.current_hp};
            } else if constexpr (std::is_same_v<T, ScenarioUnitHitReceived>) {
                return UnitHitReceived{.target = resolve(e.target)};
            } else if constexpr (std::is_same_v<T, ScenarioUnitKilled>) {
                const UnitInstanceId target = resolve(e.target);
                return UnitKilled{.target = target};
            } else if constexpr (std::is_same_v<T, ScenarioUnitReviveStarted>) {
                return UnitReviveStarted{.target = resolve(e.target)};
            } else if constexpr (std::is_same_v<T, ScenarioUnitRevived>) {
                const UnitInstanceId target = resolve(e.target);
                if (e.current_hp.has_value()) {
                    validate_hp(target, *e.current_hp, e.target, "UnitRevived", 1);
                }
                return UnitRevived{.target = target, .current_hp = e.current_hp};
            } else if constexpr (std::is_same_v<T, ScenarioCastEffectStarted>) {
                return CastEffectStarted{.caster = resolve(e.caster), .role = e.effect};
            } else if constexpr (std::is_same_v<T, ScenarioBattleEffectStarted>) {
                auto resolved_targets =
                    e.targets.empty() ? TargetSet{} : resolve_targets(e.targets);
                UnitInstanceId resolved_target{};
                if (!e.target.empty()) {
                    resolved_target = resolve(e.target);
                }
                return BattleEffectStarted{.source = resolve(e.source),
                                           .role = e.effect,
                                           .visual_role = e.visual_role,
                                           .target = resolved_target,
                                           .targets = std::move(resolved_targets)};
            } else if constexpr (std::is_same_v<T, ScenarioUnitCreated>) {
                throw std::logic_error(
                    "BattleScenarioExecutor::resolve_event: lifecycle event ScenarioUnitCreated"
                    " (alias=" +
                    e.alias + ") cannot be resolved as a visual event");
            } else if constexpr (std::is_same_v<T, ScenarioUnitRetreated>) {
                throw std::logic_error(
                    "BattleScenarioExecutor::resolve_event: lifecycle event ScenarioUnitRetreated"
                    " (unit=" +
                    e.unit + ") cannot be resolved as a visual event");
            } else {
                static_assert(unsupported_battle_visual_event<T>);
            }
        },
        event);
}

void BattleScenarioPlayer::load(BattleScenario scenario, const std::string& scenario_path) {
    scenario_ = std::move(scenario);
    scenario_path_ = scenario_path;
    playing_ = false;
    completed_ = false;
    sequence_index_ = 0;
    step_index_ = 0;
    step_submitted_ = false;
    current_step_id_.clear();
    current_sequence_id_.clear();
}

void BattleScenarioPlayer::play(const std::string& sequence_id, std::size_t start_step_index) {
    auto it = std::ranges::find_if(scenario_.sequences,
                                   [&](const auto& seq) { return seq.id == sequence_id; });
    if (it == scenario_.sequences.end()) {
        throw std::runtime_error("BattleScenarioPlayer::play: sequence not found: " + sequence_id);
    }
    const auto& seq = *it;
    if (start_step_index > seq.steps.size()) {
        throw std::runtime_error("BattleScenarioPlayer::play: start_step_index " +
                                 std::to_string(start_step_index) + " exceeds step count " +
                                 std::to_string(seq.steps.size()) + " for sequence " + sequence_id);
    }
    sequence_index_ = static_cast<std::size_t>(std::distance(scenario_.sequences.begin(), it));
    step_index_ = start_step_index;
    playing_ = true;
    completed_ = false;
    step_submitted_ = false;
    current_sequence_id_ = sequence_id;
    current_step_id_.clear();
}

namespace {

bool is_lifecycle_event(const BattleScenarioEvent& event) {
    return std::holds_alternative<ScenarioUnitCreated>(event) ||
           std::holds_alternative<ScenarioUnitRetreated>(event);
}

std::string step_composition_error(const std::string& scenario_path, const std::string& seq_id,
                                   const std::string& step_id, const std::string& env_id,
                                   const std::string& message) {
    std::string out;
    out += "scenario=" + scenario_path;
    out += " seq=" + seq_id;
    out += " step=" + step_id;
    out += " envelope=" + env_id;
    out += " " + message;
    return out;
}

} // namespace

void BattleScenarioPlayer::update(IBattlePresentationSink& presentation,
                                  BattleScenarioRuntime&   runtime) {
    if (!playing_ || completed_) {
        return;
    }

    auto& sequence = scenario_.sequences[sequence_index_];

    if (step_index_ >= sequence.steps.size()) {
        completed_ = true;
        playing_ = false;
        return;
    }

    auto& step = sequence.steps[step_index_];

    if (!step_submitted_) {
        // ── Step composition validation ─────────────────────────────────
        // Rule 1: setup_units may only contain UnitCreated events
        if (step.id == "setup_units") {
            for (const auto& scenario_env : step.envelopes) {
                if (!std::holds_alternative<ScenarioUnitCreated>(scenario_env.event)) {
                    throw std::runtime_error(step_composition_error(
                        scenario_path_, current_sequence_id_, step.id, scenario_env.id,
                        "setup_units step may only contain UnitCreated events. "
                        "Found non-lifecycle event in setup_units."));
                }
            }
        } else {
            // Rule 2: regular steps must be all-lifecycle or all-visual, never mixed
            bool has_lifecycle = false;
            bool has_visual = false;
            for (const auto& scenario_env : step.envelopes) {
                if (is_lifecycle_event(scenario_env.event)) {
                    has_lifecycle = true;
                } else {
                    has_visual = true;
                }
            }
            if (has_lifecycle && has_visual) {
                // Find the first lifecycle envelope to report
                for (const auto& scenario_env : step.envelopes) {
                    if (is_lifecycle_event(scenario_env.event)) {
                        throw std::runtime_error(step_composition_error(
                            scenario_path_, current_sequence_id_, step.id, scenario_env.id,
                            "lifecycle events (UnitCreated/UnitRetreated) must be in "
                            "lifecycle-only immediate steps. "
                            "Cannot mix with visual events in the same step."));
                    }
                }
            }
        }

        // ── Lifecycle-only step ─────────────────────────────────────────
        // Detect if every event in this step is a lifecycle event.
        bool all_lifecycle = true;
        bool has_any_lifecycle = false;
        for (const auto& scenario_env : step.envelopes) {
            if (is_lifecycle_event(scenario_env.event)) {
                has_any_lifecycle = true;
            } else {
                all_lifecycle = false;
            }
        }

        if (all_lifecycle && has_any_lifecycle) {
            for (const auto& scenario_env : step.envelopes) {
                if (scenario_env.at.event_id.has_value() || scenario_env.at.cue != "start") {
                    throw std::runtime_error(
                        "UnitCreated/UnitRetreated cannot use at/cue scheduling. "
                        "Envelope '" +
                        scenario_env.id + "' in step '" + step.id + "' sequence '" +
                        current_sequence_id_ + "' has scheduling which is not yet supported.");
                }

                // Busy check for UnitRetreated in lifecycle-only branch:
                // a retreat must not execute while animation engine is busy
                if (std::holds_alternative<ScenarioUnitRetreated>(scenario_env.event) &&
                    (presentation.has_active_visual_step() || presentation.animation_busy())) {
                    throw std::runtime_error(step_composition_error(
                        scenario_path_, current_sequence_id_, step.id, scenario_env.id,
                        "UnitRetreated cannot execute while visual step is active or "
                        "animation engine is busy. Wait for current visual step to complete "
                        "before retreating units."));
                }

                BattleScenarioExecutor::execute_envelope(
                    scenario_env, runtime, current_sequence_id_, step.id, scenario_path_);
            }
            current_step_id_ = step.id;
            ++step_index_;
            current_step_id_.clear();
            return;
        }

        // ── Visual step ─────────────────────────────────────────────────
        std::vector<BattleVisualEventEnvelope> visual_envelopes;

        for (const auto& scenario_env : step.envelopes) {
            // Reject UnitRetreated during active visual execution
            if (std::holds_alternative<ScenarioUnitRetreated>(scenario_env.event) &&
                (presentation.has_active_visual_step() || presentation.animation_busy())) {
                throw std::runtime_error(step_composition_error(
                    scenario_path_, current_sequence_id_, step.id, scenario_env.id,
                    "UnitRetreated cannot execute while visual step is active or "
                    "animation engine is busy. Wait for current visual step to complete "
                    "before retreating units."));
            }

            auto result = BattleScenarioExecutor::execute_envelope(
                scenario_env, runtime, current_sequence_id_, step.id, scenario_path_);

            if (result.has_value()) {
                visual_envelopes.push_back(BattleVisualEventEnvelope{
                    .id = scenario_env.id,
                    .optional = scenario_env.optional,
                    .at =
                        BattleVisualEventSchedule{
                            .event_id = scenario_env.at.event_id,
                            .cue = scenario_env.at.cue,
                        },
                    .event = std::move(result.value()),
                });
            }
        }

        BattleVisualStep visual_step;
        visual_step.id = step.id;
        visual_step.complete = step.complete;
        visual_step.envelopes = std::move(visual_envelopes);

        const auto exec_result = presentation.submit_visual_step(visual_step);
        current_step_id_ = step.id;
        step_submitted_ = true;

        if (exec_result.failed || presentation.visual_step_failed()) {
            std::string diag;
            for (const auto& d : exec_result.diagnostics) {
                diag += d;
                diag += "; ";
            }
            presentation.cancel_visual_step();
            step_submitted_ = false;
            current_step_id_.clear();
            throw std::runtime_error("BattleScenarioPlayer: visual step '" + step.id +
                                     "' failed: " + diag);
        }

        // If the step completed immediately (e.g. all commandless events),
        // finish it now and advance on the same update call.
        if (exec_result.complete) {
            presentation.finish_visual_step();
            ++step_index_;
            step_submitted_ = false;
            current_step_id_.clear();
        }
        return;
    }

    // A visual step is in progress — check for late failure
    if (presentation.visual_step_failed()) {
        const auto& exec = presentation.visual_step_execution();
        std::string diag;
        for (const auto& d : exec.diagnostics) {
            diag += d;
            diag += "; ";
        }
        const std::string failed_step_id = current_step_id_;
        presentation.cancel_visual_step();
        step_submitted_ = false;
        current_step_id_.clear();
        throw std::runtime_error("BattleScenarioPlayer: visual step '" + failed_step_id +
                                 "' failed during playback: " + diag);
    }

    if (presentation.visual_step_complete()) {
        presentation.finish_visual_step();
        ++step_index_;
        step_submitted_ = false;
        current_step_id_.clear();
    }
}

} // namespace d2engine
