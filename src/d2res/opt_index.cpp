#include "opt_index.hpp"
#include "byte_reader.hpp"
#include <cctype>

namespace d2res {

static std::string idx_upper(std::string_view s) {
    std::string result(s);
    for (char& c : result)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return result;
}

IndexMap IndexParser::parse(std::span<const uint8_t> data) {
    ByteReader r{data};
    IndexMap   map;

    const int32_t     frames_count = r.i32le();
    const std::size_t count = static_cast<std::size_t>(frames_count);
    map.entries.reserve(count);
    map.name_to_index.reserve(count);
    map.image_name_to_index.reserve(count);
    map.id_to_indices.reserve(count);

    for (int32_t i = 0; i < frames_count; ++i) {
        IndexEntry e;
        e.id = r.i32le();
        e.name = r.null_terminated_string();
        e.related_offset = r.i32le();
        e.size = r.i32le();

        const std::size_t idx = map.entries.size();
        const std::string key = idx_upper(e.name);

        auto [_, inserted] = map.name_to_index.try_emplace(key, idx);
        if (!inserted) {
            map.warnings.push_back("duplicate index name: '" + e.name +
                                   "' (keeping first occurrence)");
        }

        if (!e.is_animation()) {
            map.image_name_to_index.try_emplace(key, idx);
        }

        map.id_to_indices[e.id].push_back(idx);
        map.entries.push_back(std::move(e));
    }

    return map;
}

std::vector<std::string> IndexMap::overlay_variants(const std::string& name) const {
    const std::string key = idx_upper(name);
    const auto        name_it = name_to_index.find(key);
    if (name_it == name_to_index.end()) {
        return {};
    }

    const IndexEntry& entry = entries[name_it->second];
    if (entry.is_animation()) {
        return {};
    }

    const auto id_it = id_to_indices.find(entry.id);
    if (id_it == id_to_indices.end()) {
        return {};
    }

    std::vector<std::string> result;
    for (const std::size_t idx : id_it->second) {
        if (idx == name_it->second)
            continue;
        const IndexEntry& other = entries[idx];
        if (other.is_animation())
            continue;
        result.push_back(other.name);
    }

    return result;
}

} // namespace d2res
