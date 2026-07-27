#include "d2engine/audio/null_audio_service.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>

namespace {

TEST(NullAudioService, HasSixRealBuses) {
    EXPECT_EQ(d2::audio::kAudioBusCount, 6U);
}

TEST(NullAudioService, UsesCanonicalBusCountStorage) {
    std::ifstream     input(std::filesystem::path{OPENDIS2_SOURCE_DIR} /
                            "src/d2engine/audio/null_audio_service.hpp");
    const std::string source{std::istreambuf_iterator<char>(input),
                             std::istreambuf_iterator<char>()};
    EXPECT_NE(source.find("std::array<float, kAudioBusCount> gains_{}"), std::string::npos);
    EXPECT_EQ(source.find("std::array<float, 6>"), std::string::npos);
    EXPECT_EQ(source.find("1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F"), std::string::npos);
}

TEST(NullAudioService, DefaultsEveryBusToFullGain) {
    d2::audio::NullAudioService service;
    for (std::size_t index = 0; index < d2::audio::kAudioBusCount; ++index) {
        const auto bus = static_cast<d2::audio::AudioBus>(index);
        EXPECT_FLOAT_EQ(service.bus_gain(bus), 1.0F);
    }
}

TEST(NullAudioService, AssignsAndClampsFiniteGains) {
    d2::audio::NullAudioService service;
    service.set_bus_gain(d2::audio::AudioBus::Music, 0.4F);
    service.set_bus_gain(d2::audio::AudioBus::Ambience, -1.0F);
    service.set_bus_gain(d2::audio::AudioBus::Sfx, 2.0F);
    EXPECT_FLOAT_EQ(service.bus_gain(d2::audio::AudioBus::Music), 0.4F);
    EXPECT_FLOAT_EQ(service.bus_gain(d2::audio::AudioBus::Ambience), 0.0F);
    EXPECT_FLOAT_EQ(service.bus_gain(d2::audio::AudioBus::Sfx), 1.0F);
}

TEST(NullAudioService, KeepsValidBusesIndependent) {
    d2::audio::NullAudioService service;
    service.set_bus_gain(d2::audio::AudioBus::Master, 0.2F);
    service.set_bus_gain(d2::audio::AudioBus::Music, 0.8F);
    EXPECT_FLOAT_EQ(service.bus_gain(d2::audio::AudioBus::Master), 0.2F);
    EXPECT_FLOAT_EQ(service.bus_gain(d2::audio::AudioBus::Music), 0.8F);
}

TEST(NullAudioService, RejectsSentinelBus) {
    d2::audio::NullAudioService service;
    EXPECT_THROW(service.set_bus_gain(d2::audio::AudioBus::Count, 0.5F), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(service.bus_gain(d2::audio::AudioBus::Count)),
                 std::invalid_argument);
}

TEST(NullAudioService, RejectsArbitraryInvalidBus) {
    d2::audio::NullAudioService service;
    constexpr auto              invalid_bus = static_cast<d2::audio::AudioBus>(255);
    EXPECT_THROW(service.set_bus_gain(invalid_bus, 0.5F), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(service.bus_gain(invalid_bus)), std::invalid_argument);
}

TEST(NullAudioService, RejectsNonFiniteGain) {
    d2::audio::NullAudioService service;
    EXPECT_THROW(
        service.set_bus_gain(d2::audio::AudioBus::Ui, std::numeric_limits<float>::quiet_NaN()),
        std::invalid_argument);
    EXPECT_THROW(
        service.set_bus_gain(d2::audio::AudioBus::Ui, std::numeric_limits<float>::infinity()),
        std::invalid_argument);
}

TEST(NullAudioService, AcceptsNonNegativeFiniteRealDelta) {
    d2::audio::NullAudioService service;
    EXPECT_NO_THROW(service.update(16.0F));
    EXPECT_NO_THROW(service.update(0.0F));
}

TEST(NullAudioService, RejectsInvalidRealDelta) {
    d2::audio::NullAudioService service;
    EXPECT_THROW(service.update(-0.1F), std::invalid_argument);
    EXPECT_THROW(service.update(std::numeric_limits<float>::quiet_NaN()), std::invalid_argument);
    EXPECT_THROW(service.update(std::numeric_limits<float>::infinity()), std::invalid_argument);
}

TEST(NullAudioService, CueAndStopAreSafe) {
    d2::audio::NullAudioService service;
    EXPECT_NO_THROW(service.play_one_shot({.id = "ui.confirm", .bus = d2::audio::AudioBus::Ui}));
    EXPECT_NO_THROW(service.stop_all());
}

TEST(NullAudioService, PreviewIsUnavailableAndRetainsReason) {
    d2::audio::NullAudioService service{"no playback device"};
    const auto                  status = service.preview_status();
    EXPECT_EQ(status.state, d2::audio::DebugAudioPreviewState::Unavailable);
    EXPECT_EQ(status.message, "no playback device");
    EXPECT_FALSE(service.play_preview({.display_name = "test", .encoded_payload = {1, 2, 3}}));
    EXPECT_NO_THROW(service.stop_preview());
}

} // namespace
