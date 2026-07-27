#include "battle_scenario_bootstrapper.hpp"
#include "battle_scenario.hpp"

#include "../assets/asset_runtime.hpp"
#include "../assets/game_data_registry.hpp"
#include "../battle_view/battle_scenario_executor.hpp"
#include "../battle_view/battle_slot.hpp"

#include <d2log/log.hpp>
#include <algorithm>
#include <stdexcept>
#include <variant>

namespace d2engine {

namespace {

std::size_t compute_attacker_count(const BattleScenarioSequence& seq) {
    const auto setup_it =
        std::ranges::find_if(seq.steps, [](const auto& s) { return s.id == "setup_units"; });
    if (setup_it == seq.steps.end())
        return 0;
    std::size_t count = 0;
    for (const auto& env : setup_it->envelopes) {
        if (std::holds_alternative<ScenarioUnitCreated>(env.event)) {
            auto coord = parse_position_string(std::get<ScenarioUnitCreated>(env.event).slot);
            if (coord.has_value() && coord->side == BattleSide::Attacker)
                ++count;
        }
    }
    return count;
}

// Validate scheduling rules and run setup_units into runtime.
// Returns start_step (0 if no setup_units step, 1 after executing it).
std::size_t execute_startup_setup_units(const BattleScenarioSequence& seq,
                                        BattleScenarioRuntime&        runtime,
                                        const std::filesystem::path&  scenario_path) {
    const auto setup_it =
        std::ranges::find_if(seq.steps, [](const auto& s) { return s.id == "setup_units"; });
    if (setup_it == seq.steps.end())
        return 0;

    const auto& step = *setup_it;
    for (const auto& env : step.envelopes) {
        if (!std::holds_alternative<ScenarioUnitCreated>(env.event)) {
            throw std::runtime_error("setup_units step may only contain UnitCreated events. "
                                     "scenario=" +
                                     scenario_path.string() + " seq=" + seq.id +
                                     " step=setup_units envelope=" + env.id);
        }
        if (env.at.event_id.has_value() || env.at.cue != "start") {
            throw std::runtime_error("setup_units envelopes must not use scheduling. "
                                     "scenario=" +
                                     scenario_path.string() + " seq=" + seq.id +
                                     " step=setup_units envelope=" + env.id);
        }
    }
    for (const auto& env : step.envelopes) {
        BattleScenarioExecutor::execute_envelope(env, runtime, seq.id, "setup_units",
                                                 scenario_path.string());
    }
    return 1;
}

// Minimal IBattlePresentationSink that captures unit creation data without rendering.
// Used in preflight dry-run so setup_units can be executed and validated.
class CapturingPresentationSink final : public IBattlePresentationSink {
public:
    UnitVisualProfileId add_visual_profile(UnitAnimationRoleSet roles) override {
        return profiles_.add(std::move(roles));
    }

    UnitLifecycleVisualProfileId
    add_lifecycle_profile(std::string /*name*/, UnitLifecycleVisualProfile /*profile*/) override {
        return UnitLifecycleVisualProfileId{};
    }

    [[nodiscard]] const UnitVisualProfileRegistry& unit_profiles() const override {
        return profiles_;
    }

    void on_unit_created(UnitInstanceId /*unit_id*/) override {}
    void on_unit_retreated(UnitInstanceId /*unit_id*/) override {}

    BattleVisualStepExecution submit_visual_step(const BattleVisualStep& /*step*/) override {
        return {};
    }
    void               finish_visual_step() override {}
    void               cancel_visual_step() override {}
    [[nodiscard]] bool has_active_visual_step() const override { return false; }
    [[nodiscard]] bool visual_step_complete() const override { return true; }
    [[nodiscard]] bool visual_step_failed() const override { return false; }
    [[nodiscard]] bool animation_busy() const override { return false; }
    [[nodiscard]] const BattleVisualStepExecution& visual_step_execution() const override {
        return exec_;
    }

private:
    UnitVisualProfileRegistry profiles_;
    BattleVisualStepExecution exec_;
};

// Minimal IBattleUnitCreationService that forwards to the real factory
// and captures the result of each create_unit call.
class CapturingUnitCreationService final : public IBattleUnitCreationService {
public:
    struct Entry {
        std::string      alias;
        UnitCreationData data;
    };

    explicit CapturingUnitCreationService(IBattleUnitCreationService& real) : real_(real) {}

    UnitCreationData create_unit(const UnitCreationRequest& request) override {
        auto data = real_.create_unit(request);
        captured_.push_back({.alias = request.alias, .data = data});
        return data;
    }

