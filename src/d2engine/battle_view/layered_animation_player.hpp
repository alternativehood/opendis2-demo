#pragma once
#include "layered_animation_clip.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace d2engine {

struct LayeredAnimationPlayer {
    const LayeredAnimationClip* clip = nullptr;
    uint32_t                    elapsed_ms = 0;
    bool                        looping = false;
    bool                        reverse_playback = false;
};

// Compute total duration of a sequence by summing all frame durations.
[[nodiscard]] inline uint32_t sequence_total_ms(const AnimationSequence& seq) noexcept {
    uint32_t total = 0;
    for (const auto& f : seq.frames) {
        total += static_cast<uint32_t>(f.duration_ms > 0 ? f.duration_ms : 40u);
    }
    return total;
}

// Find which frame index is active at a given phase_ms within a sequence.
// phase_ms must be in [0, total_ms).
[[nodiscard]] inline std::size_t frame_at_time(const AnimationSequence& seq,
                                               float                    phase_ms) noexcept {
    const std::size_t n = seq.frames.size();
    if (n == 0)
        return 0;
    float t = 0.0f;
    for (std::size_t i = 0; i < n; ++i) {
        const auto dur =
            static_cast<float>(seq.frames[i].duration_ms > 0 ? seq.frames[i].duration_ms : 40u);
        t += dur;
        if (phase_ms < t)
            return i;
    }
    return n - 1u;
}

// Returns zero-based frame index for the given slot.
// Phase is derived from real elapsed time against driver total duration.
// Optional layers shorter or longer than the driver are handled correctly.
[[nodiscard]] inline std::size_t sample_frame(const LayeredAnimationPlayer& player,
                                              LayerSlot                     slot) {
    if (player.clip == nullptr)
        return 0;
    const auto* slot_seq = layer_sequence(*player.clip, slot);
    if (slot_seq == nullptr || slot_seq->frames.empty())
        return 0;

    const auto* driver = driver_sequence(*player.clip);
    if (driver == nullptr || driver->frames.empty())
        return 0;

    const uint32_t driver_total = sequence_total_ms(*driver);
    if (driver_total == 0)
        return 0;

    // Compute phase_ms within one driver cycle.
    float phase_ms = 0.0f;
    // NOLINTNEXTLINE(bugprone-branch-clone)
    if (player.looping) {
        phase_ms =
            std::fmod(static_cast<float>(player.elapsed_ms), static_cast<float>(driver_total));
    } else {
        // Clamp just below total so we never go past the last frame.
        phase_ms = std::min(static_cast<float>(player.elapsed_ms),
                            static_cast<float>(driver_total) - 0.5f);
    }

    // Normalized phase in [0, 1).
    float p = phase_ms / static_cast<float>(driver_total);
    if (player.reverse_playback) {
        p = 1.0f - p;
    }

    // Map p into the target layer by its own cumulative durations.
    const uint32_t own_total = sequence_total_ms(*slot_seq);
    if (own_total == 0)
        return 0;
    const float own_phase_ms = p * static_cast<float>(own_total);
    return frame_at_time(*slot_seq, own_phase_ms);
}

} // namespace d2engine
