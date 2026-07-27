#include <d2adventure_render/contained_stack_presentation_contributor.hpp>
#include <d2adventure_render/contained_stack_presentation_builder.hpp>
#include <d2adventure_render/map_geometry.hpp>
#include <d2adventure_render/map_preparer.hpp>

#include <d2engine/assets/contained_stack_shield_asset_catalog_builder.hpp>
#include <d2engine/assets/stack_banner_asset_catalog_builder.hpp>

#include <d2runtime/AdventureWorldState.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

namespace {

using d2engine::adventure_render::AdventureMapGeometry;
using d2engine::adventure_render::ContainedStackShieldAssetCatalog;
using d2engine::adventure_render::PickEntryKind;
using d2engine::adventure_render::WorldRenderLevel;

struct Catalogs {
    ContainedStackShieldAssetCatalog shield;
};

Catalogs make_catalogs() {
    Catalogs catalogs;
    catalogs.shield = d2engine::detail::build_contained_stack_shield_asset_catalog_from_metadata(
        [](std::string_view, std::string_view animation) {
            d2engine::AnimationSequence sequence;
            sequence.name = std::string(animation);
            sequence.container_path = "Imgs/IsoCmon.ff";
            if (animation == "G000RR0005SHLC8") {
                sequence.native_canvas_w = 640;
                sequence.native_canvas_h = 480;
                sequence.canvas_foot_x = 321;
                sequence.canvas_foot_y = 323;
                sequence.frames.push_back({.image_name = "ZC", .index = 0, .duration_ms = 100});
                return sequence;
            }
            if (animation == "G000RR0005SHLV8") {
                sequence.native_canvas_w = 640;
                sequence.native_canvas_h = 480;
                sequence.canvas_foot_x = 321;
                sequence.canvas_foot_y = 307;
                sequence.frames.push_back({.image_name = "0C", .index = 0, .duration_ms = 100});
                return sequence;
            }
            sequence.native_canvas_w = 300;
            sequence.native_canvas_h = 200;
            sequence.canvas_foot_x = 10;
            sequence.canvas_foot_y = 20;
            sequence.frames.push_back({.image_name = "TI", .index = 0, .duration_ms = 100});
            return sequence;
        },
        [](std::string_view, std::string_view sprite) {
            struct SpriteMeta {
                int canvas_width;
                int canvas_height;
                int canvas_foot_x;
                int canvas_foot_y;
            };
            if (sprite == "ZC") {
                return SpriteMeta{640, 480, 321, 323};
            }
            if (sprite == "0C") {
                return SpriteMeta{640, 480, 321, 307};
            }
            return SpriteMeta{300, 200, 10, 20};
        });

    return catalogs;
}

d2runtime::AdventureWorldState make_world() {
    d2runtime::AdventureWorldState world;
    world.map_width = 20;
    world.map_height = 20;
    world.terrain.width = 20;
    world.terrain.height = 20;
    world.terrain.tiles.assign(400, {});

    d2runtime::AdventureSubraceRef subrace;
    subrace.id = "SUB1";
    subrace.race_id = "g000rr0000";
    subrace.banner = 2;
    world.subraces.push_back(subrace);

    d2runtime::AdventureUnitInstance unit;
    unit.id = "UNIT1";
    unit.type_id = "G000UU0001";
    unit.current_hp = 10;
    world.units.push_back(unit);

    return world;
}

d2runtime::AdventureStack make_stack(std::string id, std::string inside, std::string subrace) {
    d2runtime::AdventureStack stack;
    stack.id = std::move(id);
    stack.leader_id = "UNIT1";
    stack.subrace = std::move(subrace);
    stack.inside = std::move(inside);
    return stack;
}

d2runtime::AdventureCity make_city(std::string id, std::string stack_id, int x = 5, int y = 5) {
    d2runtime::AdventureCity city;
    city.id = std::move(id);
    city.stack_id = std::move(stack_id);
    for (int dy = 0; dy < 2; ++dy) {
        for (int dx = 0; dx < 2; ++dx) {
            city.footprint.push_back({x + dx, y + dy});
        }
    }
    return city;
}

d2runtime::AdventureCapital make_capital(std::string id, std::string visiting_stack_id, int x = 8,
                                         int y = 8) {
    d2runtime::AdventureCapital capital;
    capital.id = std::move(id);
    capital.visiting_stack_id = std::move(visiting_stack_id);
    for (int dy = 0; dy < 2; ++dy) {
        for (int dx = 0; dx < 2; ++dx) {
            capital.footprint.push_back({x + dx, y + dy});
        }
    }
    return capital;
}

template <typename Fn> void expect_throw_contains(Fn&& fn, const std::string& needle) {
    try {
        fn();
        FAIL() << "expected exception containing: " << needle;
    } catch (const std::exception& e) {
        EXPECT_NE(std::string(e.what()).find(needle), std::string::npos) << e.what();
    }
}

const d2engine::adventure_render::PreparedAdventureRenderPrimitive*
find_primitive(const d2engine::adventure_render::PrepareResult& result,
               std::string_view                                 debug_label) {
    const auto it = std::find_if(result.graph.world.begin(), result.graph.world.end(),
                                 [&](const auto& prim) { return prim.debug_label == debug_label; });
    if (it == result.graph.world.end()) {
        return nullptr;
    }
    return &*it;
}

void expect_shield_primitive(const d2engine::adventure_render::PrepareResult& result,
                             std::string_view debug_label, std::string_view stack_id,
                             const std::vector<d2runtime::FootprintCell>& footprint,
                             const std::string&                           expected_record) {
    const auto* prim = find_primitive(result, debug_label);
    ASSERT_NE(prim, nullptr);
    EXPECT_EQ(prim->phase, d2engine::adventure_render::AdventureRenderPhase::World);
    EXPECT_EQ(prim->level, WorldRenderLevel::Actor);
    EXPECT_EQ(prim->stable_id, d2engine::adventure_render::stable_render_id(
                                   "ContainedStackShield:" + std::string(stack_id)));
    EXPECT_EQ(prim->local_suborder, 0);
    EXPECT_EQ(prim->container_path, "Imgs/IsoCmon.ff");
    EXPECT_EQ(prim->record_name, expected_record);
    EXPECT_FALSE(prim->animation.has_value());
    EXPECT_EQ(prim->alpha, 1.0f);
    EXPECT_EQ(prim->footprint, footprint);
    EXPECT_EQ(prim->depth_anchor,
              d2engine::adventure_render::AdventureMapGeometry::derive_depth_anchor(footprint));
    const auto foot =
        d2engine::adventure_render::AdventureMapGeometry::from_source(20, 20).cell_foot_anchor(
            prim->depth_anchor);
    EXPECT_EQ(prim->draw_origin.x, foot.x - 10);
    EXPECT_EQ(prim->draw_origin.y, foot.y - 20);
    EXPECT_EQ(prim->src_width, 300);
    EXPECT_EQ(prim->src_height, 200);
    EXPECT_EQ(prim->visual_bounds.min_x, prim->draw_origin.x);
    EXPECT_EQ(prim->visual_bounds.min_y, prim->draw_origin.y);
    EXPECT_EQ(prim->visual_bounds.max_x, prim->draw_origin.x + 300);
    EXPECT_EQ(prim->visual_bounds.max_y, prim->draw_origin.y + 200);
}

} // namespace

