#include "d2engine/assets/debug_sound_catalog.hpp"
#include "d2res/mqdb.hpp"
#include "d2res/wdb_audio_payload.hpp"
#include "d2res/wdb_decoder.hpp"

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#ifndef DISCIPLES2_GAME_ROOT
#define DISCIPLES2_GAME_ROOT ""
#endif

namespace {

struct Candidate {
    std::string               logical_name;
    std::uint16_t             source_format_tag = 0;
    d2res::WdbPlaybackPayload payload;
};

std::filesystem::path game_root() {
    return std::filesystem::path{DISCIPLES2_GAME_ROOT};
}

std::string decoder_report() {
    std::ostringstream result;
    result << " decoders=";
    for (int index = 0; index < MIX_GetNumAudioDecoders(); ++index) {
        if (index != 0)
            result << ',';
        result << (MIX_GetAudioDecoder(index) != nullptr ? MIX_GetAudioDecoder(index) : "<null>");
    }
    return result.str();
}

std::string failure_report(const Candidate& candidate) {
    return "logical sound=" + candidate.logical_name +
           " source_format_tag=" + std::to_string(candidate.source_format_tag) + " encoding=" +
           (candidate.payload.encoding == d2res::WdbPlaybackEncoding::Wave ? "WAV" : "MP3") +
           " normalized_payload_size=" + std::to_string(candidate.payload.bytes.size()) +
           " SDL_GetError=" + SDL_GetError() + decoder_report();
}

std::optional<Candidate> find_candidate(const std::filesystem::path& path,
                                        const std::uint16_t          format_tag) {
    if (!std::filesystem::exists(path))
        return std::nullopt;
    const d2res::MqdbContainer container = d2res::MqdbContainer::open(path);
    const d2res::WdbDecoder    decoder(container);
    for (const auto& name : decoder.list_sounds()) {
        const d2res::DecodedSound decoded = decoder.decode_sound(name);
        if (!decoded.metadata.contains("format_tag") ||
            decoded.metadata["format_tag"].get<int>() != format_tag) {
            continue;
        }
        return Candidate{.logical_name = name,
                         .source_format_tag = format_tag,
                         .payload = d2res::make_wdb_playback_payload(decoded.payload)};
    }
    return std::nullopt;
}

class SdlMixerRealDataTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!MIX_Init())
            GTEST_SKIP() << "MIX_Init failed: " << SDL_GetError();
        initialized_ = true;
        const SDL_AudioSpec spec{.format = SDL_AUDIO_F32, .channels = 2, .freq = 48000};
        mixer_ = MIX_CreateMixer(&spec);
        ASSERT_NE(mixer_, nullptr) << SDL_GetError();
    }

    void TearDown() override {
        if (mixer_ != nullptr)
            MIX_DestroyMixer(mixer_);
        if (initialized_)
            MIX_Quit();
    }

    MIX_Audio* load(const Candidate& candidate) const {
        SDL_IOStream* io =
            SDL_IOFromConstMem(candidate.payload.bytes.data(), candidate.payload.bytes.size());
        if (io == nullptr)
            return nullptr;
        return MIX_LoadAudio_IO(mixer_, io, true, true);
    }

    MIX_Mixer* mixer_ = nullptr;
    bool       initialized_ = false;
};

TEST_F(SdlMixerRealDataTest, LoadsPcmWaveAfterNormalization) {
    const auto candidate = find_candidate(game_root() / "Sounds/AudioRgn.wdb", 1);
    if (!candidate.has_value())
        GTEST_SKIP() << "no PCM WAVE format-tag 1 entry available";
    ASSERT_EQ(candidate->payload.encoding, d2res::WdbPlaybackEncoding::Wave);
    ASSERT_EQ(candidate->payload.source_format_tag, 1);
    ASSERT_GE(candidate->payload.bytes.size(), 12U);
    EXPECT_EQ(std::string(candidate->payload.bytes.begin(), candidate->payload.bytes.begin() + 4),
              "RIFF");
    EXPECT_EQ(
        std::string(candidate->payload.bytes.begin() + 8, candidate->payload.bytes.begin() + 12),
        "WAVE");
    MIX_Audio* audio = load(*candidate);
    ASSERT_NE(audio, nullptr) << failure_report(*candidate);
    MIX_DestroyAudio(audio);
}

TEST_F(SdlMixerRealDataTest, LoadsMpegWaveDataAfterNormalization) {
    const auto candidate = find_candidate(game_root() / "Sounds/AudioRgn.wdb", 85);
    if (!candidate.has_value())
        GTEST_SKIP() << "no MPEG-in-WAVE format-tag 85 entry available";
    ASSERT_EQ(candidate->payload.encoding, d2res::WdbPlaybackEncoding::Mp3);
    ASSERT_EQ(candidate->payload.source_format_tag, 85);
    ASSERT_GE(candidate->payload.bytes.size(), 2U);
    EXPECT_NE(std::string(candidate->payload.bytes.begin(), candidate->payload.bytes.begin() + 2),
              "RI");
    EXPECT_EQ(candidate->payload.bytes[0], 0xFF);
    EXPECT_EQ(candidate->payload.bytes[1] & 0xE0U, 0xE0U);
    MIX_Audio* audio = load(*candidate);
    ASSERT_NE(audio, nullptr) << failure_report(*candidate);
    MIX_DestroyAudio(audio);
}

TEST_F(SdlMixerRealDataTest, LoadsReportedMidgardRegressionWhenAvailable) {
    d2engine::DebugSoundCatalog catalog{game_root()};
    const auto&                 bank = catalog.ensure_loaded(d2engine::DebugSoundSource::Midgard);
    if (bank.state == d2engine::DebugSoundLoadState::Missing)
        GTEST_SKIP() << bank.error;
    ASSERT_EQ(bank.state, d2engine::DebugSoundLoadState::Loaded) << bank.error;
    const auto selected = std::find_if(bank.sounds.begin(), bank.sounds.end(),
                                       [](const d2engine::DebugSoundEntry& sound) {
                                           return sound.logical_name == "G000UU0045IDLE1";
                                       });
    if (selected == bank.sounds.end())
        GTEST_SKIP() << "regression sound is absent";
    const auto encoded =
        catalog.load_encoded_sound(d2engine::DebugSoundSource::Midgard, selected->logical_name);
    ASSERT_EQ(encoded.detected_format, "MP3");
    ASSERT_GE(encoded.payload.size(), 2U);
    EXPECT_EQ(encoded.payload[0], 0xFF);
    EXPECT_EQ(encoded.payload[1] & 0xE0U, 0xE0U);
    Candidate  candidate{.logical_name = encoded.logical_name,
                         .source_format_tag = 85,
                         .payload = {.bytes = std::move(encoded.payload),
                                     .encoding = d2res::WdbPlaybackEncoding::Mp3,
                                     .source_format_tag = 85}};
    MIX_Audio* audio = load(candidate);
    ASSERT_NE(audio, nullptr) << failure_report(candidate);
    MIX_DestroyAudio(audio);
}

} // namespace
