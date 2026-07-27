#pragma once

#include <string>

namespace d2gamedata {

enum class SourceKind { Null, Dbf, Text, Asset, Inferred, Unknown };

inline std::string to_string(SourceKind kind) {
    switch (kind) {
    case SourceKind::Null:
        return "null";
    case SourceKind::Dbf:
        return "dbf";
    case SourceKind::Text:
        return "text";
    case SourceKind::Asset:
        return "asset";
    case SourceKind::Inferred:
        return "inferred";
    case SourceKind::Unknown:
        return "unknown";
    }
    return "unknown";
}

} // namespace d2gamedata
