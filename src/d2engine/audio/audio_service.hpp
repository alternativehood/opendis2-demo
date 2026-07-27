#pragma once

#include <cstddef>
#include <string>

namespace d2::audio {

enum class AudioBus {
    Master,
    Music,
    Ambience,
    Sfx,
    Ui,
    Voice,
    Count,
};

inline constexpr std::size_t kAudioBusCount = static_cast<std::size_t>(AudioBus::Count);

struct AudioCue {
    std::string id;
    AudioBus    bus = AudioBus::Sfx;
};

class AudioService {
public:
    virtual ~AudioService() = default;

    virtual void                play_one_shot(const AudioCue& cue) = 0;
    virtual void                stop_all() = 0;
    virtual void                set_bus_gain(AudioBus bus, float gain) = 0;
    [[nodiscard]] virtual float bus_gain(AudioBus bus) const = 0;
    virtual void                update(float real_delta_ms) = 0;
};

} // namespace d2::audio
