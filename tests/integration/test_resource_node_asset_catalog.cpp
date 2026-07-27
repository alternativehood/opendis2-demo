#include <gtest/gtest.h>

#include <d2adventure_render/terrain/resource_node_asset_catalog.hpp>
#include <d2engine/assets/ff_asset_store.hpp>
#include <d2engine/assets/resource_node_asset_catalog_builder.hpp>
#include <d2engine/assets/image_asset_key.hpp>
#include <d2runtime/AdventureResourceNode.hpp>

#include <cstdlib>
#include <filesystem>
#include <set>
#include <string>

namespace fs = std::filesystem;

static fs::path game_root() {
    const char* env = std::getenv("DISCIPLES2_GAME_ROOT");
    if (env != nullptr && env[0] != '\0')
        return fs::path(env);
#ifdef DISCIPLES2_GAME_ROOT
    const auto from_define = fs::path(DISCIPLES2_GAME_ROOT);
    if (!from_define.empty() && fs::is_directory(from_define))
        return from_define;
#endif
    return {};
}

// ── All six kinds resolved via real FF store ────────────────────────────

TEST(ResourceNodeAssetCatalogReal, AllSixKindsResolved) {
    const auto root = game_root();
    if (root.empty())
        GTEST_SKIP() << "DISCIPLES2_GAME_ROOT not set";

    d2engine::FfAssetStore store(root);
    const auto             catalog = d2engine::build_resource_node_asset_catalog(store);

    EXPECT_EQ(catalog.visuals.size(), 6u);

    // resolve does not throw for any kind
    for (int i = 0; i < 6; ++i) {
        const auto kind = static_cast<d2runtime::AdventureResourceKind>(i);
        EXPECT_NO_THROW({ static_cast<void>(catalog.resolve(kind)); }) << "kind=" << i;
    }
}

// ── GoldMine is static in IsoCmon ──────────────────────────────────────

TEST(ResourceNodeAssetCatalogReal, GoldMineIsStaticIsoCmonGL) {
    const auto root = game_root();
    if (root.empty())
        GTEST_SKIP() << "DISCIPLES2_GAME_ROOT not set";

    d2engine::FfAssetStore store(root);
    const auto             catalog = d2engine::build_resource_node_asset_catalog(store);

    const auto& visual = catalog.resolve(d2runtime::AdventureResourceKind::GoldMine);
    const auto* sv = std::get_if<d2engine::adventure_render::StaticResourceNodeVisual>(&visual);
    ASSERT_NE(sv, nullptr) << "GoldMine must be static";
    EXPECT_EQ(sv->container_path, "Imgs/IsoCmon.ff");
    EXPECT_EQ(sv->logical_sprite, "G000CR0000GL");
}

// ── Mana kinds are animated in IsoAnim ──────────────────────────────────

struct ManaAssertion {
    d2runtime::AdventureResourceKind kind;
    const char*                      animation_name;
    const char*                      label;
    int                              expected_frames;
};

static const ManaAssertion kManaAssertions[] = {
    {d2runtime::AdventureResourceKind::RedMana, "G000CR0000RD", "RedMana", 35},
    {d2runtime::AdventureResourceKind::YellowMana, "G000CR0000YE", "YellowMana", 35},
    {d2runtime::AdventureResourceKind::OrangeMana, "G000CR0000RG", "OrangeMana", 35},
    {d2runtime::AdventureResourceKind::WhiteMana, "G000CR0000WH", "WhiteMana", 35},
    {d2runtime::AdventureResourceKind::BlueMana, "G000CR0000GR", "BlueMana", 25},
};

TEST(ResourceNodeAssetCatalogReal, RedManaIsAnimatedInIsoAnim) {
    const auto root = game_root();
    if (root.empty())
        GTEST_SKIP() << "DISCIPLES2_GAME_ROOT not set";

    d2engine::FfAssetStore store(root);
    const auto             catalog = d2engine::build_resource_node_asset_catalog(store);

    const auto& visual = catalog.resolve(d2runtime::AdventureResourceKind::RedMana);
    const auto* av = std::get_if<d2engine::adventure_render::AnimatedResourceNodeVisual>(&visual);
    ASSERT_NE(av, nullptr) << "RedMana must be animated";
    EXPECT_EQ(av->container_path, "Imgs/IsoAnim.ff");
    EXPECT_EQ(av->logical_animation, "G000CR0000RD");
    EXPECT_TRUE(av->animation_data.is_looping);
    EXPECT_EQ(av->animation_data.frames.size(), 35u);
    EXPECT_GT(av->animation_data.native_canvas_w, 0);
    EXPECT_GT(av->animation_data.native_canvas_h, 0);
}

