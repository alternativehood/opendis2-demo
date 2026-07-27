#include "audio_runtime_factory.hpp"

#include "null_audio_service.hpp"
#include "sdl_mixer_audio_service.hpp"

#include <d2log/log.hpp>

#include <memory>
#include <string>

namespace d2::audio {

std::unique_ptr<AudioRuntime> create_audio_runtime() {
    try {
        return std::make_unique<SdlMixerAudioService>();
    } catch (const std::exception& exception) {
        const std::string reason = exception.what();
        d2log::get("d2.audio")
            ->warn("SDL3_mixer unavailable; using null audio backend: {}", reason);
        return std::make_unique<NullAudioService>(reason);
    }
}

} // namespace d2::audio
