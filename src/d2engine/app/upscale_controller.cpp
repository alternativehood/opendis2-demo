#include "upscale_controller.hpp"

namespace d2engine {

void UpscaleController::set_mode(UpscaleMode mode) noexcept {
    requested_settings_.mode = mode;
}

void UpscaleController::set_fsr_sharpness(float sharpness) noexcept {
    requested_settings_.fsr_sharpness = sharpness;
    requested_settings_.clamp();
}

void UpscaleController::reset() noexcept {
    requested_settings_ = {};
}

} // namespace d2engine
