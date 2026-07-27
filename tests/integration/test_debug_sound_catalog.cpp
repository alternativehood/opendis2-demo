#include "d2engine/assets/debug_sound_catalog.hpp"

#include <gtest/gtest.h>

#include <filesystem>

#ifndef DISCIPLES2_GAME_ROOT
#define DISCIPLES2_GAME_ROOT ""
#endif

namespace {

void expect_loaded(const d2engine::DebugSoundSource source, const bool expect_sounds) {
    d2engine::DebugSoundCatalog catalog{std::filesystem::path{DISCIPLES2_GAME_ROOT}};
    const auto&                 bank = catalog.ensure_loaded(source);
    if (bank.state == d2engine::DebugSoundLoadState::Missing) {
        GTEST_SKIP() << "Game file not found: " << bank.relative_path;
    }
    ASSERT_EQ(bank.state, d2engine::DebugSoundLoadState::Loaded) << bank.error;
    if (expect_sounds) {
        EXPECT_FALSE(bank.sounds.empty());
    }
}

TEST(DebugSoundCatalogIntegration, AudioRgnLoads) {
    expect_loaded(d2engine::DebugSoundSource::AudioRgn, true);
}

TEST(DebugSoundCatalogIntegration, BattleLoads) {
    expect_loaded(d2engine::DebugSoundSource::Battle, true);
}

TEST(DebugSoundCatalogIntegration, MidgardLoads) {
    expect_loaded(d2engine::DebugSoundSource::Midgard, true);
}

TEST(DebugSoundCatalogIntegration, CapitalLoadsIncludingAnEmptyBank) {
    expect_loaded(d2engine::DebugSoundSource::Capital, false);
    d2engine::DebugSoundCatalog catalog{std::filesystem::path{DISCIPLES2_GAME_ROOT}};
    const auto&                 bank = catalog.ensure_loaded(d2engine::DebugSoundSource::Capital);
    if (bank.state == d2engine::DebugSoundLoadState::Missing) {
        GTEST_SKIP() << "Game file not found: " << bank.relative_path;
    }
    ASSERT_EQ(bank.state, d2engine::DebugSoundLoadState::Loaded) << bank.error;
    EXPECT_TRUE(bank.sounds.empty());
}

} // namespace
