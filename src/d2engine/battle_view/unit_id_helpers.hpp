#pragma once

#include <cctype>
#include <string>

namespace d2engine {

inline std::string unit_type_from_resource_unit_id(const std::string& unit_id) {
    if (unit_id.size() <= 4u) {
        return {};
    }
    std::string type = unit_id.substr(4);
    for (char& c : type) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return type;
}

inline std::string resource_unit_id_from_unit_type(const std::string& unit_type) {
    std::string id = "g000";
    for (const char c : unit_type) {
        id += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return id;
}

} // namespace d2engine
