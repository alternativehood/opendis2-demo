#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace {

std::string read_source(const std::filesystem::path& path) {
    std::ifstream const in(std::filesystem::path{OPENDIS2_SOURCE_DIR} / path);
    std::ostringstream  out;
    out << in.rdbuf();
    return out.str();
}

TEST(AudioArchitecture, ApplicationOwnsAndUpdatesAudioRuntime) {
    const auto header = read_source("src/d2engine/app/application.hpp");
    const auto source = read_source("src/d2engine/app/application.cpp");
    const auto owner = header.find("std::unique_ptr<d2::audio::AudioRuntime> audio_runtime_");
    ASSERT_NE(owner, std::string::npos);
    EXPECT_EQ(header.find("std::unique_ptr<d2::audio::AudioRuntime> audio_runtime_", owner + 1),
              std::string::npos);
    EXPECT_NE(source.find("d2::audio::create_audio_runtime()"), std::string::npos);
    const auto update = source.find("audio_runtime_->update(delta_ms)");
    ASSERT_NE(update, std::string::npos);
    EXPECT_EQ(source.find("audio_service_->update(delta_ms)", update + 1), std::string::npos);
    EXPECT_NE(source.find(".real_delta_ms = delta_ms"), std::string::npos);
    EXPECT_NE(header.find("debug_audio_preview()"), std::string::npos);
}

TEST(AudioArchitecture, AudioLayerStaysAnimationAndBackendFree) {
    const auto audio_dir = std::filesystem::path{OPENDIS2_SOURCE_DIR} / "src/d2engine/audio";
    for (const auto& entry : std::filesystem::recursive_directory_iterator(audio_dir)) {
        if (entry.is_regular_file()) {
            EXPECT_EQ(read_source(std::filesystem::relative(entry.path(), OPENDIS2_SOURCE_DIR))
                          .find("animation_delta_ms"),
                      std::string::npos);
        }
    }
    const auto contract = read_source("src/d2engine/audio/audio_service.hpp");
    for (const auto* forbidden : {"WDB", "SDL", "mixer", "codec", "filesystem"}) {
        EXPECT_EQ(contract.find(forbidden), std::string::npos) << forbidden;
    }
    const auto preview = read_source("src/d2engine/audio/debug_audio_preview.hpp");
    for (const auto* forbidden : {"WDB", "filesystem", "Mqdb", "WdbDecoder"}) {
        EXPECT_EQ(preview.find(forbidden), std::string::npos) << forbidden;
    }
}

TEST(AudioArchitecture, MixerSymbolsStayInTheRuntimeBackend) {
    const auto audio_dir = std::filesystem::path{OPENDIS2_SOURCE_DIR} / "src/d2engine/audio";
    for (const auto& entry : std::filesystem::recursive_directory_iterator(audio_dir)) {
        if (!entry.is_regular_file())
            continue;
        const auto source =
            read_source(std::filesystem::relative(entry.path(), OPENDIS2_SOURCE_DIR));
        if (entry.path().filename() != "sdl_mixer_audio_service.cpp" &&
            entry.path().filename() != "sdl_mixer_audio_service.hpp") {
            EXPECT_EQ(source.find("MIX_"), std::string::npos) << entry.path();
        }
        EXPECT_EQ(source.find(std::string{"Mix_"} + "OpenAudio"), std::string::npos);
        EXPECT_EQ(source.find(std::string{"Mix_"} + "LoadWAV"), std::string::npos);
    }
}

TEST(AudioArchitecture, DebugCatalogStaysOutOfGameplayAudio) {
    const auto application_header = read_source("src/d2engine/app/application.hpp");
    const auto application_source = read_source("src/d2engine/app/application.cpp");
    const auto owner = application_header.find("debug_sound_catalog_;");
    ASSERT_NE(owner, std::string::npos);
    EXPECT_EQ(application_header.find("debug_sound_catalog_;", owner + 1), std::string::npos);
    EXPECT_NE(application_source.find("std::make_unique<DebugSoundCatalog>(config_.game_root)"),
              std::string::npos);
    for (const auto* path :
         {"src/d2engine/app/adventure_screen.cpp", "src/d2engine/app/battle_screen.cpp"}) {
        const auto source = read_source(path);
        for (const auto* name : {"AudioRgn.wdb", "Battle.wdb", "Midgard.wdb", "Capital.wdb"}) {
            EXPECT_EQ(source.find(name), std::string::npos) << path << ": " << name;
        }
    }
}

} // namespace
