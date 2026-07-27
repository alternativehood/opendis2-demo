#include <d2adventure_render/map_geometry.hpp>
#include <d2adventure_render/map_preparer.hpp>
#include <d2adventure_render/site_contributor.hpp>
#include <d2adventure_render/terrain/site_asset_catalog.hpp>
#include <d2engine/assets/stack_banner_asset_catalog_builder.hpp>
#include <d2engine/assets/render_graph_asset_collector.hpp>
#include <d2runtime/AdventureWorldState.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace d2ar = d2engine::adventure_render;

namespace {

d2ar::AdventureAnimationFrame make_frame(std::string record_name, int duration_ms, int width,
                                         int height) {
    return {.record_name = std::move(record_name),
            .duration_ms = duration_ms,
            .canvas_width = width,
            .canvas_height = height};
}

d2ar::StaticSiteVisual make_static_visual(std::string logical_sprite) {
    d2ar::StaticSiteVisual visual;
    visual.container_path = "Imgs/IsoCmon.ff";
    visual.logical_sprite = std::move(logical_sprite);
    visual.canvas_foot_x = 12;
    visual.canvas_foot_y = 18;
    visual.canvas_width = 64;
    visual.canvas_height = 48;
    visual.content_bounds = {0, 0, 64, 48};
    return visual;
}

d2ar::AnimatedSiteLayer make_animated_layer(std::string logical_animation, int frame_count) {
    d2ar::AnimatedSiteLayer layer;
    layer.container_path = "Imgs/IsoAnim.ff";
    layer.logical_animation = std::move(logical_animation);
    layer.canvas_foot_x = 14;
    layer.canvas_foot_y = 20;
    layer.content_bounds = {0, 0, 80 + std::max(0, frame_count - 1),
                            70 + std::max(0, frame_count - 1)};
    layer.animation.animation_name = layer.logical_animation;
    layer.animation.native_canvas_w = 320;
    layer.animation.native_canvas_h = 320;
    layer.animation.is_looping = true;
    layer.animation.timing_source = d2ar::AdventureAnimationTimingSource::ProvisionalFallback;
    for (int i = 0; i < frame_count; ++i) {
        layer.animation.frames.push_back(
            make_frame(layer.logical_animation + "_F" + std::to_string(i), 90, 80 + i, 70 + i));
    }
    return layer;
}

d2ar::StackBannerAssetCatalog make_banner_catalog() {
    d2ar::StackBannerAssetCatalog catalog;
    catalog.frames.resize(5);
    for (std::size_t i = 0; i < catalog.frames.size(); ++i) {
        catalog.frames[i] = {.container_path = "Imgs/IsoCmon.ff",
                             .record_name = "TI",
                             .canvas_width = 300,
                             .canvas_height = 200,
                             .canvas_foot_x = 10,
                             .canvas_foot_y = 20,
                             .content_bounds = {1, 2, 3, 4}};
    }
    return catalog;
}

d2ar::SiteAssetCatalog make_catalog() {
    d2ar::SiteAssetCatalog catalog;

    catalog.mage_visuals[0] = d2ar::SiteVisual{make_static_visual("G000SI0000MAGE00")};
    catalog.mage_visuals[1] = d2ar::SiteVisual{make_static_visual("G000SI0000MAGE01")};
    catalog.mage_visuals[2] = d2ar::SiteVisual{make_static_visual("G000SI0000MAGE02")};
    catalog.mage_visuals[3] = d2ar::SiteVisual{make_static_visual("G000SI0000MAGE03")};

    catalog.merchant_visuals[0] = d2ar::SiteVisual{
        d2ar::AnimatedSiteVisual{.body = make_animated_layer("G000SI0000MERH00", 20),
                                 .shadow = make_animated_layer("G000SI0000MERH00S", 20)}};
    catalog.merchant_visuals[1] = d2ar::SiteVisual{make_static_visual("G000SI0000MERH01")};
    catalog.merchant_visuals[2] = d2ar::SiteVisual{make_static_visual("G000SI0000MERH02")};
    catalog.merchant_visuals[3] = d2ar::SiteVisual{make_static_visual("G000SI0000MERH03")};
    catalog.merchant_visuals[4] = d2ar::SiteVisual{make_static_visual("G000SI0000MERH04")};
    catalog.merchant_visuals[5] = d2ar::SiteVisual{make_static_visual("G000SI0000MERH05")};
    catalog.merchant_visuals[6] = d2ar::SiteVisual{make_static_visual("G000SI0000MERH06")};
    catalog.merchant_visuals[7] = d2ar::SiteVisual{
        d2ar::AnimatedSiteVisual{.body = make_animated_layer("G000SI0000MERH07", 7)}};

    catalog.mercenary_visuals[0] = d2ar::SiteVisual{make_static_visual("G000SI0000MERC00")};
    catalog.mercenary_visuals[1] = d2ar::SiteVisual{make_static_visual("G000SI0000MERC01")};
    catalog.mercenary_visuals[2] = d2ar::SiteVisual{make_static_visual("G000SI0000MERC02")};
    catalog.mercenary_visuals[3] = d2ar::SiteVisual{make_static_visual("G000SI0000MERC03")};
    catalog.mercenary_visuals[4] = d2ar::SiteVisual{make_static_visual("G000SI0000MERC04")};

    catalog.trainer_visuals[0] = d2ar::SiteVisual{make_static_visual("G000SI0000TRAI00")};
    catalog.trainer_visuals[1] = d2ar::SiteVisual{make_static_visual("G000SI0000TRAI01")};
    catalog.trainer_visuals[2] = d2ar::SiteVisual{make_static_visual("G000SI0000TRAI02")};
    catalog.trainer_visuals[3] = d2ar::SiteVisual{make_static_visual("G000SI0000TRAI03")};

    return catalog;
}

d2runtime::AdventureSite make_site(std::string id, d2runtime::AdventureSiteKind kind, int image_iso,
                                   std::vector<d2runtime::MapCellCoord> footprint) {
    d2runtime::AdventureSite site;
    site.id = std::move(id);
    site.kind = kind;
    site.title = "title";
    site.description = "desc";
    site.image_iso = image_iso;
    site.position = footprint.empty() ? d2runtime::MapCellCoord{} : footprint.front();
    site.footprint = std::move(footprint);
    return site;
}

} // namespace

