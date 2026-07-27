#include "d2gamedata/DbfGameDataIndex.hpp"

#include <d2res/dbf_reader.hpp>
#include <d2log/log.hpp>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <fstream>
#include <numeric>
#include <regex>

namespace fs = std::filesystem;

namespace d2gamedata {

static auto kLog = d2log::get("gamedata");

static bool looks_like_global_id(const std::string& value) {
    if (value.size() != 10)
        return false;
    if (value[0] != 'G' && value[0] != 'g')
        return false;
    for (std::size_t i = 1; i < value.size(); ++i) {
        auto c = value[i];
        if ((c < '0' || c > '9') && (c < 'A' || c > 'Z') && (c < 'a' || c > 'z'))
            return false;
    }
    return true;
}

void DbfGameDataIndex::load_directory(const std::string& dir_path) {
    if (!fs::is_directory(dir_path))
        return;
    for (const auto& entry : fs::recursive_directory_iterator(dir_path)) {
        if (entry.path().extension() == ".dbf") {
            std::string resolved = fs::canonical(entry.path()).string();
            load_dbf_file(resolved);
        }
    }
}

void DbfGameDataIndex::load_dbf_file(const std::string& file_path) {
    // Persistent duplicate protection
    if (loaded_dbf_files_.count(file_path)) {
        return;
    }
    loaded_dbf_files_.insert(file_path);

    std::ifstream ifs(file_path, std::ios::binary);
    if (!ifs) {
        ++dbf_files_failed_;
        scan_errors_.push_back("Cannot open: " + file_path);
        return;
    }
    auto                 sz = static_cast<std::size_t>(fs::file_size(file_path));
    std::vector<uint8_t> buf(sz);
    if (!ifs.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(sz))) {
        ++dbf_files_failed_;
        scan_errors_.push_back("Cannot read: " + file_path);
        return;
    }

    try {
        d2res::DbfReader reader(buf);
        auto             records = reader.read_records();
        auto             fields = reader.list_fields();

        std::string filename = fs::path(file_path).filename().string();
        int         active_rows = 0;

        bool is_tglobal = (filename.find("Tglobal") != std::string::npos ||
                           filename.find("tglobal") != std::string::npos);

        int num_fields = static_cast<int>(fields.size());
        int row_idx = 0;
        for (const auto& rec : records) {
            std::vector<std::pair<std::string, std::string>> row_fields;
            for (const auto& field : fields) {
                auto it = rec.find(field.name);
                if (it != rec.end()) {
                    row_fields.emplace_back(field.name, it->second);
                }
            }

            index_row(filename, fs::path(file_path), row_fields, row_idx);

            if (is_tglobal) {
                auto txt_it = rec.find("TEXT");
                auto id_it = rec.find("STR_ID");
                if (id_it == rec.end())
                    id_it = rec.find("STRID");
                if (id_it == rec.end())
                    id_it = rec.find("ID");
                if (id_it != rec.end() && txt_it != rec.end()) {
                    std::string id_val = id_it->second;
                    std::string txt_val = txt_it->second;
                    if (!id_val.empty() && !txt_val.empty()) {
                        text_map_[id_val] = txt_val;
                    }
                }
                ++text_rows_loaded_;
            }
            ++row_idx;
            ++active_rows;
        }

        dbf_files_loaded_ += 1;
        dbf_rows_scanned_ += active_rows;
        dbf_field_values_scanned_ += active_rows * num_fields;

    } catch (const std::exception& e) {
        ++dbf_files_failed_;
        scan_errors_.push_back("Error scanning " + file_path + ": " + e.what());
        D2_LOG_WARN(kLog, "DbfGameDataIndex: error scanning {}: {}", file_path, e.what());
    }
}

int DbfGameDataIndex::index_row(const std::string& table_name, const fs::path& file_path,
                                const std::vector<std::pair<std::string, std::string>>& fields,
                                int                                                     row_idx) {
    int gid_count = 0;
    for (const auto& [field_name, value] : fields) {
        if (looks_like_global_id(value)) {
            std::string upper_id = value;
            for (auto& c : upper_id)
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

            DbfValueRef ref;
            ref.id = upper_id;
            ref.table_name = table_name;
            ref.file_path = file_path;
            ref.row_index = static_cast<size_t>(row_idx);
            ref.field_name = field_name;

            for (const auto& f : fields)
                ref.row_fields[f.first] = f.second;

            if (auto it = ref.row_fields.find("NAME_TXT"); it != ref.row_fields.end())
                ref.name_txt_id = it->second;
            if (auto it = ref.row_fields.find("DESC_TXT"); it != ref.row_fields.end())
                ref.desc_txt_id = it->second;

            global_id_index_[upper_id].push_back(std::move(ref));
            ++gid_count;
        }
    }
    return gid_count;
}

const std::vector<DbfValueRef>& DbfGameDataIndex::find_global_id(std::string_view id) const {
    static const std::vector<DbfValueRef> kEmpty;
    auto                                  it = global_id_index_.find(std::string(id));
    if (it != global_id_index_.end())
        return it->second;
    return kEmpty;
}

std::string DbfGameDataIndex::resolve_text(const std::string& text_id) const {
    auto it = text_map_.find(text_id);
    if (it != text_map_.end())
        return it->second;
    return {};
}

int DbfGameDataIndex::text_count() const {
    return static_cast<int>(text_map_.size());
}

DbfScanReport DbfGameDataIndex::scan_report() const {
    DbfScanReport report;
    report.dbf_files_loaded = dbf_files_loaded_;
    report.dbf_files_failed = dbf_files_failed_;
    report.dbf_rows_scanned = dbf_rows_scanned_;
    report.dbf_field_values_scanned = dbf_field_values_scanned_;
    report.text_rows_loaded = text_rows_loaded_;
    report.errors = scan_errors_;

    std::set<std::string> seen_tables;
    for (const auto& [id, refs] : global_id_index_) {
        for (const auto& ref : refs)
            seen_tables.insert(ref.table_name);
    }
    report.tables_with_global_ids = static_cast<int>(seen_tables.size());

    int indexed = 0;
    for (const auto& [id, refs] : global_id_index_)
        indexed += static_cast<int>(refs.size());
    report.indexed_global_values = indexed;
    report.unique_global_ids = static_cast<int>(global_id_index_.size());

    for (const auto& t : seen_tables)
        report.tables_with_gids.push_back(t);

    return report;
}

} // namespace d2gamedata