    std::vector<Entry> captured_;

private:
    IBattleUnitCreationService& real_;
};

} // namespace

BattleBootstrapPreflight preflight_battle_session(AssetRuntime& assets, GameDataRegistry& game_data,
                                                  const UnitAttackVisualIntentMap& attack_intents,
                                                  const std::filesystem::path&     scenario_path,
                                                  const std::string& preferred_seq_id) {

    BattleBootstrapPreflight pf;

    pf.scenario = load_battle_scenario(scenario_path);
    if (pf.scenario.sequences.empty())
        throw std::runtime_error("scenario has no sequences");
    pf.play_seq_id = preferred_seq_id.empty()
                         ? pf.scenario.autoplay.value_or(pf.scenario.sequences.front().id)
                         : preferred_seq_id;

    d2log::get("d2.app")->info("v3_scenario_preflight path={} id={} terrain={} sequences={} seq={}",
                               scenario_path.string(), pf.scenario.id, pf.scenario.terrain,
                               pf.scenario.sequences.size(), pf.play_seq_id);

    const auto seq_it = std::ranges::find_if(pf.scenario.sequences,
                                             [&](const auto& s) { return s.id == pf.play_seq_id; });
    if (seq_it == pf.scenario.sequences.end())
        throw std::runtime_error("scenario: play sequence '" + pf.play_seq_id + "' not found");

    pf.attacker_count = compute_attacker_count(*seq_it);

    // Fallible IO: build catalog and factory before caller clears existing session.
    pf.catalog = std::make_unique<RawFfAnimationCatalog>(assets, std::string{"Imgs/Batunits.ff"},
                                                         std::string{"Imgs/Battle.ff"});
    pf.unit_factory = std::make_unique<BattleUnitFactory>(game_data, *pf.catalog, attack_intents);

    // Dry-run setup_units in a temp scene to validate unit creation and capture results.
    // This ensures commit() can replay without calling the factory again.
    {
        BattleScene                  temp_scene;
        CapturingPresentationSink    temp_presentation;
        CapturingUnitCreationService temp_factory(*pf.unit_factory);
        BattleScenarioRuntime        temp_runtime(temp_scene, temp_presentation, temp_factory);

        pf.start_step = execute_startup_setup_units(*seq_it, temp_runtime, scenario_path);

        pf.initial_units.reserve(temp_factory.captured_.size());
        for (auto& entry : temp_factory.captured_) {
            pf.initial_units.push_back(
                {.alias = std::move(entry.alias), .data = std::move(entry.data)});
        }
    }

    d2log::get("d2.app")->info("v3_attacker_count count={}", pf.attacker_count);
    return pf;
}

BattleSessionBootstrapResult commit_battle_session(BattleBootstrapPreflight     pf,
                                                   BattleSessionBootstrapParams p) {
    BattleSessionBootstrapResult result;
    result.scenario = std::move(pf.scenario);
    result.play_seq_id = std::move(pf.play_seq_id);
    result.start_step = pf.start_step;

    // Transfer catalog and factory ownership.
    p.out_catalog = std::move(pf.catalog);
    p.out_factory = std::move(pf.unit_factory);

    p.out_presenter = std::make_unique<BattlePresenter>(p.scene, UnitVisualProfileRegistry{},
                                                        UnitLifecycleVisualProfileRegistry{},
                                                        BattleEffectRoleSet{}, pf.attacker_count);
    p.out_runtime =
        std::make_unique<BattleScenarioRuntime>(p.scene, *p.out_presenter, *p.out_factory);

    // Replay pre-validated unit setup — no factory calls, no IO, should not throw.
    for (const auto& unit : pf.initial_units) {
        p.out_runtime->apply_unit_from_data(unit.alias, unit.data);
    }

    // Load selection markers: non-fatal on failure.
    try {
        p.out_presenter->set_marker_large_animation(
            p.assets.animation_sequence("Imgs/Battle.ff", "MRKCURLARGEA"));
    } catch (const std::exception& e) {
        d2log::get("d2.app")->warn("v3_marker_large_load_failed error={}", e.what());
    }
    try {
        p.out_presenter->set_marker_small_animation(
            p.assets.animation_sequence("Imgs/Battle.ff", "MRKCURSMALLA"));
    } catch (const std::exception& e) {
        d2log::get("d2.app")->warn("v3_marker_small_load_failed error={}", e.what());
    }

    return result;
}

} // namespace d2engine
