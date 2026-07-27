#include <gtest/gtest.h>

#include <d2adventure_render/adventure_render_types.hpp>
#include <d2adventure_render/map_geometry.hpp>
#include <d2adventure_render/map_preparer.hpp>
#include <d2adventure_render/preparation_context.hpp>
#include <d2adventure_render/resource_node_contributor.hpp>
#include <d2adventure_render/terrain/resource_node_asset_catalog.hpp>
#include <d2runtime/AdventureResourceNode.hpp>
#include <d2runtime/AdventureWorldState.hpp>

#include <d2engine/assets/image_asset_key.hpp>
#include <d2engine/assets/render_graph_asset_collector.hpp>

#include <set>
#include <string>
#include <vector>

using namespace d2engine::adventure_render;
using namespace d2runtime;

// ── Helpers ───────────────────────────────────────────────────────────────

static ResourceNodeAssetCatalog goldmine_only_catalog() {
    ResourceNodeAssetCatalog cat;
    StaticResourceNodeVisual vis;
    vis.container_path = "Imgs/IsoCmon.ff";
    vis.logical_sprite = "G000CR0000GL";
    vis.canvas_width = 200;
    vis.canvas_height = 160;
    vis.canvas_foot_x = 80;
    vis.canvas_foot_y = 130;
    cat.visuals[AdventureResourceKind::GoldMine] = std::move(vis);
    return cat;
}

static ResourceNodeAssetCatalog redmana_only_catalog() {
    ResourceNodeAssetCatalog   cat;
    AnimatedResourceNodeVisual vis;
    vis.container_path = "Imgs/IsoAnim.ff";
    vis.logical_animation = "G000CR0000RD";
    vis.canvas_foot_x = 65;
    vis.canvas_foot_y = 115;
    AdventureAnimationFrame af;
    af.record_name = "G000CR0000RD_F1";
    af.duration_ms = 100;
    af.canvas_width = 180;
    af.canvas_height = 150;
    vis.animation_data.animation_name = "G000CR0000RD";
    vis.animation_data.frames.push_back(std::move(af));
    vis.animation_data.is_looping = true;
    cat.visuals[AdventureResourceKind::RedMana] = std::move(vis);
    return cat;
}

static ResourceNodeAssetCatalog full_catalog() {
    ResourceNodeAssetCatalog cat;

    // Gold Mine: static
    {
        StaticResourceNodeVisual vis;
        vis.container_path = "Imgs/IsoCmon.ff";
        vis.logical_sprite = "G000CR0000GL";
        vis.canvas_width = 200;
        vis.canvas_height = 160;
        vis.canvas_foot_x = 80;
        vis.canvas_foot_y = 130;
        cat.visuals[AdventureResourceKind::GoldMine] = std::move(vis);
    }

    // Mana entries — each with one frame
    struct ManaVis {
        AdventureResourceKind kind;
        const char*           anim_name;
        const char*           frame_name;
        int                   fx;
        int                   fy;
    };
    const ManaVis mana_vis[] = {
        {AdventureResourceKind::RedMana, "G000CR0000RD", "G000CR0000RD_F1", 65, 115},
        {AdventureResourceKind::YellowMana, "G000CR0000YE", "G000CR0000YE_F1", 72, 122},
        {AdventureResourceKind::OrangeMana, "G000CR0000RG", "G000CR0000RG_F1", 87, 140},
        {AdventureResourceKind::WhiteMana, "G000CR0000WH", "G000CR0000WH_F1", 57, 102},
        {AdventureResourceKind::BlueMana, "G000CR0000GR", "G000CR0000GR_F1", 68, 117},
    };

    for (const auto& m : mana_vis) {
        AnimatedResourceNodeVisual vis;
        vis.container_path = "Imgs/IsoAnim.ff";
        vis.logical_animation = m.anim_name;
        vis.canvas_foot_x = m.fx;
        vis.canvas_foot_y = m.fy;
        AdventureAnimationFrame af;
        af.record_name = m.frame_name;
        af.duration_ms = 100;
        af.canvas_width = 180;
        af.canvas_height = 150;
        vis.animation_data.animation_name = m.anim_name;
        vis.animation_data.frames.push_back(std::move(af));
        vis.animation_data.is_looping = true;
        cat.visuals[m.kind] = std::move(vis);
    }

    return cat;
}

