#pragma once

#include <string>

namespace d2engine {

struct BattleTuningState;

struct DebugCommandResult {
    std::string output;
    bool        request_quit = false;
    bool        request_save = false;
    bool        request_reset = false;
};

// Executes stdin debug console commands against a BattleTuningState.
// Returned output is a ready-to-print string (newline-terminated lines).
// Extracted from Application so it can be unit-tested without SDL.
class DebugCommandHandler {
public:
    explicit DebugCommandHandler(BattleTuningState& state) : state_(state) {}
    [[nodiscard]] DebugCommandResult execute(const std::string& raw_line);

private:
    BattleTuningState& state_;
};

} // namespace d2engine
