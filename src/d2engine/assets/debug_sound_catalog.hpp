#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace d2engine {

enum class DebugSoundSource {
    AudioRgn,
    Battle,
    Midgard,
    Capital,
    Count,
};

inline constexpr std::size_t kDebugSoundSourceCount =
    static_cast<std::size_t>(DebugSoundSource::Count);

enum class DebugSoundLoadState {
    NotLoaded,
    Loaded,
    Missing,
    Failed,
};

struct DebugSoundEntry {
    std::string   logical_name;
    std::uint64_t payload_size = 0;
};

struct DebugEncodedSound {
    std::string               logical_name;
    std::vector<std::uint8_t> payload;
    std::string               detected_format;
};

struct DebugSoundBank {
    DebugSoundSource             source;
    std::string                  display_name;
    std::filesystem::path        relative_path;
    DebugSoundLoadState          state = DebugSoundLoadState::NotLoaded;
    std::vector<DebugSoundEntry> sounds;
    std::string                  error;
};

class DebugSoundCatalog final {
public:
    explicit DebugSoundCatalog(std::filesystem::path game_root);

    [[nodiscard]] std::span<const DebugSoundBank> banks() const noexcept;
    [[nodiscard]] const DebugSoundBank&           bank(DebugSoundSource source) const;
    [[nodiscard]] const DebugSoundBank&           ensure_loaded(DebugSoundSource source);
    [[nodiscard]] DebugEncodedSound               load_encoded_sound(DebugSoundSource source,
                                                                     std::string_view logical_name);

private:
    std::filesystem::path                              game_root_;
    std::array<DebugSoundBank, kDebugSoundSourceCount> banks_;
};

} // namespace d2engine
