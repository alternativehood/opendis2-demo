#pragma once

#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <d2runtime/AdventureIsoDirection.hpp>
#include <d2runtime/AdventureStackPresentationResolver.hpp>
#include <d2runtime/MapCellCoord.hpp>

namespace d2engine::adventure_render {

// ── Render phase: global compositing domain ─────────────────────────────
// Ordered: Terrain < GroundOverlay < World < WorldOverlay < Fog < UIOverlay.
// Each phase is a distinct compositing domain.
enum class AdventureRenderPhase : int {
    Terrain = 0,
    GroundOverlay = 1,
    World = 2,
    WorldOverlay = 3,
    Fog = 4,
    UIOverlay = 5,
};

// ── World render level: local within equivalent IsoDepth ────────────────
// Not a global object-type draw layer.
// Confirmed same-cell ordering: GroundObject (forests) < Structure < Actor (units) < Foreground.
enum class WorldRenderLevel : int {
    GroundObject = 0,  // forests, flat decorations
    Structure = 1,     // mountains, cities, ruins, sites
    ActorUnderlay = 2, // visuals under actor feet (selection, markers, auras)
    Actor = 3,         // units, stacks
    ActorOverlay = 4,  // visuals above actor (highlights, overlays, front-half effects)
    Foreground = 5,    // foreground overlays
};

enum class AdventureRenderVisibilityGroup {
    Default,
    Banners,
};

enum class AdventurePrimitiveRole : std::uint8_t {
    Unspecified,

    MapStackBody,
    MapStackBanner,

    SiteBody,
    SiteBanner,

    RuinBody,
    RuinBanner,

    ContainedStackShield,
};

[[nodiscard]] constexpr std::string_view
adventure_primitive_role_name(AdventurePrimitiveRole role) {
    switch (role) {
    case AdventurePrimitiveRole::Unspecified:
        return "Unspecified";
    case AdventurePrimitiveRole::MapStackBody:
        return "MapStackBody";
    case AdventurePrimitiveRole::MapStackBanner:
        return "MapStackBanner";
    case AdventurePrimitiveRole::SiteBody:
        return "SiteBody";
    case AdventurePrimitiveRole::SiteBanner:
        return "SiteBanner";
    case AdventurePrimitiveRole::RuinBody:
        return "RuinBody";
    case AdventurePrimitiveRole::RuinBanner:
        return "RuinBanner";
    case AdventurePrimitiveRole::ContainedStackShield:
        return "ContainedStackShield";
    }
    return "Unspecified";
}

[[nodiscard]] constexpr bool is_banner_primitive_role(AdventurePrimitiveRole role) {
    return role == AdventurePrimitiveRole::MapStackBanner ||
           role == AdventurePrimitiveRole::SiteBanner || role == AdventurePrimitiveRole::RuinBanner;
}

constexpr bool world_level_before(WorldRenderLevel a, WorldRenderLevel b) {
    switch (a) {
    case WorldRenderLevel::GroundObject:
        return b != WorldRenderLevel::GroundObject;
    case WorldRenderLevel::Structure:
        return b == WorldRenderLevel::ActorUnderlay || b == WorldRenderLevel::Actor ||
               b == WorldRenderLevel::ActorOverlay || b == WorldRenderLevel::Foreground;
    case WorldRenderLevel::ActorUnderlay:
        return b == WorldRenderLevel::Actor || b == WorldRenderLevel::ActorOverlay ||
               b == WorldRenderLevel::Foreground;
    case WorldRenderLevel::Actor:
        return b == WorldRenderLevel::ActorOverlay || b == WorldRenderLevel::Foreground;
    case WorldRenderLevel::ActorOverlay:
        return b == WorldRenderLevel::Foreground;
    case WorldRenderLevel::Foreground:
        return false;
    }
    return false;
}

// ── Stable render id: deterministic total-order tie-break ───────────────
using StableRenderId = std::uint64_t;
[[nodiscard]] constexpr StableRenderId stable_render_id(std::string_view value) {
    StableRenderId hash = 14695981039346656037ull;
    for (const char c : value) {
        hash ^= static_cast<unsigned char>(c);
        hash *= 1099511628211ull;
    }
    return hash;
}

// ── Screen-space point ──────────────────────────────────────────────────
struct ScreenPoint {
    int x = 0;
    int y = 0;
};

struct CanvasContentBounds {
    int min_x = 0;
    int min_y = 0;
    int max_x = 0; // exclusive
    int max_y = 0; // exclusive

