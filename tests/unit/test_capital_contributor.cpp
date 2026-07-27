#include <d2adventure_render/capital_contributor.hpp>
#include <d2adventure_render/map_geometry.hpp>
#include <d2adventure_render/map_preparer.hpp>
#include <d2adventure_render/terrain/capital_asset_catalog.hpp>

#include <d2runtime/AdventureWorldState.hpp>

#include <gtest/gtest.h>

#include <functional>
#include <string>
#include <string_view>

namespace {

using d2engine::adventure_render::AdventureAnimationTimingSource;
using d2engine::adventure_render::AdventureMapGeometry;
using d2engine::adventure_render::AdventureMapPreparer;
using d2engine::adventure_render::AnimatedCapitalVisual;
using d2engine::adventure_render::CapitalAssetCatalog;
using d2engine::adventure_render::CapitalVisualSet;
using d2engine::adventure_render::CapitalVisualState;
using d2engine::adventure_render::MapCell;
using d2engine::adventure_render::PrepareDiagnosticKind;
using d2engine::adventure_render::ResolvedCapitalVisual;
using d2engine::adventure_render::WorldRenderLevel;

AnimatedCapitalVisual make_visual(std::string name) {
    AnimatedCapitalVisual visual;
    visual.container_path = "Imgs/IsoAnim.ff";
    visual.logical_animation_name = std::move(name);
    visual.canvas_foot_x = 151;
    visual.canvas_foot_y = 252;
    visual.animation_data.animation_name = visual.logical_animation_name;
    visual.animation_data.native_canvas_w = 320;
    visual.animation_data.native_canvas_h = 320;
    visual.animation_data.is_looping = true;
    visual.animation_data.timing_source = AdventureAnimationTimingSource::ProvisionalFallback;
    visual.animation_data.frames = {
        {.record_name = "H0", .duration_ms = 100, .canvas_width = 320, .canvas_height = 320},
        {.record_name = "H1", .duration_ms = 100, .canvas_width = 480, .canvas_height = 400},
    };
    return visual;
}

CapitalAssetCatalog make_catalog() {
    CapitalAssetCatalog catalog;
    catalog.visuals.emplace("g000rr0000", CapitalVisualSet{make_visual("G000FT0000HU0"),
                                                           make_visual("G000FT0000HUC0")});
    return catalog;
}

d2runtime::AdventureWorldState make_world() {
    d2runtime::AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;

    d2runtime::AdventureSubraceRef subrace;
    subrace.id = "sub1";
    subrace.race_id = "g000rr0000";
    world.subraces.push_back(subrace);

    d2runtime::AdventureCapital capital;
    capital.id = "capital_human";
    capital.owner = "owner1";
    capital.subrace = "sub1";
    capital.position = {5, 5};
    for (int y = 5; y < 8; ++y) {
        for (int x = 5; x < 8; ++x) {
            capital.footprint.push_back({x, y});
        }
    }
    world.capitals.push_back(capital);

    return world;
}

void expect_throw_contains(const std::function<void()>& fn, const std::string& needle) {
    try {
        fn();
        FAIL() << "expected exception containing: " << needle;
    } catch (const std::exception& e) {
        EXPECT_NE(std::string(e.what()).find(needle), std::string::npos) << e.what();
    }
}

auto make_resolver(ResolvedCapitalVisual resolved) {
    return [resolved = std::move(resolved)](const d2runtime::AdventureWorldState& world,
                                            const d2runtime::AdventureCapital&    capital,
                                            std::string_view) {
        (void)world;
        (void)capital;
        return resolved;
    };
}

} // namespace

