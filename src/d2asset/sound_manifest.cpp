#include "sound_manifest.hpp"

#include "asset_error.hpp"
#include "asset_id.hpp"

#include <fstream>
#include <limits>
#include <string_view>

#include <nlohmann/json.hpp>

namespace d2asset {
namespace {

namespace fs = std::filesystem;
using Json = nlohmann::json;

[[noreturn]] void malformed_sound(const std::string& message, const std::string& context,
                                  const fs::path& path) {
    throw AssetError(AssetErrorCode::MalformedSound, message, context, path);
}

bool safe_relative_path(const fs::path& path) {
    if (path.empty() || path.is_absolute())
        return false;
    const fs::path normalized = path.lexically_normal();
    if (normalized.empty() || normalized == ".")
        return false;
    for (const auto& component : normalized) {
        if (component == "..")
            return false;
    }
    return true;
}

const Json& require_field(const Json& object, std::string_view field, Json::value_t type,
                          const std::string& context, const fs::path& path) {
    if (!object.is_object())
        malformed_sound("sound value must be an object", context, path);
    const auto it = object.find(field);
    if (it == object.end() || it->type() != type) {
        malformed_sound("required sound field has invalid type", context + "." + std::string(field),
                        path);
    }
    return *it;
}

std::string require_string(const Json& object, std::string_view field, const std::string& context,
                           const fs::path& path) {
    const std::string value =
        require_field(object, field, Json::value_t::string, context, path).get<std::string>();
    if (value.empty())
        malformed_sound("required sound string is empty", context + "." + std::string(field), path);
    return value;
}

std::uint64_t integer_value(const Json& value, const std::string& context, const fs::path& path) {
    if (!value.is_number_integer() && !value.is_number_unsigned())
        malformed_sound("sound numeric field must be an integer", context, path);
    if (value.is_number_unsigned())
        return value.get<std::uint64_t>();
    const std::int64_t number = value.get<std::int64_t>();
    if (number < 0)
        malformed_sound("sound numeric field must not be negative", context, path);
    return static_cast<std::uint64_t>(number);
}

std::uint64_t require_positive_u64(const Json& object, std::string_view field,
                                   const std::string& context, const fs::path& path) {
    const auto it = object.find(field);
    if (it == object.end()) {
        malformed_sound("required sound field is missing", context + "." + std::string(field),
                        path);
    }
    const std::uint64_t value = integer_value(*it, context + "." + std::string(field), path);
    if (value == 0) {
        malformed_sound("sound numeric field must be positive", context + "." + std::string(field),
                        path);
    }
    return value;
}

template <typename T>
std::optional<T> optional_positive_integer(const Json& object, std::string_view field,
                                           const std::string& context, const fs::path& path,
                                           SoundAsset& sound) {
    const auto it = object.find(field);
    if (it == object.end() || it->is_null())
        return std::nullopt;
    const std::uint64_t value = integer_value(*it, context + "." + std::string(field), path);
    if (value == 0) {
        sound.warnings.push_back(
            {.sound_asset_id = sound.sound_asset_id,
             .message = std::string(field) + " is zero and is treated as unavailable"});
        return std::nullopt;
    }
    if (value > static_cast<std::uint64_t>(std::numeric_limits<T>::max())) {
        malformed_sound("sound numeric field is outside the supported range",
                        context + "." + std::string(field), path);
    }
    return static_cast<T>(value);
}

void validate_identity(const std::string& actual, const std::string& expected,
                       std::string_view field, const std::string& context, const fs::path& path,
                       bool case_insensitive) {
    const bool matches = case_insensitive ? normalize_ascii(actual) == normalize_ascii(expected)
                                          : actual == expected;
    if (!matches) {
        malformed_sound("sound sidecar identity does not match manifest",
                        context + "." + std::string(field), path);
    }
}

} // namespace

const char* to_string(SoundFormat value) noexcept {
    switch (value) {
    case SoundFormat::Wave:
        return "wave";
    case SoundFormat::Unknown:
        return "unknown";
    }
    return "unknown";
}

SoundAsset SoundManifest::load(const fs::path& asset_root, const fs::path& sidecar_path,
                               const std::string& sound_asset_id, const std::string& logical_name,
                               const std::string& container_id) {
    std::ifstream input(asset_root / sidecar_path);
    if (!input) {
        throw AssetError(AssetErrorCode::MalformedSound, "sound sidecar cannot be opened",
                         sound_asset_id, sidecar_path);
    }

    Json root;
    try {
        input >> root;
    } catch (const Json::exception& error) {
        throw AssetError(AssetErrorCode::MalformedSound,
                         std::string("invalid sound JSON: ") + error.what(), sound_asset_id,
                         sidecar_path);
    }
    if (!root.is_object())
        malformed_sound("sound root must be an object", sound_asset_id, sidecar_path);

    const std::uint64_t version =
        require_positive_u64(root, "sound_schema_version", sound_asset_id, sidecar_path);
    if (version != sound_schema_version) {
        throw AssetError(AssetErrorCode::UnsupportedSoundSchema,
                         "unsupported sound sidecar schema version",
                         sound_asset_id + ".sound_schema_version", sidecar_path);
    }

    SoundAsset sound{
        .sound_asset_id = sound_asset_id,
        .logical_name = logical_name,
        .container_id = container_id,
        .sidecar_path = sidecar_path,
        .payload_path = {},
        .payload_size = 0,
        .format = SoundFormat::Unknown,
        .format_tag = std::nullopt,
        .channels = std::nullopt,
        .sample_rate = std::nullopt,
        .bit_depth = std::nullopt,
        .duration_ms = std::nullopt,
        .warnings = {},
    };
    validate_identity(require_string(root, "asset_id", sound_asset_id, sidecar_path),
                      sound_asset_id, "asset_id", sound_asset_id, sidecar_path, false);
    validate_identity(require_string(root, "logical_name", sound_asset_id, sidecar_path),
                      logical_name, "logical_name", sound_asset_id, sidecar_path, true);
    validate_identity(require_string(root, "container_id", sound_asset_id, sidecar_path),
                      container_id, "container_id", sound_asset_id, sidecar_path, false);

    sound.payload_path = require_string(root, "payload_path", sound_asset_id, sidecar_path);
    if (!safe_relative_path(sound.payload_path) ||
        sound.payload_path.string().find('\\') != std::string::npos ||
        sound.payload_path.generic_string() != sound.payload_path.string()) {
        throw AssetError(AssetErrorCode::UnsafePath,
                         "sound payload path must be a safe forward-slash package-relative path",
                         sound_asset_id + ".payload_path", sound.payload_path);
    }
    sound.payload_size = require_positive_u64(root, "payload_size", sound_asset_id, sidecar_path);

    const std::string format =
        require_string(root, "detected_format", sound_asset_id, sidecar_path);
    if (format == "wave") {
        sound.format = SoundFormat::Wave;
    } else if (format == "unknown") {
        sound.format = SoundFormat::Unknown;
        sound.warnings.push_back(
            {.sound_asset_id = sound_asset_id, .message = "sound payload format is unknown"});
    } else {
        malformed_sound("detected_format must be wave or unknown",
                        sound_asset_id + ".detected_format", sidecar_path);
    }

    sound.format_tag = optional_positive_integer<std::uint32_t>(root, "format_tag", sound_asset_id,
                                                                sidecar_path, sound);
    sound.channels = optional_positive_integer<std::uint32_t>(root, "channels", sound_asset_id,
                                                              sidecar_path, sound);
    sound.sample_rate = optional_positive_integer<std::uint32_t>(
        root, "sample_rate", sound_asset_id, sidecar_path, sound);
    sound.bit_depth = optional_positive_integer<std::uint32_t>(root, "bit_depth", sound_asset_id,
                                                               sidecar_path, sound);
    sound.duration_ms = optional_positive_integer<std::uint64_t>(
        root, "duration_ms", sound_asset_id, sidecar_path, sound);
    if (sound.format == SoundFormat::Wave && sound.format_tag.has_value() &&
        *sound.format_tag != 1U && *sound.format_tag != 85U) {
        sound.warnings.push_back(
            {.sound_asset_id = sound_asset_id,
             .message = "WAVE format tag is preserved but runtime playback support is unknown"});
    }

    const auto warnings = root.find("warnings");
    if (warnings == root.end() || !warnings->is_array())
        malformed_sound("warnings must be an array", sound_asset_id + ".warnings", sidecar_path);
    for (std::size_t index = 0; index < warnings->size(); ++index) {
        if (!(*warnings)[index].is_string()) {
            malformed_sound("sound warning must be a string",
                            sound_asset_id + ".warnings[" + std::to_string(index) + "]",
                            sidecar_path);
        }
        sound.warnings.push_back(
            {.sound_asset_id = sound_asset_id, .message = (*warnings)[index].get<std::string>()});
    }

    std::error_code ec;
    const fs::path  payload = asset_root / sound.payload_path;
    if (!fs::is_regular_file(payload, ec)) {
        throw AssetError(AssetErrorCode::MissingSoundPayload, "sound payload is missing",
                         sound_asset_id, sound.payload_path);
    }
    const std::uintmax_t actual_size = fs::file_size(payload, ec);
    if (ec || actual_size != sound.payload_size) {
        throw AssetError(AssetErrorCode::SoundPayloadSizeMismatch,
                         "sound payload size does not match sidecar", sound_asset_id,
                         sound.payload_path);
    }
    return sound;
}

} // namespace d2asset
