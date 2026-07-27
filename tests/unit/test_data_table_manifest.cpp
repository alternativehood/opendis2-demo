#include "d2asset/asset_database.hpp"
#include "d2asset/asset_error.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "tests/test_process.hpp"

namespace fs = std::filesystem;

namespace {

class DataTableManifestTest : public ::testing::Test {
protected:
    void SetUp() override {
        static std::size_t sequence = 0;
        root_ = fs::temp_directory_path() /
                ("d2_data_table_" + std::to_string(test_support::process_id()) + "_" +
                 std::to_string(++sequence));
        fs::create_directories(root_ / "data/Globals/Gunits.dbf");
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(root_, ec);
    }

    void write_package(const nlohmann::json& sidecar) {
        std::ofstream(root_ / "data/Globals/Gunits.dbf/Gunits.json") << sidecar.dump(2) << '\n';
        const nlohmann::json manifest{
            {"asset_schema_version", 1},
            {"containers",
             nlohmann::json::array({{{"container_id", "globals/gunits.dbf"},
                                     {"path", "Globals/Gunits.dbf"},
                                     {"content_kinds", nlohmann::json::array({"data"})}}})},
            {"assets", nlohmann::json::array({{{"asset_id", "globals/gunits.dbf/gunits"},
                                               {"logical_name", "Gunits"},
                                               {"type", "data_table"},
                                               {"container_id", "globals/gunits.dbf"},
                                               {"path", "data/Globals/Gunits.dbf/Gunits.json"}}})},
            {"warnings", nlohmann::json::array()}};
        std::ofstream(root_ / "game_manifest.json") << manifest.dump(2) << '\n';
    }

    static nlohmann::json complete_sidecar() {
        return {
            {"data_table_schema_version", 1},
            {"asset_id", "globals/gunits.dbf/gunits"},
            {"logical_name", "Gunits"},
            {"container_id", "globals/gunits.dbf"},
            {"kind", "dbf"},
            {"columns", nlohmann::json::array({{{"name", "UNIT_ID"},
                                                {"source_type", "C"},
                                                {"width", 10},
                                                {"decimal_count", 0},
                                                {"extensions", nullptr}},
                                               {{"name", "LEVEL"},
                                                {"source_type", "N"},
                                                {"width", 2},
                                                {"decimal_count", 0},
                                                {"extensions", nullptr}},
                                               {{"name", "ACTIVE"},
                                                {"source_type", nullptr},
                                                {"width", nullptr},
                                                {"decimal_count", nullptr},
                                                {"extensions", nullptr}},
                                               {{"name", "META"},
                                                {"source_type", nullptr},
                                                {"width", nullptr},
                                                {"decimal_count", nullptr},
                                                {"extensions", nullptr}}})},
            {"rows", nlohmann::json::array(
                         {{{"row_key", "00000000"},
                           {"values", nlohmann::json::array(
                                          {{{"name", "UNIT_ID"}, {"value", "G000UN0001"}},
                                           {{"name", "LEVEL"}, {"value", "12"}},
                                           {{"name", "ACTIVE"}, {"value", "true"}},
                                           {{"name", "META"},
                                            {"value",
                                             {{"tags", nlohmann::json::array({"unit", 7, true})},
                                              {"unknown", nullptr}}}}})}}})},
            {"warnings", nlohmann::json::array({"source warning"})},
            {"extensions", {{"source", "Gunits.dbf"}}},
        };
    }

