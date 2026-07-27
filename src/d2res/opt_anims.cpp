#include "opt_anims.hpp"
#include "byte_reader.hpp"

namespace d2res {

AnimMap AnimsParser::parse(std::span<const uint8_t>        data,
                           const std::vector<std::string>& anim_names) {
    ByteReader  r{data};
    AnimMap     map;
    std::size_t block_index = 0;

    while (r.remaining() > 1) {
        if (block_index >= anim_names.size()) {
            throw ParseError("animation block count (" + std::to_string(block_index + 1) +
                                 ") exceeds anim_names size (" + std::to_string(anim_names.size()) +
                                 ")",
                             r.pos());
        }

        AnimBlock block;
        block.name = anim_names[block_index];

        const int32_t frames_count = r.i32le();
        block.frames.reserve(static_cast<std::size_t>(frames_count));
        for (int32_t fi = 0; fi < frames_count; ++fi)
            block.frames.push_back(r.null_terminated_string());

        map.name_to_block[block.name] = map.blocks.size();
        map.blocks.push_back(std::move(block));
        ++block_index;
    }

    return map;
}

} // namespace d2res
