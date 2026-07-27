#include "cli/runtime_package.hpp"
#include "cli/extraction_orchestrator.hpp"
#include "d2asset/asset_database.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "tests/test_process.hpp"

namespace fs = std::filesystem;

namespace {

class RuntimePackageTest : public ::testing::Test {
protected:
    void SetUp() override {
        static std::size_t sequence = 0;
        root_ = fs::temp_directory_path() /
                ("d2_runtime_package_test_" + std::to_string(test_support::process_id()) + "_" +
                 std::to_string(++sequence));
        std::error_code ec;
        fs::remove_all(root_, ec);
        fs::create_directories(root_, ec);
        ASSERT_FALSE(ec);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(root_, ec);
    }

    void write_minimal_package() {
        for (const std::string_view directory :
             {"images", "animations", "atlases", "sounds", "data", "reports"}) {
            fs::create_directories(root_ / directory);
        }
        const nlohmann::json manifest{
            {"asset_schema_version", 1},
            {"game_root", "/source/is/provenance/only"},
            {"containers", nlohmann::json::array()},
            {"assets", nlohmann::json::array()},
            {"warnings", nlohmann::json::array()},
        };
        std::ofstream(root_ / "game_manifest.json") << manifest.dump(2) << '\n';
    }

    void write_visual_package() {
        write_minimal_package();
        const fs::path image_dir = root_ / "images/Imgs/Test.ff";
        const fs::path animation_dir = root_ / "animations/Imgs/Test.ff/UNITIDLE";
        const fs::path atlas_dir = root_ / "atlases/Imgs/Test.ff";
        fs::create_directories(image_dir);
        fs::create_directories(animation_dir);
        fs::create_directories(atlas_dir);

        std::ofstream(image_dir / "A.json")
            << R"({"logical_name":"A","output_size":{"w":8,"h":8}})" << '\n';
        std::ofstream(image_dir / "B.json")
            << R"({"logical_name":"B","output_size":{"w":8,"h":8}})" << '\n';
        std::ofstream(animation_dir / "frame_000.png") << "frame-a";
        std::ofstream(animation_dir / "frame_001.png") << "frame-b";
        std::ofstream(animation_dir / "anim.json")
            << R"({"name":"UNITIDLE","frame_count":2,"frame_delay_ms":100,"frames":[{"index":0,"logical_name":"A","width":8,"height":8},{"index":1,"logical_name":"B","width":8,"height":8}]})"
            << '\n';
        std::ofstream(atlas_dir / "atlas_000.png") << "sheet";
        std::ofstream(atlas_dir / "atlas.json")
            << R"({"max_sheet_size":64,"sheet_count":1,"total_sprites":1,"skipped_sprites":0,"entries":[{"name":"A","sheet":0,"x":0,"y":0,"w":8,"h":8}]})"
            << '\n';

        const nlohmann::json manifest{
            {"asset_schema_version", 1},
            {"game_root", "/source/is/provenance/only"},
            {"containers",
             nlohmann::json::array(
                 {{{"container_id", "imgs/test.ff"},
                   {"path", "Imgs/Test.ff"},
                   {"content_kinds", nlohmann::json::array({"images", "animations"})}}})},
            {"assets",
             nlohmann::json::array({{{"asset_id", "imgs/test.ff/a"},
                                     {"logical_name", "A"},
                                     {"type", "image"},
                                     {"container_id", "imgs/test.ff"},
                                     {"path", "images/Imgs/Test.ff/A.json"}},
                                    {{"asset_id", "imgs/test.ff/b"},
                                     {"logical_name", "B"},
                                     {"type", "image"},
                                     {"container_id", "imgs/test.ff"},
                                     {"path", "images/Imgs/Test.ff/B.json"}},
                                    {{"asset_id", "imgs/test.ff/__runtime_atlas"},
                                     {"logical_name", "__runtime_atlas"},
                                     {"type", "atlas"},
                                     {"container_id", "imgs/test.ff"},
                                     {"path", "atlases/Imgs/Test.ff/atlas.json"}},
                                    {{"asset_id", "imgs/test.ff/unitidle"},
                                     {"logical_name", "UNITIDLE"},
                                     {"type", "animation"},
                                     {"container_id", "imgs/test.ff"},
                                     {"path", "animations/Imgs/Test.ff/UNITIDLE/anim.json"}}})},
            {"warnings", nlohmann::json::array()},
        };
        std::ofstream(root_ / "game_manifest.json") << manifest.dump(2) << '\n';
    }

    fs::path root_;
};

} // namespace

