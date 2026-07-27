#pragma once

#include "RefTypes.hpp"

#include <optional>
#include <string>

namespace d2gamedata {

struct TextResolution {
    TextId      id;
    bool        resolved = false;
    std::string value;
    std::string source_table;
    std::string source_file;
    int         source_row = -1;
    std::string reason_unresolved;
};

} // namespace d2gamedata
