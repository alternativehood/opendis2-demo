#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace d2engine {

enum class UpscaleMode {
    Linear,
    Nearest,
    PixelArt,
    Fsr1,
};

[[nodiscard]] std::string_view upscale_mode_name(UpscaleMode mode) noexcept;

struct UpscaleSettings {
    UpscaleMode mode = UpscaleMode::Fsr1;
    float       fsr_sharpness = 0.70F;

    void clamp() noexcept;
};

struct UpscaleExtent {
    int width = 0;
    int height = 0;
};

enum class UpscaleFallbackReason {
    OutputSmallerThanScene,
    IdenticalDimensions,
};

[[nodiscard]] std::string_view upscale_fallback_reason_name(UpscaleFallbackReason reason) noexcept;

struct UpscalePresentation {
    UpscaleSettings                      requested{};
    UpscaleMode                          effective_mode = UpscaleMode::Linear;
    UpscaleExtent                        logical_size{};
    UpscaleExtent                        output_size{};
    UpscaleExtent                        scene_pixel_size{};
    UpscaleExtent                        window_coordinate_size{};
    UpscaleExtent                        window_pixel_size{};
    float                                window_pixel_density = 1.0F;
    float                                scale_x = 1.0F;
    float                                scale_y = 1.0F;
    bool                                 sampling_active = false;
    std::optional<UpscaleFallbackReason> fallback_reason;
    std::uint64_t                        easu_pass_count = 0;
    std::uint64_t                        rcas_pass_count = 0;
    std::uint64_t                        linear_composite_count = 0;
    std::uint64_t                        nearest_composite_count = 0;
    std::uint64_t                        pixel_art_composite_count = 0;
    std::string                          renderer_name;
    std::string                          gpu_device_name;
    std::string                          gpu_backend;

    // cppcheck-suppress unusedFunction
    [[nodiscard]] bool fsr_downscale_fallback() const noexcept {
        return fallback_reason.has_value();
    }
};

[[nodiscard]] UpscaleMode resolve_effective_upscale_mode(UpscaleMode   requested,
                                                         UpscaleExtent logical_size,
                                                         UpscaleExtent output_size) noexcept;

} // namespace d2engine