TEST(RuntimePackagePaths, MapsKnownAssetTypes) {
    EXPECT_EQ(package_subtree_for_type("image"), fs::path("images"));
    EXPECT_EQ(package_subtree_for_type("animation"), fs::path("animations"));
    EXPECT_EQ(package_subtree_for_type("atlas"), fs::path("atlases"));
    EXPECT_EQ(package_subtree_for_type("sound"), fs::path("sounds"));
    EXPECT_EQ(package_subtree_for_type("data_table"), fs::path("data"));
    EXPECT_TRUE(package_subtree_for_type("unknown").empty());
}

TEST(RuntimePackagePaths, RejectsUnsafeRelativePaths) {
    EXPECT_TRUE(is_safe_package_relative_path("images/a.json"));
    EXPECT_FALSE(is_safe_package_relative_path(""));
    EXPECT_FALSE(is_safe_package_relative_path("."));
    EXPECT_FALSE(is_safe_package_relative_path("../a.json"));
    EXPECT_FALSE(is_safe_package_relative_path("images/../../a.json"));
    EXPECT_FALSE(is_safe_package_relative_path("/absolute/a.json"));
}

TEST(ExtractionOrchestrator, DetectsSupportedTypedContent) {
    d2res::ContainerEntry images;
    images.likely_content = {d2res::ContentKind::Images};
    EXPECT_TRUE(has_supported_runtime_content(images));

    d2res::ContainerEntry animations;
    animations.likely_content = {d2res::ContentKind::Animations};
    EXPECT_TRUE(has_supported_runtime_content(animations));

    d2res::ContainerEntry sounds;
    sounds.likely_content = {d2res::ContentKind::Sounds};
    EXPECT_TRUE(has_supported_runtime_content(sounds));

    d2res::ContainerEntry unknown;
    unknown.likely_content = {d2res::ContentKind::Unknown};
    EXPECT_FALSE(has_supported_runtime_content(unknown));
}

TEST_F(RuntimePackageTest, ValidatesMinimalPortablePackage) {
    write_minimal_package();
    const PackageValidationResult result = validate_runtime_package(root_);
    EXPECT_TRUE(result.valid);
    EXPECT_TRUE(fs::is_regular_file(root_ / "reports/validation_report.json"));
}

TEST_F(RuntimePackageTest, ValidationReportIncludesReferenceCounts) {
    write_visual_package();
    const nlohmann::json graph{
        {"asset_links_schema_version", 1},
        {"links", nlohmann::json::array(
                      {{{"source", {{"kind", "asset"}, {"asset_id", "imgs/test.ff/a"}}},
                        {"target_asset_id", "imgs/test.ff/unitidle"},
                        {"link_kind", "idle_animation"},
                        {"resolution", "confirmed"},
                        {"confidence", 100},
                        {"reason_code", "explicit_asset_id"},
                        {"evidence",
                         nlohmann::json::array({{{"field", "asset_id"},
                                                 {"source_value", "imgs/test.ff/unitidle"},
                                                 {"target_value", "imgs/test.ff/unitidle"}}})}}})},
        {"unresolved", nlohmann::json::array()},
        {"warnings", nlohmann::json::array()},
        {"extensions", nullptr}};
    std::ofstream(root_ / "asset_links.json") << graph.dump(2) << '\n';

    const PackageValidationResult result = validate_runtime_package(root_);
    ASSERT_TRUE(result.valid);
    EXPECT_EQ(result.asset_link_count, 1U);
    std::ifstream  report_input(root_ / "reports/validation_report.json");
    nlohmann::json report;
    report_input >> report;
    EXPECT_EQ(report["summary"]["asset_link_count"], 1);
    EXPECT_EQ(report["summary"]["unresolved_asset_link_count"], 0);
}

TEST_F(RuntimePackageTest, ReportsMissingRequiredEntries) {
    fs::create_directories(root_ / "reports");
    const PackageValidationResult result = validate_runtime_package(root_);
    EXPECT_FALSE(result.valid);
    EXPECT_GT(result.diagnostics.size(), 1U);

    std::ifstream input(root_ / "reports/validation_report.json");
    ASSERT_TRUE(input);
    nlohmann::json report;
    input >> report;
    EXPECT_FALSE(report["valid"].get<bool>());
    EXPECT_GT(report["summary"]["error_count"].get<std::size_t>(), 1U);
}

