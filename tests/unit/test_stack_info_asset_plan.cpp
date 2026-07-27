#include <gtest/gtest.h>

#include "d2engine/app/stack_info_asset_plan.hpp"
#include "d2engine/assets/canonical_containers.hpp"
#include "d2engine/assets/game_data_registry.hpp"
#include "d2engine/assets/portrait_manifest.hpp"

#include <filesystem>
#include <fstream>

using namespace d2engine;

#include <atomic>

namespace fs = std::filesystem;

GameDataRegistry make_empty_registry() {
    static std::atomic<unsigned> counter{0};
    auto                         n = counter++;
    auto tmp = fs::temp_directory_path() / ("d2_siap_t" + std::to_string(n));
    fs::create_directories(tmp);
    for (const auto& name :
         {"Tglobal.dbf", "Gattacks.dbf", "Gunits.dbf", "Gupgrade.dbf", "Graces.dbf"}) {
        std::ofstream(tmp / name).put(0x1a);
    }
    GameDataRegistry reg(tmp);
    std::error_code  ec;
    fs::remove_all(tmp, ec);
    return reg;
}

GameDataRegistry& empty_registry() {
    static GameDataRegistry reg = make_empty_registry();
    return reg;
}

TEST(StackInfoAssetPlan, PopupBackgroundIsTheVerifiedParchment) {
    StackInspectionModel model;
    model.id = "TEST";

    PortraitManifestIndex portraits{PortraitManifest{}};
    auto&                 reg = empty_registry();
    auto                  plan = plan_stack_info_assets(model, portraits, reg);

    const auto& bg = plan.popup_background;
    EXPECT_EQ(bg.container_path, kInterfContainer);
    EXPECT_NE(bg.container_path, "Imgs/Interf.ff");
    EXPECT_EQ(bg.image_name, "_PG0500IX");
    EXPECT_EQ(bg.kind, ImageAssetKind::ComposedSprite);
    EXPECT_EQ(bg.postprocess, ImagePostprocess::None);
}

TEST(StackInfoAssetPlan, PopupBackgroundKeyIsAlsoInInterfaceAssets) {
    StackInspectionModel model;
    model.id = "TEST";
    PortraitManifestIndex portraits{PortraitManifest{}};
    auto                  plan = plan_stack_info_assets(model, portraits, empty_registry());

    ASSERT_GE(plan.interface_assets.size(), 1u);
    EXPECT_EQ(plan.interface_assets[0].container_path, plan.popup_background.container_path);
    EXPECT_EQ(plan.interface_assets[0].image_name, plan.popup_background.image_name);
}

TEST(StackInfoAssetPlan, RejectsPhysicalPngRecord) {
    StackInspectionModel model;
    model.id = "TEST";
    PortraitManifestIndex portraits{PortraitManifest{}};
    auto                  plan = plan_stack_info_assets(model, portraits, empty_registry());

    EXPECT_NE(plan.popup_background.image_name, "_PG0500IX.PNG");
    EXPECT_NE(plan.popup_background.kind, ImageAssetKind::RawPng);
}

TEST(StackInfoAssetPlan, NeverUsesImgsInterfContainer) {
    StackInspectionModel model;
    model.id = "TEST";
    PortraitManifestIndex portraits{PortraitManifest{}};
    auto                  plan = plan_stack_info_assets(model, portraits, empty_registry());

    EXPECT_NE(plan.popup_background.container_path, "Imgs/Interf.ff");
    for (const auto& key : plan.interface_assets) {
        EXPECT_NE(key.container_path, "Imgs/Interf.ff");
    }
}

