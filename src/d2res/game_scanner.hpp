#pragma once
#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace d2res {

enum class ContentKind : uint8_t { Images, Animations, Sounds, SoundMapping, DataTables, Unknown };

[[nodiscard]] inline std::string to_string(ContentKind k) {
    switch (k) {
    case ContentKind::Images:
        return "images";
    case ContentKind::Animations:
        return "animations";
    case ContentKind::Sounds:
        return "sounds";
    case ContentKind::SoundMapping:
        return "sound_mapping";
    case ContentKind::DataTables:
        return "data_tables";
    case ContentKind::Unknown:
        return "unknown";
    }
    return "unknown";
}

struct ContainerEntry {
    std::string              path;
    uintmax_t                size = 0;
    std::string              sha256;
    int                      record_count = 0;
    std::vector<std::string> special_files;
    std::vector<ContentKind> likely_content;
};

struct OtherFileEntry {
    std::string              path;
    uintmax_t                size = 0;
    std::string              sha256;
    std::string              type;
    std::vector<ContentKind> likely_content;
};

struct ScanResult {
    std::string                 game_root;
    std::string                 scan_timestamp;
    int                         total_files = 0;
    std::vector<ContainerEntry> containers;
    std::vector<OtherFileEntry> other_files;
    std::vector<std::string>    warnings;

    [[nodiscard]] nlohmann::json to_json() const;
};

class GameScanner {
public:
    [[nodiscard]] static ScanResult scan(const std::string& game_root);
};

} // namespace d2res
