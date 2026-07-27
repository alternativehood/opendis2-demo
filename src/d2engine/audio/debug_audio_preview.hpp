#pragma once

#include "audio_service.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace d2::audio {

enum class DebugAudioPreviewState { Unavailable, Stopped, Playing, Failed };

struct DebugAudioPreviewRequest {
    std::string               display_name;
    std::vector<std::uint8_t> encoded_payload;
    AudioBus                  bus = AudioBus::Ambience;
    bool                      loop = false;
};

struct DebugAudioPreviewStatus {
    DebugAudioPreviewState state = DebugAudioPreviewState::Unavailable;
    std::string            current_name;
    AudioBus               bus = AudioBus::Ambience;
    bool                   loop = false;
    std::string            message;
};

class DebugAudioPreview {
public:
    virtual ~DebugAudioPreview() = default;
    [[nodiscard]] virtual bool play_preview(DebugAudioPreviewRequest request) = 0;
    virtual void               stop_preview() noexcept = 0;
    [[nodiscard]] virtual DebugAudioPreviewStatus preview_status() const = 0;
};

} // namespace d2::audio
