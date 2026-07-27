#pragma once

// Atlas packing policy:
// Use a proven rectangle packing library for sheet placement.
// Do not replace this with a custom shelf/bin-packing algorithm unless there is
// a measured pixel-parity or format requirement documented in tests.

#include "rgba_buffer.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace d2res {

struct AtlasEntry {
    std::string name;
    int         sheet = 0;
    int         x = 0, y = 0;
    int         w = 0, h = 0;
};

struct AtlasSheet {
    RgbaBuffer canvas; // RGBA8, row-major, dimensions = max_size × max_size (or smaller)
};

struct AtlasResult {
    std::vector<AtlasSheet>  sheets;
    std::vector<AtlasEntry>  entries;
    std::vector<std::string> skipped; // names of sprites that could not be placed

    [[nodiscard]] nlohmann::json to_json(std::string_view source_container, int max_size) const;
};

class AtlasPacker {
public:
    // Pack sprites onto power-of-two sheets. Sprites wider or taller than
    // max_size are added to AtlasResult::skipped.
    [[nodiscard]] static AtlasResult
    pack(const std::vector<std::pair<std::string, RgbaBuffer>>& sprites, int max_size = 4096);
};

} // namespace d2res
