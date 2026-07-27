#include <gtest/gtest.h>
#include "cli/commands_extract_anim.hpp"
#include "cli/commands_extract_atlas.hpp"
#include "cli/asset_reference_resolver.hpp"
#include "cli/runtime_package.hpp"
#include "d2res/mqdb.hpp"
#include "d2res/opt_maps.hpp"
#include "d2asset/asset_database.hpp"
#include "d2asset/engine_contract.hpp"
#include "d2res/dbf_reader.hpp"
#include "tests/test_helpers.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>

#ifndef DISCIPLES2_GAME_ROOT
#define DISCIPLES2_GAME_ROOT ""
#endif

static const std::filesystem::path GAME_ROOT{DISCIPLES2_GAME_ROOT};
namespace fs = std::filesystem;
using json = nlohmann::json;

// NOLINTBEGIN(bugprone-unchecked-optional-access)

class ExtractAnimBatUnitsBase : public ::testing::Test {
protected:
    void SetUp() override {
        container_path_ = (GAME_ROOT / "Imgs/BatUnits.ff").string();
        if (!fs::exists(container_path_))
            GTEST_SKIP() << "Game file not found: " << container_path_;
        container_ = d2res::MqdbContainer::open(container_path_);
        maps_ = d2res::parse_opt_maps(container_);
    }
    std::string          container_path_;
    d2res::MqdbContainer container_;
    d2res::OptMaps       maps_;
};

// ── Tests 1-3: shared extract-idle animation setup ────────────────────────────

class ExtractAnimIdle : public ExtractAnimBatUnitsBase {
protected:
    void SetUp() override {
        ExtractAnimBatUnitsBase::SetUp();
        if (container_path_.empty() || !fs::exists(container_path_))
            GTEST_SKIP() << "Game file not found: " << container_path_;
        dir_ = std::make_unique<TempDir>("extract_anim_idle");
        ASSERT_EQ(cmd_extract_anim_from_container(container_, maps_, dir_->str(),
                                                  "G000UU0001IDLEA1A00", "", false, false, 100),
                  0);
    }
    std::unique_ptr<TempDir> dir_;
};

TEST_F(ExtractAnimIdle, SixteenFramesWithCorrectJsonMetadata) {
    for (int i = 0; i < 16; ++i) {
        std::ostringstream ss;
        ss << "frame_" << std::setfill('0') << std::setw(3) << i << ".png";
        EXPECT_TRUE(fs::exists(dir_->path() / ss.str())) << "Missing: " << ss.str();
    }

    std::ifstream f(dir_->path() / "anim.json");
    ASSERT_TRUE(f.is_open());
    json j;
    f >> j;
    EXPECT_EQ(j["frame_count"].get<int>(), 16);
    ASSERT_EQ(j["frames"].size(), 16u);
    EXPECT_EQ(j["frames"][0]["logical_name"].get<std::string>(), "XE");
    EXPECT_EQ(j["frames"][15]["logical_name"].get<std::string>(), "YE");
}

// ── Tests 4: runtime clip with atlas + fallback ───────────────────────────────

TEST_F(ExtractAnimBatUnitsBase, RuntimeClipPreservesOrderAndUsesAtlasThenFallback) {
    TempDir        dir("anim_runtime_clip");
    const fs::path root = dir.path();
    ASSERT_EQ(cmd_extract_anim_from_container(container_, maps_, root.string(),
                                              "G000UU0001IDLEA1A00", "", false, false, 100),
              0);

    std::ofstream(root / "XE.json") << "{}\n";
    std::ofstream(root / "YE.json") << "{}\n";
    ASSERT_EQ(cmd_extract_atlas_from_container(container_, maps_, root.string(), "XE", false, 4096),
              0);

    const json manifest = {
        {"asset_schema_version", 1},
        {"containers", json::array({{{"container_id", "imgs/batunits.ff"},
                                     {"path", "Imgs/BatUnits.ff"},
                                     {"content_kinds", json::array({"images", "animations"})}}})},
        {"assets", json::array({{{"asset_id", "imgs/batunits.ff/xe"},
                                 {"logical_name", "XE"},
                                 {"type", "image"},
                                 {"container_id", "imgs/batunits.ff"},
                                 {"path", "XE.json"}},
                                {{"asset_id", "imgs/batunits.ff/ye"},
                                 {"logical_name", "YE"},
                                 {"type", "image"},
                                 {"container_id", "imgs/batunits.ff"},
                                 {"path", "YE.json"}},
                                {{"asset_id", "imgs/batunits.ff/runtime-atlas"},
                                 {"logical_name", "RuntimeAtlas"},
                                 {"type", "atlas"},
                                 {"container_id", "imgs/batunits.ff"},
                                 {"path", "atlas.json"}},
                                {{"asset_id", "imgs/batunits.ff/g000uu0001idlea1a00"},
                                 {"logical_name", "G000UU0001IDLEA1A00"},
                                 {"type", "animation"},
                                 {"container_id", "imgs/batunits.ff"},
                                 {"path", "anim.json"}}})},
        {"warnings", json::array()}};
    std::ofstream(root / "game_manifest.json") << manifest.dump(2) << '\n';

    const d2asset::AssetDatabase db = d2asset::AssetDatabase::open(root);
    const auto clip_result = db.get_animation_clip("imgs/batunits.ff/g000uu0001idlea1a00");
    ASSERT_EQ(clip_result.status, d2asset::AssetLookupStatus::Found);
    ASSERT_TRUE(clip_result.value.has_value());
    const d2asset::AnimationClip& clip = clip_result.value.value();
    ASSERT_EQ(clip.frames.size(), 16U);
    EXPECT_EQ(clip.frames.front().logical_name, "XE");
    EXPECT_EQ(clip.frames.back().logical_name, "YE");
    EXPECT_EQ(clip.classification.role, d2asset::AnimationRole::Idle);
    ASSERT_TRUE(clip.frames.front().texture_region.has_value());
    EXPECT_EQ(clip.frames.front().texture_region.value().image_asset_id, "imgs/batunits.ff/xe");
    EXPECT_TRUE(clip.frames.front().fallback_path.has_value());
    EXPECT_TRUE(clip.frames.back().fallback_path.has_value());
    EXPECT_FALSE(clip.frames.back().texture_region.has_value());
}

