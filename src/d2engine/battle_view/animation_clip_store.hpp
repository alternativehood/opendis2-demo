#pragma once

#include "animation_clip_ref.hpp"
#include "../animation/animation_sequence.hpp"

#include <utility>
#include <vector>

namespace d2engine {

class AnimationClipStore {
public:
    [[nodiscard]] AnimationClipHandle add(AnimationSequence sequence) {
        clips_.push_back(std::move(sequence));
        return AnimationClipHandle{static_cast<std::uint32_t>(clips_.size())};
    }

    [[nodiscard]] const AnimationSequence* get(AnimationClipHandle handle) const noexcept {
        if (!handle.valid() || handle.value > clips_.size()) {
            return nullptr;
        }
        return &clips_[handle.value - 1U];
    }

private:
    std::vector<AnimationSequence> clips_;
};

} // namespace d2engine
