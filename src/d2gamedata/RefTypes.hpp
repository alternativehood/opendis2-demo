#pragma once

#include <string>
#include <string_view>

namespace d2gamedata {

inline constexpr std::string_view kNullGlobalId = "G000000000";
inline constexpr std::string_view kGlobalIdPrefix = "G000";

struct GlobalId {
    std::string value;

    bool        is_null() const { return value == kNullGlobalId; }
    std::string prefix() const {
        if (value.size() >= 7)
            return value.substr(0, 7);
        return value;
    }

    bool operator==(const GlobalId& o) const { return value == o.value; }
    bool operator<(const GlobalId& o) const { return value < o.value; }
};

struct TextId {
    std::string value;

    bool operator==(const TextId& o) const { return value == o.value; }
    bool operator<(const TextId& o) const { return value < o.value; }
};

struct ScenarioObjectId {
    std::string value;

    bool operator==(const ScenarioObjectId& o) const { return value == o.value; }
    bool operator<(const ScenarioObjectId& o) const { return value < o.value; }
};

} // namespace d2gamedata
