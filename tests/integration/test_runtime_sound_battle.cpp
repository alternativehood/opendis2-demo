#include "cli/commands_extract_sounds.hpp"
#include "cli/manifest_builder.hpp"
#include "cli/runtime_package.hpp"
#include "d2asset/asset_database.hpp"
#include "d2res/mqdb.hpp"
#include "d2res/rgba_buffer.hpp"
#include "d2res/wdb_decoder.hpp"
#include "tests/test_helpers.hpp"

#include <filesystem>
#include <fstream>
#include <string>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#ifndef DISCIPLES2_GAME_ROOT
#define DISCIPLES2_GAME_ROOT ""
#endif

namespace fs = std::filesystem;

TEST(RuntimeSounds, BattleSoundPackagesAndLoadsWithoutSourceAccessAfterPublication) {
    const fs::path source = fs::path(DISCIPLES2_GAME_ROOT) / "Sounds/Battle.wdb";
    if (!fs::is_regular_file(source))
        GTEST_SKIP() << "Game file not found: " << source;

    TempDir         root("rt_snd_battle");
    const fs::path  work = root.path() / "work";
    const fs::path  package = root.path() / "package";
    std::error_code ec;
    fs::create_directories(work);
    for (const std::string_view directory :
         {"images", "animations", "atlases", "sounds", "data", "reports"}) {
        fs::create_directories(package / directory);
    }

    const d2res::MqdbContainer     container = d2res::MqdbContainer::open(source);
    const d2res::WdbDecoder        decoder(container);
    const std::vector<std::string> names = decoder.list_sounds();
    ASSERT_FALSE(names.empty());
    const std::string& logical_name = names.front();
    ASSERT_EQ(cmd_extract_sounds(source.string(), work.string(), logical_name, false), 0);

    const fs::path extractor_sidecar = work / (d2res::sanitize_filename(logical_name) + ".json");
    ASSERT_TRUE(fs::is_regular_file(extractor_sidecar));
    const std::string asset_id = ManifestBuilder::make_id("Sounds/Battle.wdb") + "/" +
                                 ManifestBuilder::make_id(logical_name);
    const fs::path    sidecar_path = runtime_package_detail::package_extracted_sound(
        extractor_sidecar, logical_name, "Sounds/Battle.wdb", asset_id, package);

    const nlohmann::json manifest{
        {"asset_schema_version", 1},
        {"containers",
         nlohmann::json::array({{{"container_id", "sounds/battle.wdb"},
                                 {"path", "Sounds/Battle.wdb"},
                                 {"content_kinds", nlohmann::json::array({"sounds"})}}})},
        {"assets", nlohmann::json::array({{{"asset_id", asset_id},
                                           {"logical_name", logical_name},
                                           {"type", "sound"},
                                           {"container_id", "sounds/battle.wdb"},
                                           {"path", sidecar_path.generic_string()}}})},
        {"warnings", nlohmann::json::array()},
    };
    std::ofstream(package / "game_manifest.json") << manifest.dump(2) << '\n';

    fs::remove_all(work);
    const d2asset::AssetDatabase database = d2asset::AssetDatabase::open(package);
    const auto                   result = database.get_sound_asset(asset_id);
    ASSERT_EQ(result.status, d2asset::AssetLookupStatus::Found);
    ASSERT_TRUE(result.value.has_value());
    const auto& sound = result.value.value();
    ASSERT_TRUE(sound.channels.has_value());
    ASSERT_TRUE(sound.sample_rate.has_value());
    EXPECT_GT(sound.channels.value(), 0U);
    EXPECT_GT(sound.sample_rate.value(), 0U);
    EXPECT_FALSE(sound.payload_path.is_absolute());
    EXPECT_TRUE(fs::is_regular_file(package / sound.payload_path));
}

TEST(RuntimeSounds, CapitalEmptyBankIsAValidExtraction) {
    const fs::path container = fs::path(DISCIPLES2_GAME_ROOT) / "Sounds/Capital.wdb";
    if (!fs::is_regular_file(container))
        GTEST_SKIP() << "Game file not found: " << container;

    TempDir out("rt_snd_capital");
    ASSERT_EQ(cmd_extract_sounds(container.string(), out.str(), "", true), 0);

    std::ifstream input(out.path() / "manifest.json");
    ASSERT_TRUE(input.is_open());
    nlohmann::json manifest;
    input >> manifest;
    EXPECT_EQ(manifest["total_sounds"].get<std::size_t>(), 0U);
    EXPECT_EQ(manifest["written_sounds"].get<std::size_t>(), 0U);
    EXPECT_TRUE(fs::is_regular_file(out.path() / "warnings.txt"));
}
