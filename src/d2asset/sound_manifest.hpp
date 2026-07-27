#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace d2asset {

inline constexpr std::uint32_t sound_schema_version = 1;

enum class SoundFormat : std::uint8_t {
    Wave,
    Unknown,
};

struct SoundWarning {
    std::string sound_asset_id;
    std::string message;
};

struct SoundAsset {
    std::string                  sound_asset_id;
    std::string                  logical_name;
    std::string                  container_id;
    std::filesystem::path        sidecar_path;
    std::filesystem::path        payload_path;
    std::uint64_t                payload_size{};
    SoundFormat                  format{SoundFormat::Unknown};
    std::optional<std::uint32_t> format_tag;
    std::optional<std::uint32_t> channels;
    std::optional<std::uint32_t> sample_rate;
    std::optional<std::uint32_t> bit_depth;
    std::optional<std::uint64_t> duration_ms;
    std::vector<SoundWarning>    warnings;
};

[[nodiscard]] const char* to_string(SoundFormat value) noexcept;

class SoundManifest {
public:
    [[nodiscard]] static SoundAsset load(const std::filesystem::path& asset_root,
                                         const std::filesystem::path& sidecar_path,
                                         const std::string&           sound_asset_id,
                                         const std::string&           logical_name,
                                         const std::string&           container_id);
};

} // namespace d2asset