static void add_node(AdventureWorldState& world, const std::string& id, int x, int y, int raw,
                     AdventureResourceKind kind) {
    AdventureResourceNode n;
    n.id = id;
    n.position = {x, y};
    n.footprint = {{x, y}};
    n.raw_resource = raw;
    n.resource_kind = kind;
    world.resource_nodes.push_back(std::move(n));
}

// ── GoldMine produces a static primitive (no animation) ──────────────

TEST(ResourceNodeContributor, GoldMineProducesStaticPrimitive) {
    const auto geometry = AdventureMapGeometry::from_source(36, 36);
    auto       catalog = goldmine_only_catalog();

    AdventureWorldState world;
    world.map_width = 36;
    world.map_height = 36;
    add_node(world, "S143CR0005", 19, 1, 0, AdventureResourceKind::GoldMine);

    AdventureMapPreparer preparer(geometry);
    preparer.add_contributor(make_resource_node_contributor(catalog));
    auto result = preparer.prepare(world);

    ASSERT_EQ(result.graph.world.size(), 1u);
    const auto& prim = result.graph.world[0];
    EXPECT_EQ(prim.container_path, "Imgs/IsoCmon.ff");
    EXPECT_EQ(prim.record_name, "G000CR0000GL");
    EXPECT_FALSE(prim.animation.has_value());
    EXPECT_EQ(prim.level, WorldRenderLevel::Structure);
    EXPECT_EQ(result.pick_entries.size(), 0u);
}

// ── RedMana produces an animated primitive ──────────────────────────

TEST(ResourceNodeContributor, RedManaProducesAnimatedPrimitive) {
    const auto geometry = AdventureMapGeometry::from_source(36, 36);
    auto       catalog = redmana_only_catalog();

    AdventureWorldState world;
    world.map_width = 36;
    world.map_height = 36;
    add_node(world, "S143CR0000", 19, 4, 1, AdventureResourceKind::RedMana);

    AdventureMapPreparer preparer(geometry);
    preparer.add_contributor(make_resource_node_contributor(catalog));
    auto result = preparer.prepare(world);

    ASSERT_EQ(result.graph.world.size(), 1u);
    const auto& prim = result.graph.world[0];
    EXPECT_EQ(prim.container_path, "Imgs/IsoAnim.ff");
    ASSERT_TRUE(prim.animation.has_value());
    EXPECT_EQ(prim.animation->animation_name, "G000CR0000RD");
    EXPECT_EQ(prim.animation->frames.size(), 1u);
    EXPECT_TRUE(prim.animation->is_looping);
    EXPECT_EQ(prim.record_name, "G000CR0000RD_F1");
    EXPECT_EQ(prim.src_width, 180);
    EXPECT_EQ(prim.src_height, 150);

    const auto foot = geometry.cell_foot_anchor({19, 4});
    EXPECT_EQ(prim.draw_origin.x, foot.x - 65);
    EXPECT_EQ(prim.draw_origin.y, foot.y - 115);
}

// ── Missing catalog entry throws ────────────────────────────────────

TEST(ResourceNodeContributor, KindMissingInCatalogThrows) {
    const auto               geometry = AdventureMapGeometry::from_source(36, 36);
    ResourceNodeAssetCatalog empty_catalog;

    AdventureWorldState world;
    world.map_width = 36;
    world.map_height = 36;
    add_node(world, "S143CR0000", 19, 4, 1, AdventureResourceKind::RedMana);

    AdventureMapPreparer preparer(geometry);
    preparer.add_contributor(make_resource_node_contributor(empty_catalog));
    EXPECT_THROW({ static_cast<void>(preparer.prepare(world)); }, std::runtime_error);
}

// ── Multiple nodes: one static + five animated ──────────────────────

