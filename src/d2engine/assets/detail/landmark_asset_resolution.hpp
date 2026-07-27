#pragma once

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <d2adventure_render/terrain/landmark_asset_id.hpp>

namespace d2engine::detail {

// ── Shared types ─────────────────────────────────────────────────────────

enum class LandmarkAssetKind { StaticSprite, Animation };

struct LandmarkAssetCandidate {
    std::string       container_path;
    std::string       logical_name;
    LandmarkAssetKind kind;
};

struct ResolvedLandmarkAsset {
    std::string       container_path;
    std::string       logical_name;
    LandmarkAssetKind kind;
};

// ── Canonical container identity ─────────────────────────────────────────
//
// Used by FfAssetStore.  Lowercase, forward-slash normalized.

inline constexpr std::string_view kIsoCmonContainer = "imgs/isocmon.ff";

// ── Landmark TYPE normalization ──────────────────────────────────────────
//
// Delegates to the single shared canonicalisation in the render layer.

using adventure_render::canonical_landmark_type_id;

// ── Candidate classification ─────────────────────────────────────────────

inline bool is_landmark_global_id(std::string_view name) {
    if (name.size() < 7)
        return false;
    if (!name.starts_with("G000MG"))
        return false;
    return std::ranges::all_of(name, [](char c) {
        return static_cast<bool>(std::isalnum(static_cast<unsigned char>(c)));
    });
}

// ── Pure candidate resolver ──────────────────────────────────────────────
//
// No filesystem, FF, or SDL.  Independently unit-testable.
//
// Policy:
//   1. If ANY exact animation candidates exist, resolve AMONG THEM ONLY.
//      Never fall through to static sprites if animation was ambiguous.
//   2. Prefer IsoCmon, then unique, else ambiguous.
//   3. Only when ZERO animation candidates exist may static resolution run.

inline std::optional<ResolvedLandmarkAsset>
resolve_landmark_candidates(std::span<const LandmarkAssetCandidate> candidates) {
    std::vector<LandmarkAssetCandidate> anims;
    std::vector<LandmarkAssetCandidate> sprites;
    for (const auto& c : candidates) {
        if (c.kind == LandmarkAssetKind::Animation)
            anims.push_back(c);
        else if (c.kind == LandmarkAssetKind::StaticSprite)
            sprites.push_back(c);
    }

    auto resolve_group = [](const std::vector<LandmarkAssetCandidate>& group)
        -> std::optional<ResolvedLandmarkAsset> {
        if (group.empty())
            return std::nullopt;

        if (group.size() == 1) {
            return ResolvedLandmarkAsset{.container_path = group[0].container_path,
                                         .logical_name = group[0].logical_name,
                                         .kind = group[0].kind};
        }

        const LandmarkAssetCandidate* preferred = nullptr;
        for (const auto& c : group) {
            if (c.container_path == kIsoCmonContainer) {
                if (preferred != nullptr)
                    return std::nullopt;
                preferred = &c;
            }
        }
        if (preferred != nullptr) {
            return ResolvedLandmarkAsset{.container_path = preferred->container_path,
                                         .logical_name = preferred->logical_name,
                                         .kind = preferred->kind};
        }

        return std::nullopt;
    };

    // Animation candidates exist → resolve among them only. Never fall back.
    if (!anims.empty())
        return resolve_group(anims);

    return resolve_group(sprites);
}

} // namespace d2engine::detail
