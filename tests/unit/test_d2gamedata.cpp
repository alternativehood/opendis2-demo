#include <gtest/gtest.h>

#include "d2gamedata/DbfGameDataIndex.hpp"
#include "d2gamedata/GlobalIdResolver.hpp"
#include "d2gamedata/RefTypes.hpp"
#include "d2gamedata/SourceKind.hpp"
#include "tests/test_helpers.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>
#include <vector>

namespace d2gamedata {

namespace {

void write_test_dbf(const std::filesystem::path&                            path,
                    const std::vector<std::pair<std::string, std::size_t>>& fields,
                    const std::vector<std::vector<std::string>>&            rows) {
    auto const n = fields.size();
    uint16_t   header_size = static_cast<uint16_t>(32 + n * 32 + 1);
    if (header_size % 2 != 0) {
        ++header_size;
    }
    uint16_t record_size = 1;
    for (const auto& [name, len] : fields) {
        record_size = static_cast<uint16_t>(record_size + static_cast<uint16_t>(len));
    }

    std::vector<uint8_t> buf(static_cast<std::size_t>(header_size) +
                                 rows.size() * static_cast<std::size_t>(record_size),
                             0x20);
    buf[0] = 0x03;
    auto record_count = static_cast<uint32_t>(rows.size());
    std::memcpy(buf.data() + 4, &record_count, 4);
    std::memcpy(buf.data() + 8, &header_size, 2);
    std::memcpy(buf.data() + 10, &record_size, 2);

    std::size_t off = 32;
    for (std::size_t i = 0; i < n; ++i) {
        std::memset(buf.data() + off, 0, 11);
        std::memcpy(buf.data() + off, fields[i].first.c_str(),
                    std::min(fields[i].first.size(), static_cast<std::size_t>(10)));
        buf[off + 11] = 'C';
        buf[off + 16] = static_cast<uint8_t>(fields[i].second);
        buf[off + 17] = 0;
        off += 32;
    }
    buf[32 + n * 32] = 0x0D;

    off = header_size;
    for (const auto& row : rows) {
        buf[off] = 0x20;
        std::size_t field_off = off + 1;
        for (std::size_t i = 0; i < n; ++i) {
            std::string value = row[i];
            if (value.size() > fields[i].second) {
                value = value.substr(0, fields[i].second);
            } else {
                value.append(fields[i].second - value.size(), ' ');
            }
            std::memcpy(buf.data() + field_off, value.data(), fields[i].second);
            field_off += fields[i].second;
        }
        off += record_size;
    }

    std::ofstream ofs(path, std::ios::binary);
    ofs.write(reinterpret_cast<const char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
}

void write_one_row_test_dbf(const std::filesystem::path&                            path,
                            const std::vector<std::pair<std::string, std::size_t>>& fields,
                            const std::vector<std::string>&                         row) {
    write_test_dbf(path, fields, std::vector<std::vector<std::string>>{row});
}

} // namespace

// ── DbfValueRef provenance tests via real DBF files ───────────────────────

TEST(D2GamedataTest, IndexRowPreservesFullProvenanceAndTextEvidence) {
    TempDir tmp{"d2gamedata_provenance"};
    auto    dbf_path = tmp.path() / "UnitData.dbf";
    write_one_row_test_dbf(
        dbf_path,
        {{"UNIT_ID", 10}, {"NAME_TXT", 10}, {"DESC_TXT", 10}, {"VALUE", 2}, {"OWNER", 10}},
        {"G000UU0001", "T_SOLDIER", "D_SOLDIER", "42", "G000RR0000"});

    DbfGameDataIndex reg;
    reg.load_dbf_file(dbf_path.string());

    // table_name
    auto refs = reg.find_global_id("G000UU0001");
    ASSERT_FALSE(refs.empty());
    EXPECT_EQ(refs[0].table_name, "UnitData.dbf");
    // file_path
    EXPECT_EQ(refs[0].file_path, dbf_path);
    // row_index
    EXPECT_EQ(refs[0].row_index, 0u);
    // field_name (for the G-value that was matched)
    auto owner_refs = reg.find_global_id("G000RR0000");
    ASSERT_FALSE(owner_refs.empty());
    EXPECT_EQ(owner_refs[0].field_name, "OWNER");
    // row_fields — all fields indexed
    EXPECT_EQ(refs[0].row_fields.at("NAME_TXT"), "T_SOLDIER");
    EXPECT_EQ(refs[0].row_fields.at("DESC_TXT"), "D_SOLDIER");
    EXPECT_EQ(refs[0].row_fields.at("VALUE"), "42");
    // name_txt_id
    EXPECT_EQ(refs[0].name_txt_id, "T_SOLDIER");
    // desc_txt_id
    EXPECT_EQ(refs[0].desc_txt_id, "D_SOLDIER");
    // Every G-value in every field is indexed; non-G values are ignored
    EXPECT_FALSE(reg.find_global_id("G000UU0001").empty());
    EXPECT_FALSE(reg.find_global_id("G000RR0000").empty());
    EXPECT_TRUE(reg.find_global_id("42").empty());
    EXPECT_TRUE(reg.find_global_id("T_SOLDIER").empty());
}

TEST(D2GamedataTest, MultipleRowsSameId) {
    TempDir tmp{"d2gamedata_multi_id"};
    write_one_row_test_dbf(tmp.path() / "A.dbf", {{"ID", 10}}, {"G000SS0001"});
    write_one_row_test_dbf(tmp.path() / "B.dbf", {{"REF", 10}}, {"G000SS0001"});

    DbfGameDataIndex reg;
    reg.load_directory(tmp.str());
    auto refs = reg.find_global_id("G000SS0001");
    ASSERT_EQ(refs.size(), 2u);
    std::set<std::string> table_names;
    for (const auto& ref : refs) {
        table_names.insert(ref.table_name);
    }
    EXPECT_EQ(table_names, (std::set<std::string>{"A.dbf", "B.dbf"}));
}

TEST(D2GamedataTest, ScanReportCountsIdsLoadedFromFiles) {
    TempDir tmp{"d2gamedata_report_ids"};
    write_one_row_test_dbf(tmp.path() / "A.dbf", {{"ID", 10}}, {"G000AA0001"});
    write_one_row_test_dbf(tmp.path() / "B.dbf", {{"ID", 10}}, {"G000BB0002"});

    DbfGameDataIndex reg;
    reg.load_directory(tmp.str());
    auto rpt = reg.scan_report();
    EXPECT_FALSE(reg.find_global_id("G000AA0001").empty());
    EXPECT_FALSE(reg.find_global_id("G000BB0002").empty());
    EXPECT_EQ(rpt.unique_global_ids, 2);
}

TEST(D2GamedataTest, ScanReportCountsUniqueIds) {
    TempDir tmp{"d2gamedata_unique_ids"};
    write_test_dbf(tmp.path() / "A.dbf", {{"ID", 10}}, {{"G000AA0001"}, {"G000AA0002"}});
    write_one_row_test_dbf(tmp.path() / "B.dbf", {{"ID", 10}}, {"G000BB0001"});

    DbfGameDataIndex reg;
    reg.load_directory(tmp.str());
    auto rpt = reg.scan_report();
    EXPECT_EQ(rpt.unique_global_ids, 3);
}

TEST(D2GamedataTest, FindNonExistentIdReturnsEmpty) {
    TempDir tmp{"d2gamedata_missing_id"};
    write_one_row_test_dbf(tmp.path() / "A.dbf", {{"ID", 10}}, {"G000AA0001"});

    DbfGameDataIndex reg;
    reg.load_directory(tmp.str());
    EXPECT_TRUE(reg.find_global_id("G000ZZ9999").empty());
}

TEST(D2GamedataTest, LoadNonexistentDirectoryDoesNotCrash) {
    DbfGameDataIndex reg;
    reg.load_directory("/tmp/does_not_exist_xyzzy");
    auto rpt = reg.scan_report();
    EXPECT_EQ(rpt.unique_global_ids, 0);
}

TEST(D2GamedataTest, LoadEmptyDirectoryDoesNotCrash) {
    TempDir          tmp{"d2gamedata_empty"};
    DbfGameDataIndex reg;
    reg.load_directory(tmp.str());
    auto rpt = reg.scan_report();
    EXPECT_EQ(rpt.unique_global_ids, 0);
}

TEST(D2GamedataTest, LoadTglobalTextAndResolve) {
    TempDir tmp{"d2gamedata_text"};
    write_one_row_test_dbf(tmp.path() / "Tglobal.dbf", {{"STR_ID", 10}, {"TEXT", 20}},
                           {"T_HELLO", "Hello World"});

    DbfGameDataIndex reg;
    reg.load_directory(tmp.str());
    EXPECT_EQ(reg.resolve_text("T_HELLO"), "Hello World");
    EXPECT_EQ(reg.resolve_text("NONEXIST"), "");
}

// ==========================================================================
// GlobalIdResolver tests — using new typed API
// ==========================================================================

TEST(D2ResolverTest, NullIdReturnsNullReference) {
    GlobalIdResolver resolver;
    auto             result = resolver.resolve(GlobalId{"G000000000"});
    EXPECT_EQ(result.status, ResolutionStatus::NullRef);
    EXPECT_EQ(result.source_kind, SourceKind::Null);
    EXPECT_EQ(result.category_kind, "null_reference");
}

TEST(D2ResolverTest, EmptyRegistryReturnsUnresolved) {
    GlobalIdResolver resolver;
    auto             result = resolver.resolve(GlobalId{"G000MG0027"});
    EXPECT_EQ(result.status, ResolutionStatus::Unresolved);
    EXPECT_EQ(result.source_kind, SourceKind::Unknown);
    EXPECT_FALSE(result.unresolved_reason.empty());
    EXPECT_NE(result.unresolved_reason.find("No game data loaded"), std::string::npos);
}

TEST(D2ResolverTest, NoFalsePositivesAcrossCalls) {
    GlobalIdResolver resolver;
    EXPECT_EQ(resolver.resolve(GlobalId{"G000MG0027"}).status, ResolutionStatus::Unresolved);
    EXPECT_EQ(resolver.resolve(GlobalId{"G001MG0027"}).status, ResolutionStatus::Unresolved);
}

// ==========================================================================
// Full end-to-end: seeded DbfGameDataIndex with text resolution
// ==========================================================================

class D2FullResolverTest : public ::testing::Test {
protected:
    void SetUp() override {
        write_one_row_test_dbf(tmp_.path() / "UnitData.dbf", {{"UNIT_ID", 10}, {"NAME_TXT", 12}},
                               {"G000UU0042", "T_UNIT_NAME"});
        write_one_row_test_dbf(tmp_.path() / "AttackData.dbf",
                               {{"ATTACK_ID", 10}, {"DESC_TXT", 13}},
                               {"G000AT0001", "D_ATTACK_DESC"});
        write_one_row_test_dbf(tmp_.path() / "GlobalData.dbf", {{"LEADER", 10}, {"NAME_TXT", 8}},
                               {"G000MG0027", "T_LEADER"});
        write_test_dbf(tmp_.path() / "Tglobal.dbf", {{"STR_ID", 13}, {"TEXT", 20}},
                       {{"T_UNIT_NAME", "Imperial Soldier"},
                        {"D_ATTACK_DESC", "A powerful strike"},
                        {"T_LEADER", "Dark Lord"}});
        reg_.load_directory(tmp_.str());
    }