TEST(SiteAssetCatalog, ExactMappingTablesAndVisualIdentity) {
    const auto catalog = make_catalog();

    for (int i = 0; i <= 3; ++i) {
        const auto* visual = catalog.find(d2runtime::AdventureSiteKind::Mage, i);
        ASSERT_NE(visual, nullptr);
        ASSERT_TRUE(std::holds_alternative<d2ar::StaticSiteVisual>(*visual));
        EXPECT_EQ(std::get<d2ar::StaticSiteVisual>(*visual).logical_sprite,
                  "G000SI0000MAGE0" + std::to_string(i));
    }

    const auto* mage0 = catalog.find(d2runtime::AdventureSiteKind::Mage, 0);
    const auto* merchant0 = catalog.find(d2runtime::AdventureSiteKind::Merchant, 0);
    const auto* merc0 = catalog.find(d2runtime::AdventureSiteKind::Mercenary, 0);
    ASSERT_NE(mage0, nullptr);
    ASSERT_NE(merchant0, nullptr);
    ASSERT_NE(merc0, nullptr);
    EXPECT_NE(mage0, merchant0);
    EXPECT_NE(mage0, merc0);
    EXPECT_NE(merchant0, merc0);

    for (int i = 0; i <= 7; ++i) {
        const auto* visual = catalog.find(d2runtime::AdventureSiteKind::Merchant, i);
        ASSERT_NE(visual, nullptr);
        if (i == 0 || i == 7) {
            ASSERT_TRUE(std::holds_alternative<d2ar::AnimatedSiteVisual>(*visual));
        } else {
            ASSERT_TRUE(std::holds_alternative<d2ar::StaticSiteVisual>(*visual));
        }
    }

    const auto* merchant0_visual = catalog.find(d2runtime::AdventureSiteKind::Merchant, 0);
    ASSERT_NE(merchant0_visual, nullptr);
    const auto& merchant0_body = std::get<d2ar::AnimatedSiteVisual>(*merchant0_visual);
    ASSERT_TRUE(merchant0_body.shadow.has_value());
    EXPECT_EQ(merchant0_body.body.animation.frames.size(), 20u);
    EXPECT_EQ(merchant0_body.shadow->animation.frames.size(), 20u);

    const auto* merchant7_visual = catalog.find(d2runtime::AdventureSiteKind::Merchant, 7);
    ASSERT_NE(merchant7_visual, nullptr);
    const auto& merchant7_body = std::get<d2ar::AnimatedSiteVisual>(*merchant7_visual);
    EXPECT_FALSE(merchant7_body.shadow.has_value());
    EXPECT_EQ(merchant7_body.body.animation.frames.size(), 7u);

    for (int i = 0; i <= 4; ++i) {
        const auto* visual = catalog.find(d2runtime::AdventureSiteKind::Mercenary, i);
        ASSERT_NE(visual, nullptr);
        ASSERT_TRUE(std::holds_alternative<d2ar::StaticSiteVisual>(*visual));
        EXPECT_EQ(std::get<d2ar::StaticSiteVisual>(*visual).logical_sprite,
                  "G000SI0000MERC0" + std::to_string(i));
    }

    for (int i = 0; i <= 3; ++i) {
        const auto* visual = catalog.find(d2runtime::AdventureSiteKind::Trainer, i);
        ASSERT_NE(visual, nullptr);
        ASSERT_TRUE(std::holds_alternative<d2ar::StaticSiteVisual>(*visual));
        EXPECT_EQ(std::get<d2ar::StaticSiteVisual>(*visual).logical_sprite,
                  "G000SI0000TRAI0" + std::to_string(i));
    }

    EXPECT_EQ(catalog.find(d2runtime::AdventureSiteKind::Mage, -1), nullptr);
    EXPECT_EQ(catalog.find(d2runtime::AdventureSiteKind::Mage, 4), nullptr);
    EXPECT_EQ(catalog.find(d2runtime::AdventureSiteKind::Merchant, -1), nullptr);
    EXPECT_EQ(catalog.find(d2runtime::AdventureSiteKind::Merchant, 8), nullptr);
    EXPECT_EQ(catalog.find(d2runtime::AdventureSiteKind::Mercenary, -1), nullptr);
    EXPECT_EQ(catalog.find(d2runtime::AdventureSiteKind::Mercenary, 5), nullptr);
    EXPECT_EQ(catalog.find(d2runtime::AdventureSiteKind::Trainer, -1), nullptr);
    EXPECT_EQ(catalog.find(d2runtime::AdventureSiteKind::Trainer, 4), nullptr);
}

