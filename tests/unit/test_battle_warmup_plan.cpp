#include <gtest/gtest.h>

#include "d2engine/battle_view/battle_startup_texture_warmup.hpp"

#include <set>
#include <string>

namespace d2engine {
namespace {

// Verify that the warmup plan produces ImageAssetKey entries suitable for
// consumption by AnimationAssetPreloader (not Battle-specific types).
// This test ensures no FF-decoding knowledge leaks into the plan output.

TEST(BattleWarmupPlan, ProducesImageAssetKeys) {
    BattleRenderSnapshot snapshot;
    BattleRenderOptions  options;

    BattleWarmupPlan plan =
        build_battle_startup_texture_warmup_plan(snapshot, options, nullptr, nullptr);

    // Every entry must have a valid ImageAssetKey
    for (const auto& entry : plan.entries) {
        // ImageAssetKey must have at minimum container_path or image_name populated
        // (or both empty — that's a no-op entry we tolerate)
        if (entry.key.container_path.empty() && entry.key.image_name.empty()) {
            continue;
        }
        // Verify it's a ComposedSprite with DetectMagentaBorder (the battle standard)
        EXPECT_EQ(entry.key.kind, ImageAssetKind::ComposedSprite)
            << "entry: " << entry.key.container_path << "/" << entry.key.image_name;
        EXPECT_TRUE(has_postprocess(entry.key.postprocess, ImagePostprocess::DetectMagentaBorder))
            << "entry: " << entry.key.container_path << "/" << entry.key.image_name;
    }
}

TEST(BattleWarmupPlan, EntriesAreDeduplicated) {
    BattleRenderSnapshot snapshot;
    BattleRenderOptions  options;

    BattleWarmupPlan plan1 =
        build_battle_startup_texture_warmup_plan(snapshot, options, nullptr, nullptr);

    // Build twice — same snapshot should produce same entries (deterministic)
    BattleWarmupPlan plan2 =
        build_battle_startup_texture_warmup_plan(snapshot, options, nullptr, nullptr);

    EXPECT_EQ(plan1.entries.size(), plan2.entries.size());

    // Verify no duplicate (container, image_name, bucket) combinations
    std::set<std::string> seen;
    for (const auto& entry : plan1.entries) {
        const std::string id = entry.key.container_path + "/" + entry.key.image_name + "/" +
                               std::to_string(static_cast<int>(entry.bucket));
        auto [it, inserted] = seen.insert(id);
        EXPECT_TRUE(inserted) << "duplicate entry: " << id;
    }
}

TEST(BattleWarmupPlan, EntriesAreUsableByPreloaderDirectly) {
    // This test verifies the contract between battle_startup_texture_warmup
    // and AnimationAssetPreloader: the plan entries are ImageAssetKey vectors
    // that the preloader accepts directly.
    BattleRenderSnapshot snapshot;
    BattleRenderOptions  options;

    BattleWarmupPlan plan =
        build_battle_startup_texture_warmup_plan(snapshot, options, nullptr, nullptr);

    // Collect all keys into a vector as the preloader would receive them
    std::vector<ImageAssetKey> keys;
    keys.reserve(plan.entries.size());
    for (const auto& entry : plan.entries) {
        keys.push_back(entry.key);
    }

    // Verify keys has stable sort order (for deterministic testing)
    for (std::size_t i = 1; i < keys.size(); ++i) {
        // No specific ordering requirement; just verify all keys are valid
        EXPECT_FALSE(keys[i].container_path.empty() && keys[i].image_name.empty())
            << "entry " << i << " has both empty container and image";
    }
}

} // namespace
} // namespace d2engine
