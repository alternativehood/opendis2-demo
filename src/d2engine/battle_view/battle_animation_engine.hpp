#pragma once

#include "battle_animation_scripts.hpp"
#include "battle_render_snapshot.hpp"
#include "command_timeline.hpp"
#include "visual_effect_resolver.hpp"

#include <deque>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace d2engine {

struct BattleAnimationInspection {
    std::optional<std::string> active_event;
    std::size_t                queued_event_count = 0;
    std::size_t                active_command_count = 0;
    BattleRenderSnapshot       snapshot;
};

struct BattleAnimationBatchResult {
    std::vector<std::string> started_event_names;
    std::vector<bool>        started;
    std::size_t              missing_event_count = 0;
};

class BattleAnimationEngine {
public:
    BattleAnimationEngine(BattleScene& scene, BattleScriptAssets assets,
                          BattleScriptParameters parameters = {});

    void                       submit(BattleVisualEvent event);
    BattleAnimationBatchResult submit_batch(const std::vector<BattleVisualEvent>& events);
    void                       update(float delta_ms);
    [[nodiscard]] std::vector<BattleVisualEvent> drain_emitted_events();
    void update_sequence_delays(std::map<std::string, int> delays);

    [[nodiscard]] bool busy() const noexcept {
        return active_event_.has_value() || !events_.empty() || timeline_.busy();
    }
    [[nodiscard]] BattleAnimationInspection inspection() const;

private:
    BattleScene&                     scene_;
    BattleScriptAssets               assets_;
    VisualEffectResolver             resolver_;
    BattleScriptParameters           parameters_;
    std::deque<BattleVisualEvent>    events_;
    std::vector<BattleVisualEvent>   emitted_events_;
    std::optional<BattleVisualEvent> active_event_;
    CommandTimeline                  timeline_;
};

} // namespace d2engine