TEST(StackInfoAssetPlan, FullGlobalIdResolvesPortrait) {
    StackInspectionModel model;
    model.id = "TEST";

    UnitInspectionModel member;
    member.type_id = "G000UU0019";
    member.formation_cells = {0};
    model.members.push_back(member);

    PortraitManifest manifest;
    {
        UnitPortraitEntry entry;
        entry.resource_unit_id = "G000UU0019";
        entry.face_record_name = "UU19_FACE.PNG";
        entry.has_face = true;
        manifest.units.push_back(std::move(entry));
    }
    PortraitManifestIndex portraits{manifest};

    auto plan = plan_stack_info_assets(model, portraits, empty_registry());

    ASSERT_EQ(plan.planned_portraits.size(), 1u);
    EXPECT_EQ(plan.planned_portraits[0].key.container_path, kFacesContainer);
    EXPECT_EQ(plan.planned_portraits[0].key.image_name, "UU19_FACE.PNG");
    EXPECT_EQ(plan.planned_portraits[0].key.kind, ImageAssetKind::RawPng);
}

TEST(StackInfoAssetPlan, ShortIdAlsoResolvesPortrait) {
    StackInspectionModel model;
    model.id = "TEST";

    UnitInspectionModel member;
    member.type_id = "UU0019";
    member.formation_cells = {0};
    model.members.push_back(member);

    PortraitManifest manifest;
    {
        UnitPortraitEntry entry;
        entry.resource_unit_id = "G000UU0019";
        entry.face_record_name = "UU19_FACE.PNG";
        entry.has_face = true;
        manifest.units.push_back(std::move(entry));
    }
    PortraitManifestIndex portraits{manifest};

    auto plan = plan_stack_info_assets(model, portraits, empty_registry());

    ASSERT_EQ(plan.planned_portraits.size(), 1u);
    EXPECT_EQ(plan.planned_portraits[0].key.image_name, "UU19_FACE.PNG");
}

TEST(StackInfoAssetPlan, MissingPortraitNonFatal) {
    StackInspectionModel model;
    model.id = "TEST";

    UnitInspectionModel member;
    member.type_id = "G000UU9999";
    member.formation_cells = {0};
    model.members.push_back(member);

    PortraitManifestIndex portraits{PortraitManifest{}};

    auto plan = plan_stack_info_assets(model, portraits, empty_registry());

    // Background key should be present
    EXPECT_FALSE(plan.popup_background.container_path.empty());
    ASSERT_GE(plan.interface_assets.size(), 1u);
    // Planned portrait should exist with no key (unresolved)
    ASSERT_EQ(plan.planned_portraits.size(), 1u);
    EXPECT_TRUE(plan.planned_portraits[0].key.container_path.empty());
}

TEST(StackInfoAssetPlan, LargeUnitSpansTwoCells) {
    StackInspectionModel model;
    model.id = "TEST";

    UnitInspectionModel member;
    member.type_id = "G000UU0050";
    member.definition_resolved = true;
    member.definition = UnitDefInspectionModel{};
    member.definition->size_small = false;
    // large unit occupies two formation cells
    member.formation_cells = {4, 5};
    model.members.push_back(member);

    PortraitManifest manifest;
    {
        UnitPortraitEntry entry;
        entry.resource_unit_id = "G000UU0050";
        entry.face_record_name = "UU50_FACE.PNG";
        entry.has_face = true;
        manifest.units.push_back(std::move(entry));
    }
    PortraitManifestIndex portraits{manifest};

    auto plan = plan_stack_info_assets(model, portraits, empty_registry());

    ASSERT_EQ(plan.planned_portraits.size(), 1u);
    EXPECT_TRUE(plan.planned_portraits[0].is_large);
    // Anchor at the first formation cell
    EXPECT_EQ(plan.planned_portraits[0].formation_cell, 4);
    EXPECT_EQ(plan.planned_portraits[0].key.image_name, "UU50_FACE.PNG");
    EXPECT_EQ(plan.planned_portraits[0].layout_path, "/stack_info/formation/large_row_2");
}

// ── Layout path routing ──────────────────────────────────────────

