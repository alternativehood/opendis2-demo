#pragma once

#include "DbfGameDataIndex.hpp"
#include "RefTypes.hpp"
#include "TextResolution.hpp"
#include "SourceKind.hpp"

#include <optional>
#include <string>
#include <vector>

namespace d2gamedata {

enum class ResolutionStatus { Resolved, Unresolved, NullRef };

struct GlobalIdResolution {
    GlobalId                      id;
    ResolutionStatus              status = ResolutionStatus::Unresolved;
    SourceKind                    source_kind = SourceKind::Unknown;
    std::optional<DbfValueRef>    primary_match;
    std::vector<DbfValueRef>      all_matches;
    std::optional<TextResolution> name;
    std::optional<TextResolution> description;
    std::string                   category_kind;
    std::string                   unresolved_reason;
};

} // namespace d2gamedata