TEST(ContainedStackPresentationContributor, VillageAndCapitalEachProduceShieldAndBannerPrimitives) {
    auto world = make_world();
    world.stacks.push_back(make_stack("STACK_CITY", "CITY1", "SUB1"));
    world.stacks.push_back(make_stack("STACK_CAP", "CAP1", "SUB1"));
    world.cities.push_back(make_city("CITY1", "STACK_CITY"));
    world.capitals.push_back(make_capital("CAP1", "STACK_CAP"));
    const auto catalogs = make_catalogs();

    const auto geometry = AdventureMapGeometry::from_source(20, 20);
    d2engine::adventure_render::AdventureMapPreparer preparer(geometry);
    preparer.add_contributor(
        d2engine::adventure_render::make_contained_stack_presentation_contributor(catalogs.shield));

    const auto result = preparer.prepare(world);
    ASSERT_EQ(result.graph.world.size(), 2u);
    ASSERT_EQ(result.pick_entries.size(), 2u);

    expect_shield_primitive(result, "ContainedStackShield:CITY1:STACK_CITY:g000rr0000",
                            "STACK_CITY", world.cities.front().footprint, "G000RR0000SHLV8");

    expect_shield_primitive(result, "ContainedStackShield:CAP1:STACK_CAP:g000rr0000", "STACK_CAP",
                            world.capitals.front().footprint, "G000RR0000SHLC8");
}

TEST(ContainedStackPresentationContributor, NoPresentationIsProducedForNullSettlementStack) {
    auto world = make_world();
    world.cities.push_back(make_city("CITY1", ""));
    world.capitals.push_back(make_capital("CAP1", "G000000000"));
    const auto catalogs = make_catalogs();

    const auto geometry = AdventureMapGeometry::from_source(20, 20);
    d2engine::adventure_render::AdventureMapPreparer preparer(geometry);
    preparer.add_contributor(
        d2engine::adventure_render::make_contained_stack_presentation_contributor(catalogs.shield));

    const auto result = preparer.prepare(world);
    EXPECT_TRUE(result.graph.world.empty());
    EXPECT_TRUE(result.pick_entries.empty());
}