TEST(CapitalContributor, ProducesOneAnimatedStructurePrimitive) {
    const auto           geometry = AdventureMapGeometry::from_source(10, 10);
    const auto           catalog = make_catalog();
    AdventureMapPreparer preparer(geometry);

    ResolvedCapitalVisual resolved;
    resolved.state = CapitalVisualState::Active;
    resolved.guardian_type_id = "g000uu9999";
    resolved.guardian_instance_id = "guardian1";
    resolved.visual = &catalog.resolve("g000rr0000", CapitalVisualState::Active);
    preparer.add_contributor(
        d2engine::adventure_render::make_capital_contributor(catalog, make_resolver(resolved)));

    const auto result = preparer.prepare(make_world());

    ASSERT_EQ(result.graph.world.size(), 1u);
    ASSERT_TRUE(result.pick_entries.empty());
    ASSERT_EQ(result.diagnostics.size(), 1u);

    const auto& prim = result.graph.world.front();
    EXPECT_EQ(prim.phase, d2engine::adventure_render::AdventureRenderPhase::World);
    EXPECT_EQ(prim.level, WorldRenderLevel::Structure);
    EXPECT_EQ(prim.stable_id,
              d2engine::adventure_render::stable_render_id("Capital:capital_human"));
    EXPECT_EQ(prim.debug_label, "Capital:capital_human");
    EXPECT_EQ(prim.local_suborder, 0);
    EXPECT_EQ(prim.footprint.size(), 9u);
    EXPECT_EQ(prim.depth_anchor, (MapCell{7, 7}));

    const auto foot = geometry.cell_foot_anchor(prim.depth_anchor);
    EXPECT_EQ(prim.draw_origin.x, foot.x - 151);
    EXPECT_EQ(prim.draw_origin.y, foot.y - 252);
    EXPECT_EQ(prim.container_path, "Imgs/IsoAnim.ff");
    EXPECT_EQ(prim.record_name, "H0");
    ASSERT_TRUE(prim.animation.has_value());
    EXPECT_EQ(prim.animation->animation_name, "G000FT0000HU0");
    EXPECT_EQ(prim.animation->frames.size(), 2u);
    EXPECT_EQ(prim.src_width, 320);
    EXPECT_EQ(prim.src_height, 320);
    EXPECT_EQ(prim.visual_bounds.min_x, prim.draw_origin.x);
    EXPECT_EQ(prim.visual_bounds.min_y, prim.draw_origin.y);
    EXPECT_EQ(prim.visual_bounds.max_x, prim.draw_origin.x + 480);
    EXPECT_EQ(prim.visual_bounds.max_y, prim.draw_origin.y + 400);

    const auto& diag = result.diagnostics.front();
    EXPECT_EQ(diag.kind, PrepareDiagnosticKind::Resolved);
    EXPECT_EQ(diag.object_id, "capital_human");
    EXPECT_EQ(diag.object_kind, "Capital");
    EXPECT_NE(diag.message.find("subrace=sub1"), std::string::npos);
    EXPECT_NE(diag.message.find("state=Active"), std::string::npos);
    EXPECT_NE(diag.message.find("guardian_type=g000uu9999"), std::string::npos);
    EXPECT_NE(diag.message.find("guardian_instance=guardian1"), std::string::npos);
    EXPECT_NE(diag.message.find("animation=G000FT0000HU0"), std::string::npos);
    EXPECT_NE(diag.message.find("frames=2"), std::string::npos);
    EXPECT_NE(diag.message.find("depth_anchor=(7,7)"), std::string::npos);
}

TEST(CapitalContributor, SelectsRuinedVisualFromResolvedState) {
    const auto           geometry = AdventureMapGeometry::from_source(10, 10);
    const auto           catalog = make_catalog();
    AdventureMapPreparer preparer(geometry);

    ResolvedCapitalVisual resolved;
    resolved.state = CapitalVisualState::Ruined;
    resolved.guardian_type_id = "g000uu9999";
    resolved.guardian_instance_id = "guardian1";
    resolved.visual = &catalog.resolve("g000rr0000", CapitalVisualState::Ruined);
    preparer.add_contributor(
        d2engine::adventure_render::make_capital_contributor(catalog, make_resolver(resolved)));

    const auto result = preparer.prepare(make_world());
    ASSERT_EQ(result.graph.world.size(), 1u);
    ASSERT_TRUE(result.graph.world.front().animation.has_value());
    EXPECT_EQ(result.graph.world.front().animation->animation_name, "G000FT0000HUC0");
}

TEST(CapitalContributor, RejectsNullVisualStrictly) {
    const auto           geometry = AdventureMapGeometry::from_source(10, 10);
    const auto           catalog = make_catalog();
    AdventureMapPreparer preparer(geometry);

    ResolvedCapitalVisual resolved;
    resolved.state = CapitalVisualState::Active;
    resolved.guardian_type_id = "g000uu9999";
    resolved.guardian_instance_id = "guardian1";
    resolved.visual = nullptr;
    preparer.add_contributor(
        d2engine::adventure_render::make_capital_contributor(catalog, make_resolver(resolved)));

    expect_throw_contains([&] { static_cast<void>(preparer.prepare(make_world())); },
                          "capital_missing_visual capital=capital_human");
}
