#include "animation_time_controller.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace d2::app {

float AnimationTimeController::speed() const noexcept {
    return speed_;
}
float AnimationTimeController::effective_time_scale() const noexcept {
    return kBaseAnimationTimeScale * speed_;
}
bool AnimationTimeController::paused() const noexcept {
    return paused_;
}

void AnimationTimeController::set_speed(float speed) {
    if (!std::isfinite(speed))
        throw std::invalid_argument("Animation speed must be finite");
    speed_ = std::clamp(speed, kMinimumSpeed, kMaximumSpeed);
}

void AnimationTimeController::set_paused(bool paused) noexcept {
    paused_ = paused;
}
// cppcheck-suppress unusedFunction ; public animation control API
void AnimationTimeController::toggle_paused() noexcept {
    paused_ = !paused_;
}
void AnimationTimeController::reset() noexcept {
    speed_ = 1.0F;
    paused_ = false;
}

float AnimationTimeController::scale_delta_ms(float real_delta_ms) const {
    if (!std::isfinite(real_delta_ms) || real_delta_ms < 0.0F)
        throw std::invalid_argument("Animation delta must be finite and non-negative");
    return paused_ ? 0.0F : real_delta_ms * effective_time_scale();
}

} // namespace d2::app
