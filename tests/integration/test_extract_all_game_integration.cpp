#include <gtest/gtest.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_set>

#include "cli/commands_extract_all.hpp"
#include "cli/commands_extract_atlas.hpp"
#include "cli/runtime_package.hpp"
#include "d2asset/asset_database.hpp"
#include "tests/test_helpers.hpp"
#include <nlohmann/json.hpp>

static const char* GAME_ROOT = DISCIPLES2_GAME_ROOT;

namespace fs = std::filesystem;

// ── Full-game extraction ──────────────────────────────────────────────────────

TEST(ExtractAllGame, FullExtractionProducesValidManifests) {
    if ((GAME_ROOT == nullptr) || std::string(GAME_ROOT).empty())
        GTEST_SKIP() << "DISCIPLES2_GAME_ROOT not set";
    if (!fs::is_directory(GAME_ROOT))
        GTEST_SKIP() << "GAME_ROOT not a directory: " << GAME_ROOT;

    TempDir out("extract_all_game_full");
    ASSERT_EQ(cmd_extract_all(GAME_ROOT, out.str()), 0);

    const fs::path extraction_path = out.path() / "extraction_manifest.json";
    ASSERT_TRUE(fs::exists(extraction_path));
    std::ifstream  extraction_file(extraction_path);
    nlohmann::json extraction_manifest;
    extraction_file >> extraction_manifest;
    EXPECT_GT(extraction_manifest["extracted"].get<int>(), 0);

    const fs::path game_path = out.path() / "game_manifest.json";
    ASSERT_TRUE(fs::exists(game_path));
    std::ifstream  game_file(game_path);
    nlohmann::json game_manifest;
    game_file >> game_manifest;
    ASSERT_EQ(game_manifest["asset_schema_version"].get<int>(), 1);
    ASSERT_FALSE(game_manifest["containers"].empty());
    ASSERT_FALSE(game_manifest["assets"].empty());

    std::unordered_set<std::string> asset_ids;
    for (const auto& asset : game_manifest["assets"]) {
        const std::string asset_id = asset["asset_id"].get<std::string>();
        EXPECT_TRUE(asset_ids.insert(asset_id).second) << "duplicate asset_id: " << asset_id;
        const fs::path asset_path = out.path() / asset["path"].get<std::string>();
        EXPECT_TRUE(fs::is_regular_file(asset_path)) << "missing: " << asset_path;
    }
}

// ── Single-container package build ───────────────────────────────────────────

TEST(AssetDatabaseRealPackage, FullPackageBuildAndLookup) {
    const fs::path source_container = fs::path(GAME_ROOT) / "Imgs/Batitems.ff";
    if (!fs::is_regular_file(source_container))
        GTEST_SKIP() << "Batitems.ff not found under game root: " << source_container;

    TempDir         base("asset_real_package");
    const fs::path  game = base.path() / "game";
    const fs::path  out_ = base.path() / "out";
    std::error_code ec;
    fs::create_directories(game / "Imgs", ec);
    ASSERT_FALSE(ec);
    fs::copy_file(source_container, game / "Imgs/Batitems.ff", fs::copy_options::overwrite_existing,
                  ec);
    ASSERT_FALSE(ec);

    RuntimePackageBuildOptions options;
    options.game_root = game;
    options.output_root = out_;
    const RuntimePackageBuildResult build = build_runtime_package(options);
    ASSERT_TRUE(build.published);

    for (const std::string_view dir :
         {"images", "animations", "atlases", "sounds", "data", "reports"}) {
        EXPECT_TRUE(fs::is_directory(out_ / dir));
    }
    const PackageValidationResult validation = validate_runtime_package(out_);
    ASSERT_TRUE(validation.valid);

    const d2asset::AssetDatabase db = d2asset::AssetDatabase::open(out_);
    EXPECT_FALSE(db.manifest().assets().empty());
    EXPECT_FALSE(db.atlas_sheets().empty());

    bool found_image = false;
    bool found_animation = false;
    for (const auto& asset : db.manifest().assets()) {
        if (asset.type == d2asset::AssetType::Image && !found_image) {
            EXPECT_EQ(db.find_image_by_id(asset.asset_id).status,
                      d2asset::AssetLookupStatus::Found);
            found_image = true;
        }
        if (asset.type == d2asset::AssetType::Animation && !found_animation) {
            EXPECT_EQ(db.find_animation_by_id(asset.asset_id).status,
                      d2asset::AssetLookupStatus::Found);
            found_animation = true;
        }
        if (found_image && found_animation)
            break;
    }
    EXPECT_TRUE(found_image);
    EXPECT_TRUE(found_animation);

    std::ifstream manifest_input(out_ / "game_manifest.json");
    ASSERT_TRUE(manifest_input);
    nlohmann::json manifest;
    manifest_input >> manifest;
    const auto image_it =
        std::find_if(manifest["assets"].begin(), manifest["assets"].end(),
                     [](const nlohmann::json& e) { return e["type"] == "image"; });
    ASSERT_NE(image_it, manifest["assets"].end());
    const std::string image_id = (*image_it)["asset_id"].get<std::string>();
    const auto        region = db.find_atlas_region_by_image_id(image_id);
    ASSERT_EQ(region.status, d2asset::AssetLookupStatus::Found);
    ASSERT_TRUE(region.value.has_value());
    const d2asset::TextureRegion& tex = region.value.value();
    EXPECT_TRUE(fs::is_regular_file(out_ / tex.sheet_path));
    EXPECT_GT(tex.rectangle.width, 0U);
    EXPECT_GT(tex.rectangle.height, 0U);
}
