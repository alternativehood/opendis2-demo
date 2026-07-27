#include <d2engine/assets/contained_stack_shield_asset_catalog_builder.hpp>
#include <d2engine/assets/ff_asset_store.hpp>

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>

namespace {

std::optional<std::filesystem::path> game_root_from_env() {
    const char* env = std::getenv("DISCIPLES2_GAME_ROOT");
    if (env == nullptr || std::string(env).empty()) {
        return std::nullopt;
    }
    return std::filesystem::path(env);
}

} // namespace

TEST(ContainedStackShieldAssetCatalogIntegration, BuildsCatalogFromRealGameData) {
    const auto game_root = game_root_from_env();
    if (!game_root.has_value()) {
        GTEST_SKIP() << "DISCIPLES2_GAME_ROOT is not set";
    }

    const auto store = d2engine::FfAssetStore(*game_root);
    const auto catalog = d2engine::build_contained_stack_shield_asset_catalog(store);

    ASSERT_EQ(catalog.assets.size(), 11u);
    EXPECT_EQ(catalog.resolve("g000rr0000", d2runtime::AdventureSettlementKind::Capital)
                  .outer_logical_name,
              "G000RR0000SHLC8");
    EXPECT_EQ(catalog.resolve("g000rr0000", d2runtime::AdventureSettlementKind::Village)
                  .outer_logical_name,
              "G000RR0000SHLV8");
    EXPECT_EQ(catalog.resolve("g000rr0004", d2runtime::AdventureSettlementKind::Village)
                  .outer_logical_name,
              "G000RR8888SHLV8");
    EXPECT_EQ(
        catalog.resolve("g000rr0005", d2runtime::AdventureSettlementKind::Capital).sprite_name,
        "ZC");
    EXPECT_EQ(
        catalog.resolve("g000rr0005", d2runtime::AdventureSettlementKind::Village).sprite_name,
        "0C");
    EXPECT_GT(
        catalog.resolve("g000rr0005", d2runtime::AdventureSettlementKind::Capital).canvas_width, 0);
    EXPECT_GT(
        catalog.resolve("g000rr0005", d2runtime::AdventureSettlementKind::Capital).canvas_height,
        0);
    EXPECT_THROW(static_cast<void>(
                     catalog.resolve("g000rr0004", d2runtime::AdventureSettlementKind::Capital)),
                 std::runtime_error);
    EXPECT_THROW(static_cast<void>(
                     catalog.resolve("g000rr9999", d2runtime::AdventureSettlementKind::Village)),
                 std::runtime_error);
}
