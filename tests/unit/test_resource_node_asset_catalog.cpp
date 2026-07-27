#include <gtest/gtest.h>

#include <d2adventure_render/terrain/resource_node_asset_catalog.hpp>
#include <d2runtime/AdventureResourceNode.hpp>

#include <string>

using namespace d2engine::adventure_render;
using namespace d2runtime;

// ── Exhaustive semantic AdventureResourceKind mapping ──────────────────

struct KindEntry {
    AdventureResourceKind kind;
    int                   raw_value;
    const char*           label;
};

static const KindEntry kKindEntries[] = {
    {AdventureResourceKind::GoldMine, 0, "GoldMine"},
    {AdventureResourceKind::RedMana, 1, "RedMana"},
    {AdventureResourceKind::YellowMana, 2, "YellowMana"},
    {AdventureResourceKind::OrangeMana, 3, "OrangeMana"},
    {AdventureResourceKind::WhiteMana, 4, "WhiteMana"},
    {AdventureResourceKind::BlueMana, 5, "BlueMana"},
};

TEST(ResourceNodeAssetCatalog, ExhaustiveKindMapping) {
    for (const auto& e : kKindEntries) {
        EXPECT_EQ(static_cast<int>(e.kind), e.raw_value) << e.label;
    }
}

// ── Exhaustive visual mapping: GoldMine static, mana kinds animated ────

struct VisualEntry {
    AdventureResourceKind kind;
    const char*           container;
    const char*           logical_id;
    bool                  is_static;
    const char*           label;
};

static const VisualEntry kVisualEntries[] = {
    {AdventureResourceKind::GoldMine, "Imgs/IsoCmon.ff", "G000CR0000GL", true, "GoldMine"},
    {AdventureResourceKind::RedMana, "Imgs/IsoAnim.ff", "G000CR0000RD", false, "RedMana"},
    {AdventureResourceKind::YellowMana, "Imgs/IsoAnim.ff", "G000CR0000YE", false, "YellowMana"},
    {AdventureResourceKind::OrangeMana, "Imgs/IsoAnim.ff", "G000CR0000RG", false, "OrangeMana"},
    {AdventureResourceKind::WhiteMana, "Imgs/IsoAnim.ff", "G000CR0000WH", false, "WhiteMana"},
    {AdventureResourceKind::BlueMana, "Imgs/IsoAnim.ff", "G000CR0000GR", false, "BlueMana"},
};

TEST(ResourceNodeAssetCatalog, ExhaustiveVisualMapping) {
    ResourceNodeAssetCatalog catalog;

    for (const auto& v : kVisualEntries) {
        if (v.is_static) {
            StaticResourceNodeVisual vis;
            vis.container_path = v.container;
            vis.logical_sprite = v.logical_id;
            vis.canvas_width = 200;
            vis.canvas_height = 160;
            vis.canvas_foot_x = 80;
            vis.canvas_foot_y = 130;
            catalog.visuals[v.kind] = std::move(vis);
        } else {
            AnimatedResourceNodeVisual vis;
            vis.container_path = v.container;
            vis.logical_animation = v.logical_id;
            vis.canvas_foot_x = 60;
            vis.canvas_foot_y = 110;
            AdventureAnimationFrame af;
            af.record_name = v.logical_id;
            af.duration_ms = 100;
            af.canvas_width = 180;
            af.canvas_height = 150;
            vis.animation_data.animation_name = v.logical_id;
            vis.animation_data.frames.push_back(std::move(af));
            vis.animation_data.is_looping = true;
            catalog.visuals[v.kind] = std::move(vis);
        }
    }

    EXPECT_EQ(catalog.visuals.size(), 6u);

    for (const auto& v : kVisualEntries) {
        const auto& resolved = catalog.resolve(v.kind);

        if (v.is_static) {
            const auto* sv = std::get_if<StaticResourceNodeVisual>(&resolved);
            ASSERT_NE(sv, nullptr) << v.label << " expected static";
            EXPECT_EQ(sv->container_path, v.container) << v.label;
            EXPECT_EQ(sv->logical_sprite, v.logical_id) << v.label;
        } else {
            const auto* av = std::get_if<AnimatedResourceNodeVisual>(&resolved);
            ASSERT_NE(av, nullptr) << v.label << " expected animated";
            EXPECT_EQ(av->container_path, v.container) << v.label;
            EXPECT_EQ(av->logical_animation, v.logical_id) << v.label;
            EXPECT_TRUE(av->animation_data.is_looping) << v.label;
        }
    }
}

TEST(ResourceNodeAssetCatalog, UnknownKindThrows) {
    ResourceNodeAssetCatalog catalog;
    EXPECT_THROW(
        { static_cast<void>(catalog.resolve(static_cast<AdventureResourceKind>(99))); },
        std::runtime_error);
}

TEST(ResourceNodeAssetCatalog, GoldMineIsStatic) {
    ResourceNodeAssetCatalog catalog;
    StaticResourceNodeVisual vis;
    vis.container_path = "Imgs/IsoCmon.ff";
    vis.logical_sprite = "G000CR0000GL";
    vis.canvas_width = 200;
    vis.canvas_height = 160;
    vis.canvas_foot_x = 80;
    vis.canvas_foot_y = 130;
    catalog.visuals[AdventureResourceKind::GoldMine] = std::move(vis);

    const auto& resolved = catalog.resolve(AdventureResourceKind::GoldMine);
    EXPECT_TRUE(std::holds_alternative<StaticResourceNodeVisual>(resolved));
}

TEST(ResourceNodeAssetCatalog, ManaKindsAreAnimated) {
    ResourceNodeAssetCatalog    catalog;
    const AdventureResourceKind mana_kinds[] = {
        AdventureResourceKind::RedMana,    AdventureResourceKind::YellowMana,
        AdventureResourceKind::OrangeMana, AdventureResourceKind::WhiteMana,
        AdventureResourceKind::BlueMana,
    };
    for (const auto k : mana_kinds) {
        AnimatedResourceNodeVisual vis;
        vis.container_path = "Imgs/IsoAnim.ff";
        vis.logical_animation = "G000CR0000XX";
        AdventureAnimationFrame af;
        af.record_name = "G000CR0000XX";
        af.duration_ms = 100;
        af.canvas_width = 180;
        af.canvas_height = 150;
        vis.animation_data.animation_name = "G000CR0000XX";
        vis.animation_data.frames.push_back(std::move(af));
        vis.animation_data.is_looping = true;
        catalog.visuals[k] = std::move(vis);
    }

    for (const auto k : mana_kinds) {
        const auto& resolved = catalog.resolve(k);
        EXPECT_TRUE(std::holds_alternative<AnimatedResourceNodeVisual>(resolved))
            << "kind=" << static_cast<int>(k);
    }
}
