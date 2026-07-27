#pragma once

#include <string>
#include <string_view>

namespace d2asset {

[[nodiscard]] inline std::string normalize_ascii(std::string_view value) {
    std::string normalized;
    normalized.reserve(value.size());
    for (const char c : value) {
        if (c >= 'A' && c <= 'Z') {
            normalized.push_back(static_cast<char>(c - 'A' + 'a'));
        } else if (c == '\\') {
            normalized.push_back('/');
        } else {
            normalized.push_back(c);
        }
    }
    return normalized;
}

[[nodiscard]] inline bool is_canonical_id(std::string_view value) {
    // NOLINTNEXTLINE(readability-simplify-boolean-expr)
    if (value.empty() || value.front() == '/' || value.back() == '/')
        return false;

    for (const char c : value) {
        const bool valid = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' ||
                           c == '-' || c == '.' || c == '/';
        if (!valid)
            return false;
    }
    return true;
}

} // namespace d2asset
