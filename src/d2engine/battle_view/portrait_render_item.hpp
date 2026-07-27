#pragma once

#include "../assets/portrait_manifest_index.hpp"
#include "../render/rect.hpp"
#include "debug_renderable_item.hpp"

#include <cstdint>
#include <optional>
#include <string>

struct SDL_Texture;

namespace d2engine {

class GameTextureCache;

// Which portrait variant the caller prefers as first choice.
enum class PortraitTextureKind : std::uint8_t {
    Face,  // FACE variant — no magenta transformation
    FaceB, // FACEB variant — magenta 255/0/255 → alpha=0
};

// Resolved portrait variant with the file record name.
struct PortraitResolution {
    std::string         record_name;
    PortraitTextureKind kind;
};

// Shared data for a single portrait renderable.
// The same item drives both drawing and debug tuning.
// When has_texture is false, a placeholder rect is drawn instead.
struct PortraitRenderItem {
    SDL_Texture* texture = nullptr;
    Rect         dest_rect;
    std::string  tree_path;
    std::string  stable_id;
    int          layer = 100;
    bool         flip_x = false;
    bool         flip_y = false;
    std::string  visual_category; // "PortraitFACE", "PortraitFACEB", or "PortraitPlaceholder"
    std::string  side;            // "a" or "d"
    bool         has_texture = false;

    [[nodiscard]] DebugRenderableItem to_tunable_item() const;
};

// Resolve which variant to load given a manifest entry and preference.
// Returns the record_name and kind, or nullopt if neither variant exists.
[[nodiscard]] std::optional<PortraitResolution>
resolve_portrait_variant(const UnitPortraitEntry& entry, PortraitTextureKind preferred_kind);

// Build a fully-populated PortraitRenderItem from a unit type + config.
// Logs diagnostic on first missing portrait per unit_type (never spams).
// When preferred_kind variant is unavailable, falls back to the other.
// Returns nullopt if no variant is available at all.
[[nodiscard]] std::optional<PortraitRenderItem>
build_portrait_render_item(const std::string&           unit_type,
                           const PortraitManifestIndex& portrait_index,
                           GameTextureCache& texture_cache, const std::string& tree_path,
                           const Rect& dest_rect, bool flip_x, const std::string& side,
                           PortraitTextureKind preferred_kind = PortraitTextureKind::Face);

// Build a placeholder PortraitRenderItem for when no texture is available
// (occupied slot, texture load failed or missing manifest entry).
// texture=nullptr, has_texture=false, visual_category="PortraitPlaceholder".
[[nodiscard]] PortraitRenderItem build_portrait_placeholder_item(const std::string& tree_path,
                                                                 const Rect& dest_rect, bool flip_x,
                                                                 const std::string& side);

// HUD dead mask overlay constants. Same container as portrait faces.
inline constexpr const char* kDeadMaskContainer = "Imgs/Faces.ff";
inline constexpr const char* kDeadMaskLargeName = "MASKDEADL.PNG";
inline constexpr const char* kDeadMaskSmallName = "MASKDEADS.PNG";

// Build a dead-mask overlay PortraitRenderItem (MASKDEADL or MASKDEADS from Imgs/Faces.ff).
// Logs dead_portrait_mask_missing warning if texture not found; returns item with
// has_texture=false. tree_path should be e.g. "/ui/right_unit_group/1_center/dead_mask".
[[nodiscard]] PortraitRenderItem
build_dead_mask_item(bool is_large, GameTextureCache& texture_cache, const std::string& tree_path,
                     const Rect& dest_rect, bool flip_x, const std::string& side);

} // namespace d2engine
