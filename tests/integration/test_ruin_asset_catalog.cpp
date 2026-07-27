#include <d2engine/assets/ff_asset_store.hpp>
#include <d2engine/assets/ruin_asset_catalog_builder.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>

namespace d2ar = d2engine::adventure_render;

namespace {

std::optional<std::filesystem::path> game_root_from_env() {
    const char* env = std::getenv("DISCIPLES2_GAME_ROOT");
    if (env == nullptr || std::string(env).empty()) {
        return std::nullopt;
    }
    return std::filesystem::path(env);
}

} // namespace

TEST(RuinAssetCatalogIntegration, BuildsRealProductionCatalog) {
    const auto game_root = game_root_from_env();
    if (!game_root.has_value()) {
        GTEST_SKIP() << "DISCIPLES2_GAME_ROOT is not set";
    }

    d2engine::FfAssetStore store(*game_root);
    const auto             catalog = d2engine::build_ruin_asset_catalog(store);

    const auto land_count = std::count_if(catalog.visuals[0].begin(), catalog.visuals[0].end(),
                                          [](const auto& entry) { return entry.has_value(); });
    const auto water_count = std::count_if(catalog.visuals[1].begin(), catalog.visuals[1].end(),
                                           [](const auto& entry) { return entry.has_value(); });

    EXPECT_EQ(land_count, 9);
    EXPECT_EQ(water_count, 9);

    const auto* land0 = catalog.find(0, d2runtime::AdventureSurfacePlacement::Land);
    const auto* land8 = catalog.find(8, d2runtime::AdventureSurfacePlacement::Land);
    const auto* water0 = catalog.find(0, d2runtime::AdventureSurfacePlacement::Water);
    const auto* water8 = catalog.find(8, d2runtime::AdventureSurfacePlacement::Water);

    ASSERT_NE(land0, nullptr);
    ASSERT_NE(land8, nullptr);
    ASSERT_NE(water0, nullptr);
    ASSERT_NE(water8, nullptr);

    EXPECT_EQ(std::get<d2ar::StaticRuinVisual>(*land0).logical_sprite, "G000RU0000000");
    EXPECT_EQ(std::get<d2ar::StaticRuinVisual>(*land8).logical_sprite, "G000RU0000008");
    EXPECT_EQ(std::get<d2ar::StaticRuinVisual>(*water0).logical_sprite, "G000RU0000100");
    EXPECT_EQ(std::get<d2ar::StaticRuinVisual>(*water8).logical_sprite, "G000RU0000108");

    EXPECT_EQ(catalog.find(9, d2runtime::AdventureSurfacePlacement::Land), nullptr);
    EXPECT_EQ(catalog.find(10, d2runtime::AdventureSurfacePlacement::Land), nullptr);
    EXPECT_EQ(catalog.find(9, d2runtime::AdventureSurfacePlacement::Water), nullptr);
    EXPECT_EQ(catalog.find(10, d2runtime::AdventureSurfacePlacement::Water), nullptr);

    for (const auto& placement : catalog.visuals) {
        for (const auto& visual : placement) {
            if (!visual.has_value()) {
                continue;
            }
            std::visit(
                [](const auto& candidate) {
                    using Visual = std::decay_t<decltype(candidate)>;
                    if constexpr (std::is_same_v<Visual, d2ar::StaticRuinVisual>) {
                        EXPECT_GT(candidate.canvas_width, 0);
                        EXPECT_GT(candidate.canvas_height, 0);
                        EXPECT_TRUE(candidate.content_bounds.valid());
                    } else {
                        EXPECT_FALSE(candidate.animation.frames.empty());
                    }
                },
                *visual);
        }
    }
}