    [[nodiscard]] int  width() const { return max_x - min_x; }
    [[nodiscard]] int  height() const { return max_y - min_y; }
    [[nodiscard]] bool valid() const { return max_x > min_x && max_y > min_y; }
};

[[nodiscard]] inline CanvasContentBounds
union_canvas_content_bounds(CanvasContentBounds lhs, const CanvasContentBounds& rhs) {
    if (!lhs.valid())
        return rhs;
    if (!rhs.valid())
        return lhs;
    lhs.min_x = std::min(lhs.min_x, rhs.min_x);
    lhs.min_y = std::min(lhs.min_y, rhs.min_y);
    lhs.max_x = std::max(lhs.max_x, rhs.max_x);
    lhs.max_y = std::max(lhs.max_y, rhs.max_y);
    return lhs;
}

// ── Sprite anchor: offset from tile center to draw origin ───────────────
struct SpriteAnchor {
    int x = 0;
    int y = 0;
};

inline constexpr SpriteAnchor kTreeSpriteAnchor{125, 145};

// ── Visual bounds: screen-space AABB for clipping/filtering ─────────────
struct VisualBounds {
    int                min_x = 0;
    int                min_y = 0;
    int                max_x = 0;
    int                max_y = 0;
    [[nodiscard]] bool contains(int px, int py) const {
        return px >= min_x && px < max_x && py >= min_y && py < max_y;
    }
};

// ── Map cell ───────────────────────────────────────────────────────────
//
// Canonical grid cell coordinates (logical grid space).
// Used for cell-level interaction: coarse hover, selection circles, etc.
using MapCell = d2runtime::MapCellCoord;

// ── Grid footprint ─────────────────────────────────────────────────────
//
// Describes which grid cells a world object occupies.
// For single-tile objects the footprint contains exactly one cell.
// For multi-tile structures (mountains, cities) it covers the full shape.
using GridFootprint = std::vector<MapCell>;

// ── Iso depth anchor ───────────────────────────────────────────────────
//
// The reference grid position used for spatial depth ordering.
// Typically the "front" or "foot" cell of the object.
// For single-tile objects this is the tile itself.
// For multi-tile footprints this should be the spatially foremost cell.
using IsoDepthAnchor = MapCell;

// ── Interaction mask (CPU-side alpha opacity bitmap) ──────────────────
//
// Compact 1-bit-per-pixel representation of sprite opacity.
// Built from decoded RGBA composed sprite data once during preload.
// Used for fine unit interaction hit-testing (pointer → alpha check).
struct InteractionMask {
    int                  width = 0;
    int                  height = 0;
    std::vector<uint8_t> bits; // row-major, MSB of each byte = leftmost pixel

    [[nodiscard]] int stride() const { return (width + 7) / 8; }

    [[nodiscard]] bool opaque(int x, int y) const {
        if (x < 0 || y < 0 || x >= width || y >= height)
            return false;
        const std::size_t idx = static_cast<std::size_t>(y) * static_cast<std::size_t>(stride()) +
                                static_cast<std::size_t>(x) / 8;
        if (idx >= bits.size())
            return false;
        return (bits[idx] & (0x80U >> (static_cast<unsigned>(x) % 8))) != 0;
    }
};

// ── Prepared render primitive ──────────────────────────────────────────
//
// Common render-ready primitive.  No semantic-specific payload.
// Semantic preparers (ForestContributor, etc.) populate primitives;
// the render graph builder never inspects semantic type.
// ── Canonical map-cell footprint and depth anchor ───────────────────────

// ── Prepared render primitive ───────────────────────────────────────────
struct AdventureAnimationFrame {
    std::string         record_name;
    int                 duration_ms = 100;
    int                 canvas_width = 0; // 0 = use visual-level default
    int                 canvas_height = 0;
    CanvasContentBounds content_bounds;
};

enum class AdventureAnimationTimingSource : std::uint8_t {
    Unspecified,
    ProvisionalFallback,
};

struct AdventureAnimationData {
    std::string                          animation_name;
    std::vector<AdventureAnimationFrame> frames;
    int                                  native_canvas_w = 0;
    int                                  native_canvas_h = 0;
    bool                                 is_looping = false;
    AdventureAnimationTimingSource timing_source = AdventureAnimationTimingSource::Unspecified;
};

struct PreparedAdventureRenderPrimitive {
    StableRenderId                 stable_id = 0;
    std::string                    debug_label;
    AdventurePrimitiveRole         semantic_role = AdventurePrimitiveRole::Unspecified;
    std::string                    semantic_object_id;
    AdventureRenderPhase           phase = AdventureRenderPhase::World;
    WorldRenderLevel               level = WorldRenderLevel::GroundObject;
    AdventureRenderVisibilityGroup visibility_group = AdventureRenderVisibilityGroup::Default;

    // Local ordering within the same (depth_anchor, level). Higher = draws later/on top.
    int local_suborder = 0;

    // Source image asset reference (static)
    std::string container_path;
    std::string record_name;

    // Animation reference (optional — set for animated landmarks etc.)
    std::optional<AdventureAnimationData> animation;

    // Draw origin (screen-space top-left of the rendered sprite)
    ScreenPoint draw_origin;

    // Authored opaque bounds within the canvas, used for banner docking and cropping decisions.
    CanvasContentBounds content_bounds;

    // Visual bounds (screen-space AABB for clipping/filtering)
    VisualBounds visual_bounds;

    // Spatial data (independent from visual bounds)
    GridFootprint  footprint;
    IsoDepthAnchor depth_anchor;

    // Pixel dimensions of the source image
    int src_width = 0;
    int src_height = 0;

    // CPU-side interaction mask for fine unit hit-testing
    // nullptr means no mask available (coarse hover only)
    std::shared_ptr<const InteractionMask> interaction_mask;

