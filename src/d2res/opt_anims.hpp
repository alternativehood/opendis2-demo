#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace d2res {

struct AnimBlock {
    std::string              name;
    std::vector<std::string> frames;
};

struct AnimMap {
    std::vector<AnimBlock>                       blocks;
    std::unordered_map<std::string, std::size_t> name_to_block;
    std::vector<std::string>                     warnings;
};

struct AnimsParser {
    static AnimMap parse(std::span<const uint8_t> data, const std::vector<std::string>& anim_names);
};

} // namespace d2res
