#include <d2engine/assets/stack_banner_asset_catalog_builder.hpp>
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

TEST(StackBannerAssetCatalogIntegration, BuildsCatalogFromRealGameData) {
    const auto game_root = game_root_from_env();
    if (!game_root.has_value()) {
        GTEST_SKIP() << "DISCIPLES2_GAME_ROOT is not set";
    }

    const auto store = d2engine::FfAssetStore(*game_root);
    const auto catalog = d2engine::build_stack_banner_asset_catalog(store);

    ASSERT_EQ(catalog.frames.size(), 15u);
    EXPECT_EQ(catalog.frames.front().container_path, "Imgs/IsoCmon.ff");
    EXPECT_EQ(catalog.resolve_banner(0).record_name, "STACK_BANNER_0000");
    EXPECT_EQ(catalog.resolve_banner(4).record_name, "STACK_BANNER_0400");
    EXPECT_EQ(catalog.resolve_banner(9).record_name, "STACK_BANNER_0900");
    EXPECT_EQ(catalog.resolve_banner(13).record_name, "STACK_BANNER_1300");
    EXPECT_EQ(catalog.resolve_banner(14).record_name, "TI");
    EXPECT_GT(catalog.frames.front().canvas_width, 0);
    EXPECT_GT(catalog.frames.front().canvas_height, 0);
    SUCCEED() << "stack banner count=" << catalog.frames.size();
}