TEST(SiteRendering, TrainerProducesStaticBodyWithoutShadow) {
    const auto                 geometry = d2ar::AdventureMapGeometry::from_source(20, 20);
    d2ar::AdventureMapPreparer preparer(geometry);
    const auto                 catalog = make_catalog();
    const auto                 banner_catalog = make_banner_catalog();
    preparer.add_contributor(d2ar::make_site_contributor(catalog, banner_catalog));

    d2runtime::AdventureWorldState world;
    world.map_width = 20;
    world.map_height = 20;
    world.sites.push_back(
        make_site("S_TRAINER", d2runtime::AdventureSiteKind::Trainer, 0, {{4, 4}, {5, 4}}));

    const auto result = preparer.prepare(world);
    ASSERT_EQ(result.graph.world.size(), 2u);
    const auto body = std::find_if(
        result.graph.world.begin(), result.graph.world.end(),
        [](const auto& primitive) { return primitive.debug_label == "Site:S_TRAINER:body"; });
    const auto banner = std::find_if(
        result.graph.world.begin(), result.graph.world.end(), [](const auto& primitive) {
            return primitive.semantic_role == d2ar::AdventurePrimitiveRole::SiteBanner;
        });
    ASSERT_NE(body, result.graph.world.end());
    ASSERT_NE(banner, result.graph.world.end());
    EXPECT_EQ(body->debug_label, "Site:S_TRAINER:body");
    EXPECT_EQ(body->record_name, "G000SI0000TRAI00");
    EXPECT_FALSE(body->animation.has_value());
    EXPECT_EQ(body->phase, d2ar::AdventureRenderPhase::World);
    EXPECT_EQ(body->level, d2ar::WorldRenderLevel::Structure);
    EXPECT_EQ(body->local_suborder, 0);
    EXPECT_EQ(body->footprint.size(), 2u);
    EXPECT_EQ(body->depth_anchor, (d2runtime::MapCellCoord{5, 4}));
    const auto foot = geometry.cell_foot_anchor(body->depth_anchor);
    EXPECT_EQ(body->draw_origin.x, foot.x - 12);
    EXPECT_EQ(body->draw_origin.y, foot.y - 18);
    EXPECT_TRUE(banner->debug_label.starts_with("SiteBanner:S_TRAINER:"));
    EXPECT_EQ(banner->phase, d2ar::AdventureRenderPhase::World);
    EXPECT_EQ(banner->level, d2ar::WorldRenderLevel::Structure);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_NE(result.diagnostics.front().message.find("kind=Trainer"), std::string::npos);
    EXPECT_NE(result.diagnostics.front().message.find("visual=static"), std::string::npos);
    EXPECT_NE(result.diagnostics.front().message.find("G000SI0000TRAI00"), std::string::npos);
}

