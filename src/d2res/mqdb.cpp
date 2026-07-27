#include "mqdb.hpp"
#include "byte_reader.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace d2res {

static constexpr int32_t FF_FILES_RECORD_ID = 2;
static constexpr int32_t WDB_FILES_RECORD_ID = 1;

// ---- helpers -------------------------------------------------------------

std::string MqdbContainer::normalize(std::string_view s) {
    std::string r(s);
    for (char& c : r)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return r;
}

// ---- open ----------------------------------------------------------------

MqdbContainer MqdbContainer::open(const std::filesystem::path& path) {
    MappedFile mapped = MappedFile::open(path);
    if (!mapped)
        throw ParseError("cannot open: " + path.string(), 0);

    MqdbContainer c;
    c.path_ = path;
    c.parse(std::move(mapped));
    return c;
}

// ---- parse ---------------------------------------------------------------

void MqdbContainer::parse(MappedFile&& mapped) {
    file_ = std::move(mapped);
    ByteReader r{std::span<const uint8_t>(file_.data(), file_.size())};
    // Verify MQDB magic
    {
        auto magic = r.fixed_string(4);
        if (magic != "MQDB")
            throw ParseError("bad magic '" + magic + "', expected 'MQDB'", 0);
    }

    // Skip remaining 24 header bytes (unknown_0, record_count[unreliable],
    // unknown_1..3, checksum — stored opaquely, not currently used)
    r.skip(24);

    // Scan MQRC records sequentially; stop on non-MQRC magic or EOF
    std::size_t scan_index = 0;
    while (r.remaining() >= 4) {
        char peek[4];
        if (!r.peek4(peek))
            break;
        if (std::memcmp(peek, "MQRC", 4) != 0)
            break;

        r.skip(4); // MQRC magic
        r.skip(4); // unknown (always 0)

        Record rec;
        rec.id = r.i32le();
        rec.size = r.i32le();
        rec.realFileSize = r.i32le();
        rec.isNotDeleted = r.i32le();
        rec.recordMagic = r.i32le();
        rec.payloadOffset = r.pos();

        auto payload_sz = static_cast<std::size_t>(rec.realFileSize > 0 ? rec.realFileSize : 0);

        if (rec.payloadOffset > file_.size() || payload_sz > file_.size() - rec.payloadOffset) {
            warnings_.push_back("scan index " + std::to_string(scan_index) +
                                " (id=" + std::to_string(rec.id) +
                                "): payload extends beyond file; truncating");
            rec.realFileSize = static_cast<int32_t>(file_.size() - rec.payloadOffset);
            r.seek(file_.size());
        } else {
            r.skip(payload_sz);
        }

        if (rec.isNotDeleted == 0) {
            warnings_.push_back("skipped deleted record at scan index " +
                                std::to_string(scan_index) + " (id=" + std::to_string(rec.id) +
                                ")");
        } else {
            rec.index = records_.size();
            records_.push_back(rec);
        }

        ++scan_index;
    }

    // Build O(1) id → record index map (first live record for each id wins)
    for (std::size_t i = 0; i < records_.size(); ++i) {
        id_to_record_index_.try_emplace(records_[i].id, i);
    }

    // Locate name table record
    // .ff style: id == 2
    // .wdb style: id == 1 with payload starting "WDB\0"
    const Record* name_rec = nullptr;
    bool          is_wdb = false;

    for (const auto& rec : records_) {
        if (rec.id == FF_FILES_RECORD_ID) {
            name_rec = &rec;
            break;
        }
    }

    if (name_rec == nullptr) {
        for (const auto& rec : records_) {
            if (rec.id == WDB_FILES_RECORD_ID && rec.realFileSize >= 4) {
                const uint8_t* p = file_.data() + rec.payloadOffset;
                if (p[0] == 'W' && p[1] == 'D' && p[2] == 'B' && p[3] == '\0') {
                    name_rec = &rec;
                    is_wdb = true;
                    break;
                }
            }
        }
    }

    if (name_rec == nullptr) {
        warnings_.emplace_back(
            "no name table found (expected record id=2 for .ff or id=1/'WDB\\0' for .wdb)");
        return;
    }

    if (is_wdb) {
        parse_wdb_name_table(*name_rec);
    } else {
        parse_ff_name_table(*name_rec);
    }
}

// ---- name table parsers --------------------------------------------------