TEST(StackInfoAssetPlan, SmallUnitCell0RoutesToSlot0) {
    StackInspectionModel model;
    model.id = "TEST";
    UnitInspectionModel member;
    member.type_id = "G000UU0001";
    member.formation_cells = {0};
    model.members.push_back(member);
    PortraitManifestIndex portraits{PortraitManifest{}};
    auto                  plan = plan_stack_info_assets(model, portraits, empty_registry());
    ASSERT_EQ(plan.planned_portraits.size(), 1u);
    EXPECT_EQ(plan.planned_portraits[0].layout_path, "/stack_info/formation/slot_0");
}

TEST(StackInfoAssetPlan, LeaderPortraitUsesComposedSpriteNotRawPng) {
    StackInspectionModel model;
    model.id = "TEST";
    model.leader_id = "S143UN0001";
    UnitInspectionModel leader;
    leader.instance_id = "S143UN0001";
    leader.type_id = "G000UU0021";
    leader.is_leader = true;
    leader.formation_cells = {3};
    model.members.push_back(leader);

    PortraitManifestIndex portraits{PortraitManifest{}};
    auto                  plan = plan_stack_info_assets(model, portraits, empty_registry());

    EXPECT_TRUE(plan.leader_portrait.has_value());
    if (plan.leader_portrait.has_value()) {
        EXPECT_EQ(plan.leader_portrait->kind, ImageAssetKind::ComposedSprite)
            << "leader portrait must use logical ComposedSprite";
        EXPECT_EQ(plan.leader_portrait->container_path, kEventsContainer);
        EXPECT_EQ(plan.leader_portrait->image_name.find(".PNG"), std::string::npos)
            << "leader portrait image_name must not be a raw .PNG record name";
    }
}

TEST(StackInfoAssetPlan, FormationOrderedByCellIndexNotMemberOrder) {
    StackInspectionModel model;
    model.id = "TEST";

    // Members added in shuffled order: cell 4 first, cell 0 second, cell 2 third
    UnitInspectionModel m4;
    m4.member_index = 2;
    m4.formation_cells = {4};
    model.members.push_back(m4);

    UnitInspectionModel m0;
    m0.member_index = 0;
    m0.formation_cells = {0};
    model.members.push_back(m0);

    UnitInspectionModel m2;
    m2.member_index = 1;
    m2.formation_cells = {2};
    model.members.push_back(m2);

    PortraitManifestIndex portraits{PortraitManifest{}};
    auto                  plan = plan_stack_info_assets(model, portraits, empty_registry());

    ASSERT_EQ(plan.planned_portraits.size(), 3u);
    // Must be ordered by formation cell: 0, 2, 4
    EXPECT_EQ(plan.planned_portraits[0].formation_cell, 0);
    EXPECT_EQ(plan.planned_portraits[1].formation_cell, 2);
    EXPECT_EQ(plan.planned_portraits[2].formation_cell, 4);
}

TEST(StackInfoAssetPlan, SmallUnitCell4RoutesToSlot4) {
    StackInspectionModel model;
    model.id = "TEST";
    UnitInspectionModel member;
    member.type_id = "G000UU0001";
    member.formation_cells = {4};
    model.members.push_back(member);
    PortraitManifestIndex portraits{PortraitManifest{}};
    auto                  plan = plan_stack_info_assets(model, portraits, empty_registry());
    ASSERT_EQ(plan.planned_portraits.size(), 1u);
    EXPECT_EQ(plan.planned_portraits[0].layout_path, "/stack_info/formation/slot_4");
}

TEST(StackInfoAssetPlan, SmallUnitCell5RoutesToSlot5) {
    StackInspectionModel model;
    model.id = "TEST";
    UnitInspectionModel member;
    member.type_id = "G000UU0001";
    member.formation_cells = {5};
    model.members.push_back(member);
    PortraitManifestIndex portraits{PortraitManifest{}};
    auto                  plan = plan_stack_info_assets(model, portraits, empty_registry());
    ASSERT_EQ(plan.planned_portraits.size(), 1u);
    EXPECT_EQ(plan.planned_portraits[0].layout_path, "/stack_info/formation/slot_5");
}

