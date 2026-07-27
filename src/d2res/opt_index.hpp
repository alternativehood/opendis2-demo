#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace d2res {

struct IndexEntry {
    int32_t     id = 0;
    std::string name;
    int32_t     related_offset = 0;
    int32_t     size = 0;

    [[nodiscard]] bool is_animation() const noexcept { return id == -1; }
};

struct IndexMap {
    std::vector<IndexEntry>                               entries;
    std::unordered_map<std::string, std::size_t>          name_to_index;       // uppercase key
    std::unordered_map<std::string, std::size_t>          image_name_to_index; // non-animation only
    std::unordered_map<int32_t, std::vector<std::size_t>> id_to_indices;       // id → entry indices
    std::vector<std::string>                              warnings;

    // Names of all non-animation entries sharing the same MQRC id as the given name.
    // Returns empty vector if name not found or no variants exist.
    [[nodiscard]] std::vector<std::string> overlay_variants(const std::string& name) const;
};

struct IndexParser {
    static IndexMap parse(std::span<const uint8_t> data);
};

} // namespace d2res
