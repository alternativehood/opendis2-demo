#pragma once

#include "animation_sequence.hpp"

#include <cstddef>
#include <cstdint>

namespace d2engine {

enum class AnimationPlayerState : std::uint8_t {
    Playing,
    Paused,
    Stopped,
    Completed,
};

class AnimationPlayer {
public:
    AnimationPlayer() = default;
    explicit AnimationPlayer(AnimationSequence sequence);

    // Load a new animation sequence
    void load(AnimationSequence seq);

    // Playback control
    void play();
    void pause();
    void stop();
    void restart();
    void set_reverse_playback(bool reverse);

    // Frame stepping (for debug)
    void step_forward();
    void step_backward();
    void step(int delta_frames);

    // Update with delta time in milliseconds
    void update(float delta_ms);

    // Current frame access
    [[nodiscard]] const AnimationFrame&    current_frame() const;
    [[nodiscard]] std::size_t              current_frame_index() const noexcept;
    [[nodiscard]] bool                     is_done() const noexcept;
    [[nodiscard]] AnimationPlayerState     state() const noexcept;
    [[nodiscard]] const AnimationSequence& sequence() const noexcept;

private:
    AnimationSequence    sequence_;
    std::size_t          current_frame_index_ = 0;
    float                elapsed_ms_ = 0.0f; // Time elapsed in current frame
    AnimationPlayerState state_ = AnimationPlayerState::Stopped;
    bool                 reverse_playback_ = false;

    [[nodiscard]] std::size_t first_playback_frame() const noexcept;
    void                      advance_frame();
    void                      retreat_frame();
};

} // namespace d2engine
