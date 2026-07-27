#include "upscale_settings.hpp"

#include <algorithm>

namespace d2engine {

std::string_view upscale_mode_name(UpscaleMode mode) noexcept {
    switch (mode) {
    case UpscaleMode::Linear:
        return "Linear (Legacy)";
    case UpscaleMode::Nearest:
        return "Nearest";
    case UpscaleMode::PixelArt:
        return "Pixel Art";
    case UpscaleMode::Fsr1:
        return "FSR 1";
    }
    return "Unknown";
}

void UpscaleSettings::clamp() noexcept {
    fsr_sharpness = std::clamp(fsr_sharpness, 0.0F, 1.0F);
}

std::string_view upscale_fallback_reason_name(UpscaleFallbackReason reason) noexcept {
    switch (reason) {
    case UpscaleFallbackReason::OutputSmallerThanScene:
        return "output is smaller than scene";
    case UpscaleFallbackReason::IdenticalDimensions:
        return "scene and output dimensions are identical";
    }
    return "unknown fallback";
}

UpscaleMode resolve_effective_upscale_mode(UpscaleMode requested, UpscaleExtent logical_size,
                                           UpscaleExtent output_size) noexcept {
    if (requested == UpscaleMode::Fsr1 &&
        (output_size.width < logical_size.width || output_size.height < logical_size.height ||
         (output_size.width == logical_size.width && output_size.height == logical_size.height))) {
        return UpscaleMode::Linear;
    }
    return requested;
}

} // namespace d2engine