    TempDir          tmp_{"d2gamedata_full_resolver"};
    DbfGameDataIndex reg_;
};

TEST_F(D2FullResolverTest, SeededIndexPreservesFullEvidenceAndTextResolution) {
    auto refs = reg_.find_global_id("G000UU0042");
    ASSERT_FALSE(refs.empty());
    EXPECT_EQ(refs[0].table_name, "UnitData.dbf");
    EXPECT_EQ(refs[0].field_name, "UNIT_ID");
    EXPECT_EQ(refs[0].row_fields.at("NAME_TXT"), "T_UNIT_NAME");
    EXPECT_EQ(reg_.resolve_text("T_UNIT_NAME"), "Imperial Soldier");

    auto atk_refs = reg_.find_global_id("G000AT0001");
    ASSERT_FALSE(atk_refs.empty());
    EXPECT_EQ(atk_refs[0].table_name, "AttackData.dbf");
    EXPECT_EQ(atk_refs[0].field_name, "ATTACK_ID");
    EXPECT_EQ(reg_.resolve_text("D_ATTACK_DESC"), "A powerful strike");
}

TEST_F(D2FullResolverTest, MultipleMatchesPreserved) {
    write_one_row_test_dbf(tmp_.path() / "Extra.dbf", {{"REF", 10}}, {"G000UU0042"});
    reg_.load_dbf_file((tmp_.path() / "Extra.dbf").string());
    auto refs = reg_.find_global_id("G000UU0042");
    ASSERT_EQ(refs.size(), 2u);
    EXPECT_EQ(refs[0].table_name, "UnitData.dbf");
    EXPECT_EQ(refs[1].table_name, "Extra.dbf");
}

TEST_F(D2FullResolverTest, ScanReportCountsUniqueIds) {
    auto rpt = reg_.scan_report();
    EXPECT_EQ(rpt.unique_global_ids, 3);
    EXPECT_EQ(rpt.indexed_global_values, 3);
    EXPECT_EQ(rpt.tables_with_global_ids, 3);
}

TEST_F(D2FullResolverTest, G000MG0027FoundInGlobalData) {
    auto refs = reg_.find_global_id("G000MG0027");
    ASSERT_FALSE(refs.empty());
    EXPECT_EQ(refs[0].table_name, "GlobalData.dbf");
    EXPECT_EQ(refs[0].field_name, "LEADER");
    EXPECT_EQ(refs[0].name_txt_id, "T_LEADER");
    EXPECT_EQ(refs[0].row_fields.size(), 2u);
}

// ==========================================================================
// Duplicate loading prevention via real file paths
// ==========================================================================

TEST(D2GamedataTest, DuplicateFileLoadDoesNotDoubleCount) {
    TempDir tmp{"d2gamedata_dup"};

    auto dbf_path = tmp.path() / "TestGid.dbf";
    write_one_row_test_dbf(dbf_path, {{"GID", 10}}, {"G000AA0001"});
    ASSERT_TRUE(std::filesystem::exists(dbf_path));

    DbfGameDataIndex reg;

    reg.load_directory(tmp.str());
    auto rpt1 = reg.scan_report();
    EXPECT_EQ(rpt1.dbf_files_loaded, 1);
    EXPECT_EQ(rpt1.indexed_global_values, 1);
    EXPECT_EQ(rpt1.unique_global_ids, 1);
    EXPECT_FALSE(reg.find_global_id("G000AA0001").empty());

    auto canon = std::filesystem::canonical(dbf_path).string();
    reg.load_dbf_file(canon);
    auto rpt2 = reg.scan_report();
    EXPECT_EQ(rpt2.dbf_files_loaded, 1);
    EXPECT_EQ(rpt2.indexed_global_values, 1);

    reg.load_directory(tmp.str());
    auto rpt3 = reg.scan_report();
    EXPECT_EQ(rpt3.dbf_files_loaded, 1);
    EXPECT_EQ(rpt3.indexed_global_values, 1);
}

// ==========================================================================
// RefTypes tests
// ==========================================================================

TEST(RefTypesTest, GlobalIdNullCheck) {
    GlobalId id{"G000000000"};
    EXPECT_TRUE(id.is_null());

    GlobalId normal{"G000MG0027"};
    EXPECT_FALSE(normal.is_null());
}

TEST(RefTypesTest, GlobalIdPrefix) {
    GlobalId id{"G000UU0001"};
    EXPECT_EQ(id.prefix(), "G000UU0");
}

} // namespace d2gamedata
