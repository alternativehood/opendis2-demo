#pragma once

#include "../battle_adapters/raw_ff_animation_catalog.hpp"
#include "../battle_view/battle_presenter.hpp"
#include "../battle_view/battle_scenario_data.hpp"
#include "../battle_view/battle_scene.hpp"
#include "../battle_view/battle_scenario_runtime.hpp"
#include "../battle_view/battle_unit_factory.hpp"
#include "../battle_view/unit_creation_types.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace d2engine {

class AssetRuntime;
class GameDataRegistry;

// ── Phase 1 result: all fallible work done before clearing the old session ──
// Preflight validates the scenario config, builds catalog/factory, and
// dry-runs setup_units to capture pre-validated unit creation data.
// Throws on any failure. Caller's existing session is untouched.
struct BattleBootstrapPreflight {
    BattleScenario                         scenario;
    std::string                            play_seq_id;
    std::size_t                            attacker_count = 0;
    std::size_t                            start_step = 0;
    std::unique_ptr<RawFfAnimationCatalog> catalog;
    std::unique_ptr<BattleUnitFactory>     unit_factory;

    // Pre-validated unit creation data from dry-run of setup_units.
    // Replayed in commit via apply_unit_from_data — no factory calls in commit.
    struct PrebuiltUnit {
        std::string      alias;
        UnitCreationData data;
    };
    std::vector<PrebuiltUnit> initial_units;
};

// ── Params shared by both APIs ─────────────────────────────────────────────
struct BattleSessionBootstrapParams {
    AssetRuntime&                    assets;
    GameDataRegistry&                game_data;
    BattleScene&                     scene;
    const UnitAttackVisualIntentMap& attack_intents;
    const std::filesystem::path&     scenario_path;
    std::string                      preferred_seq_id; // empty = use scenario.autoplay

    std::unique_ptr<RawFfAnimationCatalog>& out_catalog;
    std::unique_ptr<BattleUnitFactory>&     out_factory;
    std::unique_ptr<BattlePresenter>&       out_presenter;
    std::unique_ptr<BattleScenarioRuntime>& out_runtime;
};

struct BattleSessionBootstrapResult {
    BattleScenario scenario;
    std::string    play_seq_id;
    std::size_t    start_step = 0;
};

// ── Phase 1: validate + pre-build resources + dry-run setup_units ─────────
// Throws on any error. Safe to call BEFORE clearing the current session.
[[nodiscard]] BattleBootstrapPreflight
preflight_battle_session(AssetRuntime& assets, GameDataRegistry& game_data,
                         const UnitAttackVisualIntentMap& attack_intents,
                         const std::filesystem::path&     scenario_path,
                         const std::string&               preferred_seq_id);

// ── Phase 2: build from preflight (fatal on unexpected failure) ───────────
// Consumes pf; constructs presenter/runtime and replays initial_units via
// apply_unit_from_data (no factory calls, no IO).
// Expected scenario/resource failures are handled in preflight; any exception
// here indicates an invariant bug and is fatal — caller must not continue.
[[nodiscard]] BattleSessionBootstrapResult commit_battle_session(BattleBootstrapPreflight     pf,
                                                                 BattleSessionBootstrapParams p);

} // namespace d2engine