TEST(StackInfoAssetPlan, LargeUnitTopRowRoutesToLargeRow0) {
    StackInspectionModel model;
    model.id = "TEST";
    UnitInspectionModel member;
    member.type_id = "G000UU0050";
    member.definition_resolved = true;
    member.definition = UnitDefInspectionModel{};
    member.definition->size_small = false;
    member.formation_cells = {0, 1};
    model.members.push_back(member);
    PortraitManifestIndex portraits{PortraitManifest{}};
    auto                  plan = plan_stack_info_assets(model, portraits, empty_registry());
    ASSERT_EQ(plan.planned_portraits.size(), 1u);
    EXPECT_TRUE(plan.planned_portraits[0].is_large);
    EXPECT_EQ(plan.planned_portraits[0].layout_path, "/stack_info/formation/large_row_0");
}

TEST(StackInfoAssetPlan, LargeUnitMiddleRowRoutesToLargeRow1) {
    StackInspectionModel model;
    model.id = "TEST";
    UnitInspectionModel member;
    member.type_id = "G000UU0050";
    member.definition_resolved = true;
    member.definition = UnitDefInspectionModel{};
    member.definition->size_small = false;
    member.formation_cells = {2, 3};
    model.members.push_back(member);
    PortraitManifestIndex portraits{PortraitManifest{}};
    auto                  plan = plan_stack_info_assets(model, portraits, empty_registry());
    ASSERT_EQ(plan.planned_portraits.size(), 1u);
    EXPECT_TRUE(plan.planned_portraits[0].is_large);
    EXPECT_EQ(plan.planned_portraits[0].layout_path, "/stack_info/formation/large_row_1");
}

TEST(StackInfoAssetPlan, LargeUnitBottomRowRoutesToLargeRow2) {
    StackInspectionModel model;
    model.id = "TEST";
    UnitInspectionModel member;
    member.type_id = "G000UU0050";
    member.definition_resolved = true;
    member.definition = UnitDefInspectionModel{};
    member.definition->size_small = false;
    member.formation_cells = {4, 5};
    model.members.push_back(member);
    PortraitManifestIndex portraits{PortraitManifest{}};
    auto                  plan = plan_stack_info_assets(model, portraits, empty_registry());
    ASSERT_EQ(plan.planned_portraits.size(), 1u);
    EXPECT_TRUE(plan.planned_portraits[0].is_large);
    EXPECT_EQ(plan.planned_portraits[0].layout_path, "/stack_info/formation/large_row_2");
}

TEST(StackInfoAssetPlan, Anchor0MapsToLargeRow0) {
    StackInspectionModel model;
    model.id = "TEST";
    UnitInspectionModel member;
    member.definition_resolved = true;
    member.definition = UnitDefInspectionModel{};
    member.definition->size_small = false;
    member.formation_cells = {0};
    model.members.push_back(member);
    PortraitManifestIndex portraits{PortraitManifest{}};
    auto                  plan = plan_stack_info_assets(model, portraits, empty_registry());
    ASSERT_EQ(plan.planned_portraits.size(), 1u);
    EXPECT_EQ(plan.planned_portraits[0].layout_path, "/stack_info/formation/large_row_0");
}

TEST(StackInfoAssetPlan, Anchor1MapsToLargeRow0) {
    StackInspectionModel model;
    model.id = "TEST";
    UnitInspectionModel member;
    member.definition_resolved = true;
    member.definition = UnitDefInspectionModel{};
    member.definition->size_small = false;
    member.formation_cells = {1};
    model.members.push_back(member);
    PortraitManifestIndex portraits{PortraitManifest{}};
    auto                  plan = plan_stack_info_assets(model, portraits, empty_registry());
    ASSERT_EQ(plan.planned_portraits.size(), 1u);
    EXPECT_EQ(plan.planned_portraits[0].layout_path, "/stack_info/formation/large_row_0");
}

