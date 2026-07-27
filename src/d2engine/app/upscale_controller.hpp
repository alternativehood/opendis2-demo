#pragma once

#include "../render/upscale_settings.hpp"

namespace d2engine {

class UpscaleController {
public:
    UpscaleController() = default;

    [[nodiscard]] const UpscaleSettings& settings() const noexcept { return requested_settings_; }

    void set_mode(UpscaleMode mode) noexcept;
    void set_fsr_sharpness(float sharpness) noexcept;
    void reset() noexcept;

private:
    UpscaleSettings requested_settings_;
};

} // namespace d2engine
