#include "cli/runtime_package.hpp"
#include "d2asset/asset_database.hpp"
#include "d2res/dat_parser.hpp"
#include "d2res/dbf_reader.hpp"
#include "d2res/dlg_parser.hpp"
#include "tests/test_helpers.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

namespace {

std::string read_text(const fs::path& path) {
    std::ifstream input(path);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

TEST(RuntimeDataTables, LoadsScopedRealDbfDatAndDlgPackage) {
    const fs::path game_root = DISCIPLES2_GAME_ROOT;
    const fs::path dbf_path = game_root / "Globals/Gunits.dbf";
    const fs::path dat_path = game_root / "gameinfo.dat";
    const fs::path dlg_path = game_root / "Interf/Interf.dlg";
    if (!fs::is_regular_file(dbf_path) || !fs::is_regular_file(dat_path) ||
        !fs::is_regular_file(dlg_path)) {
        GTEST_SKIP() << "scoped DBF/DAT/DLG fixtures are unavailable";
    }

    TempDir         base("rt_data_tables");
    const fs::path  root = base.path();
    const fs::path  work = root / "work";
    const fs::path  package = root / "package";
    std::error_code ec;
    fs::create_directories(work);
    for (const std::string_view directory :
         {"images", "animations", "atlases", "sounds", "data", "reports"}) {
        fs::create_directories(package / directory);
    }

    std::ifstream             dbf_input(dbf_path, std::ios::binary);
    std::vector<std::uint8_t> dbf_bytes{std::istreambuf_iterator<char>(dbf_input),
                                        std::istreambuf_iterator<char>()};
    const d2res::DbfReader    dbf(dbf_bytes);
    std::ofstream(work / "Gunits.schema.json") << dbf.to_schema_json("Gunits.dbf").dump(2) << '\n';
    std::ofstream(work / "Gunits.records.json") << dbf.to_records_json().dump(2) << '\n';

    d2res::DatParser dat;
    dat.parse(read_text(dat_path));
    std::ofstream(work / "gameinfo.json") << dat.to_json().dump(2) << '\n';

    d2res::DlgParser dlg;
    dlg.parse(read_text(dlg_path));
    std::ofstream(work / "Interf.json") << dlg.to_json().dump(2) << '\n';

    const fs::path dbf_sidecar = runtime_package_detail::package_extracted_dbf(
        work / "Gunits.schema.json", work / "Gunits.records.json", "Gunits", "Globals/Gunits.dbf",
        "globals/gunits.dbf/gunits", package);
    const fs::path dat_sidecar = runtime_package_detail::package_extracted_dat(
        work / "gameinfo.json", "gameinfo", "gameinfo.dat", "gameinfo.dat/gameinfo", package);
    const fs::path dlg_sidecar = runtime_package_detail::package_extracted_dlg(
        work / "Interf.json", "Interf", "Interf/Interf.dlg", "interf/interf.dlg/interf", package);

    const nlohmann::json manifest{
        {"asset_schema_version", 1},
        {"containers",
         nlohmann::json::array({{{"container_id", "globals/gunits.dbf"},
                                 {"path", "Globals/Gunits.dbf"},
                                 {"content_kinds", nlohmann::json::array({"data"})}},
                                {{"container_id", "gameinfo.dat"},
                                 {"path", "gameinfo.dat"},
                                 {"content_kinds", nlohmann::json::array({"data"})}},
                                {{"container_id", "interf/interf.dlg"},
                                 {"path", "Interf/Interf.dlg"},
                                 {"content_kinds", nlohmann::json::array({"data"})}}})},
        {"assets", nlohmann::json::array({{{"asset_id", "globals/gunits.dbf/gunits"},
                                           {"logical_name", "Gunits"},
                                           {"type", "data_table"},
                                           {"container_id", "globals/gunits.dbf"},
                                           {"path", dbf_sidecar.generic_string()}},
                                          {{"asset_id", "gameinfo.dat/gameinfo"},
                                           {"logical_name", "gameinfo"},
                                           {"type", "data_table"},
                                           {"container_id", "gameinfo.dat"},
                                           {"path", dat_sidecar.generic_string()}},
                                          {{"asset_id", "interf/interf.dlg/interf"},
                                           {"logical_name", "Interf"},
                                           {"type", "data_table"},
                                           {"container_id", "interf/interf.dlg"},
                                           {"path", dlg_sidecar.generic_string()}}})},
        {"warnings", nlohmann::json::array()}};
    std::ofstream(package / "game_manifest.json") << manifest.dump(2) << '\n';
    fs::remove_all(work, ec);

    const d2asset::AssetDatabase database = d2asset::AssetDatabase::open(package);
    const auto                   dbf_table = database.get_data_table("globals/gunits.dbf/gunits");
    const auto                   dat_table = database.get_data_table("gameinfo.dat/gameinfo");
    const auto                   dlg_table = database.get_data_table("interf/interf.dlg/interf");
    ASSERT_TRUE(dbf_table.value.has_value());
    ASSERT_TRUE(dat_table.value.has_value());
    ASSERT_TRUE(dlg_table.value.has_value());
    const auto& loaded_dbf = dbf_table.value.value();
    const auto& loaded_dat = dat_table.value.value();
    const auto& loaded_dlg = dlg_table.value.value();
    EXPECT_EQ(loaded_dbf.kind, d2asset::DataTableKind::Dbf);
    EXPECT_FALSE(loaded_dbf.columns.empty());
    EXPECT_FALSE(loaded_dbf.rows.empty());
    EXPECT_EQ(loaded_dat.kind, d2asset::DataTableKind::Dat);
    EXPECT_FALSE(loaded_dat.find_row("FullName").error.has_value());
    EXPECT_EQ(loaded_dlg.kind, d2asset::DataTableKind::Dlg);
    EXPECT_FALSE(loaded_dlg.rows.empty());
    EXPECT_TRUE(loaded_dbf.sidecar_path.is_relative());
    EXPECT_TRUE(loaded_dat.sidecar_path.is_relative());
    EXPECT_TRUE(loaded_dlg.sidecar_path.is_relative());
}

} // namespace