TEST(ResourceNodeContributor, MultipleNodesCorrectPerKindVisuals) {
    const auto geometry = AdventureMapGeometry::from_source(36, 36);
    auto       catalog = full_catalog();

    AdventureWorldState world;
    world.map_width = 36;
    world.map_height = 36;
    add_node(world, "S143CR0005", 19, 1, 0, AdventureResourceKind::GoldMine);
    add_node(world, "S143CR0000", 19, 4, 1, AdventureResourceKind::RedMana);
    add_node(world, "S143CR0003", 22, 7, 2, AdventureResourceKind::YellowMana);
    add_node(world, "S143CR0002", 25, 7, 3, AdventureResourceKind::OrangeMana);
    add_node(world, "S143CR0001", 23, 3, 4, AdventureResourceKind::WhiteMana);
    add_node(world, "S143CR0004", 25, 0, 5, AdventureResourceKind::BlueMana);

    AdventureMapPreparer preparer(geometry);
    preparer.add_contributor(make_resource_node_contributor(catalog));
    auto result = preparer.prepare(world);

    EXPECT_EQ(result.graph.world.size(), 6u);
    EXPECT_EQ(result.pick_entries.size(), 0u);

    auto find_prim = [&](const std::string& id) -> const PreparedAdventureRenderPrimitive* {
        auto sid = stable_render_id("ResourceNode:" + id);
        for (const auto& p : result.graph.world) {
            if (p.stable_id == sid)
                return &p;
        }
        return nullptr;
    };

    // GoldMine: static, IsoCmon, GL
    {
        const auto* p = find_prim("S143CR0005");
        ASSERT_NE(p, nullptr);
        EXPECT_EQ(p->container_path, "Imgs/IsoCmon.ff");
        EXPECT_EQ(p->record_name, "G000CR0000GL");
        EXPECT_FALSE(p->animation.has_value());
    }
    // RedMana: animated, IsoAnim, RD
    {
        const auto* p = find_prim("S143CR0000");
        ASSERT_NE(p, nullptr);
        EXPECT_EQ(p->container_path, "Imgs/IsoAnim.ff");
        ASSERT_TRUE(p->animation.has_value());
        EXPECT_EQ(p->animation->animation_name, "G000CR0000RD");
        EXPECT_TRUE(p->animation->is_looping);
    }
    // YellowMana: animated, IsoAnim, YE (NOT GR)
    {
        const auto* p = find_prim("S143CR0003");
        ASSERT_NE(p, nullptr);
        EXPECT_EQ(p->container_path, "Imgs/IsoAnim.ff");
        ASSERT_TRUE(p->animation.has_value());
        EXPECT_EQ(p->animation->animation_name, "G000CR0000YE");
        EXPECT_TRUE(p->animation->is_looping);
    }
    // OrangeMana: animated, IsoAnim, RG
    {
        const auto* p = find_prim("S143CR0002");
        ASSERT_NE(p, nullptr);
        EXPECT_EQ(p->container_path, "Imgs/IsoAnim.ff");
        ASSERT_TRUE(p->animation.has_value());
        EXPECT_EQ(p->animation->animation_name, "G000CR0000RG");
    }
    // WhiteMana: animated, IsoAnim, WH
    {
        const auto* p = find_prim("S143CR0001");
        ASSERT_NE(p, nullptr);
        EXPECT_EQ(p->container_path, "Imgs/IsoAnim.ff");
        ASSERT_TRUE(p->animation.has_value());
        EXPECT_EQ(p->animation->animation_name, "G000CR0000WH");
    }
    // BlueMana: animated, IsoAnim, GR (NOT YE)
    {
        const auto* p = find_prim("S143CR0004");
        ASSERT_NE(p, nullptr);
        EXPECT_EQ(p->container_path, "Imgs/IsoAnim.ff");
        ASSERT_TRUE(p->animation.has_value());
        EXPECT_EQ(p->animation->animation_name, "G000CR0000GR");
    }
}

// ── Preload keys: 1 IsoCmon + 165 IsoAnim = 166 total ──────────────

