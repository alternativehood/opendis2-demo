#include "sdl_mixer_audio_service.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

namespace d2::audio {
namespace {
std::size_t checked_bus_index(const AudioBus bus) {
    const auto index = static_cast<std::size_t>(bus);
    if (index >= kAudioBusCount) {
        throw std::invalid_argument("invalid audio bus");
    }
    return index;
}

std::string sdl_error(const char* operation) {
    return std::string(operation) + ": " + SDL_GetError();
}
} // namespace

SdlMixerAudioService::SdlMixerAudioService() {
    gains_.fill(1.0F);
    try {
        if (!MIX_Init()) {
            throw std::runtime_error(sdl_error("MIX_Init failed"));
        }
        mixer_initialized_ = true;
        mixer_ = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
        if (mixer_ == nullptr) {
            throw std::runtime_error(sdl_error("MIX_CreateMixerDevice failed"));
        }
        preview_track_ = MIX_CreateTrack(mixer_);
        if (preview_track_ == nullptr) {
            throw std::runtime_error(sdl_error("MIX_CreateTrack failed"));
        }
        if (!MIX_SetMixerGain(mixer_, gains_[static_cast<std::size_t>(AudioBus::Master)])) {
            throw std::runtime_error(sdl_error("MIX_SetMixerGain failed"));
        }
        status_.state = DebugAudioPreviewState::Stopped;
    } catch (...) {
        destroy_current_audio();
        if (preview_track_ != nullptr) {
            MIX_StopTrack(preview_track_, 0);
            MIX_SetTrackAudio(preview_track_, nullptr);
            MIX_DestroyTrack(preview_track_);
        }
        if (mixer_ != nullptr)
            MIX_DestroyMixer(mixer_);
        if (mixer_initialized_)
            MIX_Quit();
        preview_track_ = nullptr;
        mixer_ = nullptr;
        mixer_initialized_ = false;
        throw;
    }
}

SdlMixerAudioService::~SdlMixerAudioService() {
    if (preview_track_ != nullptr) {
        MIX_StopTrack(preview_track_, 0);
        MIX_SetTrackAudio(preview_track_, nullptr);
    }
    destroy_current_audio();
    if (preview_track_ != nullptr) {
        MIX_DestroyTrack(preview_track_);
    }
    if (mixer_ != nullptr) {
        MIX_DestroyMixer(mixer_);
    }
    if (mixer_initialized_) {
        MIX_Quit();
    }
}

void SdlMixerAudioService::play_one_shot(const AudioCue&) {
    // Semantic cue resolution is not implemented yet. Debug preview uses the separate
    // DebugAudioPreview interface.
}

void SdlMixerAudioService::stop_all() {
    stop_preview();
}

void SdlMixerAudioService::set_bus_gain(const AudioBus bus, const float gain) {
    if (!std::isfinite(gain)) {
        throw std::invalid_argument("audio bus gain must be finite");
    }
    const auto index = checked_bus_index(bus);
    gains_[index] = std::clamp(gain, 0.0F, 1.0F);
    if (bus == AudioBus::Master) {
        if (!MIX_SetMixerGain(mixer_, gains_[index])) {
            fail(sdl_error("MIX_SetMixerGain failed"));
        }
    } else if (status_.state == DebugAudioPreviewState::Playing && status_.bus == bus) {
        apply_preview_gain();
    }
}

float SdlMixerAudioService::bus_gain(const AudioBus bus) const {
    return gains_[checked_bus_index(bus)];
}

void SdlMixerAudioService::update(const float real_delta_ms) {
    if (!std::isfinite(real_delta_ms) || real_delta_ms < 0.0F) {
        throw std::invalid_argument("audio real delta must be finite and non-negative");
    }
    if (status_.state == DebugAudioPreviewState::Playing && !MIX_TrackPlaying(preview_track_)) {
        MIX_SetTrackAudio(preview_track_, nullptr);
        destroy_current_audio();
        status_.state = DebugAudioPreviewState::Stopped;
        status_.current_name.clear();
        status_.message.clear();
    }
}

bool SdlMixerAudioService::play_preview(DebugAudioPreviewRequest request) {
    std::size_t bus_index = 0;
    try {
        bus_index = checked_bus_index(request.bus);
    } catch (const std::invalid_argument& exception) {
        fail(exception.what());
        return false;
    }
    if (request.encoded_payload.empty()) {
        fail("preview payload is empty");
        return false;
    }
    SDL_IOStream* io =
        SDL_IOFromConstMem(request.encoded_payload.data(), request.encoded_payload.size());
    if (io == nullptr) {
        fail(sdl_error("SDL_IOFromConstMem failed"));
        return false;
    }
    MIX_Audio* new_audio = MIX_LoadAudio_IO(mixer_, io, true, true);
    if (new_audio == nullptr) {
        fail(sdl_error("MIX_LoadAudio_IO failed"));
        return false;
    }
    SDL_PropertiesID options = SDL_CreateProperties();
    if (options == 0) {
        MIX_DestroyAudio(new_audio);
        fail(sdl_error("SDL_CreateProperties failed"));
        return false;
    }
    MIX_StopTrack(preview_track_, 0);
    MIX_SetTrackAudio(preview_track_, nullptr);
    destroy_current_audio();
    if (!MIX_SetTrackAudio(preview_track_, new_audio)) {
        SDL_DestroyProperties(options);
        MIX_DestroyAudio(new_audio);
        fail(sdl_error("MIX_SetTrackAudio failed"));
        return false;
    }
    current_audio_ = new_audio;
    status_ = {.state = DebugAudioPreviewState::Playing,
               .current_name = std::move(request.display_name),
               .bus = request.bus,
               .loop = request.loop};
    SDL_SetNumberProperty(options, MIX_PROP_PLAY_LOOPS_NUMBER, request.loop ? -1 : 0);
    if (!MIX_SetTrackGain(preview_track_, gains_[bus_index]) ||
        !MIX_PlayTrack(preview_track_, options)) {
        const std::string error = sdl_error("preview playback failed");
        SDL_DestroyProperties(options);
        MIX_SetTrackAudio(preview_track_, nullptr);
        destroy_current_audio();
        fail(error);
        return false;
    }
    SDL_DestroyProperties(options);
    return true;
}

void SdlMixerAudioService::stop_preview() noexcept {
    if (preview_track_ != nullptr) {
        MIX_StopTrack(preview_track_, 0);
        MIX_SetTrackAudio(preview_track_, nullptr);
    }
    destroy_current_audio();
    status_.state = DebugAudioPreviewState::Stopped;
    status_.current_name.clear();
    status_.message.clear();
}

DebugAudioPreviewStatus SdlMixerAudioService::preview_status() const {
    return status_;
}

void SdlMixerAudioService::destroy_current_audio() noexcept {
    if (current_audio_ != nullptr) {
        MIX_DestroyAudio(current_audio_);
        current_audio_ = nullptr;
    }
}

void SdlMixerAudioService::fail(std::string message) noexcept {
    status_.state = DebugAudioPreviewState::Failed;
    status_.message = std::move(message);
}

void SdlMixerAudioService::apply_preview_gain() noexcept {
    if (preview_track_ != nullptr && status_.bus != AudioBus::Master &&
        !MIX_SetTrackGain(preview_track_, gains_[static_cast<std::size_t>(status_.bus)])) {
        fail(sdl_error("MIX_SetTrackGain failed"));
    }
}

} // namespace d2::audio