TEST(ResourceNodeAssetCatalogReal, YellowManaIsAnimatedIsoAnimYE) {
    const auto root = game_root();
    if (root.empty())
        GTEST_SKIP() << "DISCIPLES2_GAME_ROOT not set";

    d2engine::FfAssetStore store(root);
    const auto             catalog = d2engine::build_resource_node_asset_catalog(store);

    const auto& visual = catalog.resolve(d2runtime::AdventureResourceKind::YellowMana);
    const auto* av = std::get_if<d2engine::adventure_render::AnimatedResourceNodeVisual>(&visual);
    ASSERT_NE(av, nullptr) << "YellowMana must be animated";
    EXPECT_EQ(av->container_path, "Imgs/IsoAnim.ff");
    EXPECT_EQ(av->logical_animation, "G000CR0000YE");
    EXPECT_TRUE(av->animation_data.is_looping);
    EXPECT_GT(av->animation_data.frames.size(), 0u);
}

TEST(ResourceNodeAssetCatalogReal, BlueManaIsAnimatedIsoAnimGR) {
    const auto root = game_root();
    if (root.empty())
        GTEST_SKIP() << "DISCIPLES2_GAME_ROOT not set";

    d2engine::FfAssetStore store(root);
    const auto             catalog = d2engine::build_resource_node_asset_catalog(store);

    const auto& visual = catalog.resolve(d2runtime::AdventureResourceKind::BlueMana);
    const auto* av = std::get_if<d2engine::adventure_render::AnimatedResourceNodeVisual>(&visual);
    ASSERT_NE(av, nullptr) << "BlueMana must be animated";
    EXPECT_EQ(av->container_path, "Imgs/IsoAnim.ff");
    EXPECT_EQ(av->logical_animation, "G000CR0000GR");
    EXPECT_EQ(av->animation_data.frames.size(), 25u);
}

TEST(ResourceNodeAssetCatalogReal, AllManaKindsAreAnimated) {
    const auto root = game_root();
    if (root.empty())
        GTEST_SKIP() << "DISCIPLES2_GAME_ROOT not set";

    d2engine::FfAssetStore store(root);
    const auto             catalog = d2engine::build_resource_node_asset_catalog(store);

    for (const auto& ma : kManaAssertions) {
        const auto& visual = catalog.resolve(ma.kind);
        const auto* av =
            std::get_if<d2engine::adventure_render::AnimatedResourceNodeVisual>(&visual);
        ASSERT_NE(av, nullptr) << ma.label << " must be animated";
        EXPECT_EQ(av->container_path, "Imgs/IsoAnim.ff") << ma.label;
        EXPECT_EQ(av->logical_animation, ma.animation_name) << ma.label;
        EXPECT_TRUE(av->animation_data.is_looping) << ma.label;
        EXPECT_EQ(av->animation_data.frames.size(), static_cast<std::size_t>(ma.expected_frames))
            << ma.label << " frame count";
    }
}

// ── Frame dimensions are positive for all animations ────────────────────

TEST(ResourceNodeAssetCatalogReal, AllAnimationFramesHavePositiveDimensions) {
    const auto root = game_root();
    if (root.empty())
        GTEST_SKIP() << "DISCIPLES2_GAME_ROOT not set";

    d2engine::FfAssetStore store(root);
    const auto             catalog = d2engine::build_resource_node_asset_catalog(store);

    for (const auto& [kind, visual] : catalog.visuals) {
        const auto* av =
            std::get_if<d2engine::adventure_render::AnimatedResourceNodeVisual>(&visual);
        if (av == nullptr)
            continue;
        for (std::size_t i = 0; i < av->animation_data.frames.size(); ++i) {
            const auto& f = av->animation_data.frames[i];
            EXPECT_GT(f.canvas_width, 0) << "kind=" << static_cast<int>(kind) << " frame=" << i;
            EXPECT_GT(f.canvas_height, 0) << "kind=" << static_cast<int>(kind) << " frame=" << i;
        }
    }
}