TEST(ResourceNodeContributor, ExactPreloadKeys) {
    const auto geometry = AdventureMapGeometry::from_source(36, 36);

    // Build catalog with GoldMine static + 5 mana animations with realistic frame counts
    ResourceNodeAssetCatalog cat;

    // GoldMine: static
    {
        StaticResourceNodeVisual vis;
        vis.container_path = "Imgs/IsoCmon.ff";
        vis.logical_sprite = "G000CR0000GL";
        vis.canvas_width = 200;
        vis.canvas_height = 160;
        vis.canvas_foot_x = 80;
        vis.canvas_foot_y = 130;
        cat.visuals[AdventureResourceKind::GoldMine] = std::move(vis);
    }

    // Mana animations with frame counts matching reality
    struct ManaSpec {
        AdventureResourceKind kind;
        const char*           anim;
        int                   frames;
    };
    const ManaSpec mana_specs[] = {
        {AdventureResourceKind::RedMana, "G000CR0000RD", 35},
        {AdventureResourceKind::YellowMana, "G000CR0000YE", 35},
        {AdventureResourceKind::OrangeMana, "G000CR0000RG", 35},
        {AdventureResourceKind::WhiteMana, "G000CR0000WH", 35},
        {AdventureResourceKind::BlueMana, "G000CR0000GR", 25},
    };

    int total_anim_frames = 0;
    for (const auto& ms : mana_specs) {
        AnimatedResourceNodeVisual vis;
        vis.container_path = "Imgs/IsoAnim.ff";
        vis.logical_animation = ms.anim;
        vis.canvas_foot_x = 60;
        vis.canvas_foot_y = 110;
        vis.animation_data.animation_name = ms.anim;
        vis.animation_data.is_looping = true;
        vis.animation_data.frames.reserve(static_cast<std::size_t>(ms.frames));
        for (int i = 0; i < ms.frames; ++i) {
            AdventureAnimationFrame af;
            af.record_name = std::string(ms.anim) + "_F" + std::to_string(i);
            af.duration_ms = 100;
            af.canvas_width = 180;
            af.canvas_height = 150;
            vis.animation_data.frames.push_back(std::move(af));
        }
        total_anim_frames += ms.frames;
        cat.visuals[ms.kind] = std::move(vis);
    }

    AdventureWorldState world;
    world.map_width = 36;
    world.map_height = 36;
    add_node(world, "CR0", 19, 1, 0, AdventureResourceKind::GoldMine);
    add_node(world, "CR1", 19, 4, 1, AdventureResourceKind::RedMana);
    add_node(world, "CR2", 22, 7, 2, AdventureResourceKind::YellowMana);
    add_node(world, "CR3", 25, 7, 3, AdventureResourceKind::OrangeMana);
    add_node(world, "CR4", 23, 3, 4, AdventureResourceKind::WhiteMana);
    add_node(world, "CR5", 25, 0, 5, AdventureResourceKind::BlueMana);

    AdventureMapPreparer preparer(geometry);
    preparer.add_contributor(make_resource_node_contributor(cat));
    auto result = preparer.prepare(world);

    ASSERT_EQ(result.graph.world.size(), 6u);

    auto keys = d2engine::collect_adventure_render_asset_keys(result.graph);

    int iso_cmon_count = 0;
    int iso_anim_count = 0;
    for (const auto& k : keys) {
        if (k.container_path == "Imgs/IsoCmon.ff")
            ++iso_cmon_count;
        else if (k.container_path == "Imgs/IsoAnim.ff")
            ++iso_anim_count;
    }

    EXPECT_EQ(iso_cmon_count, 1) << "exactly one IsoCmon key for GoldMine static sprite";
    EXPECT_EQ(iso_anim_count, total_anim_frames)
        << "exactly " << total_anim_frames << " IsoAnim frame keys";
    EXPECT_EQ(keys.size(), static_cast<std::size_t>(1 + total_anim_frames))
        << "total preload keys = 1 IsoCmon + " << total_anim_frames
        << " IsoAnim = " << (1 + total_anim_frames);

    // No IsoStill keys
    for (const auto& k : keys) {
        EXPECT_NE(k.container_path, "Imgs/IsoStill.ff")
            << "no IsoStill preload keys for resource nodes";
    }

    // No mana animation identity requested as IsoCmon static
    const std::string cmon_prefix = "Imgs/IsoCmon.ff";
    for (const auto& k : keys) {
        if (k.container_path == cmon_prefix) {
            EXPECT_EQ(k.image_name, "G000CR0000GL")
                << "only GoldMine sprite should be in IsoCmon keys; got " << k.image_name;
        }
    }

    // Total unique keys: 166 (1 + 35 + 35 + 35 + 35 + 25)
    constexpr int expected_total = 1 + 35 + 35 + 35 + 35 + 25; // 166
    EXPECT_EQ(keys.size(), expected_total);
}
