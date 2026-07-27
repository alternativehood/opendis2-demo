#include <gtest/gtest.h>

#include "d2adventure_render/tree_hash.hpp"

#include <unordered_set>

using namespace d2engine::adventure_render;

TEST(TreeHash, DeterministicSameInputSameOutput) {
    const auto v1 = stable_tree_hash(42, 100, 200);
    const auto v2 = stable_tree_hash(42, 100, 200);
    const auto v3 = stable_tree_hash(42, 100, 200);
    EXPECT_EQ(v1, v2);
    EXPECT_EQ(v2, v3);
}

TEST(TreeHash, DifferentSeedDifferentOutput) {
    const auto v1 = stable_tree_hash(42, 100, 200);
    const auto v2 = stable_tree_hash(43, 100, 200);
    EXPECT_NE(v1, v2);
}

TEST(TreeHash, DifferentCoordDifferentOutput) {
    const auto v1 = stable_tree_hash(42, 100, 200);
    const auto v2 = stable_tree_hash(42, 101, 200);
    const auto v3 = stable_tree_hash(42, 100, 201);
    EXPECT_NE(v1, v2);
    EXPECT_NE(v1, v3);
    EXPECT_NE(v2, v3);
}

TEST(TreeHash, UniformDistributionSmoke) {
    std::unordered_set<uint32_t> values;
    for (int x = 0; x < 100; ++x) {
        for (int y = 0; y < 100; ++y) {
            values.insert(stable_tree_hash(12345, x, y));
        }
    }
    // 10,000 unique (x,y) pairs should produce 10,000 unique hashes (or close)
    EXPECT_GE(values.size(), 9900u) << "hash should have very few collisions";
}

TEST(TreeHash, NoSequentialDependence) {
    // Same cell, different seeds — prove no sequential RNG state
    const auto a = stable_tree_hash(1, 5, 5);
    const auto b = stable_tree_hash(2, 5, 5);
    EXPECT_NE(a, b);
}
