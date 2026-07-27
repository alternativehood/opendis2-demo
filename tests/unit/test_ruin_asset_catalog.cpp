#include <d2engine/assets/ff_asset_store.hpp>
#include <d2engine/assets/ruin_asset_catalog_builder.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <variant>

namespace d2ar = d2engine::adventure_render;

namespace {

d2ar::RuinAssetCatalog make_catalog() {
    d2ar::RuinAssetCatalog catalog;
    catalog.visuals[0][0] = d2ar::StaticRuinVisual{"synthetic-land", "LAND_RUIN_0", 1, 2, 8, 9};
    catalog.visuals[0][10] = d2ar::StaticRuinVisual{"synthetic-land", "LAND_RUIN_10", 3, 4, 10, 11};
    catalog.visuals[0][8] = d2ar::StaticRuinVisual{"synthetic-land", "LAND_RUIN_8", 3, 4, 10, 11};
    catalog.visuals[1][0] = d2ar::StaticRuinVisual{"synthetic-water", "WATER_RUIN_0", 5, 6, 12, 13};
    catalog.visuals[1][10] =
        d2ar::StaticRuinVisual{"synthetic-water", "WATER_RUIN_10", 7, 8, 14, 15};
    catalog.visuals[1][8] = d2ar::StaticRuinVisual{"synthetic-water", "WATER_RUIN_8", 7, 8, 14, 15};
    return catalog;
}

d2ar::AnimatedRuinVisual make_animation(const char* name) {
    d2ar::AnimatedRuinVisual visual;
    visual.container_path = "synthetic-animation";
    visual.logical_animation = name;
    visual.animation.animation_name = name;
    visual.animation.native_canvas_w = 32;
    visual.animation.native_canvas_h = 24;
    visual.animation.is_looping = true;
    visual.animation.frames = {{"FRAME_0", 80, 32, 24}, {"FRAME_1", 90, 40, 28}};
    return visual;
}

} // namespace

TEST(RuinAssetCatalog, ExactPlacementAndImageLookup) {
    const auto  catalog = make_catalog();
    const auto* land = catalog.find(0, d2runtime::AdventureSurfacePlacement::Land);
    const auto* water = catalog.find(10, d2runtime::AdventureSurfacePlacement::Water);
    ASSERT_NE(land, nullptr);
    ASSERT_NE(water, nullptr);
    EXPECT_EQ(std::get<d2ar::StaticRuinVisual>(*land).logical_sprite, "LAND_RUIN_0");
    EXPECT_EQ(std::get<d2ar::StaticRuinVisual>(*water).logical_sprite, "WATER_RUIN_10");
    EXPECT_EQ(&catalog.resolve(10, d2runtime::AdventureSurfacePlacement::Water), water);
}

TEST(RuinAssetCatalog, VariantSupportsIndependentAnimatedPlacementAssets) {
    auto catalog = make_catalog();
    catalog.visuals[0][9] = make_animation("LAND_ANIM_9");
    catalog.visuals[0][10] = make_animation("LAND_ANIM_10");
    catalog.visuals[1][9] = make_animation("WATER_ANIM_9");
    catalog.visuals[1][10] = make_animation("WATER_ANIM_10");

    for (const auto placement : {d2runtime::AdventureSurfacePlacement::Land,
                                 d2runtime::AdventureSurfacePlacement::Water}) {
        ASSERT_TRUE(std::holds_alternative<d2ar::StaticRuinVisual>(*catalog.find(0, placement)));
        ASSERT_TRUE(std::holds_alternative<d2ar::StaticRuinVisual>(*catalog.find(8, placement)));
        ASSERT_TRUE(std::holds_alternative<d2ar::AnimatedRuinVisual>(*catalog.find(9, placement)));
        ASSERT_TRUE(std::holds_alternative<d2ar::AnimatedRuinVisual>(*catalog.find(10, placement)));
    }
    EXPECT_NE(std::get<d2ar::StaticRuinVisual>(
                  *catalog.find(0, d2runtime::AdventureSurfacePlacement::Land))
                  .logical_sprite,
              std::get<d2ar::StaticRuinVisual>(
                  *catalog.find(0, d2runtime::AdventureSurfacePlacement::Water))
                  .logical_sprite);
}

TEST(RuinAssetCatalog, EmptyProductionStyleStoreFailsFast) {
    const auto temp_root = std::filesystem::temp_directory_path() / "opendis2_empty_ruin_catalog";
    std::filesystem::remove_all(temp_root);
    std::filesystem::create_directories(temp_root);
    std::filesystem::create_directories(temp_root / "Imgs");
    std::ofstream(temp_root / "Imgs" / "IsoCmon.ff").close();

    d2engine::FfAssetStore store(temp_root);
    try {
        (void)d2engine::build_ruin_asset_catalog(store);
        FAIL() << "expected build_ruin_asset_catalog to throw";
    } catch (const std::runtime_error& e) {
        EXPECT_NE(
            std::string(e.what()).find("ruin_asset_catalog_missing_sprite logical=G000RU0000000"),
            std::string::npos);
    }

    std::filesystem::remove_all(temp_root);
}
