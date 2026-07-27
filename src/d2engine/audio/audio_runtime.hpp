#pragma once

#include "audio_service.hpp"
#include "debug_audio_preview.hpp"

namespace d2::audio {

class AudioRuntime : public AudioService, public DebugAudioPreview {
public:
    ~AudioRuntime() override = default;
};

} // namespace d2::audio
