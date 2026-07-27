#include <gtest/gtest.h>

#include "d2asset/asset_database.hpp"
#include "d2asset/asset_error.hpp"

#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "tests/test_process.hpp"

namespace fs = std::filesystem;
using d2asset::AssetDatabase;
using d2asset::AssetError;
using d2asset::AssetErrorCode;
using d2asset::AssetLookupStatus;

namespace {

nlohmann::json container(std::string id, std::string path) {
    return {{"container_id", std::move(id)},
            {"path", std::move(path)},
            {"content_kinds", nlohmann::json::array({"images"})}};
}

nlohmann::json asset(std::string id, std::string name, std::string type, std::string container_id,
                     std::string path) {
    return {{"asset_id", std::move(id)},
            {"logical_name", std::move(name)},
            {"type", std::move(type)},
            {"container_id", std::move(container_id)},
            {"path", std::move(path)}};
}

class SyntheticAssetPackage : public ::testing::Test {
protected:
    void SetUp() override {
        static std::size_t sequence = 0;
        root_ = fs::temp_directory_path() /
                ("d2asset_test_" + std::to_string(test_support::process_id()) + "_" +
                 std::to_string(++sequence));
        std::error_code ec;
        fs::remove_all(root_, ec);
        fs::create_directories(root_, ec);
        manifest_ = {
            {"asset_schema_version", 1},
            {"containers", nlohmann::json::array({container("imgs/test.ff", "Imgs/Test.ff")})},
            {"assets", nlohmann::json::array()},
            {"warnings", nlohmann::json::array()}};
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(root_, ec);
    }

    void add_asset(std::string id, std::string name, std::string type, std::string path,
                   std::string container_id = "imgs/test.ff") {
        const fs::path relative(path);
        fs::create_directories((root_ / relative).parent_path());
        std::ofstream(root_ / relative) << "{}\n";
        manifest_["assets"].push_back(asset(std::move(id), std::move(name), std::move(type),
                                            std::move(container_id), std::move(path)));
    }

    void write_json(const fs::path& relative, const nlohmann::json& value) {
        fs::create_directories((root_ / relative).parent_path());
        std::ofstream(root_ / relative) << value.dump(2) << '\n';
    }

    void add_image(std::string id, std::string name, const std::string& path,
                   std::string                       container_id = "imgs/test.ff",
                   std::optional<d2asset::PixelSize> output_size = std::nullopt) {
        if (container_id.empty())
            container_id = "imgs/test.ff";
        add_asset(std::move(id), std::move(name), "image", path, std::move(container_id));
        nlohmann::json sidecar = nlohmann::json::object();
        if (output_size.has_value()) {
            sidecar["output_size"] = {{"w", output_size->width}, {"h", output_size->height}};
        }
        write_json(path, sidecar);
    }

    void add_atlas(std::string id, const std::string& path, nlohmann::json entries,
                   std::string container_id = "imgs/test.ff", int max_sheet_size = 64,
                   int sheet_count = 1, std::string name = "Atlas") {
        if (container_id.empty())
            container_id = "imgs/test.ff";
        add_asset(std::move(id), std::move(name), "atlas", path, std::move(container_id));
        write_json(path, {{"source_container", "synthetic"},
                          {"max_sheet_size", max_sheet_size},
                          {"sheet_count", sheet_count},
                          {"total_sprites", entries.size()},
                          {"skipped_sprites", 0},
                          {"entries", std::move(entries)}});
        for (int i = 0; i < sheet_count; ++i) {
            std::ostringstream filename;
            filename << "atlas_" << std::setfill('0') << std::setw(3) << i << ".png";
            std::ofstream((root_ / path).parent_path() / filename.str()) << "png";
        }
    }

    static nlohmann::json animation_frame(int index, std::string name, int width = 8,
                                          int height = 9) {
        return {{"index", index},
                {"logical_name", std::move(name)},
                {"width", width},
                {"height", height}};
    }

    void add_animation(std::string id, std::string name, const std::string& path,
                       nlohmann::json frames, std::optional<nlohmann::json> delay = 100,
                       std::string container_id = "imgs/test.ff") {
        const std::string sidecar_name = name;
        add_asset(std::move(id), std::move(name), "animation", path, std::move(container_id));
        nlohmann::json sidecar = {{"name", sidecar_name},
                                  {"frame_count", frames.size()},
                                  {"source_container", "synthetic"},
                                  {"frames", std::move(frames)}};
        if (delay.has_value())
            sidecar["frame_delay_ms"] = std::move(delay.value());
        write_json(path, sidecar);
    }

    void add_sound(std::string id, std::string name, const std::string& path,
                   std::string payload_path, std::string format = "wave",
                   std::string container_id = "imgs/test.ff") {
        const std::string asset_id = id;
        const std::string logical_name = name;
        const std::string owner = container_id;
        add_asset(std::move(id), std::move(name), "sound", path, std::move(container_id));
        fs::create_directories((root_ / payload_path).parent_path());
        std::ofstream(root_ / payload_path, std::ios::binary) << "sound";
        write_json(path, {{"sound_schema_version", 1},
                          {"asset_id", asset_id},
                          {"logical_name", logical_name},
                          {"container_id", owner},
                          {"payload_path", payload_path},
                          {"payload_size", 5},
                          {"detected_format", std::move(format)},
                          {"format_tag", 85},
                          {"channels", 1},
                          {"sample_rate", 44100},
                          {"bit_depth", nullptr},
                          {"duration_ms", nullptr},
                          {"warnings", nlohmann::json::array()}});
    }

    void add_data_table(std::string id, std::string name, const std::string& path,
                        std::string kind = "dat", nlohmann::json columns = nlohmann::json::array(),
                        nlohmann::json rows = nlohmann::json::array(),
                        std::string    container_id = "imgs/test.ff") {
        const std::string asset_id = id;
        const std::string logical_name = name;
        const std::string owner = container_id;
        add_asset(std::move(id), std::move(name), "data_table", path, std::move(container_id));
        write_json(path, {{"data_table_schema_version", 1},
                          {"asset_id", asset_id},
                          {"logical_name", logical_name},
                          {"container_id", owner},
                          {"kind", std::move(kind)},
                          {"columns", std::move(columns)},
                          {"rows", std::move(rows)},
                          {"warnings", nlohmann::json::array()},
                          {"extensions", nullptr}});
    }

