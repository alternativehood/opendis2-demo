#pragma once

namespace d2::app {

class AnimationTimeController final {
public:
    // The one manually tuned production value. 1.00x is relative to this scale.
    static constexpr float kBaseAnimationTimeScale = 1.75F;
    static constexpr float kMinimumSpeed = 0.1F;
    static constexpr float kMaximumSpeed = 4.0F;

    [[nodiscard]] float speed() const noexcept;
    [[nodiscard]] float effective_time_scale() const noexcept;
    [[nodiscard]] bool  paused() const noexcept;

    void                set_speed(float speed);
    void                set_paused(bool paused) noexcept;
    void                toggle_paused() noexcept;
    void                reset() noexcept;
    [[nodiscard]] float scale_delta_ms(float real_delta_ms) const;

private:
    float speed_ = 1.0F;
    bool  paused_ = false;
};

} // namespace d2::app