TEST(StackInfoAssetPlan, Anchor2MapsToLargeRow1) {
    StackInspectionModel model;
    model.id = "TEST";
    UnitInspectionModel member;
    member.definition_resolved = true;
    member.definition = UnitDefInspectionModel{};
    member.definition->size_small = false;
    member.formation_cells = {2};
    model.members.push_back(member);
    PortraitManifestIndex portraits{PortraitManifest{}};
    auto                  plan = plan_stack_info_assets(model, portraits, empty_registry());
    ASSERT_EQ(plan.planned_portraits.size(), 1u);
    EXPECT_EQ(plan.planned_portraits[0].layout_path, "/stack_info/formation/large_row_1");
}

TEST(StackInfoAssetPlan, Anchor3MapsToLargeRow1) {
    StackInspectionModel model;
    model.id = "TEST";
    UnitInspectionModel member;
    member.definition_resolved = true;
    member.definition = UnitDefInspectionModel{};
    member.definition->size_small = false;
    member.formation_cells = {3};
    model.members.push_back(member);
    PortraitManifestIndex portraits{PortraitManifest{}};
    auto                  plan = plan_stack_info_assets(model, portraits, empty_registry());
    ASSERT_EQ(plan.planned_portraits.size(), 1u);
    EXPECT_EQ(plan.planned_portraits[0].layout_path, "/stack_info/formation/large_row_1");
}

TEST(StackInfoAssetPlan, Anchor4MapsToLargeRow2) {
    StackInspectionModel model;
    model.id = "TEST";
    UnitInspectionModel member;
    member.definition_resolved = true;
    member.definition = UnitDefInspectionModel{};
    member.definition->size_small = false;
    member.formation_cells = {4};
    model.members.push_back(member);
    PortraitManifestIndex portraits{PortraitManifest{}};
    auto                  plan = plan_stack_info_assets(model, portraits, empty_registry());
    ASSERT_EQ(plan.planned_portraits.size(), 1u);
    EXPECT_EQ(plan.planned_portraits[0].layout_path, "/stack_info/formation/large_row_2");
}

TEST(StackInfoAssetPlan, Anchor5MapsToLargeRow2) {
    StackInspectionModel model;
    model.id = "TEST";
    UnitInspectionModel member;
    member.definition_resolved = true;
    member.definition = UnitDefInspectionModel{};
    member.definition->size_small = false;
    member.formation_cells = {5};
    model.members.push_back(member);
    PortraitManifestIndex portraits{PortraitManifest{}};
    auto                  plan = plan_stack_info_assets(model, portraits, empty_registry());
    ASSERT_EQ(plan.planned_portraits.size(), 1u);
    EXPECT_EQ(plan.planned_portraits[0].layout_path, "/stack_info/formation/large_row_2");
}

TEST(StackInfoAssetPlan, FrameAssetsAreVerifiedIconsResources) {
    StackInspectionModel model;
    model.id = "TEST";
    PortraitManifestIndex portraits{PortraitManifest{}};
    auto                  plan = plan_stack_info_assets(model, portraits, empty_registry());

    EXPECT_EQ(plan.small_frame.container_path, kIconsContainer);
    EXPECT_EQ(plan.small_frame.image_name, "BORDERUNITSMALL.PNG");
    EXPECT_EQ(plan.small_frame.kind, ImageAssetKind::RawPng);

    EXPECT_EQ(plan.large_frame.container_path, kIconsContainer);
    EXPECT_EQ(plan.large_frame.image_name, "BORDERUNITLARGE.PNG");
    EXPECT_EQ(plan.large_frame.kind, ImageAssetKind::RawPng);

    auto has_small = std::ranges::any_of(plan.interface_assets, [&](const auto& k) {
        return k.image_name == "BORDERUNITSMALL.PNG";
    });
    auto has_large = std::ranges::any_of(plan.interface_assets, [&](const auto& k) {
        return k.image_name == "BORDERUNITLARGE.PNG";
    });
    EXPECT_TRUE(has_small);
    EXPECT_TRUE(has_large);
}
