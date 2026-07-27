#pragma once

#include "../assets/image_asset_key.hpp"
#include "battle_animation_scripts.hpp"
#include "battle_render_snapshot.hpp"
#include "battle_renderer.hpp"
#include "battle_visual_step.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace d2engine {

struct BattleScenario;
class BattleScenarioRuntime;

/// Priority bucket for a warmup image key.
enum class WarmupBucket : std::uint8_t {
    Critical,
    NearFuture,
    Later,
};

/// A warmup plan entry: which texture to preload and how urgently.
struct BattleWarmupEntry {
    ImageAssetKey key;
    WarmupBucket  bucket = WarmupBucket::Critical;
};

/// Structured plan of textures needed for battle startup.
struct BattleWarmupPlan {
    std::vector<BattleWarmupEntry> entries;
    std::vector<std::string>       diagnostics;
};

struct BattleScenarioWarmupInput {
    const BattleScenario*  scenario = nullptr;
    std::string            sequence_id;
    std::size_t            start_step_index = 0;
    BattleScenarioRuntime* runtime = nullptr;
};

/// Build the set of logical textures required for battle startup.
/// The returned plan contains ImageAssetKey entries suitable for
/// consumption by AnimationAssetPreloader or RenderAssetRuntime.
/// This function is Battle-specific: it walks battle snapshot, options,
/// and scenario data to determine WHICH resources are needed.
/// HOW they are decoded is delegated to the caller.
[[nodiscard]] BattleWarmupPlan build_battle_startup_texture_warmup_plan(
    const BattleRenderSnapshot& snapshot, const BattleRenderOptions& options,
    const BattleScriptContext* script_context, const BattleScenarioWarmupInput* scenario_input);

} // namespace d2engine