    void add_fallback_frame(const std::string& animation_path, int index) {
        std::ostringstream filename;
        filename << "frame_" << std::setfill('0') << std::setw(3) << index << ".png";
        std::ofstream((root_ / animation_path).parent_path() / filename.str()) << "png";
    }

    static nlohmann::json atlas_entry(std::string name, int sheet = 0, int x = 1, int y = 2,
                                      int width = 3, int height = 4) {
        return {{"name", std::move(name)},
                {"sheet", sheet},
                {"x", x},
                {"y", y},
                {"w", width},
                {"h", height}};
    }

    void write_manifest() {
        std::ofstream(root_ / "game_manifest.json") << manifest_.dump(2) << '\n';
    }

    static void expect_error(AssetErrorCode code, const std::function<void()>& action) {
        try {
            action();
            FAIL() << "expected AssetError";
        } catch (const AssetError& error) {
            EXPECT_EQ(error.code(), code) << error.what();
        }
    }

    fs::path       root_;
    nlohmann::json manifest_;
};

TEST_F(SyntheticAssetPackage, LoadsAllTypedAssetsAndExactIds) {
    add_asset("imgs/test.ff/picture", "Picture", "image", "Imgs/Test.ff/Picture.json");
    add_animation("imgs/test.ff/walk", "Walk", "Imgs/Test.ff/Walk/anim.json",
                  nlohmann::json::array());
    add_sound("imgs/test.ff/hit", "Hit", "Imgs/Test.ff/Hit.json", "Imgs/Test.ff/Hit.wav");
    add_atlas("imgs/test.ff/sheet", "Imgs/Test.ff/atlas.json", nlohmann::json::array(), {}, 64, 0,
              "Sheet");
    add_data_table("imgs/test.ff/table", "Table", "Imgs/Test.ff/table.json");
    write_manifest();

    const AssetDatabase db = AssetDatabase::open(root_);
    EXPECT_EQ(db.manifest().assets().size(), 5U);
    EXPECT_EQ(db.find_image_by_id("imgs/test.ff/picture").status, AssetLookupStatus::Found);
    EXPECT_EQ(db.find_animation_by_id("imgs/test.ff/walk").status, AssetLookupStatus::Found);
    EXPECT_EQ(db.find_sound_by_id("imgs/test.ff/hit").status, AssetLookupStatus::Found);
    EXPECT_EQ(db.find_atlas_by_id("imgs/test.ff/sheet").status, AssetLookupStatus::Found);
    EXPECT_EQ(db.find_data_table_by_id("imgs/test.ff/table").status, AssetLookupStatus::Found);
}

TEST_F(SyntheticAssetPackage, LogicalNameLookupIsCaseInsensitiveAndPreservesCase) {
    add_asset("imgs/test.ff/picture", "MixedCase", "image", "Imgs/Test.ff/Picture.json");
    write_manifest();

    const AssetDatabase db = AssetDatabase::open(root_);
    const auto          result = db.find_image("mixedcase");
    ASSERT_EQ(result.status, AssetLookupStatus::Found);
    ASSERT_TRUE(result.value.has_value());
    // The assertion above establishes the optional precondition for this test.
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    EXPECT_EQ(result.value->logical_name, "MixedCase");
}

TEST_F(SyntheticAssetPackage, MissingAndWrongTypedLookupsReturnNotFound) {
    add_asset("imgs/test.ff/picture", "Picture", "image", "Imgs/Test.ff/Picture.json");
    write_manifest();

    const AssetDatabase db = AssetDatabase::open(root_);
    EXPECT_EQ(db.find_image_by_id("imgs/test.ff/missing").status, AssetLookupStatus::NotFound);
    EXPECT_EQ(db.find_animation_by_id("imgs/test.ff/picture").status, AssetLookupStatus::NotFound);
    EXPECT_EQ(db.find_image("missing").status, AssetLookupStatus::NotFound);
}

TEST_F(SyntheticAssetPackage, DuplicateLogicalNamesAreAmbiguousPerType) {
    manifest_["containers"].push_back(container("imgs/other.ff", "Imgs/Other.ff"));
    add_asset("imgs/test.ff/picture", "Picture", "image", "Imgs/Test.ff/Picture.json");
    add_asset("imgs/other.ff/picture", "PICTURE", "image", "Imgs/Other.ff/Picture.json",
              "imgs/other.ff");
    add_sound("imgs/test.ff/picture-sound", "Picture", "Imgs/Test.ff/PictureSound.json",
              "Imgs/Test.ff/PictureSound.wav");
    write_manifest();

    const AssetDatabase db = AssetDatabase::open(root_);
    const auto          images = db.find_image("picture");
    EXPECT_EQ(images.status, AssetLookupStatus::Ambiguous);
    EXPECT_EQ(images.matching_asset_ids.size(), 2U);
    EXPECT_EQ(db.find_sound("picture").status, AssetLookupStatus::Found);
}

TEST_F(SyntheticAssetPackage, MissingManifestIsReported) {
    expect_error(AssetErrorCode::MissingManifest, [this] { (void)AssetDatabase::open(root_); });
}

TEST_F(SyntheticAssetPackage, InvalidJsonIsReported) {
    std::ofstream(root_ / "game_manifest.json") << "{";
    expect_error(AssetErrorCode::InvalidJson, [this] { (void)AssetDatabase::open(root_); });
}

TEST_F(SyntheticAssetPackage, UnsupportedSchemaIsReported) {
    manifest_["asset_schema_version"] = 2;
    write_manifest();
    expect_error(AssetErrorCode::UnsupportedSchema, [this] { (void)AssetDatabase::open(root_); });
}

TEST_F(SyntheticAssetPackage, MissingRequiredFieldIsReported) {
    manifest_.erase("warnings");
    write_manifest();
    expect_error(AssetErrorCode::InvalidJson, [this] { (void)AssetDatabase::open(root_); });
}

TEST_F(SyntheticAssetPackage, DuplicateAssetIdIsRejected) {
    add_asset("imgs/test.ff/picture", "Picture", "image", "Imgs/Test.ff/Picture.json");
    add_asset("imgs/test.ff/picture", "Other", "image", "Imgs/Test.ff/Other.json");
    write_manifest();
    expect_error(AssetErrorCode::DuplicateId, [this] { (void)AssetDatabase::open(root_); });
}

TEST_F(SyntheticAssetPackage, UnknownContainerReferenceIsRejected) {
    add_asset("missing.ff/picture", "Picture", "image", "Picture.json", "missing.ff");
    write_manifest();
    expect_error(AssetErrorCode::MalformedEntry, [this] { (void)AssetDatabase::open(root_); });
}

TEST_F(SyntheticAssetPackage, AbsolutePathIsRejected) {
    manifest_["assets"].push_back(
        asset("imgs/test.ff/picture", "Picture", "image", "imgs/test.ff", "/tmp/Picture.json"));
    write_manifest();
    expect_error(AssetErrorCode::UnsafePath, [this] { (void)AssetDatabase::open(root_); });
}

TEST_F(SyntheticAssetPackage, ParentTraversalIsRejected) {
    manifest_["assets"].push_back(
        asset("imgs/test.ff/picture", "Picture", "image", "imgs/test.ff", "../Picture.json"));
    write_manifest();
    expect_error(AssetErrorCode::UnsafePath, [this] { (void)AssetDatabase::open(root_); });
}

TEST_F(SyntheticAssetPackage, MissingFileIsRejected) {
    manifest_["assets"].push_back(asset("imgs/test.ff/picture", "Picture", "image", "imgs/test.ff",
                                        "Imgs/Test.ff/Missing.json"));
    write_manifest();
    expect_error(AssetErrorCode::MissingFile, [this] { (void)AssetDatabase::open(root_); });
}

TEST_F(SyntheticAssetPackage, DirectoryPathIsRejected) {
    fs::create_directories(root_ / "Imgs/Test.ff/Directory");
    manifest_["assets"].push_back(asset("imgs/test.ff/picture", "Picture", "image", "imgs/test.ff",
                                        "Imgs/Test.ff/Directory"));
    write_manifest();
    expect_error(AssetErrorCode::MissingFile, [this] { (void)AssetDatabase::open(root_); });
}

TEST_F(SyntheticAssetPackage, UnknownAssetTypeIsPreservedButNotTyped) {
    add_asset("imgs/test.ff/future", "Future", "future_type", "Imgs/Test.ff/Future.json");
    write_manifest();

    const AssetDatabase db = AssetDatabase::open(root_);
    ASSERT_EQ(db.manifest().assets().size(), 1U);
    EXPECT_EQ(db.manifest().assets().front().type, d2asset::AssetType::Unknown);
    EXPECT_EQ(db.manifest().assets().front().type_name, "future_type");
    EXPECT_EQ(db.find_image_by_id("imgs/test.ff/future").status, AssetLookupStatus::NotFound);
}

TEST_F(SyntheticAssetPackage, LoadsAtlasAndLooksUpExactImageRegion) {
    add_image("imgs/test.ff/picture", "Picture", "Imgs/Test.ff/Picture.json", {},
              d2asset::PixelSize{.width = 10, .height = 12});
    add_atlas("imgs/test.ff/atlas", "Imgs/Test.ff/atlas.json",
              nlohmann::json::array({atlas_entry("PICTURE")}));
    write_manifest();

    const AssetDatabase db = AssetDatabase::open(root_);
    const auto          result = db.find_atlas_region_by_image_id("imgs/test.ff/picture");
    ASSERT_EQ(result.status, AssetLookupStatus::Found);
    if (!result.value.has_value()) {
        FAIL() << "found atlas result has no value";
        return;
    }
    const auto& region = result.value.value();
    EXPECT_EQ(region.atlas_asset_id, "imgs/test.ff/atlas");
    EXPECT_EQ(region.image_asset_id, "imgs/test.ff/picture");
    EXPECT_EQ(region.logical_name, "PICTURE");
    EXPECT_EQ(region.sheet_path, fs::path("Imgs/Test.ff/atlas_000.png"));
    EXPECT_EQ(region.sheet_index, 0U);
    EXPECT_EQ(region.rectangle.x, 1U);
    EXPECT_EQ(region.rectangle.y, 2U);
    EXPECT_EQ(region.rectangle.width, 3U);
    EXPECT_EQ(region.rectangle.height, 4U);
    if (!region.source_size.has_value()) {
        FAIL() << "source dimensions were not preserved";
        return;
    }
    EXPECT_EQ(region.source_size.value().width, 10U);
    EXPECT_EQ(region.source_size.value().height, 12U);
    EXPECT_FALSE(region.trimmed_size.has_value());
    EXPECT_FALSE(region.trim_offset.has_value());
    EXPECT_FALSE(region.pivot.has_value());
    EXPECT_FALSE(region.anchor.has_value());
}

TEST_F(SyntheticAssetPackage, AtlasNameResolutionIsScopedToContainer) {
    manifest_["containers"].push_back(container("imgs/other.ff", "Imgs/Other.ff"));
    add_image("imgs/test.ff/picture", "Picture", "Imgs/Test.ff/Picture.json");
    add_image("imgs/other.ff/picture", "PICTURE", "Imgs/Other.ff/Picture.json", "imgs/other.ff");
    add_atlas("imgs/test.ff/atlas", "Imgs/Test.ff/atlas.json",
              nlohmann::json::array({atlas_entry("picture")}));
    write_manifest();

    const AssetDatabase db = AssetDatabase::open(root_);
    EXPECT_EQ(db.find_atlas_region_by_image_id("imgs/test.ff/picture").status,
              AssetLookupStatus::Found);
    EXPECT_EQ(db.find_atlas_region_by_image_id("imgs/other.ff/picture").status,
              AssetLookupStatus::NotFound);
}

TEST_F(SyntheticAssetPackage, MissingAtlasImageMappingIsRejected) {
    add_atlas("imgs/test.ff/atlas", "Imgs/Test.ff/atlas.json",
              nlohmann::json::array({atlas_entry("Missing")}));
    write_manifest();
    expect_error(AssetErrorCode::MalformedAtlas, [this] { (void)AssetDatabase::open(root_); });
}

TEST_F(SyntheticAssetPackage, AmbiguousSameContainerImageMappingIsRejected) {
    add_image("imgs/test.ff/picture-a", "Picture", "Imgs/Test.ff/PictureA.json");
    add_image("imgs/test.ff/picture-b", "PICTURE", "Imgs/Test.ff/PictureB.json");
    add_atlas("imgs/test.ff/atlas", "Imgs/Test.ff/atlas.json",
              nlohmann::json::array({atlas_entry("picture")}));
    write_manifest();
    expect_error(AssetErrorCode::MalformedAtlas, [this] { (void)AssetDatabase::open(root_); });
}

TEST_F(SyntheticAssetPackage, DuplicateAtlasEntryNameIsRejectedCaseInsensitively) {
    add_image("imgs/test.ff/picture", "Picture", "Imgs/Test.ff/Picture.json");
    add_atlas("imgs/test.ff/atlas", "Imgs/Test.ff/atlas.json",
              nlohmann::json::array({atlas_entry("Picture"), atlas_entry("PICTURE")}));
    write_manifest();
    expect_error(AssetErrorCode::DuplicateAtlasEntry, [this] { (void)AssetDatabase::open(root_); });
}

TEST_F(SyntheticAssetPackage, MalformedAtlasRequiredFieldIsRejected) {
    add_image("imgs/test.ff/picture", "Picture", "Imgs/Test.ff/Picture.json");
    add_atlas("imgs/test.ff/atlas", "Imgs/Test.ff/atlas.json",
              nlohmann::json::array({atlas_entry("Picture")}));
    nlohmann::json const malformed = {{"sheet_count", 1},
                                      {"total_sprites", 1},
                                      {"skipped_sprites", 0},
                                      {"entries", nlohmann::json::array({atlas_entry("Picture")})}};
    write_json("Imgs/Test.ff/atlas.json", malformed);
    write_manifest();
    expect_error(AssetErrorCode::MalformedAtlas, [this] { (void)AssetDatabase::open(root_); });
}

TEST_F(SyntheticAssetPackage, InvalidAtlasDimensionsAndSheetIndexAreRejected) {
    add_image("imgs/test.ff/picture", "Picture", "Imgs/Test.ff/Picture.json");
    add_atlas("imgs/test.ff/atlas", "Imgs/Test.ff/atlas.json",
              nlohmann::json::array({atlas_entry("Picture", 1)}));
    write_manifest();
    expect_error(AssetErrorCode::MalformedAtlas, [this] { (void)AssetDatabase::open(root_); });

    write_json("Imgs/Test.ff/atlas.json",
               {{"source_container", "synthetic"},
                {"max_sheet_size", 64},
                {"sheet_count", 1},
                {"total_sprites", 1},
                {"skipped_sprites", 0},
                {"entries", nlohmann::json::array({atlas_entry("Picture")})}});
    add_atlas("imgs/test.ff/atlas-two", "Imgs/Test.ff/atlas-two/atlas.json",
              nlohmann::json::array({atlas_entry("Picture", 0, 0, 0, 0, 4)}));
    write_manifest();
    expect_error(AssetErrorCode::MalformedAtlas, [this] { (void)AssetDatabase::open(root_); });
}

TEST_F(SyntheticAssetPackage, AtlasRectangleOutsideBoundsIsRejected) {
    add_image("imgs/test.ff/picture", "Picture", "Imgs/Test.ff/Picture.json");
    add_atlas("imgs/test.ff/atlas", "Imgs/Test.ff/atlas.json",
              nlohmann::json::array({atlas_entry("Picture", 0, 63, 0, 2, 4)}));
    write_manifest();
    expect_error(AssetErrorCode::InvalidAtlasRectangle,
                 [this] { (void)AssetDatabase::open(root_); });
}

TEST_F(SyntheticAssetPackage, MissingAtlasSheetIsRejected) {
    add_image("imgs/test.ff/picture", "Picture", "Imgs/Test.ff/Picture.json");
    add_atlas("imgs/test.ff/atlas", "Imgs/Test.ff/atlas.json",
              nlohmann::json::array({atlas_entry("Picture")}));
    fs::remove(root_ / "Imgs/Test.ff/atlas_000.png");
    write_manifest();
    expect_error(AssetErrorCode::MissingAtlasSheet, [this] { (void)AssetDatabase::open(root_); });
}

TEST_F(SyntheticAssetPackage, NonRegularAtlasSheetIsRejected) {
    add_image("imgs/test.ff/picture", "Picture", "Imgs/Test.ff/Picture.json");
    add_atlas("imgs/test.ff/atlas", "Imgs/Test.ff/atlas.json",
              nlohmann::json::array({atlas_entry("Picture")}));
    fs::remove(root_ / "Imgs/Test.ff/atlas_000.png");
    fs::create_directory(root_ / "Imgs/Test.ff/atlas_000.png");
    write_manifest();
    expect_error(AssetErrorCode::MissingAtlasSheet, [this] { (void)AssetDatabase::open(root_); });
}

TEST_F(SyntheticAssetPackage, MissingImageOutputSizeRemainsUnset) {
    add_image("imgs/test.ff/picture", "Picture", "Imgs/Test.ff/Picture.json");
    add_atlas("imgs/test.ff/atlas", "Imgs/Test.ff/atlas.json",
              nlohmann::json::array({atlas_entry("Picture")}));
    write_manifest();

    const auto result =
        AssetDatabase::open(root_).find_atlas_region_by_image_id("imgs/test.ff/picture");
    ASSERT_EQ(result.status, AssetLookupStatus::Found);
    if (!result.value.has_value()) {
        FAIL() << "found atlas result has no value";
        return;
    }
    EXPECT_FALSE(result.value.value().source_size.has_value());
}

TEST_F(SyntheticAssetPackage, InvalidImageOutputSizeIsRejected) {
    add_image("imgs/test.ff/picture", "Picture", "Imgs/Test.ff/Picture.json");
    write_json("Imgs/Test.ff/Picture.json", {{"output_size", {{"w", 0}, {"h", 4}}}});
    add_atlas("imgs/test.ff/atlas", "Imgs/Test.ff/atlas.json",
              nlohmann::json::array({atlas_entry("Picture")}));
    write_manifest();
    expect_error(AssetErrorCode::MalformedAtlas, [this] { (void)AssetDatabase::open(root_); });
}

TEST_F(SyntheticAssetPackage, AtlasLookupReturnsNotFoundAndAmbiguous) {
    add_image("imgs/test.ff/packed", "Packed", "Imgs/Test.ff/Packed.json");
    add_image("imgs/test.ff/unpacked", "Unpacked", "Imgs/Test.ff/Unpacked.json");
    add_atlas("imgs/test.ff/atlas-a", "Imgs/Test.ff/a/atlas.json",
              nlohmann::json::array({atlas_entry("Packed")}), {}, 64, 1, "AtlasA");
    add_atlas("imgs/test.ff/atlas-b", "Imgs/Test.ff/b/atlas.json",
              nlohmann::json::array({atlas_entry("PACKED")}), {}, 64, 1, "AtlasB");
    write_manifest();

    const AssetDatabase db = AssetDatabase::open(root_);
    EXPECT_EQ(db.find_atlas_region_by_image_id("imgs/test.ff/unpacked").status,
              AssetLookupStatus::NotFound);
    const auto packed = db.find_atlas_region_by_image_id("imgs/test.ff/packed");
    EXPECT_EQ(packed.status, AssetLookupStatus::Ambiguous);
    EXPECT_EQ(packed.matching_asset_ids,
              (std::vector<std::string>{"imgs/test.ff/atlas-a", "imgs/test.ff/atlas-b"}));
}

TEST_F(SyntheticAssetPackage, AnimationFrameImageIdUsesStandardAtlasLookup) {
    add_image("imgs/test.ff/frame-01", "Frame01", "Imgs/Test.ff/Frame01.json");
    add_atlas("imgs/test.ff/atlas", "Imgs/Test.ff/atlas.json",
              nlohmann::json::array({atlas_entry("Frame01")}));
    write_manifest();

    const AssetDatabase db = AssetDatabase::open(root_);
    const std::string   animation_frame_image_id = "imgs/test.ff/frame-01";
    EXPECT_EQ(db.find_atlas_region_by_image_id(animation_frame_image_id).status,
              AssetLookupStatus::Found);
}

TEST_F(SyntheticAssetPackage, LoadsOrderedAnimationClipAndPreservesExistingLookup) {
    add_image("imgs/test.ff/frame-a", "FrameA", "Imgs/Test.ff/FrameA.json");
    add_image("imgs/test.ff/frame-b", "FrameB", "Imgs/Test.ff/FrameB.json");
    add_animation("imgs/test.ff/idle", "UnitIdle", "Imgs/Test.ff/UnitIdle/anim.json",
                  nlohmann::json::array({animation_frame(0, "FrameA", 10, 11),
                                         animation_frame(1, "FrameB", 12, 13)}));
    add_fallback_frame("Imgs/Test.ff/UnitIdle/anim.json", 0);
    add_fallback_frame("Imgs/Test.ff/UnitIdle/anim.json", 1);
    write_manifest();

    const AssetDatabase db = AssetDatabase::open(root_);
    EXPECT_EQ(db.find_animation_by_id("imgs/test.ff/idle").status, AssetLookupStatus::Found);
    const auto result = db.get_animation_clip("imgs/test.ff/idle");
    ASSERT_EQ(result.status, AssetLookupStatus::Found);
    if (!result.value.has_value()) {
        FAIL() << "found animation clip has no value";
        return;
    }
    const auto& clip = result.value.value();
    EXPECT_EQ(clip.animation_asset_id, "imgs/test.ff/idle");
    EXPECT_EQ(clip.logical_name, "UnitIdle");
    ASSERT_EQ(clip.frames.size(), 2U);
    EXPECT_EQ(clip.frames[0].index, 0U);
    EXPECT_EQ(clip.frames[0].logical_name, "FrameA");
    EXPECT_EQ(clip.frames[0].source_size.width, 10U);
    EXPECT_EQ(clip.frames[1].index, 1U);
    EXPECT_EQ(clip.frames[1].logical_name, "FrameB");
    EXPECT_EQ(clip.classification.role, d2asset::AnimationRole::Idle);
    EXPECT_EQ(clip.classification.matched_token, "idle");
    EXPECT_EQ(clip.loop_mode, d2asset::LoopMode::Unknown);
    EXPECT_EQ(clip.facing_direction, d2asset::FacingDirection::Unknown);
}

TEST_F(SyntheticAssetPackage, RejectsMalformedAnimationStructure) {
    add_animation("imgs/test.ff/walk", "Walk", "Imgs/Test.ff/Walk/anim.json",
                  nlohmann::json::array({animation_frame(0, "FrameA")}));
    const nlohmann::json sidecar = {
        {"name", "Walk"},
        {"frame_count", 2},
        {"frame_delay_ms", 100},
        {"frames", nlohmann::json::array({animation_frame(0, "FrameA")})}};
    write_json("Imgs/Test.ff/Walk/anim.json", sidecar);
    write_manifest();
    expect_error(AssetErrorCode::MalformedAnimation, [this] { (void)AssetDatabase::open(root_); });
}

TEST_F(SyntheticAssetPackage, RejectsInvalidAnimationFrameAndNameMismatch) {
    add_animation("imgs/test.ff/walk", "Walk", "Imgs/Test.ff/Walk/anim.json",
                  nlohmann::json::array({animation_frame(1, "FrameA", 0, 4)}));
    write_manifest();
    expect_error(AssetErrorCode::MalformedAnimation, [this] { (void)AssetDatabase::open(root_); });

    write_json("Imgs/Test.ff/Walk/anim.json",
               {{"name", "Run"},
                {"frame_count", 1},
                {"frame_delay_ms", 100},
                {"frames", nlohmann::json::array({animation_frame(0, "FrameA")})}});
    expect_error(AssetErrorCode::MalformedAnimation, [this] { (void)AssetDatabase::open(root_); });
}

TEST_F(SyntheticAssetPackage, RejectsInvalidAnimationFrameDimensionsAndTypes) {
    add_animation("imgs/test.ff/walk", "Walk", "Imgs/Test.ff/Walk/anim.json",
                  nlohmann::json::array({animation_frame(0, "FrameA", 0, 4)}));
    write_manifest();
    expect_error(AssetErrorCode::MalformedAnimation, [this] { (void)AssetDatabase::open(root_); });

    write_json(
        "Imgs/Test.ff/Walk/anim.json",
        {{"name", "Walk"},
         {"frame_count", 1},
         {"frame_delay_ms", 100},
         {"frames",
          nlohmann::json::array(
              {{{"index", 0}, {"logical_name", "FrameA"}, {"width", "invalid"}, {"height", 4}}})}});
    expect_error(AssetErrorCode::MalformedAnimation, [this] { (void)AssetDatabase::open(root_); });
}

TEST_F(SyntheticAssetPackage, ResolvesAnimationFramesWithinContainerAndUsesAtlas) {
    manifest_["containers"].push_back(container("imgs/other.ff", "Imgs/Other.ff"));
    add_image("imgs/test.ff/frame", "Frame", "Imgs/Test.ff/Frame.json");
    add_image("imgs/other.ff/frame", "FRAME", "Imgs/Other.ff/Frame.json", "imgs/other.ff");
    add_atlas("imgs/test.ff/atlas", "Imgs/Test.ff/atlas.json",
              nlohmann::json::array({atlas_entry("frame")}));
    add_animation("imgs/test.ff/attack", "Attack", "Imgs/Test.ff/Attack/anim.json",
                  nlohmann::json::array({animation_frame(0, "FRAME")}));
    write_manifest();

    const auto result = AssetDatabase::open(root_).get_animation_clip("imgs/test.ff/attack");
    ASSERT_EQ(result.status, AssetLookupStatus::Found);
    if (!result.value.has_value()) {
        FAIL() << "found animation clip has no value";
        return;
    }
    const auto& frame = result.value.value().frames.front();
    if (!frame.image_asset_id.has_value()) {
        FAIL() << "resolved frame has no image ID";
        return;
    }
    EXPECT_EQ(frame.image_asset_id.value(), "imgs/test.ff/frame");
    if (!frame.texture_region.has_value()) {
        FAIL() << "resolved frame has no atlas region";
        return;
    }
    EXPECT_EQ(frame.texture_region.value().atlas_asset_id, "imgs/test.ff/atlas");
    EXPECT_FALSE(frame.fallback_path.has_value());
    EXPECT_TRUE(frame.resolved());
}

TEST_F(SyntheticAssetPackage, PreservesFallbackFramesAndReportsUnresolvedFrames) {
    add_animation(
        "imgs/test.ff/move", "Move", "Imgs/Test.ff/Move/anim.json",
        nlohmann::json::array({animation_frame(0, "Fallback"), animation_frame(1, "Missing")}));
    add_fallback_frame("Imgs/Test.ff/Move/anim.json", 0);
    write_manifest();

    const AssetDatabase db = AssetDatabase::open(root_);
    const auto          result = db.get_animation_clip("imgs/test.ff/move");
    ASSERT_EQ(result.status, AssetLookupStatus::Found);
    if (!result.value.has_value()) {
        FAIL() << "found animation clip has no value";
        return;
    }
    const auto& clip = result.value.value();
    ASSERT_EQ(clip.frames.size(), 2U);
    EXPECT_EQ(clip.frames[0].fallback_path,
              std::optional<fs::path>("Imgs/Test.ff/Move/frame_000.png"));
    EXPECT_TRUE(clip.frames[0].resolved());
    EXPECT_FALSE(clip.frames[1].resolved());
    EXPECT_GE(clip.warnings.size(), 3U);
    EXPECT_EQ(db.animation_diagnostics().size(), clip.warnings.size());
}

TEST_F(SyntheticAssetPackage, ReportsAmbiguousAnimationAtlasWithoutSelectingRegion) {
    add_image("imgs/test.ff/frame", "Frame", "Imgs/Test.ff/Frame.json");
    add_atlas("imgs/test.ff/atlas-a", "Imgs/Test.ff/a/atlas.json",
              nlohmann::json::array({atlas_entry("Frame")}), {}, 64, 1, "AtlasA");
    add_atlas("imgs/test.ff/atlas-b", "Imgs/Test.ff/b/atlas.json",
              nlohmann::json::array({atlas_entry("Frame")}), {}, 64, 1, "AtlasB");
    add_animation("imgs/test.ff/hit", "Hit", "Imgs/Test.ff/Hit/anim.json",
                  nlohmann::json::array({animation_frame(0, "Frame")}));
    add_fallback_frame("Imgs/Test.ff/Hit/anim.json", 0);
    write_manifest();

    const auto result = AssetDatabase::open(root_).get_animation_clip("imgs/test.ff/hit");
    ASSERT_EQ(result.status, AssetLookupStatus::Found);
    if (!result.value.has_value()) {
        FAIL() << "found animation clip has no value";
        return;
    }
    const auto& clip = result.value.value();
    EXPECT_FALSE(clip.frames.front().texture_region.has_value());
    EXPECT_TRUE(clip.frames.front().fallback_path.has_value());
    const auto warning =
        std::ranges::find_if(clip.warnings, [](const d2asset::AnimationWarning& item) {
            return item.message == "animation frame image maps to multiple atlases";
        });
    ASSERT_NE(warning, clip.warnings.end());
    EXPECT_EQ(warning->matching_asset_ids,
              (std::vector<std::string>{"imgs/test.ff/atlas-a", "imgs/test.ff/atlas-b"}));
}

TEST_F(SyntheticAssetPackage, UsesExplicitAnimationTimingProvenance) {
    add_animation("imgs/test.ff/cast", "Cast", "Imgs/Test.ff/Cast/anim.json",
                  nlohmann::json::array({animation_frame(0, "Frame")}), 75);
    add_fallback_frame("Imgs/Test.ff/Cast/anim.json", 0);
    add_animation("imgs/test.ff/defend", "Defend", "Imgs/Test.ff/Defend/anim.json",
                  nlohmann::json::array({animation_frame(0, "Frame")}), "invalid");
    add_fallback_frame("Imgs/Test.ff/Defend/anim.json", 0);
    add_animation("imgs/test.ff/hit", "Hit", "Imgs/Test.ff/Hit/anim.json",
                  nlohmann::json::array({animation_frame(0, "Frame")}), std::nullopt);
    add_fallback_frame("Imgs/Test.ff/Hit/anim.json", 0);
    add_animation("imgs/test.ff/death", "Death", "Imgs/Test.ff/Death/anim.json",
                  nlohmann::json::array({animation_frame(0, "Frame")}), 0);
    add_fallback_frame("Imgs/Test.ff/Death/anim.json", 0);
    write_manifest();

    const AssetDatabase db = AssetDatabase::open(root_);
    const auto          cast = db.get_animation_clip("imgs/test.ff/cast");
    const auto          defend = db.get_animation_clip("imgs/test.ff/defend");
    const auto          hit = db.get_animation_clip("imgs/test.ff/hit");
    const auto          death = db.get_animation_clip("imgs/test.ff/death");
    if (!cast.value.has_value() || !defend.value.has_value() || !hit.value.has_value() ||
        !death.value.has_value()) {
        FAIL() << "timing test clip lookup failed";
        return;
    }
    EXPECT_EQ(cast.value->frames.front().timing.duration_ms, 75U);
    EXPECT_EQ(cast.value->frames.front().timing.source, d2asset::TimingSource::ProvisionalSidecar);
    EXPECT_EQ(defend.value->frames.front().timing.duration_ms, 100U);
    EXPECT_EQ(defend.value->frames.front().timing.source, d2asset::TimingSource::FallbackDefault);
    EXPECT_FALSE(defend.value->warnings.empty());
    EXPECT_EQ(hit.value->frames.front().timing.source, d2asset::TimingSource::FallbackDefault);
    EXPECT_EQ(death.value->frames.front().timing.source, d2asset::TimingSource::FallbackDefault);
}

TEST_F(SyntheticAssetPackage, ClassifiesDocumentedAnimationRolesAndUnknown) {
    const std::vector<std::pair<std::string, d2asset::AnimationRole>> cases = {
        {"UnitIdle", d2asset::AnimationRole::Idle},
        {"UnitMove", d2asset::AnimationRole::Move},
        {"UnitAttack", d2asset::AnimationRole::Attack},
        {"UnitHit", d2asset::AnimationRole::Hit},
        {"UnitDeath", d2asset::AnimationRole::Death},
        {"UnitCast", d2asset::AnimationRole::Cast},
        {"UnitDefend", d2asset::AnimationRole::Defend},
        {"UnitMystery", d2asset::AnimationRole::Unknown},
    };
    for (std::size_t i = 0; i < cases.size(); ++i) {
        const std::string id = "imgs/test.ff/role-" + std::to_string(i);
        add_animation(id, cases[i].first, "Imgs/Test.ff/Role" + std::to_string(i) + "/anim.json",
                      nlohmann::json::array());
    }
    write_manifest();

    const AssetDatabase db = AssetDatabase::open(root_);
    for (std::size_t i = 0; i < cases.size(); ++i) {
        const auto result = db.get_animation_clip("imgs/test.ff/role-" + std::to_string(i));
        if (!result.value.has_value()) {
            FAIL() << "classification clip lookup failed";
            return;
        }
        EXPECT_EQ(result.value->classification.role, cases[i].second);
        if (cases[i].second == d2asset::AnimationRole::Unknown) {
            EXPECT_TRUE(result.value->classification.matched_token.empty());
        } else {
            EXPECT_FALSE(result.value->classification.matched_token.empty());
        }
    }
}

TEST_F(SyntheticAssetPackage, AnimationClipLookupReturnsNotFoundForMissingAndWrongType) {
    add_image("imgs/test.ff/picture", "Picture", "Imgs/Test.ff/Picture.json");
    write_manifest();

    const AssetDatabase db = AssetDatabase::open(root_);
    EXPECT_EQ(db.get_animation_clip("imgs/test.ff/missing").status, AssetLookupStatus::NotFound);
    EXPECT_EQ(db.get_animation_clip("imgs/test.ff/picture").status, AssetLookupStatus::NotFound);
}

TEST_F(SyntheticAssetPackage, LoadsCompleteSoundAndPreservesReferenceLookup) {
    add_sound("imgs/test.ff/hit", "Hit", "Imgs/Test.ff/Hit.json", "Imgs/Test.ff/Hit.wav");
    write_manifest();

    const AssetDatabase db = AssetDatabase::open(root_);
    EXPECT_EQ(db.find_sound_by_id("imgs/test.ff/hit").status, AssetLookupStatus::Found);
    EXPECT_EQ(db.find_sound("hit").status, AssetLookupStatus::Found);
    const auto result = db.get_sound_asset("imgs/test.ff/hit");
    ASSERT_EQ(result.status, AssetLookupStatus::Found);
    if (!result.value.has_value()) {
        FAIL() << "found sound asset has no value";
        return;
    }
    const auto& sound = result.value.value();
    EXPECT_EQ(sound.format, d2asset::SoundFormat::Wave);
    EXPECT_EQ(sound.payload_path, fs::path("Imgs/Test.ff/Hit.wav"));
    EXPECT_EQ(sound.payload_size, 5U);
    EXPECT_EQ(sound.format_tag, 85U);
    EXPECT_EQ(sound.channels, 1U);
    EXPECT_EQ(sound.sample_rate, 44100U);
    EXPECT_FALSE(sound.bit_depth.has_value());
    EXPECT_FALSE(sound.duration_ms.has_value());
}

TEST_F(SyntheticAssetPackage, LoadsUnknownSoundFormatWithWarning) {
    add_sound("imgs/test.ff/mystery", "Mystery", "Imgs/Test.ff/Mystery.json",
              "Imgs/Test.ff/Mystery.bin", "unknown");
    write_manifest();

    const AssetDatabase db = AssetDatabase::open(root_);
    const auto          result = db.get_sound_asset("imgs/test.ff/mystery");
    if (!result.value.has_value()) {
        FAIL() << "found sound asset has no value";
        return;
    }
    const auto& sound = result.value.value();
    EXPECT_EQ(sound.format, d2asset::SoundFormat::Unknown);
    EXPECT_FALSE(sound.warnings.empty());
    EXPECT_EQ(db.sound_diagnostics().size(), sound.warnings.size());
}

TEST_F(SyntheticAssetPackage, SoundLookupReturnsNotFoundForMissingAndWrongType) {
    add_image("imgs/test.ff/picture", "Picture", "Imgs/Test.ff/Picture.json");
    write_manifest();

    const AssetDatabase db = AssetDatabase::open(root_);
    EXPECT_EQ(db.get_sound_asset("imgs/test.ff/missing").status, AssetLookupStatus::NotFound);
    EXPECT_EQ(db.get_sound_asset("imgs/test.ff/picture").status, AssetLookupStatus::NotFound);
}

TEST_F(SyntheticAssetPackage, RejectsMissingAndUnsupportedSoundSchema) {
    add_sound("imgs/test.ff/hit", "Hit", "Imgs/Test.ff/Hit.json", "Imgs/Test.ff/Hit.wav");
    nlohmann::json sidecar;
    {
        std::ifstream input(root_ / "Imgs/Test.ff/Hit.json");
        input >> sidecar;
    }
    sidecar.erase("sound_schema_version");
    write_json("Imgs/Test.ff/Hit.json", sidecar);
    write_manifest();
    expect_error(AssetErrorCode::MalformedSound, [this] { (void)AssetDatabase::open(root_); });

    sidecar["sound_schema_version"] = 2;
    write_json("Imgs/Test.ff/Hit.json", sidecar);
    expect_error(AssetErrorCode::UnsupportedSoundSchema,
                 [this] { (void)AssetDatabase::open(root_); });
}

TEST_F(SyntheticAssetPackage, RejectsSoundIdentityAndMalformedNumericFields) {
    add_sound("imgs/test.ff/hit", "Hit", "Imgs/Test.ff/Hit.json", "Imgs/Test.ff/Hit.wav");
    nlohmann::json sidecar;
    {
        std::ifstream input(root_ / "Imgs/Test.ff/Hit.json");
        input >> sidecar;
    }
    sidecar["asset_id"] = "wrong";
    write_json("Imgs/Test.ff/Hit.json", sidecar);
    write_manifest();
    expect_error(AssetErrorCode::MalformedSound, [this] { (void)AssetDatabase::open(root_); });

    sidecar["asset_id"] = "imgs/test.ff/hit";
    sidecar["channels"] = -1;
    write_json("Imgs/Test.ff/Hit.json", sidecar);
    expect_error(AssetErrorCode::MalformedSound, [this] { (void)AssetDatabase::open(root_); });
}

TEST_F(SyntheticAssetPackage, RejectsUnsafeMissingAndMismatchedSoundPayload) {
    add_sound("imgs/test.ff/hit", "Hit", "Imgs/Test.ff/Hit.json", "Imgs/Test.ff/Hit.wav");
    nlohmann::json sidecar;
    {
        std::ifstream input(root_ / "Imgs/Test.ff/Hit.json");
        input >> sidecar;
    }
    sidecar["payload_path"] = "../Hit.wav";
    write_json("Imgs/Test.ff/Hit.json", sidecar);
    write_manifest();
    expect_error(AssetErrorCode::UnsafePath, [this] { (void)AssetDatabase::open(root_); });

    sidecar["payload_path"] = "Imgs/Test.ff/Missing.wav";
    write_json("Imgs/Test.ff/Hit.json", sidecar);
    expect_error(AssetErrorCode::MissingSoundPayload, [this] { (void)AssetDatabase::open(root_); });

    sidecar["payload_path"] = "Imgs/Test.ff/Hit.wav";
    sidecar["payload_size"] = 6;
    write_json("Imgs/Test.ff/Hit.json", sidecar);
    expect_error(AssetErrorCode::SoundPayloadSizeMismatch,
                 [this] { (void)AssetDatabase::open(root_); });
}

TEST_F(SyntheticAssetPackage, SoundPackageRemainsPortableAfterRelocation) {
    add_sound("imgs/test.ff/hit", "Hit", "Imgs/Test.ff/Hit.json", "Imgs/Test.ff/Hit.wav");
    write_manifest();
    const fs::path relocated = root_.parent_path() / (root_.filename().string() + "_relocated");
    fs::rename(root_, relocated);
    root_ = relocated;
    EXPECT_EQ(AssetDatabase::open(root_).get_sound_asset("imgs/test.ff/hit").status,
              AssetLookupStatus::Found);
}

} // namespace
