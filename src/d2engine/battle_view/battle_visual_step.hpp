#pragma once

#include "battle_visual_event.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace d2engine {

enum class BattleVisualStepCompletion : std::uint8_t {
    Immediate,
    AllRequiredTracksFinished,
};

struct BattleVisualEventSchedule {
    std::optional<std::string> event_id;
    std::string                cue = "start";
};

struct BattleVisualEventEnvelope {
    std::string               id;
    bool                      optional = false;
    BattleVisualEventSchedule at;
    BattleVisualEvent         event;
};

struct BattleVisualStep {
    std::string                            id;
    BattleVisualStepCompletion             complete = BattleVisualStepCompletion::Immediate;
    std::vector<BattleVisualEventEnvelope> envelopes;
};

} // namespace d2engine