// ── Foot within bounds for animated entries ────────────────────────────

TEST(ResourceNodeAssetCatalogReal, AnimatedFootWithinBounds) {
    const auto root = game_root();
    if (root.empty())
        GTEST_SKIP() << "DISCIPLES2_GAME_ROOT not set";

    d2engine::FfAssetStore store(root);
    const auto             catalog = d2engine::build_resource_node_asset_catalog(store);

    for (const auto& [kind, visual] : catalog.visuals) {
        const auto* av =
            std::get_if<d2engine::adventure_render::AnimatedResourceNodeVisual>(&visual);
        if (av == nullptr)
            continue;
        EXPECT_GE(av->canvas_foot_x, 0) << "kind=" << static_cast<int>(kind);
        EXPECT_GE(av->canvas_foot_y, 0) << "kind=" << static_cast<int>(kind);
        EXPECT_LE(av->canvas_foot_x, av->animation_data.native_canvas_w)
            << "kind=" << static_cast<int>(kind);
        EXPECT_LE(av->canvas_foot_y, av->animation_data.native_canvas_h)
            << "kind=" << static_cast<int>(kind);
    }
}

// ── No fallback to GoldMine sprite for mana kinds ──────────────────────

TEST(ResourceNodeAssetCatalogReal, NoFallbackToGoldSpriteForManaKinds) {
    const auto root = game_root();
    if (root.empty())
        GTEST_SKIP() << "DISCIPLES2_GAME_ROOT not set";

    d2engine::FfAssetStore store(root);
    const auto             catalog = d2engine::build_resource_node_asset_catalog(store);

    const auto& gold_visual = catalog.resolve(d2runtime::AdventureResourceKind::GoldMine);
    const auto* gold_sv =
        std::get_if<d2engine::adventure_render::StaticResourceNodeVisual>(&gold_visual);
    ASSERT_NE(gold_sv, nullptr);

    for (const auto& ma : kManaAssertions) {
        const auto& mana_visual = catalog.resolve(ma.kind);
        const auto* mana_av =
            std::get_if<d2engine::adventure_render::AnimatedResourceNodeVisual>(&mana_visual);
        ASSERT_NE(mana_av, nullptr) << ma.label;

        // Must NOT be IsoCmon
        EXPECT_NE(mana_av->container_path, gold_sv->container_path)
            << ma.label << " must not use IsoCmon.ff";
        // Must NOT use the gold mine sprite name
        EXPECT_NE(mana_av->logical_animation, gold_sv->logical_sprite)
            << ma.label << " must not use gold mine sprite";
    }
}

// ── No mana identity queried through IsoCmon.ff ────────────────────────

TEST(ResourceNodeAssetCatalogReal, NoManaIdentityInIsoCmon) {
    const auto root = game_root();
    if (root.empty())
        GTEST_SKIP() << "DISCIPLES2_GAME_ROOT not set";

    d2engine::FfAssetStore store(root);
    const auto             catalog = d2engine::build_resource_node_asset_catalog(store);

    const std::string mana_identities[] = {
        "G000CR0000RD", "G000CR0000YE", "G000CR0000RG", "G000CR0000WH", "G000CR0000GR",
    };

    for (const auto& kind :
         {d2runtime::AdventureResourceKind::RedMana, d2runtime::AdventureResourceKind::YellowMana,
          d2runtime::AdventureResourceKind::OrangeMana, d2runtime::AdventureResourceKind::WhiteMana,
          d2runtime::AdventureResourceKind::BlueMana}) {
        const auto& visual = catalog.resolve(kind);
        const auto* av =
            std::get_if<d2engine::adventure_render::AnimatedResourceNodeVisual>(&visual);
        ASSERT_NE(av, nullptr) << "kind=" << static_cast<int>(kind);
        EXPECT_NE(av->container_path, "Imgs/IsoCmon.ff")
            << "kind=" << static_cast<int>(kind) << " must not use IsoCmon.ff";
    }
}
