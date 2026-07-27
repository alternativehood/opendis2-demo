#pragma once

#include <string>
#include <vector>

namespace d2engine {

struct BattleVisualStepExecution {
    enum class EnvelopeState : std::uint8_t { Waiting, Started, Skipped, Failed, Completed };

    std::string                step_id;
    std::vector<EnvelopeState> envelope_states;
    std::vector<std::string>   started_envelope_ids;
    std::vector<std::string>   waiting_envelope_ids;
    std::vector<std::string>   skipped_optional_envelope_ids;
    std::vector<std::string>   missing_required_envelope_ids;
    std::vector<std::string>   fired_cues;
    std::vector<std::string>   diagnostics;
    bool                       complete = false;
    bool                       failed = false;
};

} // namespace d2engine