TEST_F(RuntimePackageTest, RejectsUnsafeManifestAssetPath) {
    write_minimal_package();
    nlohmann::json manifest;
    {
        std::ifstream input(root_ / "game_manifest.json");
        input >> manifest;
    }
    manifest["containers"].push_back({{"container_id", "imgs/test.ff"},
                                      {"path", "Imgs/Test.ff"},
                                      {"content_kinds", nlohmann::json::array({"images"})}});
    manifest["assets"].push_back({{"asset_id", "imgs/test.ff/bad"},
                                  {"logical_name", "BAD"},
                                  {"type", "image"},
                                  {"container_id", "imgs/test.ff"},
                                  {"path", "../bad.json"}});
    std::ofstream(root_ / "game_manifest.json") << manifest.dump(2) << '\n';

    const PackageValidationResult result = validate_runtime_package(root_);
    EXPECT_FALSE(result.valid);
    EXPECT_TRUE(std::ranges::any_of(
        result.diagnostics, [](const auto& item) { return item.code == "unsafe_asset_path"; }));
}

TEST_F(RuntimePackageTest, ValidatesCompleteVisualPackageContract) {
    write_visual_package();
    const PackageValidationResult validation = validate_runtime_package(root_);
    ASSERT_TRUE(validation.valid);

    const d2asset::AssetDatabase database = d2asset::AssetDatabase::open(root_);
    const auto                   clip_result = database.get_animation_clip("imgs/test.ff/unitidle");
    ASSERT_EQ(clip_result.status, d2asset::AssetLookupStatus::Found);
    if (!clip_result.value.has_value()) {
        FAIL() << "found animation clip has no value";
        return;
    }
    const d2asset::AnimationClip& clip = clip_result.value.value();
    ASSERT_EQ(clip.frames.size(), 2U);
    EXPECT_TRUE(clip.frames[0].texture_region.has_value());
    EXPECT_TRUE(clip.frames[0].fallback_path.has_value());
    EXPECT_FALSE(clip.frames[1].texture_region.has_value());
    EXPECT_TRUE(clip.frames[1].fallback_path.has_value());
}

TEST_F(RuntimePackageTest, ReportsMissingReferencedFile) {
    write_visual_package();
    fs::remove(root_ / "animations/Imgs/Test.ff/UNITIDLE/anim.json");
    const PackageValidationResult validation = validate_runtime_package(root_);
    EXPECT_FALSE(validation.valid);
    EXPECT_TRUE(std::ranges::any_of(validation.diagnostics, [](const auto& item) {
        return item.code == "missing_asset_file";
    }));
}

TEST_F(RuntimePackageTest, ReportsMalformedRuntimeSidecar) {
    write_visual_package();
    std::ofstream(root_ / "animations/Imgs/Test.ff/UNITIDLE/anim.json") << "{}\n";
    const PackageValidationResult validation = validate_runtime_package(root_);
    EXPECT_FALSE(validation.valid);
    EXPECT_TRUE(std::ranges::any_of(validation.diagnostics, [](const auto& item) {
        return item.code == "malformed_animation";
    }));
}

TEST_F(RuntimePackageTest, ReportsMalformedAtlasSidecar) {
    write_visual_package();
    std::ofstream(root_ / "atlases/Imgs/Test.ff/atlas.json") << "{}\n";
    const PackageValidationResult validation = validate_runtime_package(root_);
    EXPECT_FALSE(validation.valid);
    EXPECT_TRUE(std::ranges::any_of(
        validation.diagnostics, [](const auto& item) { return item.code == "malformed_atlas"; }));
}

TEST_F(RuntimePackageTest, PreservesManifestWarningsAndCustomReportPath) {
    write_minimal_package();
    nlohmann::json manifest;
    {
        std::ifstream input(root_ / "game_manifest.json");
        input >> manifest;
    }
    manifest["warnings"].push_back("recoverable source warning");
    std::ofstream(root_ / "game_manifest.json") << manifest.dump(2) << '\n';

    const fs::path                custom_report = root_ / "custom/report.json";
    const PackageValidationResult validation = validate_runtime_package(root_, custom_report);
    EXPECT_TRUE(validation.valid);
    EXPECT_TRUE(fs::is_regular_file(custom_report));
    EXPECT_TRUE(std::ranges::any_of(validation.diagnostics, [](const auto& item) {
        return item.severity == PackageDiagnosticSeverity::Warning &&
               item.code == "manifest_warning";
    }));
}

TEST_F(RuntimePackageTest, BuilderPreservesExistingDestination) {
    const fs::path marker = root_ / "marker.txt";
    std::ofstream(marker) << "keep";

    RuntimePackageBuildOptions options;
    options.game_root = root_;
    options.output_root = root_;
    const RuntimePackageBuildResult result = build_runtime_package(options);

    EXPECT_FALSE(result.published);
    EXPECT_TRUE(fs::is_regular_file(marker));
    EXPECT_TRUE(std::ranges::any_of(
        result.diagnostics, [](const auto& item) { return item.code == "destination_exists"; }));
}

