#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace d2gamedata {

struct DbfValueRef {
    std::string           id;
    std::string           table_name;
    std::filesystem::path file_path;
    size_t                row_index = 0;
    std::string           field_name;

    std::map<std::string, std::string> row_fields;

    std::string name_txt_id;
    std::string desc_txt_id;
    std::string category_kind;
};

struct DbfScanReport {
    int dbf_files_loaded = 0;
    int dbf_files_failed = 0;
    int dbf_rows_scanned = 0;
    int dbf_field_values_scanned = 0;
    int tables_with_global_ids = 0;
    int indexed_global_values = 0;
    int unique_global_ids = 0;
    int text_rows_loaded = 0;

    std::vector<std::string> tables_with_gids;
    std::vector<std::string> errors;
};

class DbfGameDataIndex {
public:
    DbfGameDataIndex() = default;
    DbfGameDataIndex(const DbfGameDataIndex&) = default;
    DbfGameDataIndex(DbfGameDataIndex&&) = default;
    DbfGameDataIndex& operator=(const DbfGameDataIndex&) = default;
    DbfGameDataIndex& operator=(DbfGameDataIndex&&) = default;
    ~DbfGameDataIndex() = default;

    void load_directory(const std::string& dir_path);
    void load_dbf_file(const std::string& file_path);

    const std::vector<DbfValueRef>& find_global_id(std::string_view id) const;

    std::string resolve_text(const std::string& text_id) const;
    int         text_count() const;

    DbfScanReport scan_report() const;

private:
    std::map<std::string, std::vector<DbfValueRef>> global_id_index_;
    std::map<std::string, std::string>              text_map_;
    std::vector<std::string>                        scan_errors_;
    std::set<std::string>                           loaded_dbf_files_;

    int dbf_files_loaded_ = 0;
    int dbf_files_failed_ = 0;
    int dbf_rows_scanned_ = 0;
    int dbf_field_values_scanned_ = 0;
    int text_rows_loaded_ = 0;

    int index_row(const std::string& table_name, const std::filesystem::path& file_path,
                  const std::vector<std::pair<std::string, std::string>>& fields, int row_idx);
};

} // namespace d2gamedata