TEST(SiteRendering, MerchantZeroProducesBodyAndShadowAndCollectorFrames) {
    const auto                 geometry = d2ar::AdventureMapGeometry::from_source(20, 20);
    d2ar::AdventureMapPreparer preparer(geometry);
    const auto                 catalog = make_catalog();
    const auto                 banner_catalog = make_banner_catalog();
    preparer.add_contributor(d2ar::make_site_contributor(catalog, banner_catalog));

    d2runtime::AdventureWorldState world;
    world.map_width = 20;
    world.map_height = 20;
    world.sites.push_back(
        make_site("S_M0", d2runtime::AdventureSiteKind::Merchant, 0, {{4, 4}, {5, 4}, {5, 5}}));
    world.sites.push_back(make_site("S_M7", d2runtime::AdventureSiteKind::Merchant, 7, {{7, 7}}));

    const auto result = preparer.prepare(world);

    ASSERT_EQ(result.graph.world.size(), 5u);

    const auto body0 =
        std::find_if(result.graph.world.begin(), result.graph.world.end(),
                     [](const auto& prim) { return prim.debug_label == "Site:S_M0:body"; });
    const auto shadow0 =
        std::find_if(result.graph.world.begin(), result.graph.world.end(),
                     [](const auto& prim) { return prim.debug_label == "Site:S_M0:shadow"; });
    const auto body7 =
        std::find_if(result.graph.world.begin(), result.graph.world.end(),
                     [](const auto& prim) { return prim.debug_label == "Site:S_M7:body"; });
    ASSERT_NE(body0, result.graph.world.end());
    ASSERT_NE(shadow0, result.graph.world.end());
    ASSERT_NE(body7, result.graph.world.end());
    EXPECT_TRUE(body0->animation.has_value());
    EXPECT_TRUE(shadow0->animation.has_value());
    EXPECT_TRUE(body7->animation.has_value());
    EXPECT_FALSE(body7->animation_sync_source_id.has_value());
    EXPECT_TRUE(shadow0->animation_sync_source_id.has_value());
    EXPECT_EQ(*shadow0->animation_sync_source_id, body0->stable_id);
    EXPECT_FLOAT_EQ(shadow0->alpha, 1.0f);
    EXPECT_EQ(body0->depth_anchor, (d2runtime::MapCellCoord{5, 5}));
    EXPECT_EQ(body0->footprint.size(), 3u);
    EXPECT_EQ(body0->footprint, shadow0->footprint);

    const auto            keys = d2engine::collect_adventure_render_asset_keys(result.graph);
    std::set<std::string> asset_ids;
    for (const auto& key : keys) {
        asset_ids.insert(key.container_path + "/" + key.image_name);
    }
    for (int i = 0; i < 20; ++i) {
        EXPECT_TRUE(asset_ids.contains("Imgs/IsoAnim.ff/G000SI0000MERH00_F" + std::to_string(i)));
        EXPECT_TRUE(asset_ids.contains("Imgs/IsoAnim.ff/G000SI0000MERH00S_F" + std::to_string(i)));
    }
    for (int i = 0; i < 7; ++i) {
        EXPECT_TRUE(asset_ids.contains("Imgs/IsoAnim.ff/G000SI0000MERH07_F" + std::to_string(i)));
    }
}

TEST(SiteRendering, EmptyFootprintAndGenericOnlySitesDoNotRender) {
    const auto                 geometry = d2ar::AdventureMapGeometry::from_source(20, 20);
    d2ar::AdventureMapPreparer preparer(geometry);
    const auto                 catalog = make_catalog();
    const auto                 banner_catalog = make_banner_catalog();
    preparer.add_contributor(d2ar::make_site_contributor(catalog, banner_catalog));

    d2runtime::AdventureWorldState world;
    world.map_width = 20;
    world.map_height = 20;
    world.sites.push_back(make_site("S_EMPTY", d2runtime::AdventureSiteKind::Mage, 2, {}));
    world.map_objects.push_back({.id = "GENERIC",
                                 .kind = d2runtime::AdventureMapObjectKind::SiteTrainer,
                                 .position = {1, 1},
                                 .footprint = {{1, 1}}});

    const auto result = preparer.prepare(world);

    EXPECT_TRUE(result.graph.world.empty());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics.front().kind, d2ar::PrepareDiagnosticKind::UnresolvedNoSprite);
    EXPECT_NE(result.diagnostics.front().message.find("reason=empty_footprint"), std::string::npos);
}