void MqdbContainer::parse_ff_name_table(const Record& table_rec) {
    ByteReader r{std::span<const uint8_t>(file_.data() + table_rec.payloadOffset,
                                          static_cast<std::size_t>(table_rec.realFileSize))};

    int32_t const count = r.i32le();
    if (count <= 0) {
        if (count < 0)
            warnings_.push_back("ff name table: negative filesCount=" + std::to_string(count));
        return;
    }

    // Build record-id → vector-index map for name assignment
    std::unordered_map<int32_t, std::size_t> id_to_idx;
    for (std::size_t i = 0; i < records_.size(); ++i)
        id_to_idx[records_[i].id] = i;

    for (int32_t i = 0; i < count; ++i) {
        if (r.remaining() < 260) {
            warnings_.push_back("ff name table: truncated at entry " + std::to_string(i));
            break;
        }
        std::string const name = r.fixed_string(256);
        int32_t const     id = r.i32le();

        auto it = id_to_idx.find(id);
        if (it == id_to_idx.end()) {
            warnings_.push_back("ff name table entry '" + name + "' references id=" +
                                std::to_string(id) + " with no matching MQRC record");
            continue;
        }
        std::size_t const idx = it->second;
        records_[idx].name = name;

        std::string const norm = normalize(name);
        if (!name_to_records_[norm].empty())
            warnings_.push_back("duplicate name in name table: '" + name + "'");
        name_to_records_[norm].push_back(idx);
    }
}

void MqdbContainer::parse_wdb_name_table(const Record& table_rec) {
    auto total = static_cast<std::size_t>(table_rec.realFileSize);
    if (total < 8)
        return;

    ByteReader r{std::span<const uint8_t>(file_.data() + table_rec.payloadOffset, total)};

    r.skip(8); // "WDB\0" magic + 4 unknown bytes

    // filesCount = realFileSize / 24 (integer division handles alignment)
    auto const count = static_cast<int32_t>((total - 8) / 24);

    std::unordered_map<int32_t, std::size_t> id_to_idx;
    for (std::size_t i = 0; i < records_.size(); ++i)
        id_to_idx[records_[i].id] = i;

    for (int32_t i = 0; i < count; ++i) {
        if (r.remaining() < 24) {
            warnings_.push_back("wdb name table: truncated at entry " + std::to_string(i));
            break;
        }
        int32_t const     id = r.i32le();
        std::string const name = r.fixed_string(20);

        auto it = id_to_idx.find(id);
        if (it == id_to_idx.end()) {
            warnings_.push_back("wdb name table entry '" + name + "' references id=" +
                                std::to_string(id) + " with no matching MQRC record");
            continue;
        }
        std::size_t const idx = it->second;
        records_[idx].name = name;

        std::string const norm = normalize(name);
        if (!name_to_records_[norm].empty())
            warnings_.push_back("duplicate name in wdb name table: '" + name + "'");
        name_to_records_[norm].push_back(idx);
    }
}

// ---- public API ----------------------------------------------------------

std::optional<Record> MqdbContainer::find_by_name(std::string_view name) const {
    auto it = name_to_records_.find(normalize(name));
    if (it == name_to_records_.end() || it->second.empty())
        return std::nullopt;
    return records_[it->second[0]];
}

std::optional<Record> MqdbContainer::find_by_id(int32_t id) const {
    auto it = id_to_record_index_.find(id);
    if (it == id_to_record_index_.end())
        return std::nullopt;
    return records_[it->second];
}

std::vector<std::string> MqdbContainer::names() const {
    std::vector<std::string> result;
    result.reserve(name_to_records_.size());
    for (const auto& [norm, indices] : name_to_records_) {
        if (!indices.empty())
            result.push_back(records_[indices[0]].name);
    }
    std::ranges::sort(result);
    return result;
}

Entry MqdbContainer::read_record(std::size_t index) const {
    if (index >= records_.size()) {
        throw std::out_of_range("record index " + std::to_string(index) +
                                " out of range (size=" + std::to_string(records_.size()) + ")");
    }
    const Record& rec = records_[index];
    Entry         e;
    e.record = rec;
    if (rec.realFileSize > 0) {
        const uint8_t* start = file_.data() + rec.payloadOffset;
        e.payload.assign(start, start + static_cast<std::size_t>(rec.realFileSize));
    }
    return e;
}

Entry MqdbContainer::read_name(std::string_view name) const {
    auto rec = find_by_name(name);
    if (!rec)
        throw std::runtime_error("name not found in container: " + std::string(name));
    return read_record(rec->index);
}

std::span<const uint8_t> MqdbContainer::payload_view(std::size_t index) const {
    if (index >= records_.size()) {
        throw std::out_of_range("record index " + std::to_string(index) +
                                " out of range (size=" + std::to_string(records_.size()) + ")");
    }
    const Record& rec = records_[index];
    if (rec.realFileSize <= 0)
        return {};
    const uint8_t* start = file_.data() + rec.payloadOffset;
    return std::span<const uint8_t>(start, static_cast<std::size_t>(rec.realFileSize));
}

} // namespace d2res
