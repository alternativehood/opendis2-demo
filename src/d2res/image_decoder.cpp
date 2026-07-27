#include "image_decoder.hpp"
#include "byte_reader.hpp"
#include "png_metadata.hpp"
#include "raw_inventory.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>

namespace d2res {

namespace {

std::string to_upper(std::string_view s) {
    std::string out(s);
    for (char& c : out)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return out;
}

nlohmann::json build_metadata(const std::string& logical_name, const std::string& container_path,
                              const std::string& base_image, std::size_t palette_id,
                              const std::string&                             transparency_mode,
                              const std::array<std::array<uint8_t, 4>, 256>& palette,
                              uint8_t transparent_color_index, const ImageFrame& frame) {
    // Transparent color BGRA → hex string (note: palette is BGRA order)
    const auto&        tc = palette[transparent_color_index];
    std::ostringstream hex;
    hex << std::hex << std::uppercase << std::setfill('0') << '#' << std::setw(2)
        << static_cast<int>(tc[2])                  // R
        << std::setw(2) << static_cast<int>(tc[1])  // G
        << std::setw(2) << static_cast<int>(tc[0]); // B
    const std::string tc_hex = hex.str();

    nlohmann::json parts = nlohmann::json::array();
    for (const auto& p : frame.pieces) {
        parts.push_back(
            {{"source_rect", {{"x", p.base_x}, {"y", p.base_y}, {"w", p.width}, {"h", p.height}}},
             {"dest", {{"x", p.output_x}, {"y", p.output_y}}}});
    }

    return {{"logical_name", logical_name},
            {"source_container", container_path},
            {"base_image", base_image},
            {"palette_id", static_cast<int>(palette_id)},
            {"transparency_mode", transparency_mode},
            {"transparent_colors_bgr", nlohmann::json::array({tc_hex})},
            {"parts", parts},
            {"output_size", {{"w", frame.output_width}, {"h", frame.output_height}}}};
}

} // namespace

ImageResourceDecoder::ImageResourceDecoder(const MqdbContainer& container, const OptMaps& maps)
    : container_(container), maps_(maps) {}

std::vector<std::string> ImageResourceDecoder::list_images() const {
    std::vector<std::string> out;
    out.reserve(maps_.index_map.entries.size());
    for (const auto& e : maps_.index_map.entries) {
        if (!e.is_animation())
            out.push_back(e.name);
    }
    return out;
}

std::shared_ptr<const RgbaBuffer>
ImageResourceDecoder::get_base_png(const IndexEntry& entry) const {
    auto rec = container_.find_by_id(entry.id);
    if (!rec.has_value()) {
        rec = container_.find_by_name(entry.name);
        if (!rec.has_value()) {
            rec = container_.find_by_name(entry.name + ".PNG");
        }
    }

    if (!rec.has_value()) {
        throw ParseError("base PNG record not found for: " + entry.name +
                             " (id=" + std::to_string(entry.id) + ")",
                         0);
    }

    const std::size_t cache_key = rec->index;

    {
        std::scoped_lock const lock(cache_mutex_);
        if (auto* cached = base_cache_.get(cache_key)) {
            return *cached;
        }
    }

    auto view = container_.payload_view(rec->index);
    auto decoded = std::make_shared<RgbaBuffer>(decode_base_png(std::span<const uint8_t>(view)));

    std::scoped_lock const lock(cache_mutex_);
    if (auto* existing = base_cache_.get(cache_key)) {
        return *existing;
    }
    base_cache_.put(cache_key, decoded);
    return decoded;
}

DecodedImage ImageResourceDecoder::decode_image(std::string_view logical_name) const {
    return decode_image_impl(logical_name, false);
}

DecodedImage
ImageResourceDecoder::decode_image_with_research_metadata(std::string_view logical_name) const {
    return decode_image_impl(logical_name, true);
}

DecodedImage ImageResourceDecoder::decode_image_impl(std::string_view logical_name,
                                                     bool             research_metadata) const {
    const std::string key = to_upper(logical_name);

    auto idx_it = maps_.index_map.image_name_to_index.find(key);
    if (idx_it == maps_.index_map.image_name_to_index.end() ||
        idx_it->second >= maps_.index_map.entries.size()) {
        throw ParseError("image not found in IndexMap: " + std::string(logical_name), 0);
    }
    const IndexEntry& entry = maps_.index_map.entries[idx_it->second];

    // Locate ImageFrame in ImageMap
    auto blk_it = maps_.image_map.frame_name_to_block.find(key);
    if (blk_it == maps_.image_map.frame_name_to_block.end()) {
        throw ParseError("image not found in ImageMap: " + std::string(logical_name), 0);
    }
    const std::size_t block_idx = blk_it->second;
    const ImageBlock& block = maps_.image_map.blocks[block_idx];

    // Find the frame within the block
    const ImageFrame* frame_ptr = nullptr;
    for (const auto& f : block.frames) {
        if (to_upper(f.name) == key) {
            frame_ptr = &f;
            break;
        }
    }
    if (frame_ptr == nullptr) {
        throw ParseError("frame not found in ImageBlock: " + std::string(logical_name), 0);
    }
    const ImageFrame& frame = *frame_ptr;

    std::optional<d2res::Entry> base_entry;
    if (research_metadata) {
        auto rec = container_.find_by_name(entry.name);
        if (!rec.has_value()) {
            rec = container_.find_by_name(entry.name + ".PNG");
        }
        if (rec.has_value()) {
            base_entry = container_.read_record(rec->index);
        }
    }

    const auto base = get_base_png(entry);

    // Determine transparency mode string
    std::string tmode = "None";
    if (block.opacity_algorithm == 300 || block.opacity_algorithm == 400) {
        tmode = "AdditiveBlend";
    } else if (block.opacity_algorithm > 0) {
        tmode = "ColorKeyMagentaRange";
    }

    // Composite
    RgbaBuffer composited = compose_image(frame, *base, block.transparent_color_index,
                                          block.palette, block.opacity_algorithm);

    nlohmann::json meta =
        build_metadata(std::string(logical_name), container_.path().string(), entry.name, block_idx,
                       tmode, block.palette, block.transparent_color_index, frame);

    DecodedImage result;
    result.logical_name = std::string(logical_name);
    result.width = composited.width;
    result.height = composited.height;
    result.rgba = std::move(composited.rgba);
    result.metadata = std::move(meta);
    if (research_metadata) {
        result.metadata["research_metadata"]["layout_authority"] = "OPT ImageMap";
        if (base_entry.has_value()) {
            result.metadata["research_metadata"]["raw_record"] =
                to_json(inspect_raw_record(*base_entry));
            result.metadata["research_metadata"]["png_metadata"] =
                to_json(scan_png_metadata(base_entry->payload));
        }
    }
    return result;
}

} // namespace d2res
