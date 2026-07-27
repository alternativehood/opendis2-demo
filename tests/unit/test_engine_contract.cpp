#include "d2asset/engine_contract.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "tests/test_process.hpp"

namespace fs = std::filesystem;

namespace {

class EngineContractTest : public ::testing::Test {
protected:
    void SetUp() override {
        static std::size_t sequence = 0;
        root_ = fs::temp_directory_path() /
                ("d2_engine_contract_" + std::to_string(test_support::process_id()) + "_" +
                 std::to_string(++sequence));
        std::error_code ec;
        fs::remove_all(root_, ec);
        fs::create_directories(root_ / "images/Imgs/Test.ff");
        fs::create_directories(root_ / "animations/Imgs/Test.ff/UNITIDLE");
        fs::create_directories(root_ / "atlases/Imgs/Test.ff");
        fs::create_directories(root_ / "sounds/Sfx/Test.wdb");
        fs::create_directories(root_ / "data/Globals/Gunits.dbf");
        write_package();
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(root_, ec);
    }

    void write_package() {
        std::ofstream(root_ / "images/Imgs/Test.ff/Packed.json")
            << R"({"output_size":{"w":8,"h":9}})" << '\n';
        std::ofstream(root_ / "images/Imgs/Test.ff/Fallback.json") << "{}\n";
        std::ofstream(root_ / "animations/Imgs/Test.ff/UNITIDLE/frame_001.png") << "png";
        std::ofstream(root_ / "animations/Imgs/Test.ff/UNITIDLE/anim.json")
            << R"({"name":"UNITIDLE","frame_count":3,"frame_delay_ms":75,"frames":[{"index":0,"logical_name":"Packed","width":8,"height":9},{"index":1,"logical_name":"Fallback","width":10,"height":11},{"index":2,"logical_name":"Missing","width":12,"height":13}]})"
            << '\n';
        std::ofstream(root_ / "atlases/Imgs/Test.ff/atlas_000.png") << "png";
        std::ofstream(root_ / "atlases/Imgs/Test.ff/atlas.json")
            << R"({"max_sheet_size":64,"sheet_count":1,"total_sprites":1,"skipped_sprites":0,"entries":[{"name":"Packed","sheet":0,"x":1,"y":2,"w":8,"h":9}]})"
            << '\n';
        std::ofstream(root_ / "sounds/Sfx/Test.wdb/Hit.wav", std::ios::binary) << "sound";
        std::ofstream(root_ / "sounds/Sfx/Test.wdb/Hit.json")
            << R"({"sound_schema_version":1,"asset_id":"sfx/test.wdb/hit","logical_name":"Hit","container_id":"sfx/test.wdb","payload_path":"sounds/Sfx/Test.wdb/Hit.wav","payload_size":5,"detected_format":"wave","format_tag":85,"channels":1,"sample_rate":44100,"bit_depth":null,"duration_ms":null,"warnings":[]})"
            << '\n';
        std::ofstream(root_ / "data/Globals/Gunits.dbf/Gunits.json")
            << R"({"data_table_schema_version":1,"asset_id":"globals/gunits.dbf/gunits","logical_name":"Gunits","container_id":"globals/gunits.dbf","kind":"dbf","columns":[{"name":"UNIT_ID","source_type":"C","width":10,"decimal_count":0,"extensions":null},{"name":"META","source_type":null,"width":null,"decimal_count":null,"extensions":null}],"rows":[{"row_key":"00000000","values":[{"name":"UNIT_ID","value":"G000UN0001"},{"name":"META","value":{"tags":["unit",7,true]}}]}],"warnings":[],"extensions":null})"
            << '\n';

        const nlohmann::json manifest{
            {"asset_schema_version", 1},
            {"game_root", "/source/not/required"},
            {"containers",
             nlohmann::json::array(
                 {{{"container_id", "imgs/test.ff"},
                   {"path", "Imgs/Test.ff"},
                   {"content_kinds", nlohmann::json::array({"images", "animations"})}},
                  {{"container_id", "sfx/test.wdb"},
                   {"path", "Sfx/Test.wdb"},
                   {"content_kinds", nlohmann::json::array({"sounds"})}},
                  {{"container_id", "globals/gunits.dbf"},
                   {"path", "Globals/Gunits.dbf"},
                   {"content_kinds", nlohmann::json::array({"data"})}}})},
            {"assets",
             nlohmann::json::array({{{"asset_id", "imgs/test.ff/packed"},
                                     {"logical_name", "Packed"},
                                     {"type", "image"},
                                     {"container_id", "imgs/test.ff"},
                                     {"path", "images/Imgs/Test.ff/Packed.json"}},
                                    {{"asset_id", "imgs/test.ff/fallback"},
                                     {"logical_name", "Fallback"},
                                     {"type", "image"},
                                     {"container_id", "imgs/test.ff"},
                                     {"path", "images/Imgs/Test.ff/Fallback.json"}},
                                    {{"asset_id", "imgs/test.ff/__runtime_atlas"},
                                     {"logical_name", "__runtime_atlas"},
                                     {"type", "atlas"},
                                     {"container_id", "imgs/test.ff"},
                                     {"path", "atlases/Imgs/Test.ff/atlas.json"}},
                                    {{"asset_id", "imgs/test.ff/unitidle"},
                                     {"logical_name", "UNITIDLE"},
                                     {"type", "animation"},
                                     {"container_id", "imgs/test.ff"},
                                     {"path", "animations/Imgs/Test.ff/UNITIDLE/anim.json"}},
                                    {{"asset_id", "sfx/test.wdb/hit"},
                                     {"logical_name", "Hit"},
                                     {"type", "sound"},
                                     {"container_id", "sfx/test.wdb"},
                                     {"path", "sounds/Sfx/Test.wdb/Hit.json"}},
                                    {{"asset_id", "globals/gunits.dbf/gunits"},
                                     {"logical_name", "Gunits"},
                                     {"type", "data_table"},
                                     {"container_id", "globals/gunits.dbf"},
                                     {"path", "data/Globals/Gunits.dbf/Gunits.json"}}})},
            {"warnings", nlohmann::json::array()}};
        std::ofstream(root_ / "game_manifest.json") << manifest.dump(2) << '\n';
        const nlohmann::json links{
            {"asset_links_schema_version", 1},
            {"links",
             nlohmann::json::array(
                 {{{"source",
                    {{"kind", "data_row"},
                     {"table_asset_id", "globals/gunits.dbf/gunits"},
                     {"row_key", "00000000"}}},
                   {"target_asset_id", "imgs/test.ff/unitidle"},
                   {"link_kind", "idle_animation"},
                   {"resolution", "heuristic"},
                   {"confidence", 80},
                   {"reason_code", "unit_animation_prefix"},
                   {"evidence", nlohmann::json::array({{{"field", "UNIT_ID"},
                                                        {"source_value", "G000UN0001"},
                                                        {"target_value", "UNITIDLE"}}})}}})},
            {"unresolved", nlohmann::json::array()},
            {"warnings", nlohmann::json::array()},
            {"extensions", nullptr}};
        std::ofstream(root_ / "asset_links.json") << links.dump(2) << '\n';
    }

