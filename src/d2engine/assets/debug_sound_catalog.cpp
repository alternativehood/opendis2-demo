#include "debug_sound_catalog.hpp"

#include "d2res/mqdb.hpp"
#include "d2res/wdb_decoder.hpp"
#include "d2res/wdb_audio_payload.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace d2engine {
namespace {

std::size_t checked_source_index(const DebugSoundSource source) {
    const auto index = static_cast<std::size_t>(source);
    if (index >= kDebugSoundSourceCount) {
        throw std::invalid_argument("invalid debug sound source");
    }
    return index;
}

bool case_insensitive_less(const DebugSoundEntry& lhs, const DebugSoundEntry& rhs) {
    return std::lexicographical_compare(lhs.logical_name.begin(), lhs.logical_name.end(),
                                        rhs.logical_name.begin(), rhs.logical_name.end(),
                                        [](const unsigned char left, const unsigned char right) {
                                            return std::tolower(left) < std::tolower(right);
                                        });
}

DebugSoundBank make_debug_sound_bank(const DebugSoundSource source, const char* display_name,
                                     const char* relative_path) {
    DebugSoundBank bank{};
    bank.source = source;
    bank.display_name = display_name;
    bank.relative_path = relative_path;
    return bank;
}

} // namespace

DebugSoundCatalog::DebugSoundCatalog(std::filesystem::path game_root)
    : game_root_(std::move(game_root)),
      banks_{{make_debug_sound_bank(DebugSoundSource::AudioRgn, "AudioRgn", "Sounds/AudioRgn.wdb"),
              make_debug_sound_bank(DebugSoundSource::Battle, "Battle", "Sounds/Battle.wdb"),
              make_debug_sound_bank(DebugSoundSource::Midgard, "Midgard", "Sounds/Midgard.wdb"),
              make_debug_sound_bank(DebugSoundSource::Capital, "Capital", "Sounds/Capital.wdb")}} {}

std::span<const DebugSoundBank> DebugSoundCatalog::banks() const noexcept {
    return banks_;
}

const DebugSoundBank& DebugSoundCatalog::bank(const DebugSoundSource source) const {
    return banks_[checked_source_index(source)];
}

const DebugSoundBank& DebugSoundCatalog::ensure_loaded(const DebugSoundSource source) {
    DebugSoundBank& result = banks_[checked_source_index(source)];
    if (result.state != DebugSoundLoadState::NotLoaded) {
        return result;
    }

    const std::filesystem::path        path = game_root_ / result.relative_path;
    std::error_code                    error_code;
    const std::filesystem::file_status status = std::filesystem::status(path, error_code);
    if (error_code == std::errc::no_such_file_or_directory ||
        status.type() == std::filesystem::file_type::not_found) {
        result.state = DebugSoundLoadState::Missing;
        result.sounds.clear();
        result.error = "missing sound bank: " + path.string();
        return result;
    }
    if (error_code) {
        result.state = DebugSoundLoadState::Failed;
        result.sounds.clear();
        result.error = "sound bank " + path.string() + ": " + error_code.message();
        return result;
    }
    if (!std::filesystem::is_regular_file(status)) {
        result.state = DebugSoundLoadState::Failed;
        result.sounds.clear();
        result.error = "sound bank " + path.string() + ": not a regular file";
        return result;
    }

    try {
        const d2res::MqdbContainer container = d2res::MqdbContainer::open(path);
        const d2res::WdbDecoder    decoder(container);
        for (const std::string& logical_name : decoder.list_sounds()) {
            if (logical_name == "<unnamed>") {
                continue;
            }
            const auto record = container.find_by_name(logical_name);
            if (!record.has_value()) {
                throw std::runtime_error("sound record not found: " + logical_name);
            }
            result.sounds.push_back({.logical_name = logical_name,
                                     .payload_size = static_cast<std::uint64_t>(
                                         container.payload_view(record->index).size())});
        }
        std::ranges::sort(result.sounds, case_insensitive_less);
        result.state = DebugSoundLoadState::Loaded;
        result.error.clear();
    } catch (const std::exception& exception) {
        result.state = DebugSoundLoadState::Failed;
        result.sounds.clear();
        result.error = "sound bank " + path.string() + ": " + exception.what();
    }
    return result;
}

DebugEncodedSound DebugSoundCatalog::load_encoded_sound(const DebugSoundSource source,
                                                        const std::string_view logical_name) {
    const DebugSoundBank& loaded = ensure_loaded(source);
    const auto            path = game_root_ / loaded.relative_path;
    if (loaded.state != DebugSoundLoadState::Loaded) {
        throw std::runtime_error("cannot load sound '" + std::string(logical_name) + "' from " +
                                 loaded.display_name + " (" + path.string() +
                                 "): bank is not loaded");
    }
    const auto selected =
        std::ranges::find(loaded.sounds, logical_name, &DebugSoundEntry::logical_name);
    if (selected == loaded.sounds.end()) {
        throw std::runtime_error("unknown sound '" + std::string(logical_name) + "' from " +
                                 loaded.display_name + " (" + path.string() + ")");
    }
    try {
        const d2res::MqdbContainer container = d2res::MqdbContainer::open(path);
        d2res::DecodedSound       decoded = d2res::WdbDecoder(container).decode_sound(logical_name);
        d2res::WdbPlaybackPayload playable = d2res::make_wdb_playback_payload(decoded.payload);
        const char*               detected_format =
            playable.encoding == d2res::WdbPlaybackEncoding::Wave ? "WAV" : "MP3";
        return {.logical_name = std::move(decoded.logical_name),
                .payload = std::move(playable.bytes),
                .detected_format = detected_format};
    } catch (const std::exception& exception) {
        throw std::runtime_error("cannot load sound '" + std::string(logical_name) + "' from " +
                                 loaded.display_name + " (" + path.string() +
                                 "): " + exception.what());
    }
}

} // namespace d2engine
