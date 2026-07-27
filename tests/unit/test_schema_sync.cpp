#include "d2asset/schemas.hpp"
#include "d2engine/app/config_schemas.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

namespace fs = std::filesystem;

namespace {

std::string read_file(const fs::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return {};
    }
    std::stringstream buf;
    buf << file.rdbuf();
    return buf.str();
}

} // namespace

TEST(SchemaSync, AtlasManifestMatchesFile) {
    const fs::path schema_path =
        fs::path(OPENDIS2_SOURCE_DIR) / "schemas" / "atlas_manifest.schema.json";
    const std::string file_content = read_file(schema_path);
    ASSERT_FALSE(file_content.empty()) << "Cannot read " << schema_path;
    EXPECT_EQ(file_content, d2asset::schemas::atlas_manifest());
}

TEST(SchemaSync, RuntimeAssetManifestMatchesFile) {
    const fs::path schema_path =
        fs::path(OPENDIS2_SOURCE_DIR) / "schemas" / "runtime_asset_manifest.schema.json";
    const std::string file_content = read_file(schema_path);
    ASSERT_FALSE(file_content.empty()) << "Cannot read " << schema_path;
    EXPECT_EQ(file_content, d2asset::schemas::runtime_asset_manifest());
}

TEST(SchemaSync, BattleScreenConfigMatchesFile) {
    const fs::path schema_path =
        fs::path(OPENDIS2_SOURCE_DIR) / "schemas" / "battle_screen.schema.json";
    const std::string file_content = read_file(schema_path);
    ASSERT_FALSE(file_content.empty()) << "Cannot read " << schema_path;
    EXPECT_EQ(file_content, d2engine::schemas::battle_screen());
}