    fs::path root_;
};

TEST_F(EngineContractTest, ReportsAtlasFallbackUnresolvedAndSoundDeterministically) {
    const d2asset::InspectionRequest request{
        .asset_root = root_,
        .animation_asset_id = "imgs/test.ff/unitidle",
        .sound_asset_id = "sfx/test.wdb/hit",
        .data_table_asset_id = std::nullopt,
        .row_key = std::nullopt,
        .reference_table_asset_id = std::nullopt,
        .reference_row_key = std::nullopt,
        .reference_target_asset_id = std::nullopt,
    };
    const d2asset::InspectionResult first = d2asset::inspect_runtime_assets(request);
    const d2asset::InspectionResult second = d2asset::inspect_runtime_assets(request);
    ASSERT_TRUE(first.report.has_value());
    ASSERT_TRUE(second.report.has_value());
    if (!first.report.has_value() || !second.report.has_value()) {
        FAIL() << "inspection report missing";
        return;
    }

    const std::string output = d2asset::serialize_inspection_json(first.report.value());
    EXPECT_EQ(output, d2asset::serialize_inspection_json(second.report.value()));
    std::ifstream snapshot(fs::path(OPENDIS2_SOURCE_DIR) /
                           "tests/fixtures/engine_contract_v1.json");
    ASSERT_TRUE(snapshot);
    const std::string expected((std::istreambuf_iterator<char>(snapshot)),
                               std::istreambuf_iterator<char>());
    EXPECT_EQ(output, expected);
    const nlohmann::json parsed = nlohmann::json::parse(output);
    EXPECT_EQ(parsed["inspection_schema_version"], 1);
    EXPECT_EQ(parsed["animation"]["frame_count"], 3);
    EXPECT_TRUE(parsed["animation"]["frames"][0]["resolved"].get<bool>());
    EXPECT_FALSE(parsed["animation"]["frames"][0]["texture_region"].is_null());
    EXPECT_EQ(parsed["animation"]["frames"][0]["texture_region"]["sheet_path"],
              "atlases/Imgs/Test.ff/atlas_000.png");
    EXPECT_EQ(parsed["animation"]["frames"][1]["fallback_path"],
              "animations/Imgs/Test.ff/UNITIDLE/frame_001.png");
    EXPECT_TRUE(parsed["animation"]["frames"][2]["texture_region"].is_null());
    EXPECT_FALSE(parsed["animation"]["frames"][2]["resolved"].get<bool>());
    EXPECT_EQ(parsed["sound"]["asset_id"], "sfx/test.wdb/hit");
    EXPECT_EQ(parsed["sound"]["format"], "wave");
    EXPECT_EQ(parsed["sound"]["payload_path"], "sounds/Sfx/Test.wdb/Hit.wav");
    EXPECT_EQ(parsed["sound"]["sample_rate"], 44100);
    EXPECT_EQ(output.back(), '\n');
}

TEST_F(EngineContractTest, CommandReturnsStableLookupAndPackageErrors) {
    std::ostringstream missing_animation;
    EXPECT_EQ(d2asset::run_d2asset_inspect({root_.string(), "--animation", "imgs/test.ff/missing"},
                                           missing_animation),
              4);
    EXPECT_EQ(nlohmann::json::parse(missing_animation.str())["error"]["code"],
              "animation_not_found");

    std::ostringstream wrong_sound;
    EXPECT_EQ(d2asset::run_d2asset_inspect({root_.string(), "--animation", "imgs/test.ff/unitidle",
                                            "--sound", "imgs/test.ff/packed"},
                                           wrong_sound),
              5);
    EXPECT_EQ(nlohmann::json::parse(wrong_sound.str())["error"]["code"], "sound_not_found");

    std::ostringstream wrong_table;
    EXPECT_EQ(d2asset::run_d2asset_inspect({root_.string(), "--animation", "imgs/test.ff/unitidle",
                                            "--data-table", "imgs/test.ff/packed"},
                                           wrong_table),
              6);
    EXPECT_EQ(nlohmann::json::parse(wrong_table.str())["error"]["code"], "data_table_not_found");

    std::ostringstream missing_row;
    EXPECT_EQ(d2asset::run_d2asset_inspect({root_.string(), "--animation", "imgs/test.ff/unitidle",
                                            "--data-table", "globals/gunits.dbf/gunits", "--row",
                                            "missing"},
                                           missing_row),
              7);
    EXPECT_EQ(nlohmann::json::parse(missing_row.str())["error"]["code"], "data_row_not_found");

    std::ostringstream missing_package;
    EXPECT_EQ(d2asset::run_d2asset_inspect(
                  {(root_ / "missing").string(), "--animation", "imgs/test.ff/unitidle"},
                  missing_package),
              3);
    EXPECT_EQ(nlohmann::json::parse(missing_package.str())["error"]["detail_code"],
              "missing_manifest");

    std::ofstream(root_ / "game_manifest.json") << "{";
    std::ostringstream invalid_package;
    EXPECT_EQ(d2asset::run_d2asset_inspect({root_.string(), "--animation", "imgs/test.ff/unitidle"},
                                           invalid_package),
              3);
    EXPECT_EQ(nlohmann::json::parse(invalid_package.str())["error"]["detail_code"], "invalid_json");
}

TEST_F(EngineContractTest, UsageErrorsAreJsonAndRelocatedPackageStillOpens) {
    std::ostringstream usage;
    EXPECT_EQ(d2asset::run_d2asset_inspect({}, usage), 2);
    EXPECT_EQ(nlohmann::json::parse(usage.str())["error"]["code"], "invalid_arguments");

    std::ostringstream row_without_table;
    EXPECT_EQ(d2asset::run_d2asset_inspect(
                  {root_.string(), "--animation", "imgs/test.ff/unitidle", "--row", "row"},
                  row_without_table),
              2);

    const fs::path relocated = root_.parent_path() / (root_.filename().string() + "_relocated");
    fs::rename(root_, relocated);
    root_ = relocated;
    const auto result =
        d2asset::inspect_runtime_assets({.asset_root = root_,
                                         .animation_asset_id = "imgs/test.ff/unitidle",
                                         .sound_asset_id = std::nullopt,
                                         .data_table_asset_id = std::nullopt,
                                         .row_key = std::nullopt,
                                         .reference_table_asset_id = std::nullopt,
                                         .reference_row_key = std::nullopt,
                                         .reference_target_asset_id = std::nullopt});
    ASSERT_TRUE(result.report.has_value());
    if (!result.report.has_value()) {
        FAIL() << "relocated inspection report missing";
        return;
    }
    EXPECT_FALSE(result.report.value().contains("sound"));
}

TEST_F(EngineContractTest, ReportsSelectedDataRowDeterministically) {
    const d2asset::InspectionRequest request{
        .asset_root = root_,
        .animation_asset_id = "imgs/test.ff/unitidle",
        .sound_asset_id = std::nullopt,
        .data_table_asset_id = "globals/gunits.dbf/gunits",
        .row_key = "00000000",
        .reference_table_asset_id = std::nullopt,
        .reference_row_key = std::nullopt,
        .reference_target_asset_id = std::nullopt,
    };
    const auto result = d2asset::inspect_runtime_assets(request);
    ASSERT_TRUE(result.report.has_value());
    if (!result.report.has_value()) {
        FAIL() << "data-table inspection report missing";
        return;
    }
    const auto& table = result.report.value()["data_table"];
    EXPECT_EQ(table["kind"], "dbf");
    EXPECT_EQ(table["row_count"], 1);
    EXPECT_EQ(table["selected_row"]["row_key"], "00000000");
    EXPECT_EQ(table["selected_row"]["values"][0]["name"], "UNIT_ID");
    EXPECT_EQ(table["selected_row"]["values"][0]["value"]["kind"], "string");
    EXPECT_EQ(table["selected_row"]["values"][1]["value"]["value"][0]["name"], "tags");
}

TEST_F(EngineContractTest, InspectsReferenceSourceAndTargetWithStableFailures) {
    std::ostringstream source_output;
    EXPECT_EQ(d2asset::run_d2asset_inspect({root_.string(), "--animation", "imgs/test.ff/unitidle",
                                            "--reference-table", "globals/gunits.dbf/gunits",
                                            "--reference-row", "00000000"},
                                           source_output),
              0);
    const auto source = nlohmann::json::parse(source_output.str());
    ASSERT_EQ(source["references"]["outgoing"].size(), 1U);
    EXPECT_EQ(source["references"]["outgoing"][0]["link_kind"], "idle_animation");

    std::ostringstream target_output;
    EXPECT_EQ(d2asset::run_d2asset_inspect({root_.string(), "--animation", "imgs/test.ff/unitidle",
                                            "--reference-target", "imgs/test.ff/unitidle"},
                                           target_output),
              0);
    EXPECT_EQ(nlohmann::json::parse(target_output.str())["references"]["incoming"].size(), 1U);

    std::ostringstream missing_source;
    EXPECT_EQ(d2asset::run_d2asset_inspect({root_.string(), "--animation", "imgs/test.ff/unitidle",
                                            "--reference-table", "globals/gunits.dbf/gunits",
                                            "--reference-row", "missing"},
                                           missing_source),
              8);
    EXPECT_EQ(nlohmann::json::parse(missing_source.str())["error"]["code"],
              "reference_source_not_found");

    std::ostringstream missing_target;
    EXPECT_EQ(d2asset::run_d2asset_inspect({root_.string(), "--animation", "imgs/test.ff/unitidle",
                                            "--reference-target", "missing"},
                                           missing_target),
              9);
    EXPECT_EQ(nlohmann::json::parse(missing_target.str())["error"]["code"],
              "reference_target_not_found");

    std::ostringstream incomplete_source;
    EXPECT_EQ(d2asset::run_d2asset_inspect({root_.string(), "--animation", "imgs/test.ff/unitidle",
                                            "--reference-table", "globals/gunits.dbf/gunits"},
                                           incomplete_source),
              2);

    std::ostringstream conflicting;
    EXPECT_EQ(d2asset::run_d2asset_inspect({root_.string(), "--animation", "imgs/test.ff/unitidle",
                                            "--reference-table", "globals/gunits.dbf/gunits",
                                            "--reference-row", "00000000", "--reference-target",
                                            "imgs/test.ff/unitidle"},
                                           conflicting),
              2);
}

TEST(EngineContractEnums, StableStringsAndGenericPaths) {
    EXPECT_STREQ(d2asset::to_string(d2asset::AnimationRole::Attack), "attack");
    EXPECT_STREQ(d2asset::to_string(d2asset::LoopMode::Unknown), "unknown");
    EXPECT_STREQ(d2asset::to_string(d2asset::FacingDirection::Unknown), "unknown");
    EXPECT_STREQ(d2asset::to_string(d2asset::TimingSource::FallbackDefault), "fallback_default");
    EXPECT_EQ(d2asset::package_path_string(fs::path("a") / "b" / ".." / "c.json"), "a/c.json");
}

} // namespace
