#include "null_audio_service.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace d2::audio {
namespace {

std::size_t checked_bus_index(const AudioBus bus) {
    const auto index = static_cast<std::size_t>(bus);
    if (index >= kAudioBusCount) {
        throw std::invalid_argument("invalid audio bus");
    }
    return index;
}

} // namespace

NullAudioService::NullAudioService(std::string unavailable_reason)
    : unavailable_reason_(std::move(unavailable_reason)) {
    gains_.fill(1.0F);
}

void NullAudioService::play_one_shot(const AudioCue&) {}

void NullAudioService::stop_all() {}

void NullAudioService::set_bus_gain(const AudioBus bus, const float gain) {
    if (!std::isfinite(gain)) {
        throw std::invalid_argument("audio bus gain must be finite");
    }
    gains_[checked_bus_index(bus)] = std::clamp(gain, 0.0F, 1.0F);
}

float NullAudioService::bus_gain(const AudioBus bus) const {
    return gains_[checked_bus_index(bus)];
}

void NullAudioService::update(const float real_delta_ms) {
    if (!std::isfinite(real_delta_ms) || real_delta_ms < 0.0F) {
        throw std::invalid_argument("audio real delta must be finite and non-negative");
    }
}

bool NullAudioService::play_preview(DebugAudioPreviewRequest) {
    return false;
}

void NullAudioService::stop_preview() noexcept {}

DebugAudioPreviewStatus NullAudioService::preview_status() const {
    return {.state = DebugAudioPreviewState::Unavailable, .message = unavailable_reason_};
}

} // namespace d2::audio
