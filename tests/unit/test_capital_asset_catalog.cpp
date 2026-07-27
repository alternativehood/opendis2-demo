#include <d2adventure_render/terrain/capital_asset_catalog.hpp>

#include <gtest/gtest.h>

#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

using d2engine::adventure_render::AdventureAnimationFrame;
using d2engine::adventure_render::AdventureAnimationTimingSource;
using d2engine::adventure_render::AnimatedCapitalVisual;
using d2engine::adventure_render::CapitalAssetCatalog;
using d2engine::adventure_render::CapitalVisualSet;
using d2engine::adventure_render::CapitalVisualState;

AnimatedCapitalVisual make_visual(std::string animation_name, int foot_x, int foot_y,
                                  std::initializer_list<std::pair<const char*, int>> frames) {
    AnimatedCapitalVisual visual;
    visual.container_path = "Imgs/IsoAnim.ff";
    visual.logical_animation_name = std::move(animation_name);
    visual.canvas_foot_x = foot_x;
    visual.canvas_foot_y = foot_y;
    visual.animation_data.animation_name = visual.logical_animation_name;
    visual.animation_data.is_looping = true;
    visual.animation_data.timing_source = AdventureAnimationTimingSource::ProvisionalFallback;
    visual.animation_data.native_canvas_w = 320;
    visual.animation_data.native_canvas_h = 320;
    for (const auto& [name, duration] : frames) {
        AdventureAnimationFrame frame;
        frame.record_name = name;
        frame.duration_ms = duration;
        frame.canvas_width = 320;
        frame.canvas_height = 320;
        visual.animation_data.frames.push_back(std::move(frame));
    }
    return visual;
}

CapitalAssetCatalog make_catalog() {
    CapitalAssetCatalog catalog;
    catalog.visuals.emplace(
        "g000rr0000",
        CapitalVisualSet{make_visual("G000FT0000HU0", 151, 252, {{"H0", 100}, {"H1", 100}}),
                         make_visual("G000FT0000HUC0", 151, 252, {{"H0", 100}, {"H1", 100}})});
    catalog.visuals.emplace("g000rr0001", CapitalVisualSet{make_visual("G000FT0000DWC0", 161, 249,
                                                                       {{"D0", 100}, {"D1", 100}}),
                                                           std::nullopt});
    catalog.visuals.emplace(
        "g000rr0002", CapitalVisualSet{make_visual("G000FT0000HE0", 160, 288, {{"E0", 100}}),
                                       make_visual("G000FT0000HEC0", 160, 288, {{"E0", 100}})});
    catalog.visuals.emplace(
        "g000rr0003", CapitalVisualSet{make_visual("G000FT0000UN0", 159, 246, {{"U0", 100}}),
                                       make_visual("G000FT0000UNC0", 159, 246, {{"U0", 100}})});
    catalog.visuals.emplace(
        "g000rr0005", CapitalVisualSet{make_visual("G000FT0000EL0", 160, 254, {{"L0", 100}}),
                                       make_visual("G000FT0000ELC0", 160, 254, {{"L0", 100}})});
    return catalog;
}

} // namespace

TEST(CapitalAssetCatalog, ResolvesPlayableRaceMappings) {
    const auto catalog = make_catalog();

    EXPECT_EQ(catalog.visuals.size(), 5u);

    const auto& human = catalog.resolve("G000RR0000", CapitalVisualState::Active);
    EXPECT_EQ(human.container_path, "Imgs/IsoAnim.ff");
    EXPECT_EQ(human.logical_animation_name, "G000FT0000HU0");
    EXPECT_TRUE(human.animation_data.is_looping);
    EXPECT_EQ(human.animation_data.timing_source,
              AdventureAnimationTimingSource::ProvisionalFallback);

    const auto& dwarf = catalog.resolve("g000rr0001", CapitalVisualState::Active);
    EXPECT_EQ(dwarf.logical_animation_name, "G000FT0000DWC0");
    EXPECT_EQ(dwarf.container_path, "Imgs/IsoAnim.ff");

    EXPECT_THROW(static_cast<void>(catalog.resolve("G000RR0001", CapitalVisualState::Ruined)),
                 std::runtime_error);

    const auto& heretic = catalog.resolve("G000RR0002", CapitalVisualState::Ruined);
    EXPECT_EQ(heretic.logical_animation_name, "G000FT0000HEC0");

    const auto& heretic_active = catalog.resolve("G000RR0002", CapitalVisualState::Active);
    EXPECT_EQ(heretic_active.logical_animation_name, "G000FT0000HE0");

    const auto& undead = catalog.resolve("g000rr0003", CapitalVisualState::Active);
    EXPECT_EQ(undead.logical_animation_name, "G000FT0000UN0");

    const auto& undead_ruined = catalog.resolve("g000rr0003", CapitalVisualState::Ruined);
    EXPECT_EQ(undead_ruined.logical_animation_name, "G000FT0000UNC0");

    const auto& elf = catalog.resolve("G000RR0005", CapitalVisualState::Active);
    EXPECT_EQ(elf.logical_animation_name, "G000FT0000EL0");

    const auto& elf_ruined = catalog.resolve("G000RR0005", CapitalVisualState::Ruined);
    EXPECT_EQ(elf_ruined.logical_animation_name, "G000FT0000ELC0");
}

TEST(CapitalAssetCatalog, RejectsNeutralUnknownAndEmptyRaceIds) {
    const auto catalog = make_catalog();

    EXPECT_THROW(static_cast<void>(catalog.resolve("g000rr0004", CapitalVisualState::Active)),
                 std::runtime_error);
    EXPECT_THROW(static_cast<void>(catalog.resolve("g000rr9999", CapitalVisualState::Ruined)),
                 std::runtime_error);
    EXPECT_THROW(static_cast<void>(catalog.resolve("", CapitalVisualState::Active)),
                 std::runtime_error);
    EXPECT_THROW(static_cast<void>(catalog.resolve("g000rr0000x", CapitalVisualState::Ruined)),
                 std::runtime_error);
}

TEST(CapitalAssetCatalog, DoesNotFallbackToHuman) {
    const auto catalog = make_catalog();

    try {
        static_cast<void>(catalog.resolve("g000rr0004", CapitalVisualState::Active));
        FAIL() << "expected neutral race lookup to throw";
    } catch (const std::exception& e) {
        const std::string msg = e.what();
        EXPECT_NE(msg.find("g000rr0004"), std::string::npos);
        EXPECT_EQ(msg.find("g000rr0000"), std::string::npos);
    }
}

TEST(CapitalAssetCatalog, RuinedDwarfVisualUnavailableIsStrict) {
    const auto catalog = make_catalog();

    try {
        static_cast<void>(catalog.resolve("g000rr0001", CapitalVisualState::Ruined));
        FAIL() << "expected ruined dwarf lookup to throw";
    } catch (const std::exception& e) {
        const std::string msg = e.what();
        EXPECT_NE(msg.find("capital_ruined_visual_unavailable"), std::string::npos);
        EXPECT_NE(msg.find("g000rr0001"), std::string::npos);
        EXPECT_NE(msg.find("state=Ruined"), std::string::npos);
    }
}