TEST(ContainedStackPresentationContributor, SelectsBannerFrameAndPreservesFootprintAndAnchor) {
    auto world = make_world();
    world.stacks.push_back(make_stack("STACK_CITY", "CITY1", "SUB1"));
    world.cities.push_back(make_city("CITY1", "STACK_CITY", 6, 7));
    const auto catalogs = make_catalogs();

    const auto geometry = AdventureMapGeometry::from_source(20, 20);
    d2engine::adventure_render::AdventureMapPreparer preparer(geometry);
    preparer.add_contributor(
        d2engine::adventure_render::make_contained_stack_presentation_contributor(catalogs.shield));

    const auto result = preparer.prepare(world);
    expect_shield_primitive(result, "ContainedStackShield:CITY1:STACK_CITY:g000rr0000",
                            "STACK_CITY", world.cities.front().footprint, "G000RR0000SHLV8");
}

TEST(ContainedStackPresentationContributor,
     PresentationBuilderPopulatesResolvedShieldAndBannerMetadata) {
    auto world = make_world();
    world.stacks.push_back(make_stack("STACK_CITY", "CITY1", "SUB1"));
    world.cities.push_back(make_city("CITY1", "STACK_CITY"));
    const auto catalogs = make_catalogs();

    const auto  geometry = AdventureMapGeometry::from_source(20, 20);
    const auto* stack = world.find_stack("STACK_CITY");
    ASSERT_NE(stack, nullptr);
    const auto location = world.find_contained_stack_location(*stack);
    ASSERT_TRUE(location.has_value());
    const auto* subrace = world.find_subrace(stack->subrace);
    ASSERT_NE(subrace, nullptr);

    d2engine::adventure_render::ContainedStackPresentationBuilder builder(catalogs.shield,
                                                                          geometry);
    const auto presentation = builder.build(world, *stack, *location, *subrace);

    EXPECT_EQ(presentation.shield_outer_logical_name, "G000RR0000SHLV8");
    EXPECT_EQ(presentation.shield_sprite_name, "G000RR0000SHLV8");
    EXPECT_EQ(presentation.shield.debug_label, "ContainedStackShield:CITY1:STACK_CITY:g000rr0000");
}

TEST(ContainedStackPresentationContributor, RejectsMalformedSettlementRelationships) {
    const auto geometry = AdventureMapGeometry::from_source(20, 20);
    const auto catalogs = make_catalogs();
    auto       prepare_world = [&](d2runtime::AdventureWorldState world) {
        d2engine::adventure_render::AdventureMapPreparer preparer(geometry);
        preparer.add_contributor(
            d2engine::adventure_render::make_contained_stack_presentation_contributor(
                catalogs.shield));
        return preparer.prepare(world);
    };

    auto base_city = make_city("CITY1", "STACK_CITY");
    auto base_stack = make_stack("STACK_CITY", "CITY1", "SUB1");

    {
        auto world = make_world();
        world.cities.push_back(base_city);
        expect_throw_contains([&] { (void)prepare_world(std::move(world)); },
                              "contained_stack_shield_missing_stack");
    }
    {
        auto world = make_world();
        world.cities.push_back(base_city);
        world.stacks.push_back(make_stack("STACK_CITY", "", "SUB1"));
        expect_throw_contains([&] { (void)prepare_world(std::move(world)); },
                              "contained_stack_shield_stack_is_map_visible");
    }
    {
        auto world = make_world();
        base_stack.inside = "OTHER";
        world.cities.push_back(base_city);
        world.stacks.push_back(base_stack);
        expect_throw_contains([&] { (void)prepare_world(std::move(world)); },
                              "contained_stack_shield_inside_mismatch");
    }
    {
        auto world = make_world();
        base_stack.inside = "CITY1";
        base_stack.subrace.clear();
        world.cities.push_back(base_city);
        world.stacks.push_back(base_stack);
        expect_throw_contains([&] { (void)prepare_world(std::move(world)); },
                              "contained_stack_shield_missing_subrace");
    }
    {
        auto world = make_world();
        base_stack.inside = "CITY1";
        base_stack.subrace = "MISSING";
        world.cities.push_back(base_city);
        world.stacks.push_back(base_stack);
        expect_throw_contains([&] { (void)prepare_world(std::move(world)); },
                              "contained_stack_shield_dangling_subrace");
    }
    {
        auto world = make_world();
        base_stack.inside = "CITY1";
        base_stack.subrace = "SUB1";
        world.cities.push_back(make_city("CITY1", "STACK_CITY"));
        world.cities.front().footprint.clear();
        world.stacks.push_back(base_stack);
        expect_throw_contains([&] { (void)prepare_world(std::move(world)); },
                              "contained_stack_shield_missing_footprint");
    }
}
