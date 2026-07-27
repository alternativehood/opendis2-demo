#include "animation_manifest.hpp"

#include "asset_error.hpp"
#include "asset_id.hpp"

#include <array>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string_view>

#include <nlohmann/json.hpp>

namespace d2asset {
namespace {

namespace fs = std::filesystem;
using Json = nlohmann::json;

constexpr std::uint32_t kFallbackFrameDelayMs = 100;

[[noreturn]] void malformed_animation(const std::string& message, const std::string& context,
                                      const fs::path& path) {
    throw AssetError(AssetErrorCode::MalformedAnimation, message, context, path);
}

const Json& require_field(const Json& object, const char* field, Json::value_t type,
                          const std::string& context, const fs::path& path) {
    if (!object.is_object())
        malformed_animation("animation value must be an object", context, path);
    const auto it = object.find(field);
    if (it == object.end() || it->type() != type) {
        malformed_animation("required animation field has invalid type", context + "." + field,
                            path);
    }
    return *it;
}

std::string require_non_empty_string(const Json& object, const char* field,
                                     const std::string& context, const fs::path& path) {
    const std::string value =
        require_field(object, field, Json::value_t::string, context, path).get<std::string>();
    if (value.empty())
        malformed_animation("required animation string is empty", context + "." + field, path);
    return value;
}

std::uint32_t require_non_negative_integer(const Json& object, const char* field,
                                           const std::string& context, const fs::path& path) {
    const auto it = object.find(field);
    if (it == object.end() || (!it->is_number_integer() && !it->is_number_unsigned())) {
        malformed_animation("required animation field must be an integer", context + "." + field,
                            path);
    }
    if (it->is_number_unsigned()) {
        const std::uint64_t value = it->get<std::uint64_t>();
        if (value > std::numeric_limits<std::uint32_t>::max()) {
            malformed_animation("animation integer is outside the supported range",
                                context + "." + field, path);
        }
        return static_cast<std::uint32_t>(value);
    }
    const std::int64_t value = it->get<std::int64_t>();
    if (value < 0 || std::cmp_greater(value, std::numeric_limits<std::uint32_t>::max())) {
        malformed_animation("animation integer is outside the supported range",
                            context + "." + field, path);
    }
    return static_cast<std::uint32_t>(value);
}

std::uint32_t require_positive_integer(const Json& object, const char* field,
                                       const std::string& context, const fs::path& path) {
    const std::uint32_t value = require_non_negative_integer(object, field, context, path);
    if (value == 0)
        malformed_animation("animation integer must be positive", context + "." + field, path);
    return value;
}

FrameTiming read_timing(const Json& root, AnimationClip& clip) {
    const auto delay = root.find("frame_delay_ms");
    if (delay != root.end() && (delay->is_number_integer() || delay->is_number_unsigned())) {
        if (delay->is_number_unsigned()) {
            const std::uint64_t value = delay->get<std::uint64_t>();
            if (value > 0 && value <= std::numeric_limits<std::uint32_t>::max()) {
                return {.duration_ms = static_cast<std::uint32_t>(value),
                        .source = TimingSource::ProvisionalSidecar};
            }
        } else {
            const std::int64_t value = delay->get<std::int64_t>();
            if (value > 0 && !std::cmp_greater(value, std::numeric_limits<std::uint32_t>::max())) {
                return {.duration_ms = static_cast<std::uint32_t>(value),
                        .source = TimingSource::ProvisionalSidecar};
            }
        }
    }

    clip.warnings.push_back(
        {.animation_asset_id = clip.animation_asset_id,
         .frame_index = std::nullopt,
         .logical_name = {},
         .message = "frame_delay_ms is absent, malformed, or non-positive; using 100 ms fallback",
         .matching_asset_ids = {}});
    return {.duration_ms = kFallbackFrameDelayMs, .source = TimingSource::FallbackDefault};
}

fs::path frame_path(const fs::path& sidecar_path, std::size_t index) {
    std::ostringstream filename;
    filename << "frame_" << std::setfill('0') << std::setw(3) << index << ".png";
    return sidecar_path.parent_path() / filename.str();
}

AnimationClassification classify(std::string_view logical_name) {
    struct Rule {
        std::string_view token;
        AnimationRole    role{AnimationRole::Unknown};
    };
    static constexpr std::array<Rule, 14> rules{{
        {.token = "death", .role = AnimationRole::Death},
        {.token = "dead", .role = AnimationRole::Death},
        {.token = "die", .role = AnimationRole::Death},
        {.token = "attack", .role = AnimationRole::Attack},
        {.token = "atk", .role = AnimationRole::Attack},
        {.token = "defend", .role = AnimationRole::Defend},
        {.token = "block", .role = AnimationRole::Defend},
        {.token = "cast", .role = AnimationRole::Cast},
        {.token = "spell", .role = AnimationRole::Cast},
        {.token = "idle", .role = AnimationRole::Idle},
        {.token = "move", .role = AnimationRole::Move},
        {.token = "walk", .role = AnimationRole::Move},
        {.token = "run", .role = AnimationRole::Move},
        {.token = "hit", .role = AnimationRole::Hit},
    }};

    const std::string normalized = normalize_ascii(logical_name);
    for (const Rule& rule : rules) {
        if (normalized.find(rule.token) != std::string::npos) {
            return {.role = rule.role,
                    .matched_token = std::string(rule.token),
                    .reason = "animation logical name contains documented role token"};
        }
    }
    return {.role = AnimationRole::Unknown,
            .matched_token = {},
            .reason = "animation logical name contains no documented role token"};
}

} // namespace

AnimationClip AnimationManifest::load(const fs::path& asset_root, const fs::path& sidecar_path,
                                      const std::string& animation_asset_id,
                                      const std::string& logical_name,
                                      const std::string& container_id) {
    std::ifstream input(asset_root / sidecar_path);
    if (!input) {
        throw AssetError(AssetErrorCode::MalformedAnimation, "animation sidecar cannot be opened",
                         animation_asset_id, sidecar_path);
    }

    Json root;
    try {
        input >> root;
    } catch (const Json::exception& error) {
        throw AssetError(AssetErrorCode::MalformedAnimation,
                         std::string("invalid animation JSON: ") + error.what(), animation_asset_id,
                         sidecar_path);
    }
    if (!root.is_object())
        malformed_animation("animation root must be an object", animation_asset_id, sidecar_path);

    AnimationClip clip{
        .animation_asset_id = animation_asset_id,
        .logical_name = logical_name,
        .container_id = container_id,
        .sidecar_path = sidecar_path,
        .frames = {},
        .loop_mode = LoopMode::Unknown,
        .classification = classify(logical_name),
        .facing_direction = FacingDirection::Unknown,
        .warnings = {},
    };

    const std::string sidecar_name =
        require_non_empty_string(root, "name", animation_asset_id, sidecar_path);
    if (normalize_ascii(sidecar_name) != normalize_ascii(logical_name)) {
        malformed_animation("animation sidecar name does not match manifest logical name",
                            animation_asset_id + ".name", sidecar_path);
    }

    const std::uint32_t frame_count =
        require_non_negative_integer(root, "frame_count", animation_asset_id, sidecar_path);
    const Json& frames =
        require_field(root, "frames", Json::value_t::array, animation_asset_id, sidecar_path);
    if (frame_count != frames.size()) {
        malformed_animation("frame_count does not match frames count",
                            animation_asset_id + ".frame_count", sidecar_path);
    }

    const FrameTiming timing = read_timing(root, clip);
    clip.frames.reserve(frames.size());
    for (std::size_t i = 0; i < frames.size(); ++i) {
        const std::string   context = animation_asset_id + ".frames[" + std::to_string(i) + "]";
        const Json&         frame = frames[i];
        const std::uint32_t index =
            require_non_negative_integer(frame, "index", context, sidecar_path);
        if (index != i) {
            malformed_animation("animation frame indexes must be contiguous and ordered",
                                context + ".index", sidecar_path);
        }

        AnimationFrameRef ref{
            .index = i,
            .logical_name = require_non_empty_string(frame, "logical_name", context, sidecar_path),
            .source_size =
                {
                    .width = require_positive_integer(frame, "width", context, sidecar_path),
                    .height = require_positive_integer(frame, "height", context, sidecar_path),
                },
            .timing = timing,
            .image_asset_id = std::nullopt,
            .texture_region = std::nullopt,
            .fallback_path = std::nullopt,
        };
        const fs::path  fallback = frame_path(sidecar_path, i);
        std::error_code ec;
        if (fs::is_regular_file(asset_root / fallback, ec))
            ref.fallback_path = fallback;
        clip.frames.push_back(std::move(ref));
    }
    return clip;
}

} // namespace d2asset
