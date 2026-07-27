#pragma once

#include "audio_runtime.hpp"

#include <array>

namespace d2::audio {

class NullAudioService final : public AudioRuntime {
public:
    explicit NullAudioService(std::string unavailable_reason = "audio backend is not available");

    void                                  play_one_shot(const AudioCue& cue) override;
    void                                  stop_all() override;
    void                                  set_bus_gain(AudioBus bus, float gain) override;
    [[nodiscard]] float                   bus_gain(AudioBus bus) const override;
    void                                  update(float real_delta_ms) override;
    [[nodiscard]] bool                    play_preview(DebugAudioPreviewRequest request) override;
    void                                  stop_preview() noexcept override;
    [[nodiscard]] DebugAudioPreviewStatus preview_status() const override;

private:
    std::array<float, kAudioBusCount> gains_{};
    std::string                       unavailable_reason_;
};

} // namespace d2::audio