TEST_F(RuntimePackageTest, BuildsPublishesAndMovesEmptyPortablePackage) {
    const fs::path game_root = root_ / "game";
    const fs::path package_root = root_ / "package";
    const fs::path moved_root = root_ / "moved-package";
    fs::create_directories(game_root);

    RuntimePackageBuildOptions options;
    options.game_root = game_root;
    options.output_root = package_root;
    const RuntimePackageBuildResult build = build_runtime_package(options);
    ASSERT_TRUE(build.published);
    EXPECT_TRUE(fs::is_regular_file(package_root / "game_manifest.json"));
    EXPECT_TRUE(fs::is_regular_file(package_root / "reports/build_report.json"));
    EXPECT_TRUE(fs::is_regular_file(package_root / "reports/validation_report.json"));

    fs::rename(package_root, moved_root);
    const PackageValidationResult validation = validate_runtime_package(moved_root);
    EXPECT_TRUE(validation.valid);
}

TEST_F(RuntimePackageTest, InvalidGameRootDoesNotCreateDestination) {
    RuntimePackageBuildOptions options;
    options.game_root = root_ / "missing-game";
    options.output_root = root_ / "package";
    const RuntimePackageBuildResult result = build_runtime_package(options);
    EXPECT_FALSE(result.published);
    EXPECT_FALSE(fs::exists(options.output_root));
    EXPECT_TRUE(std::ranges::any_of(
        result.diagnostics, [](const auto& item) { return item.code == "invalid_game_root"; }));
}

TEST_F(RuntimePackageTest, CommandWrappersReturnExpectedExitCodes) {
    write_minimal_package();
    EXPECT_EQ(cmd_validate_runtime_assets(root_.string(), ""), 0);
    EXPECT_NE(cmd_validate_runtime_assets((root_ / "missing").string(), ""), 0);
    EXPECT_NE(
        cmd_build_runtime_assets((root_ / "missing-game").string(), (root_ / "package").string()),
        0);
}

TEST_F(RuntimePackageTest, CanonicalizesWaveAndUnknownSoundPayloads) {
    const fs::path work = root_ / "work";
    const fs::path package = root_ / "package";
    fs::create_directories(work);
    fs::create_directories(package);

    std::ofstream(work / "Hit.wav", std::ios::binary) << "wave";
    std::ofstream(work / "Hit.json")
        << R"({"logical_name":"Hit","source_container":"/source/Sounds/Test.wdb","payload_size":4,"detected_format":"WAV","format_tag":85,"channels":1,"sample_rate":44100})"
        << '\n';
    const fs::path wave_sidecar = runtime_package_detail::package_extracted_sound(
        work / "Hit.json", "Hit", "Sounds/Test.wdb", "sounds/test.wdb/hit", package);
    EXPECT_EQ(wave_sidecar, fs::path("sounds/Sounds/Test.wdb/Hit.json"));
    EXPECT_TRUE(fs::is_regular_file(package / "sounds/Sounds/Test.wdb/Hit.wav"));

    nlohmann::json wave;
    std::ifstream(package / wave_sidecar) >> wave;
    EXPECT_EQ(wave["sound_schema_version"], 1);
    EXPECT_EQ(wave["detected_format"], "wave");
    EXPECT_EQ(wave["payload_path"], "sounds/Sounds/Test.wdb/Hit.wav");
    EXPECT_EQ(wave["payload_size"], 4);
    EXPECT_EQ(wave["source_container"], "/source/Sounds/Test.wdb");

    std::ofstream(work / "Mystery.bin", std::ios::binary) << "data";
    std::ofstream(work / "Mystery.json")
        << R"({"logical_name":"Mystery","payload_size":4,"detected_format":"UNKNOWN"})" << '\n';
    const fs::path unknown_sidecar = runtime_package_detail::package_extracted_sound(
        work / "Mystery.json", "Mystery", "Sounds/Test.wdb", "sounds/test.wdb/mystery", package);
    EXPECT_TRUE(fs::is_regular_file(package / "sounds/Sounds/Test.wdb/Mystery.bin"));
    nlohmann::json unknown;
    std::ifstream(package / unknown_sidecar) >> unknown;
    EXPECT_EQ(unknown["detected_format"], "unknown");
    EXPECT_EQ(unknown["payload_path"], "sounds/Sounds/Test.wdb/Mystery.bin");
}

