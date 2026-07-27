#pragma once

#include "atlas_manifest.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace d2asset {

enum class TimingSource : std::uint8_t {
    ProvisionalSidecar,
    FallbackDefault,
};

enum class LoopMode : std::uint8_t {
    Unknown,
    Once,
    Loop,
};

enum class AnimationRole : std::uint8_t {
    Unknown,
    Idle,
    Move,
    Attack,
    Hit,
    Death,
    Cast,
    Defend,
};

enum class FacingDirection : std::uint8_t {
    Unknown,
};

struct FrameTiming {
    std::uint32_t duration_ms{};
    TimingSource  source{TimingSource::FallbackDefault};
};

struct AnimationClassification {
    AnimationRole role{AnimationRole::Unknown};
    std::string   matched_token;
    std::string   reason;
};

struct AnimationWarning {
    std::string                animation_asset_id;
    std::optional<std::size_t> frame_index;
    std::string                logical_name;
    std::string                message;
    std::vector<std::string>   matching_asset_ids;
};

struct AnimationFrameRef {
    std::size_t                          index{};
    std::string                          logical_name;
    PixelSize                            source_size;
    FrameTiming                          timing;
    std::optional<std::string>           image_asset_id;
    std::optional<TextureRegion>         texture_region;
    std::optional<std::filesystem::path> fallback_path;

    [[nodiscard]] bool resolved() const noexcept {
        return texture_region.has_value() || fallback_path.has_value();
    }
};

struct AnimationClip {
    std::string                    animation_asset_id;
    std::string                    logical_name;
    std::string                    container_id;
    std::filesystem::path          sidecar_path;
    std::vector<AnimationFrameRef> frames;
    LoopMode                       loop_mode{LoopMode::Unknown};
    AnimationClassification        classification;
    FacingDirection                facing_direction{FacingDirection::Unknown};
    std::vector<AnimationWarning>  warnings;
};

class AnimationManifest {
public:
    [[nodiscard]] static AnimationClip load(const std::filesystem::path& asset_root,
                                            const std::filesystem::path& sidecar_path,
                                            const std::string&           animation_asset_id,
                                            const std::string&           logical_name,
                                            const std::string&           container_id);
};

} // namespace d2asset
