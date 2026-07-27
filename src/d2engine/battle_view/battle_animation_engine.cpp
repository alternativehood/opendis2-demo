#include "battle_animation_engine.hpp"

#include <algorithm>
#include <utility>

namespace d2engine {

BattleAnimationEngine::BattleAnimationEngine(BattleScene& scene, BattleScriptAssets assets,
                                             BattleScriptParameters parameters)
    : scene_(scene), assets_(assets), resolver_(scene_, assets_.unit_profiles),
      parameters_(std::move(std::move(parameters))) {}

void BattleAnimationEngine::submit(BattleVisualEvent event) {
    events_.push_back(std::move(event));
}

BattleAnimationBatchResult
BattleAnimationEngine::submit_batch(const std::vector<BattleVisualEvent>& events) {
    BattleAnimationBatchResult result;
    const auto                 context =
        BattleScriptContext{.scene = scene_, .assets = assets_, .parameters = parameters_};

    for (const auto& original_event : events) {
        auto expanded = resolver_.expand(original_event);
        bool original_started = false;

        for (std::size_t j = 0; j < expanded.size(); ++j) {
            const auto& ev = expanded[j];

            auto command = BattleAnimationScripts::build(ev, context);
            if (!command.has_value()) {
                if (j == 0) {
                    ++result.missing_event_count;
                }
                continue;
            }
            if (j == 0) {
                original_started = true;
            }
            result.started_event_names.emplace_back(battle_visual_event_name(ev));
            timeline_.enqueue(std::move(*command));
            if (!active_event_.has_value()) {
                active_event_ = ev;
            }
        }

        result.started.push_back(original_started);
    }

    timeline_.update(scene_, 0.0f);
    auto emitted = timeline_.drain_emitted_events();
    emitted_events_.insert(emitted_events_.end(), emitted.begin(), emitted.end());
    if (!timeline_.busy()) {
        active_event_.reset();
    }
    return result;
}

void BattleAnimationEngine::update_sequence_delays(std::map<std::string, int> delays) {
    parameters_.sequence_frame_delays = std::move(delays);
}

std::vector<BattleVisualEvent> BattleAnimationEngine::drain_emitted_events() {
    std::vector<BattleVisualEvent> result;
    result.swap(emitted_events_);
    return result;
}

void BattleAnimationEngine::update(float delta_ms) {
    float remaining_ms = std::max(delta_ms, 0.0f);
    if (active_event_.has_value()) {
        timeline_.update(scene_, remaining_ms);
        auto emitted = timeline_.drain_emitted_events();
        emitted_events_.insert(emitted_events_.end(), emitted.begin(), emitted.end());
        remaining_ms = 0.0f;
        if (!timeline_.busy()) {
            active_event_.reset();
        }
    }

    while (!active_event_.has_value() && !events_.empty()) {
        BattleVisualEvent const raw_event = std::move(events_.front());
        events_.pop_front();

        auto expanded = resolver_.expand(raw_event);
        bool any_queued = false;

        for (auto& ev : expanded) {
            auto command = BattleAnimationScripts::build(
                ev,
                BattleScriptContext{.scene = scene_, .assets = assets_, .parameters = parameters_});
            if (!command.has_value()) {
                continue;
            }
            if (!active_event_.has_value()) {
                active_event_ = ev;
            }
            any_queued = true;
            timeline_.enqueue(std::move(*command));
        }

        if (!any_queued) {
            continue;
        }

        timeline_.update(scene_, remaining_ms);
        auto emitted = timeline_.drain_emitted_events();
        emitted_events_.insert(emitted_events_.end(), emitted.begin(), emitted.end());
        remaining_ms = 0.0f;
        if (timeline_.busy()) {
            break;
        }
        active_event_.reset();
    }
}

BattleAnimationInspection BattleAnimationEngine::inspection() const {
    BattleAnimationInspection result{
        .queued_event_count = events_.size(),
        .active_command_count = timeline_.active_count(),
        .snapshot = scene_.snapshot(),
    };
    if (active_event_.has_value()) {
        result.active_event = std::string{battle_visual_event_name(*active_event_)};
    }
    return result;
}

} // namespace d2engine
