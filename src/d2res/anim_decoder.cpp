#include "anim_decoder.hpp"
#include "byte_reader.hpp"
#include <cctype>
#include <sstream>

namespace d2res {

namespace {

std::string upper(std::string_view s) {
    std::string out(s);
    for (char& c : out)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return out;
}

// Find AnimBlock index by name, case-insensitively.
// Returns npos if not found.
std::size_t find_anim_block(const AnimMap& amap, std::string_view name) {
    const std::string key(name);
    // Exact lookup first
    auto it = amap.name_to_block.find(key);
    if (it != amap.name_to_block.end())
        return it->second;
    // Case-insensitive linear search
    const std::string upper_key = upper(name);
    for (std::size_t i = 0; i < amap.blocks.size(); ++i) {
        if (upper(amap.blocks[i].name) == upper_key)
            return i;
    }
    return std::string::npos;
}

DecodedImage make_placeholder(uint32_t w, uint32_t h) {
    DecodedImage img;
    img.logical_name = "(placeholder)";
    img.width = (w != 0u) ? w : 1;
    img.height = (h != 0u) ? h : 1;
    img.rgba.assign(static_cast<std::size_t>(img.width) * img.height * 4, 0);
    return img;
}

} // namespace

AnimationDecoder::AnimationDecoder(const MqdbContainer& container, const OptMaps& maps)
    : images_(container, maps), container_(container), maps_(maps) {}

std::vector<std::string> AnimationDecoder::list_animations() const {
    std::vector<std::string> names;
    names.reserve(maps_.anim_map.blocks.size());
    for (const auto& block : maps_.anim_map.blocks)
        names.push_back(block.name);
    return names;
}

std::vector<std::string> AnimationDecoder::list_animation_frames(std::string_view name) const {
    const std::size_t block_idx = find_anim_block(maps_.anim_map, name);
    if (block_idx == std::string::npos)
        throw ParseError("animation not found: " + std::string(name), 0);
    return maps_.anim_map.blocks[block_idx].frames;
}

DecodedAnimation AnimationDecoder::decode_animation(std::string_view name) const {
    return decode_animation_impl(name, false);
}

DecodedAnimation
AnimationDecoder::decode_animation_with_research_metadata(std::string_view name) const {
    return decode_animation_impl(name, true);
}

DecodedAnimation AnimationDecoder::decode_animation_impl(std::string_view name,
                                                         bool             research_metadata) const {
    const std::size_t block_idx = find_anim_block(maps_.anim_map, name);
    if (block_idx == std::string::npos)
        throw ParseError("animation not found: " + std::string(name), 0);

    const AnimBlock& block = maps_.anim_map.blocks[block_idx];

    DecodedAnimation result;
    result.name = block.name;

    uint32_t last_w = 0;
    uint32_t last_h = 0;

    nlohmann::json frame_arr = nlohmann::json::array();

    for (const auto& frame_name : block.frames) {
        DecodedImage img;
        try {
            img = research_metadata ? images_.decode_image_with_research_metadata(frame_name)
                                    : images_.decode_image(frame_name);
            last_w = img.width;
            last_h = img.height;
        } catch (const std::exception& e) {
            std::ostringstream warn;
            warn << "frame decode failed [" << frame_name << "]: " << e.what();
            result.warnings.push_back(warn.str());
            img = make_placeholder(last_w, last_h);
            img.logical_name = frame_name;
        }

        const std::size_t idx = result.frames.size();
        nlohmann::json    frame_meta = {{"index", static_cast<int>(idx)},
                                        {"logical_name", frame_name},
                                        {"width", img.width},
                                        {"height", img.height}};
        if (research_metadata) {
            frame_meta["research_metadata"] = img.metadata;
        }
        frame_arr.push_back(std::move(frame_meta));

        result.frames.push_back(std::move(img));
    }

    result.metadata = {{"name", block.name},
                       {"frame_count", static_cast<int>(result.frames.size())},
                       {"source_container", container_.path().string()},
                       {"frame_delay_ms", 100},
                       {"frames", frame_arr}};

    return result;
}

} // namespace d2res
