#pragma once
#include "image_decoder.hpp"
#include "mqdb.hpp"
#include "opt_maps.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace d2res {

struct DecodedAnimation {
    std::string               name;
    std::vector<DecodedImage> frames;
    nlohmann::json            metadata;
    std::vector<std::string>  warnings;
};

class AnimationDecoder {
public:
    AnimationDecoder(const MqdbContainer& container, const OptMaps& maps);

    // Names of all animations (AnimMap block order).
    std::vector<std::string> list_animations() const;

    // Ordered logical frame names for the named animation — no pixel decoding.
    // Throws ParseError if the animation name is not found.
    std::vector<std::string> list_animation_frames(std::string_view name) const;

    // Decode all frames for the named animation (case-insensitive).
    // On per-frame failure, inserts a placeholder and records a warning.
    // Throws ParseError if the animation name is not found.
    DecodedAnimation decode_animation(std::string_view name) const;
    DecodedAnimation decode_animation_with_research_metadata(std::string_view name) const;

private:
    ImageResourceDecoder images_;
    const MqdbContainer& container_;
    const OptMaps&       maps_;

    DecodedAnimation decode_animation_impl(std::string_view name, bool research_metadata) const;
};

} // namespace d2res
