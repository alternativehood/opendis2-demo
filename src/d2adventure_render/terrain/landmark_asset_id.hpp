#pragma once

#include <cctype>
#include <string>
#include <string_view>

namespace d2engine::adventure_render {

inline std::string canonical_landmark_type_id(std::string_view id) {
    std::string result;
    result.reserve(id.size());
    for (char c : id)
        result.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    return result;
}

} // namespace d2engine::adventure_render
