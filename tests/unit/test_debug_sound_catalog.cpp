#include "d2engine/assets/debug_sound_catalog.hpp"
#include "tests/test_helpers.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace {

TEST(DebugSoundCatalog, HasExactLazyBankDescriptors) {
    TempDir                     root("debug_sound_catalog_descriptors");
    d2engine::DebugSoundCatalog catalog(root.path());
    const auto                  banks = catalog.banks();
    ASSERT_EQ(banks.size(), d2engine::kDebugSoundSourceCount);
    EXPECT_EQ(banks[0].display_name, "AudioRgn");
    EXPECT_EQ(banks[0].relative_path, "Sounds/AudioRgn.wdb");
    EXPECT_EQ(banks[1].display_name, "Battle");
    EXPECT_EQ(banks[1].relative_path, "Sounds/Battle.wdb");
    EXPECT_EQ(banks[2].display_name, "Midgard");
    EXPECT_EQ(banks[2].relative_path, "Sounds/Midgard.wdb");
    EXPECT_EQ(banks[3].display_name, "Capital");
    EXPECT_EQ(banks[3].relative_path, "Sounds/Capital.wdb");
    for (const auto& bank : banks) {
        EXPECT_EQ(bank.state, d2engine::DebugSoundLoadState::NotLoaded);
    }
}

TEST(DebugSoundCatalog, MissingBankIsStableAndDoesNotLoadOthers) {
    TempDir                     root("debug_sound_catalog_missing");
    d2engine::DebugSoundCatalog catalog(root.path());
    const auto&                 first = catalog.ensure_loaded(d2engine::DebugSoundSource::AudioRgn);
    EXPECT_EQ(first.state, d2engine::DebugSoundLoadState::Missing);
    EXPECT_TRUE(first.sounds.empty());
    EXPECT_NE(first.error.find("Sounds/AudioRgn.wdb"), std::string::npos);
    const auto& repeated = catalog.ensure_loaded(d2engine::DebugSoundSource::AudioRgn);
    EXPECT_EQ(&first, &repeated);
    EXPECT_EQ(catalog.bank(d2engine::DebugSoundSource::Battle).state,
              d2engine::DebugSoundLoadState::NotLoaded);
}

TEST(DebugSoundCatalog, ExistingDirectoryBecomesStableFailure) {
    TempDir root("debug_sound_catalog_directory");
    std::filesystem::create_directories(root.path() / "Sounds/AudioRgn.wdb");
    d2engine::DebugSoundCatalog catalog(root.path());
    const auto&                 first = catalog.ensure_loaded(d2engine::DebugSoundSource::AudioRgn);
    EXPECT_EQ(first.state, d2engine::DebugSoundLoadState::Failed);
    EXPECT_NE(first.error.find("not a regular file"), std::string::npos);
    const auto& repeated = catalog.ensure_loaded(d2engine::DebugSoundSource::AudioRgn);
    EXPECT_EQ(&first, &repeated);
    EXPECT_EQ(catalog.bank(d2engine::DebugSoundSource::Battle).state,
              d2engine::DebugSoundLoadState::NotLoaded);
}

TEST(DebugSoundCatalog, MalformedRegularFileBecomesFailure) {
    TempDir root("debug_sound_catalog_malformed");
    std::filesystem::create_directories(root.path() / "Sounds");
    std::ofstream(root.path() / "Sounds/AudioRgn.wdb") << "not an mqdb";
    d2engine::DebugSoundCatalog catalog(root.path());
    const auto&                 bank = catalog.ensure_loaded(d2engine::DebugSoundSource::AudioRgn);
    EXPECT_EQ(bank.state, d2engine::DebugSoundLoadState::Failed);
    EXPECT_NE(bank.error.find("Sounds/AudioRgn.wdb"), std::string::npos);
    EXPECT_TRUE(bank.sounds.empty());
}

TEST(DebugSoundCatalog, RejectsInvalidSources) {
    TempDir                     root("debug_sound_catalog_invalid");
    d2engine::DebugSoundCatalog catalog(root.path());
    EXPECT_THROW(static_cast<void>(catalog.bank(d2engine::DebugSoundSource::Count)),
                 std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(catalog.ensure_loaded(static_cast<d2engine::DebugSoundSource>(255))),
        std::invalid_argument);
}

} // namespace