// ── Test 5: full canonical package build ─────────────────────────────────────

TEST_F(ExtractAnimBatUnitsBase, ScopedCanonicalPackageValidatesAndLoads) {
    TempDir        extract_dir("anim_package_extract");
    const fs::path extracted = extract_dir.path();
    {
        TempDir         base("anim_package_root");
        const fs::path  root = base.path();
        const fs::path  package_root = root / "package";
        std::error_code ec;
        for (const std::string_view directory :
             {"images/Imgs/BatUnits.ff", "animations/Imgs/BatUnits.ff", "atlases/Imgs/BatUnits.ff",
              "sounds", "data", "reports"}) {
            fs::create_directories(package_root / directory, ec);
            ASSERT_FALSE(ec);
        }

        ASSERT_EQ(cmd_extract_anim_from_container(container_, maps_, extracted.string(),
                                                  "G000UU0001IDLEA1A00", "", false, false, 100),
                  0);

        const fs::path animation_dir =
            package_root / "animations/Imgs/BatUnits.ff/G000UU0001IDLEA1A00";
        fs::copy(extracted, animation_dir,
                 fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
        ASSERT_FALSE(ec);
        std::ofstream(package_root / "images/Imgs/BatUnits.ff/XE.json")
            << R"({"logical_name":"XE"})" << '\n';
        ASSERT_EQ(cmd_extract_atlas_from_container(
                      container_, maps_, (package_root / "atlases/Imgs/BatUnits.ff").string(), "XE",
                      false, 4096),
                  0);

        const json manifest = {
            {"asset_schema_version", 1},
            {"game_root", "/unavailable/source/root"},
            {"containers",
             json::array({{{"container_id", "imgs/batunits.ff"},
                           {"path", "Imgs/BatUnits.ff"},
                           {"content_kinds", json::array({"images", "animations"})}}})},
            {"assets",
             json::array(
                 {{{"asset_id", "imgs/batunits.ff/xe"},
                   {"logical_name", "XE"},
                   {"type", "image"},
                   {"container_id", "imgs/batunits.ff"},
                   {"path", "images/Imgs/BatUnits.ff/XE.json"}},
                  {{"asset_id", "imgs/batunits.ff/__runtime_atlas"},
                   {"logical_name", "__runtime_atlas"},
                   {"type", "atlas"},
                   {"container_id", "imgs/batunits.ff"},
                   {"path", "atlases/Imgs/BatUnits.ff/atlas.json"}},
                  {{"asset_id", "imgs/batunits.ff/g000uu0001idlea1a00"},
                   {"logical_name", "G000UU0001IDLEA1A00"},
                   {"type", "animation"},
                   {"container_id", "imgs/batunits.ff"},
                   {"path", "animations/Imgs/BatUnits.ff/G000UU0001IDLEA1A00/anim.json"}}})},
            {"warnings", json::array()}};
        std::ofstream(package_root / "game_manifest.json") << manifest.dump(2) << '\n';

        const PackageValidationResult validation = validate_runtime_package(package_root);
        ASSERT_TRUE(validation.valid);
        const d2asset::AssetDatabase database = d2asset::AssetDatabase::open(package_root);
        const auto clip = database.get_animation_clip("imgs/batunits.ff/g000uu0001idlea1a00");
        ASSERT_EQ(clip.status, d2asset::AssetLookupStatus::Found);
        ASSERT_TRUE(clip.value.has_value());
        const d2asset::AnimationClip& value = clip.value.value();
        EXPECT_EQ(value.frames.size(), 16U);
        EXPECT_TRUE(value.frames.front().texture_region.has_value());
        EXPECT_TRUE(value.frames.back().fallback_path.has_value());

        const d2asset::InspectionResult inspection = d2asset::inspect_runtime_assets(
            {.asset_root = package_root,
             .animation_asset_id = "imgs/batunits.ff/g000uu0001idlea1a00",
             .sound_asset_id = std::nullopt,
             .data_table_asset_id = std::nullopt,
             .row_key = std::nullopt,
             .reference_table_asset_id = std::nullopt,
             .reference_row_key = std::nullopt,
             .reference_target_asset_id = std::nullopt});
        ASSERT_TRUE(inspection.report.has_value());
        const json& report = inspection.report.value();
        EXPECT_EQ(report["animation"]["frame_count"], 16);
        EXPECT_FALSE(report["animation"]["frames"][0]["texture_region"].is_null());
    }
}

// ── Test 6: DBF reference resolution ──────────────────────────────────────────

TEST_F(ExtractAnimBatUnitsBase, ScopedGunitsRowResolvesToBatUnitsAnimation) {
    const fs::path dbf_path = GAME_ROOT / "Globals/Gunits.dbf";
    if (!fs::is_regular_file(dbf_path))
        GTEST_SKIP() << "Game file not found: " << dbf_path;

    TempDir extract_dir("anim_gunits_extract");
    {
        TempDir         base("anim_gunits_root");
        const fs::path  root = base.path();
        const fs::path  extracted = extract_dir.path();
        std::error_code error;

        ASSERT_EQ(cmd_extract_anim_from_container(container_, maps_, extracted.string(),
                                                  "G000UU0001IDLEA1A00", "", false, false, 100),
                  0);

        for (const std::string_view directory : {"images", "animations/Imgs/BatUnits.ff", "atlases",
                                                 "sounds", "data/Globals/Gunits.dbf", "reports"}) {
            fs::create_directories(root / directory, error);
            ASSERT_FALSE(error);
        }

        const fs::path animation_dir = root / "animations/Imgs/BatUnits.ff/G000UU0001IDLEA1A00";
        fs::copy(extracted, animation_dir,
                 fs::copy_options::recursive | fs::copy_options::overwrite_existing, error);
        ASSERT_FALSE(error);

        std::ifstream             input(dbf_path, std::ios::binary);
        std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(input),
                                        std::istreambuf_iterator<char>()};
        const d2res::DbfReader    reader(bytes);

        const fs::path work = root / "work";
        fs::create_directories(work);
        std::ofstream(work / "Gunits.schema.json")
            << reader.to_schema_json("Gunits.dbf").dump(2) << '\n';
        std::ofstream(work / "Gunits.records.json") << reader.to_records_json().dump(2) << '\n';
        const fs::path table_path = runtime_package_detail::package_extracted_dbf(
            work / "Gunits.schema.json", work / "Gunits.records.json", "Gunits",
            "Globals/Gunits.dbf", "globals/gunits.dbf/gunits", root);

        const json manifest{
            {"asset_schema_version", 1},
            {"containers", json::array({{{"container_id", "globals/gunits.dbf"},
                                         {"path", "Globals/Gunits.dbf"},
                                         {"content_kinds", json::array({"data"})}},
                                        {{"container_id", "imgs/batunits.ff"},
                                         {"path", "Imgs/BatUnits.ff"},
                                         {"content_kinds", json::array({"animations"})}}})},
            {"assets",
             json::array(
                 {{{"asset_id", "globals/gunits.dbf/gunits"},
                   {"logical_name", "Gunits"},
                   {"type", "data_table"},
                   {"container_id", "globals/gunits.dbf"},
                   {"path", table_path.generic_string()}},
                  {{"asset_id", "imgs/batunits.ff/g000uu0001idlea1a00"},
                   {"logical_name", "G000UU0001IDLEA1A00"},
                   {"type", "animation"},
                   {"container_id", "imgs/batunits.ff"},
                   {"path", "animations/Imgs/BatUnits.ff/G000UU0001IDLEA1A00/anim.json"}}})},
            {"warnings", json::array()}};
        std::ofstream(root / "game_manifest.json") << manifest.dump(2) << '\n';
        fs::remove_all(work, error);

        AssetReferenceResolver::write(root);
        const d2asset::AssetDatabase database = d2asset::AssetDatabase::open(root);
        const auto                   table = database.get_data_table("globals/gunits.dbf/gunits");
        ASSERT_TRUE(table.value.has_value());
        const auto row = std::ranges::find_if(table.value->rows, [](const auto& candidate) {
            const d2asset::DataValue* value = candidate.find("UNIT_ID");
            return value != nullptr && value->kind() == d2asset::DataValueKind::String &&
                   std::get<std::string>(value->value) == "g000uu0001";
        });
        ASSERT_NE(row, table.value->rows.end());

        const auto links =
            database.links_from_data_row(table.value->data_table_asset_id, row->row_key);
        ASSERT_TRUE(links.value.has_value());
        ASSERT_EQ(links.value->size(), 1U);
        EXPECT_EQ(links.value->front().target_asset_id, "imgs/batunits.ff/g000uu0001idlea1a00");
        EXPECT_EQ(links.value->front().reason_code, "unit_animation_prefix");
        EXPECT_EQ(links.value->front().evidence.front().source_value, "g000uu0001");
    }
}

// NOLINTEND(bugprone-unchecked-optional-access)
