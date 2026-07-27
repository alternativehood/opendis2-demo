#pragma once

// ── AdventureScenarioLoader ─────────────────────────────────────────────
//
// Semantic-only scenario loading (no graphical deps):
//   1. Read .sg file
//   2. Parse via SgParser
//   3. Build AdventureWorldState via AdventureWorldBuilder
//   4. Return GameSession + diagnostics
//
// This path is usable by headless and graphical modes.
// Graphical terrain preparation happens separately (see AdventureMapPreparer).

#include <d2game/GameSession.hpp>
#include <d2runtime/AdventureWorldBuilder.hpp>
#include <d2runtime/AdventureWorldState.hpp>

#include <memory>
#include <string>
#include <vector>

namespace opendis2 {

// ── Semantic load result (no graphical deps) ───────────────────────────
//
struct ScenarioLoadResult {
    std::unique_ptr<d2game::GameSession>    session;
    std::vector<d2runtime::BuildDiagnostic> build_diagnostics;
    std::size_t                             parsed_stack_count = 0;
    std::size_t                             warning_count = 0;
    std::size_t                             error_count = 0;
    bool                                    success = false;
};

// ── Loader ─────────────────────────────────────────────────────────────
//
class AdventureScenarioLoader {
public:
    // Load a scenario from a .sg file path (semantic only).
    // Returns nullptr on file I/O failure.
    [[nodiscard]] static std::unique_ptr<ScenarioLoadResult>
    load_semantic(const std::string&                          scenario_path,
                  d2game::AdventureUnitMovementProfileCatalog movement_profiles = {});

private:
    AdventureScenarioLoader() = default;
};

} // namespace opendis2
