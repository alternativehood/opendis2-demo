#include "atlas_packer.hpp"

#define STB_RECT_PACK_IMPLEMENTATION
#include <stb_rect_pack.h>

#include <algorithm>
#include <cstring>
#include <sstream>
#include <vector>

namespace d2res {

namespace {

struct LayoutRect {
    stbrp_rect  rect = {};
    std::size_t original_index = 0;
};

struct Placement {
    AtlasEntry  entry;
    std::size_t original_index = 0;
};

} // namespace

AtlasResult AtlasPacker::pack(const std::vector<std::pair<std::string, RgbaBuffer>>& sprites,
                              int                                                    max_size) {
    AtlasResult result;

    if (sprites.empty() || max_size <= 0) {
        return result;
    }

    std::vector<LayoutRect> rects;
    rects.reserve(sprites.size());
    for (std::size_t i = 0; i < sprites.size(); ++i) {
        const auto& [name, buf] = sprites[i];
        int const sw = static_cast<int>(buf.width);
        int const sh = static_cast<int>(buf.height);

        if (sw > max_size || sh > max_size) {
            std::ostringstream msg;
            msg << name << " (" << sw << "x" << sh << " exceeds max_size=" << max_size << ")";
            result.skipped.push_back(msg.str());
            continue;
        }

        LayoutRect lr;
        lr.rect.id = static_cast<int>(i);
        lr.rect.w = static_cast<stbrp_coord>(sw);
        lr.rect.h = static_cast<stbrp_coord>(sh);
        lr.original_index = i;
        rects.push_back(lr);
    }

    if (rects.empty()) {
        return result;
    }

    std::ranges::sort(rects, [](const LayoutRect& a, const LayoutRect& b) {
        int const area_a = static_cast<int>(a.rect.w) * static_cast<int>(a.rect.h);
        int const area_b = static_cast<int>(b.rect.w) * static_cast<int>(b.rect.h);
        if (area_a != area_b)
            return area_a > area_b;
        return a.rect.h > b.rect.h;
    });

    std::vector<uint8_t> node_buf(static_cast<std::size_t>(max_size) * 2 * sizeof(stbrp_node));
    stbrp_context        pack_context;
    stbrp_node*          nodes = reinterpret_cast<stbrp_node*>(node_buf.data());
    int                  node_count = max_size * 2;

    std::vector<Placement> placements;
    std::vector<int>       sheet_used_w;
    std::vector<int>       sheet_used_h;

    auto start_new_sheet = [&]() {
        sheet_used_w.push_back(0);
        sheet_used_h.push_back(0);
        stbrp_init_target(&pack_context, max_size, max_size, nodes, node_count);
    };

    start_new_sheet();

    for (auto& lr : rects) {
        stbrp_pack_rects(&pack_context, &lr.rect, 1);

        if (lr.rect.was_packed == 0) {
            start_new_sheet();
            stbrp_pack_rects(&pack_context, &lr.rect, 1);
        }

        if (lr.rect.was_packed == 0) {
            std::ostringstream msg;
            msg << sprites[lr.original_index].first << " (" << lr.rect.w << "x" << lr.rect.h
                << ") could not be packed even on fresh sheet";
            result.skipped.push_back(msg.str());
            continue;
        }

        int sheet_idx = static_cast<int>(sheet_used_w.size()) - 1;

        Placement p;
        p.entry.name = sprites[lr.original_index].first;
        p.entry.sheet = sheet_idx;
        p.entry.x = lr.rect.x;
        p.entry.y = lr.rect.y;
        p.entry.w = lr.rect.w;
        p.entry.h = lr.rect.h;
        p.original_index = lr.original_index;
        placements.push_back(p);

        result.entries.push_back(p.entry);

        int right = lr.rect.x + lr.rect.w;
        int bottom = lr.rect.y + lr.rect.h;
        if (right > sheet_used_w.back())
            sheet_used_w.back() = right;
        if (bottom > sheet_used_h.back())
            sheet_used_h.back() = bottom;
    }

    // Allocate tightly-sized canvases
    result.sheets.reserve(sheet_used_w.size());
    for (std::size_t s = 0; s < sheet_used_w.size(); ++s) {
        int const used_w = sheet_used_w[s];
        int const used_h = sheet_used_h[s];

        AtlasSheet sheet;
        sheet.canvas.width = static_cast<uint32_t>(used_w);
        sheet.canvas.height = static_cast<uint32_t>(used_h);
        sheet.canvas.rgba.resize(
            static_cast<std::size_t>(used_w) * static_cast<std::size_t>(used_h) * 4U, 0);
        result.sheets.push_back(std::move(sheet));
    }

    // O(N) blit from placements directly
    for (const auto& p : placements) {
        const auto& src_buf = sprites[p.original_index].second;
        AtlasSheet& sheet = result.sheets[static_cast<std::size_t>(p.entry.sheet)];
        int const   sw = p.entry.w;
        int const   sh = p.entry.h;
        int const   cw = static_cast<int>(sheet.canvas.width);
        for (int row = 0; row < sh; ++row) {
            const uint8_t* src = src_buf.rgba.data() +
                                 static_cast<std::size_t>(row) * static_cast<std::size_t>(sw) * 4U;
            uint8_t*       dst =
                sheet.canvas.rgba.data() +
                ((static_cast<std::size_t>(p.entry.y + row) * static_cast<std::size_t>(cw) +
                  static_cast<std::size_t>(p.entry.x)) *
                 4U);
            std::memcpy(dst, src, static_cast<std::size_t>(sw) * 4);
        }
    }

    return result;
}

nlohmann::json AtlasResult::to_json(std::string_view source_container, int max_size) const {
    nlohmann::json json_result = nlohmann::json::object();
    json_result["source_container"] = source_container;
    json_result["max_sheet_size"] = max_size;
    json_result["sheet_count"] = static_cast<int>(sheets.size());
    json_result["total_sprites"] = static_cast<int>(entries.size());
    json_result["skipped_sprites"] = static_cast<int>(skipped.size());

    nlohmann::json entries_json = nlohmann::json::array();
    for (const auto& e : entries) {
        nlohmann::json entry;
        entry["name"] = e.name;
        entry["sheet"] = e.sheet;
        entry["x"] = e.x;
        entry["y"] = e.y;
        entry["w"] = e.w;
        entry["h"] = e.h;
        entries_json.push_back(std::move(entry));
    }
    json_result["entries"] = std::move(entries_json);

    nlohmann::json skipped_json = nlohmann::json::array();
    for (const auto& s : skipped) {
        skipped_json.push_back(s);
    }
    json_result["skipped"] = std::move(skipped_json);

    return json_result;
}

} // namespace d2res
