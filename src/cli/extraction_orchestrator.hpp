#pragma once

#include "d2res/game_scanner.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

enum class ExtractedAssetKind : std::uint8_t {
    Image,
    Animation,
    Sound,
    DataTable,
};

struct ExtractedAsset {
    ExtractedAssetKind    kind{ExtractedAssetKind::Image};
    std::string           logical_name;
    std::filesystem::path sidecar_path;
};

struct ContainerExtraction {
    std::string                 container_path;
    std::vector<std::string>    content_kinds;
    std::filesystem::path       output_path;
    std::vector<ExtractedAsset> assets;
    std::vector<std::string>    warnings;
    bool                        supported{false};
    bool                        succeeded{false};
    std::string                 error;
};

struct ExtractionOptions {
    bool animation_frame_atlases{true};
    int  atlas_max_size{4096};
};

[[nodiscard]] bool has_supported_runtime_content(const d2res::ContainerEntry& container);

[[nodiscard]] ContainerExtraction extract_container_assets(const std::filesystem::path& game_root,
                                                           const d2res::ContainerEntry& container,
                                                           const std::filesystem::path& output_path,
                                                           const ExtractionOptions& options = {});

[[nodiscard]] const char* to_string(ExtractedAssetKind kind) noexcept;