    fs::path root_;
};

TEST_F(DataTableManifestTest, LoadsValuesAndProvidesStructuredAccess) {
    write_package(complete_sidecar());
    const d2asset::AssetDatabase database = d2asset::AssetDatabase::open(root_);
    const auto table_result = database.get_data_table("globals/gunits.dbf/gunits");
    ASSERT_EQ(table_result.status, d2asset::AssetLookupStatus::Found);
    ASSERT_TRUE(table_result.value.has_value());
    if (!table_result.value.has_value()) {
        FAIL() << "data table missing";
        return;
    }
    const d2asset::DataTable& table = table_result.value.value();
    EXPECT_EQ(table.kind, d2asset::DataTableKind::Dbf);
    ASSERT_EQ(table.columns.size(), 4U);
    EXPECT_EQ(table.columns[0].name, "UNIT_ID");
    EXPECT_EQ(table.columns[0].source_type, "C");
    EXPECT_EQ(table.rows.size(), 1U);
    EXPECT_EQ(table.warnings.size(), 1U);

    EXPECT_EQ(table.string_value("00000000", "UNIT_ID").value, "G000UN0001");
    EXPECT_EQ(table.integer_value("00000000", "LEVEL").value, 12);
    EXPECT_EQ(table.floating_value("00000000", "LEVEL").value, 12.0);
    EXPECT_EQ(table.boolean_value("00000000", "ACTIVE").value, true);
    const auto meta = table.find_value("00000000", "META");
    ASSERT_TRUE(meta.value.has_value());
    if (!meta.value.has_value()) {
        FAIL() << "metadata value missing";
        return;
    }
    EXPECT_EQ(meta.value.value()->kind(), d2asset::DataValueKind::Object);

    const auto missing_row = table.find_row("missing");
    ASSERT_TRUE(missing_row.error.has_value());
    EXPECT_EQ(missing_row.error.value_or(d2asset::DataAccessError{}).code,
              d2asset::DataAccessErrorCode::RowNotFound);
    const auto missing_column = table.find_value("00000000", "missing");
    ASSERT_TRUE(missing_column.error.has_value());
    EXPECT_EQ(missing_column.error.value_or(d2asset::DataAccessError{}).column_name, "missing");
    const auto bad_conversion = table.integer_value("00000000", "UNIT_ID");
    ASSERT_TRUE(bad_conversion.error.has_value());
    EXPECT_EQ(bad_conversion.error.value_or(d2asset::DataAccessError{}).code,
              d2asset::DataAccessErrorCode::TypeConversion);
}

TEST_F(DataTableManifestTest, RejectsUnsupportedSchemaIdentityAndDuplicates) {
    auto sidecar = complete_sidecar();
    sidecar.erase("data_table_schema_version");
    write_package(sidecar);
    try {
        (void)d2asset::AssetDatabase::open(root_);
        FAIL() << "expected missing schema error";
    } catch (const d2asset::AssetError& error) {
        EXPECT_EQ(error.code(), d2asset::AssetErrorCode::MalformedDataTable);
    }

    sidecar = complete_sidecar();
    sidecar["data_table_schema_version"] = 2;
    write_package(sidecar);
    try {
        (void)d2asset::AssetDatabase::open(root_);
        FAIL() << "expected unsupported schema error";
    } catch (const d2asset::AssetError& error) {
        EXPECT_EQ(error.code(), d2asset::AssetErrorCode::UnsupportedDataTableSchema);
    }

    sidecar = complete_sidecar();
    sidecar["logical_name"] = "Other";
    write_package(sidecar);
    try {
        (void)d2asset::AssetDatabase::open(root_);
        FAIL() << "expected identity error";
    } catch (const d2asset::AssetError& error) {
        EXPECT_EQ(error.code(), d2asset::AssetErrorCode::MalformedDataTable);
    }

    sidecar = complete_sidecar();
    sidecar["columns"].push_back(sidecar["columns"][0]);
    write_package(sidecar);
    try {
        (void)d2asset::AssetDatabase::open(root_);
        FAIL() << "expected duplicate column error";
    } catch (const d2asset::AssetError& error) {
        EXPECT_EQ(error.code(), d2asset::AssetErrorCode::MalformedDataTable);
    }

    sidecar = complete_sidecar();
    sidecar["rows"].push_back(sidecar["rows"][0]);
    write_package(sidecar);
    try {
        (void)d2asset::AssetDatabase::open(root_);
        FAIL() << "expected duplicate row error";
    } catch (const d2asset::AssetError& error) {
        EXPECT_EQ(error.code(), d2asset::AssetErrorCode::DuplicateDataRow);
    }

    sidecar = complete_sidecar();
    sidecar["rows"][0]["values"][1]["value"] = std::numeric_limits<std::uint64_t>::max();
    write_package(sidecar);
    try {
        (void)d2asset::AssetDatabase::open(root_);
        FAIL() << "expected malformed value error";
    } catch (const d2asset::AssetError& error) {
        EXPECT_EQ(error.code(), d2asset::AssetErrorCode::MalformedDataValue);
    }
}

TEST(DataTableEnums, StableStrings) {
    EXPECT_STREQ(d2asset::to_string(d2asset::DataTableKind::Dat), "dat");
    EXPECT_STREQ(d2asset::to_string(d2asset::DataValueKind::FloatingPoint), "floating_point");
    EXPECT_STREQ(d2asset::to_string(d2asset::DataAccessErrorCode::ColumnNotFound),
                 "column_not_found");
}

} // namespace
