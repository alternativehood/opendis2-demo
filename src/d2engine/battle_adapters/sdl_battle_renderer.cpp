#include "sdl_battle_renderer.hpp"

#include "../battle_view/battle_anchor_resolver.hpp"
#include "../battle_view/battle_render_tree_contract.hpp"
#include "../render/color.hpp"
#include "../render/renderer2d.hpp"
#include "../render/sdl_render_command_renderer.hpp"

#include <algorithm>
#include <cstdio>

namespace d2engine {
namespace {

// ponytail: defaults matching DebugOverlayStyle defaults
static constexpr DebugOverlayStyle kDefaultOverlayStyle{};

[[nodiscard]] bool uses_unit_rotation(RenderLayer layer) {
    return layer != battle_render_layer(BattleRenderPass::GroundBackground) &&
           layer != battle_render_layer(BattleRenderPass::Background) &&
           layer != battle_render_layer(BattleRenderPass::BackgroundOverlay) &&
           layer != battle_render_layer(BattleRenderPass::CombatFrame);
}

// Draw all 18 battlefield slot anchors from render_tree (12 FRONT/BACK + 6 CENTER).
void draw_render_tree_slot_anchors(Renderer2D& renderer, const TreeLayout& tree, LayoutScale scale,
                                   const DebugOverlayStyle* style = nullptr) {
    const DebugOverlayStyle& s = style != nullptr ? *style : kDefaultOverlayStyle;
    const Color kSlotColor[4] = {s.slot_attacker_back, s.slot_attacker_front, s.slot_defender_back,
                                 s.slot_defender_front};

    // Collect resolved positions for 12 FRONT/BACK slot lane/depth lines
    static constexpr int kSlotCount = 12;
    Vec2                 resolved[kSlotCount];
    int                  index = 0;

    for (const char side : {'a', 'd'}) {
        for (int lane = 0; lane < 3; ++lane) {
            for (const char* depth : {"back", "front"}) {
                const BattleSlotCoord coord{
                    .side = (side == 'a') ? BattleSide::Attacker : BattleSide::Defender,
                    .lane = lane,
                    .depth = (depth[0] == 'f') ? BattleDepth::Front : BattleDepth::Back};
                const Rect r = tree.compose(battlefield_slot_tree_path(coord));
                resolved[index] = {.x = r.x * scale.sx, .y = r.y * scale.sy};
                ++index;
            }
        }
    }

    // Lane lines: connect same depth across adjacent lanes
    for (std::size_t const base : {0u, 1u, 6u, 7u}) {
        for (std::size_t lane = 0; lane < 2; ++lane) {
            const Vec2 a = resolved[base + (lane * 2)];
            const Vec2 b = resolved[base + ((lane + 1) * 2)];
            renderer.draw_line(a.x, a.y, b.x, b.y, s.lane_line);
        }
    }

    // Depth lines: connect Back/Front within same lane
    for (std::size_t i = 0; i < kSlotCount; i += 2) {
        const Vec2 a = resolved[i];
        const Vec2 b = resolved[i + 1];
        renderer.draw_line(a.x, a.y, b.x, b.y, s.depth_line);
    }

    // Anchor dots and labels for FRONT/BACK slots
    static constexpr const char* kLabels[kSlotCount] = {
        "A_BACK_0", "A_FRONT_0", "A_BACK_1", "A_FRONT_1", "A_BACK_2", "A_FRONT_2",
        "D_BACK_0", "D_FRONT_0", "D_BACK_1", "D_FRONT_1", "D_BACK_2", "D_FRONT_2",
    };
    for (std::size_t i = 0; i < kSlotCount; ++i) {
        const int   color_idx = (i >= 6 ? 2 : 0) + static_cast<int>(i % 2);
        const float sx = resolved[i].x;
        const float sy = resolved[i].y;
        const float kR = s.slot_dot_radius;
        const Rect  dot{.x = sx - kR, .y = sy - kR, .w = kR * 2.0f, .h = kR * 2.0f};
        renderer.draw_rect(dot, kSlotColor[color_idx], true);
        renderer.draw_rect(dot, Color{.r = 255, .g = 255, .b = 255, .a = 180}, false);
        renderer.draw_debug_text(sx + kR + s.label_offset_x, sy + s.label_offset_y, kLabels[i]);

        // Draw unit mount offset indicator if /unit offset differs from (0,0)
        const std::string     slot_label = kLabels[i];
        const char            side_ch = slot_label[0];
        const int             lane = slot_label.back() - '0';
        const BattleSlotCoord coord{
            .side = (side_ch == 'A') ? BattleSide::Attacker : BattleSide::Defender,
            .lane = lane,
            .depth = (slot_label[2] == 'F') ? BattleDepth::Front : BattleDepth::Back};
        const auto unit_node = tree.node(battlefield_unit_tree_path(coord));
        if (unit_node.has_value() && (unit_node->x != 0.0f || unit_node->y != 0.0f)) {
            const Vec2 mount = {.x = (sx + (unit_node->x * scale.sx)),
                                .y = (sy + (unit_node->y * scale.sy))};
            renderer.draw_line(sx, sy, mount.x, mount.y, s.mount_color);
            const float kMr = s.mount_marker_radius;
            renderer.draw_rect(
                Rect{.x = mount.x - kMr, .y = mount.y - kMr, .w = kMr * 2.0f, .h = kMr * 2.0f},
                s.mount_color, true);
        }
    }

    // CENTER slot anchors (distinct color) — same dot+label+mount logic as FRONT/BACK
    for (const char side : {'a', 'd'}) {
        for (int lane = 0; lane < 3; ++lane) {
            const BattleSlotCoord coord{.side = (side == 'a') ? BattleSide::Attacker
                                                              : BattleSide::Defender,
                                        .lane = lane,
                                        .depth = BattleDepth::Center};
            const std::string     slot_path = battlefield_slot_tree_path(coord);
            if (!tree.has_node(slot_path)) {
                continue;
            }
            const Rect  r = tree.compose(slot_path);
            const float sx = r.x * scale.sx;
            const float sy = r.y * scale.sy;
            char        label[24];
            std::snprintf(label, sizeof(label), "%c_CENTER_%d", (side == 'a') ? 'A' : 'D', lane);
            const float kR = s.center_dot_radius;
            const Rect  dot{.x = sx - kR, .y = sy - kR, .w = kR * 2.0f, .h = kR * 2.0f};
            renderer.draw_rect(dot, s.center_color, true);
            renderer.draw_rect(dot, Color{.r = 255, .g = 255, .b = 255, .a = 180}, false);
            renderer.draw_debug_text(sx + kR + s.label_offset_x, sy + s.label_offset_y, label);
            // Draw unit mount offset indicator when the /unit node has non-zero offset
            const auto unit_node = tree.node(battlefield_unit_tree_path(coord));
            if (unit_node.has_value() && (unit_node->x != 0.0f || unit_node->y != 0.0f)) {
                const Vec2 mount = {.x = sx + unit_node->x * scale.sx,
                                    .y = sy + unit_node->y * scale.sy};
                renderer.draw_line(sx, sy, mount.x, mount.y, s.mount_color);
                const float kMr = s.mount_marker_radius;
                renderer.draw_rect(
                    Rect{.x = mount.x - kMr, .y = mount.y - kMr, .w = kMr * 2.0f, .h = kMr * 2.0f},
                    s.mount_color, true);
            }
        }
    }
}

} // namespace

void SdlBattleRenderer::render_commands(const std::vector<RenderCommand>& commands,
                                        Renderer2D& renderer, LayoutScale scale,
                                        const BattleRenderOptions& options) {
    std::vector<RenderCommand> translated = commands;
    for (RenderCommand& command : translated) {
        command.allow_rotation = uses_unit_rotation(command.layer);
    }
    SdlRenderCommandRenderer::render_commands(
        translated, renderer,
        SdlRenderCommandOptions{.scale = {.sx = scale.sx, .sy = scale.sy},
                                .rotation_deg = options.rotation_deg});
}

void SdlBattleRenderer::draw_debug(const BattleRenderSnapshot& snapshot, LayoutScale scale,
                                   Renderer2D& renderer, const DebugRenderOptions& options) {
    if (!options.enabled()) {
        return;
    }
    if (options.draw_slot_anchors) {
        if (options.tree_layout != nullptr) {
            draw_render_tree_slot_anchors(renderer, *options.tree_layout, scale,
                                          options.overlay_style);
        }
    }
    if (options.tree_layout == nullptr) {
        return;
    }
    for (const auto& entity : snapshot.entities) {
        const Vec2 native_anchor = BattleAnchorResolver::resolve(
            AnchorPolicy::UnitFoot, entity, snapshot, *options.tree_layout, LayoutMetrics{});
        const Vec2 anchor = {.x = native_anchor.x * scale.sx, .y = native_anchor.y * scale.sy};
        float      y = anchor.y;
        char       label[64];
        if (options.draw_entity_ids) {
            std::snprintf(label, sizeof(label), "id:%u", entity.id.value);
            renderer.draw_debug_text(anchor.x, y, label);
            y += 10.0f;
        }
        for (const auto& track : entity.tracks) {
            if (options.draw_track_states) {
                std::snprintf(label, sizeof(label), "track:%u vis:%u",
                              static_cast<unsigned>(track.kind),
                              static_cast<unsigned>(track.visibility));
                renderer.draw_debug_text(anchor.x, y, label);
                y += 10.0f;
            }
            if (options.draw_frame_names) {
                renderer.draw_debug_text(anchor.x, y, track.current_frame_name.c_str());
                y += 10.0f;
            }
        }
    }
}

} // namespace d2engine