    // Animation clock ownership / follower routing.
    // nullopt  → this animated primitive owns its AnimationPlayer
    // value    → this animated primitive uses the AnimationPlayer owned by that stable ID
    std::optional<StableRenderId> animation_sync_source_id;

    // Normalized render alpha (0.0 = fully transparent, 1.0 = fully opaque).
    // Applied through Renderer2D texture alpha modulation (multiplies authored per-pixel alpha).
    float alpha = 1.0f;
};

// ── Pick entry: semantic association for hit-testing ────────────────────
enum class PickEntryKind : uint8_t {
    Stack,
};
struct PickEntry {
    StableRenderId stable_id = 0;
    PickEntryKind  kind = PickEntryKind::Stack;
    std::string    object_id;
};

// ── Prepare diagnostic ──────────────────────────────────────────────────
enum class PrepareDiagnosticKind : int {
    Resolved = 0,
    UnresolvedNoSprite = 1,
    UnresolvedMissingImage = 2,
    UnresolvedUnknownKind = 3,
};
struct PrepareDiagnostic {
    PrepareDiagnosticKind kind = PrepareDiagnosticKind::UnresolvedUnknownKind;
    std::string           object_id;
    std::string           object_kind;
    int                   image = -1;
    int                   index = -1;
    std::string           message;
};

struct AdventureActorIdlePlaybackPolicy {
    static constexpr int                            frame_duration_ms = 100;
    static constexpr bool                           is_looping = true;
    static constexpr AdventureAnimationTimingSource timing_source =
        AdventureAnimationTimingSource::ProvisionalFallback;
};

// ActorUnderlay local ordering within a cell
inline constexpr int kActorShadowSuborder = 0;
inline constexpr int kActorRouteMarkerSuborder = 50;
inline constexpr int kActorSelectionSuborder = 100;

struct AdventureActorShadowPresentationPolicy {
    static constexpr float alpha = 0.5f;
};

struct AdventureActorVisualFrame {
    std::string         record_name;
    int                 canvas_width = 0;
    int                 canvas_height = 0;
    CanvasContentBounds content_bounds;
};

struct AdventureActorVisualLayer {
    std::string                            container_path;
    std::string                            logical_animation_name;
    std::vector<AdventureActorVisualFrame> frames;

    int native_canvas_w = 0;
    int native_canvas_h = 0;

    int                 canvas_foot_x = 0;
    int                 canvas_foot_y = 0;
    CanvasContentBounds content_bounds;
};

struct AdventureActorVisual {
    d2runtime::AdventureActorPresentationKind presentation_kind =
        d2runtime::AdventureActorPresentationKind::Unit;

    std::string resolved_owner_id;

    AdventureActorVisualLayer                body;
    std::optional<AdventureActorVisualLayer> shadow;
};

struct AdventureResolvedFrameVisual {
    std::string_view record_name;
    ScreenPoint      draw_origin;
    int              src_width = 0;
    int              src_height = 0;
    float            alpha = 1.0f;
};

[[nodiscard]] inline AdventureResolvedFrameVisual
resolve_adventure_frame_visual(const PreparedAdventureRenderPrimitive& primitive,
                               std::optional<std::size_t>              animation_frame_index) {
    AdventureResolvedFrameVisual result;
    result.draw_origin = primitive.draw_origin;
    result.alpha = primitive.alpha;

    if (primitive.animation.has_value()) {
        if (!animation_frame_index.has_value()) {
            std::string msg = "animated primitive missing frame index stable_id=";
            msg += std::to_string(primitive.stable_id);
            msg += " label=" + primitive.debug_label;
            msg += " animation=" + primitive.animation->animation_name;
            msg += " frames=" + std::to_string(primitive.animation->frames.size());
            throw std::logic_error(msg);
        }
        const auto  idx = *animation_frame_index;
        const auto& anim_data = *primitive.animation;
        if (idx >= anim_data.frames.size()) {
            std::string msg = "animated primitive frame index out of range stable_id=";
            msg += std::to_string(primitive.stable_id);
            msg += " label=" + primitive.debug_label;
            msg += " animation=" + anim_data.animation_name;
            msg += " index=" + std::to_string(idx);
            msg += " frames=" + std::to_string(anim_data.frames.size());
            throw std::logic_error(msg);
        }
        const auto& af = anim_data.frames[idx];
        result.record_name = af.record_name;
        result.src_width = af.canvas_width > 0 ? af.canvas_width : primitive.src_width;
        result.src_height = af.canvas_height > 0 ? af.canvas_height : primitive.src_height;
    } else {
        if (animation_frame_index.has_value()) {
            std::string msg = "static primitive received frame index stable_id=";
            msg += std::to_string(primitive.stable_id);
            msg += " label=" + primitive.debug_label;
            msg += " index=" + std::to_string(*animation_frame_index);
            throw std::logic_error(msg);
        }
        result.record_name = primitive.record_name;
        result.src_width = primitive.src_width;
        result.src_height = primitive.src_height;
    }
    return result;
}

} // namespace d2engine::adventure_render
