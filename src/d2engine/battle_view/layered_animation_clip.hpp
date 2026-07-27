#pragma once
#include "../animation/animation_sequence.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace d2engine {

enum class LayerSlot : uint8_t { S1 = 0, A1 = 1, A2 = 2 };

struct LayeredAnimationClip {
    std::optional<AnimationSequence> s1;
    std::optional<AnimationSequence> a1;
    std::optional<AnimationSequence> a2;
    std::string                      family_name;
    char                             direction = 'A';
    bool                             loop_policy = false;

    [[nodiscard]] bool is_empty() const noexcept { return !s1 && !a1 && !a2; }
    static constexpr std::array<LayerSlot, 3> kDrawOrder = {LayerSlot::S1, LayerSlot::A1,
                                                            LayerSlot::A2};
};

// Returns the sequence for slot, or nullptr if absent
[[nodiscard]] inline const AnimationSequence* layer_sequence(const LayeredAnimationClip& clip,
                                                             LayerSlot slot) noexcept {
    // NOLINTNEXTLINE(bugprone-branch-clone)
    switch (slot) {
    case LayerSlot::S1:
        return clip.s1.has_value() ? &*clip.s1 : nullptr;
    case LayerSlot::A1:
        return clip.a1.has_value() ? &*clip.a1 : nullptr;
    case LayerSlot::A2:
        return clip.a2.has_value() ? &*clip.a2 : nullptr;
    }
    return nullptr;
}

// Returns the driver sequence: A1 if present, else longest present layer
[[nodiscard]] inline const AnimationSequence*
driver_sequence(const LayeredAnimationClip& clip) noexcept {
    if (clip.a1.has_value())
        return &*clip.a1;
    const AnimationSequence* best = nullptr;
    std::size_t              best_count = 0;
    for (auto slot : {LayerSlot::S1, LayerSlot::A2}) {
        const auto* seq = layer_sequence(clip, slot);
        if (seq != nullptr && seq->frames.size() > best_count) {
            best = seq;
            best_count = seq->frames.size();
        }
    }
    return best;
}

} // namespace d2engine
