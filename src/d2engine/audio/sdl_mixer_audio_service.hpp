#pragma once

#include "audio_runtime.hpp"

#include <SDL3_mixer/SDL_mixer.h>

#include <array>

namespace d2::audio {

class SdlMixerAudioService final : public AudioRuntime {
public:
    SdlMixerAudioService();
    ~SdlMixerAudioService() override;

    SdlMixerAudioService(const SdlMixerAudioService&) = delete;
    SdlMixerAudioService& operator=(const SdlMixerAudioService&) = delete;

    void                                  play_one_shot(const AudioCue& cue) override;
    void                                  stop_all() override;
    void                                  set_bus_gain(AudioBus bus, float gain) override;
    [[nodiscard]] float                   bus_gain(AudioBus bus) const override;
    void                                  update(float real_delta_ms) override;
    [[nodiscard]] bool                    play_preview(DebugAudioPreviewRequest request) override;
    void                                  stop_preview() noexcept override;
    [[nodiscard]] DebugAudioPreviewStatus preview_status() const override;

private:
    void destroy_current_audio() noexcept;
    void fail(std::string message) noexcept;
    void apply_preview_gain() noexcept;

    MIX_Mixer*                        mixer_ = nullptr;
    MIX_Track*                        preview_track_ = nullptr;
    MIX_Audio*                        current_audio_ = nullptr;
    std::array<float, kAudioBusCount> gains_{};
    DebugAudioPreviewStatus           status_{};
    bool                              mixer_initialized_ = false;
};

} // namespace d2::audio
