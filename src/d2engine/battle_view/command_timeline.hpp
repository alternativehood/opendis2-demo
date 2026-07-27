#pragma once

#include "visual_command.hpp"

#include <cstddef>
#include <deque>
#include <vector>

namespace d2engine {

class AnimationClipStore;
class BattleScene;

class CommandTimeline {
public:
    explicit CommandTimeline(const AnimationClipStore* clip_store = nullptr) noexcept
        : clip_store_(clip_store) {}

    void                                         enqueue(CommandNode command);
    void                                         update(BattleScene& scene, float delta_ms);
    [[nodiscard]] std::vector<BattleVisualEvent> drain_emitted_events();
    void                                         clear() noexcept;

    [[nodiscard]] bool        busy() const noexcept { return !commands_.empty(); }
    [[nodiscard]] std::size_t active_count() const noexcept { return commands_.size(); }

private:
    std::deque<CommandNode>        commands_;
    std::vector<BattleVisualEvent> emitted_events_;
    const AnimationClipStore*      clip_store_ = nullptr;
};

} // namespace d2engine