TEST_F(RuntimePackageTest, RejectsExtractedSoundWithoutPayload) {
    const fs::path work = root_ / "work";
    const fs::path package = root_ / "package";
    fs::create_directories(work);
    fs::create_directories(package);
    std::ofstream(work / "Missing.json")
        << R"({"logical_name":"Missing","payload_size":4,"detected_format":"WAV"})" << '\n';

    EXPECT_THROW((void)runtime_package_detail::package_extracted_sound(
                     work / "Missing.json", "Missing", "Sounds/Test.wdb", "sounds/test.wdb/missing",
                     package),
                 std::runtime_error);
    EXPECT_FALSE(fs::exists(package / "sounds/Sounds/Test.wdb/Missing.json"));
}

TEST_F(RuntimePackageTest, CanonicalizesDbfDatAndDlgTables) {
    const fs::path work = root_ / "work";
    const fs::path package = root_ / "package";
    fs::create_directories(work);
    fs::create_directories(package);

    std::ofstream(work / "Gunits.schema.json")
        << R"({"source":"Gunits.dbf","fields":[{"name":"UNIT_ID","type":"C","length":10,"decimal_count":0},{"name":"LEVEL","type":"N","length":2,"decimal_count":0}]})"
        << '\n';
    std::ofstream(work / "Gunits.records.json")
        << R"([{"UNIT_ID":"G000UN0001","LEVEL":"1"}])" << '\n';
    const fs::path dbf = runtime_package_detail::package_extracted_dbf(
        work / "Gunits.schema.json", work / "Gunits.records.json", "Gunits", "Globals/Gunits.dbf",
        "globals/gunits.dbf/gunits", package);
    nlohmann::json dbf_json;
    std::ifstream(package / dbf) >> dbf_json;
    EXPECT_EQ(dbf_json["kind"], "dbf");
    EXPECT_EQ(dbf_json["rows"][0]["row_key"], "00000000");
    EXPECT_EQ(dbf_json["columns"][0]["name"], "UNIT_ID");

    std::ofstream(work / "gameinfo.json") << R"({"Zed":"last","Alpha":"first"})" << '\n';
    const fs::path dat = runtime_package_detail::package_extracted_dat(
        work / "gameinfo.json", "gameinfo", "gameinfo.dat", "gameinfo.dat/gameinfo", package);
    nlohmann::json dat_json;
    std::ifstream(package / dat) >> dat_json;
    EXPECT_EQ(dat_json["kind"], "dat");
    EXPECT_EQ(dat_json["rows"][0]["row_key"], "Alpha");
    EXPECT_EQ(dat_json["rows"][1]["row_key"], "Zed");

    std::ofstream(work / "Interf.json")
        << R"([{"id":"DLG_A","elements":[{"type":"BUTTON","unknown":7}]},{"id":"DLG_A","extra":{"nested":[true,null]}}])"
        << '\n';
    const fs::path dlg = runtime_package_detail::package_extracted_dlg(
        work / "Interf.json", "Interf", "Interf/Interf.dlg", "interf/interf.dlg/interf", package);
    nlohmann::json dlg_json;
    std::ifstream(package / dlg) >> dlg_json;
    EXPECT_EQ(dlg_json["kind"], "dlg");
    EXPECT_EQ(dlg_json["rows"][0]["row_key"], "DLG_A");
    EXPECT_EQ(dlg_json["rows"][1]["row_key"], "00000001");
    EXPECT_EQ(dlg_json["warnings"].size(), 1U);
}

TEST_F(RuntimePackageTest, RejectsMissingOrMalformedExtractedData) {
    const fs::path work = root_ / "work";
    const fs::path package = root_ / "package";
    fs::create_directories(work);
    fs::create_directories(package);
    std::ofstream(work / "Gunits.schema.json") << R"({"fields":[]})" << '\n';
    EXPECT_THROW((void)runtime_package_detail::package_extracted_dbf(
                     work / "Gunits.schema.json", work / "Gunits.records.json", "Gunits",
                     "Globals/Gunits.dbf", "globals/gunits.dbf/gunits", package),
                 std::runtime_error);
    std::ofstream(work / "bad.json") << "{";
    EXPECT_THROW((void)runtime_package_detail::package_extracted_dat(
                     work / "bad.json", "bad", "bad.dat", "bad.dat/bad", package),
                 std::runtime_error);
    EXPECT_THROW((void)runtime_package_detail::package_extracted_dlg(
                     work / "bad.json", "bad", "bad.dlg", "bad.dlg/bad", package),
                 std::runtime_error);
}
