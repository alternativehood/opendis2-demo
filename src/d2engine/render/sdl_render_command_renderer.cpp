#include "sdl_render_command_renderer.hpp"

#include "renderer2d.hpp"
#include "text/text_layout_types.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <stdexcept>

namespace d2engine {
namespace {

[[nodiscard]] Rect scaled_rect(Rect rect, RenderCommandScale scale) {
    return {.x = rect.x * scale.sx,
            .y = rect.y * scale.sy,
            .w = rect.w * scale.sx,
            .h = rect.h * scale.sy};
}

void draw_tiled(Renderer2D& renderer, SDL_Texture* texture, const RenderCommand& command) {
    const float tile_w = command.has_source_rect ? command.source_rect.w : command.destination.w;
    const float tile_h = command.has_source_rect ? command.source_rect.h : command.destination.h;
    if (tile_w <= 0.0f || tile_h <= 0.0f) {
        renderer.draw_texture(texture, command.destination, command.alpha, command.flip_x,
                              command.flip_y);
        return;
    }
    const float right = command.destination.x + command.destination.w;
    const float bottom = command.destination.y + command.destination.h;
    for (float y = command.destination.y; y < bottom; y += tile_h) {
        for (float x = command.destination.x; x < right; x += tile_w) {
            const Rect tile{.x = x,
                            .y = y,
                            .w = std::min(tile_w, right - x),
                            .h = std::min(tile_h, bottom - y)};
            if (command.has_source_rect) {
                renderer.draw_texture(texture, command.source_rect, tile, command.alpha,
                                      command.flip_x, command.flip_y);
            } else {
                renderer.draw_texture(texture, tile, command.alpha, command.flip_x, command.flip_y);
            }
        }
    }
}

[[nodiscard]] RenderCommand scaled_command(RenderCommand command, RenderCommandScale scale) {
    command.destination = scaled_rect(command.destination, scale);
    command.font_size *= scale.sy;
    return command;
}

[[nodiscard]] TextBox text_box_for(const RenderCommand& command) {
    return {.rect = command.destination,
            .text = command.text,
            .style = {.font_face = command.font_face,
                      .font_size = command.font_size,
                      .color = command.text_color,
                      .align = command.center_text ? TextAlign::Center : TextAlign::Left,
                      .valign = command.center_text ? TextVAlign::Middle : TextVAlign::Top,
                      .wrap = TextWrapMode::None,
                      .overflow = TextOverflowMode::Clip}};
}

} // namespace

void SdlRenderCommandRenderer::render_commands(const std::vector<RenderCommand>& commands,
                                               Renderer2D&                       renderer,
                                               const SdlRenderCommandOptions&    options) {
    std::vector<const RenderCommand*> ordered;
    ordered.reserve(commands.size());
    for (const auto& command : commands) {
        ordered.push_back(&command);
    }
    std::ranges::stable_sort(ordered, {},
                             [](const RenderCommand* command) { return command->layer.order; });

    for (const RenderCommand* native_command : ordered) {
        const RenderCommand command = scaled_command(*native_command, options.scale);
        if (!command.text.empty()) {
            if (command.game_font_text) {
                if (command.font_face.empty()) {
                    throw std::runtime_error("game_font_text command missing font_face: \"" +
                                             command.text + "\"");
                }
                renderer.draw_text_box(text_box_for(command));
            } else {
                float x = command.destination.x;
                float y = command.destination.y;
                if (command.center_text) {
                    const float text_w =
                        static_cast<float>(command.text.size()) * command.font_size;
                    const float text_h = command.font_size;
                    x += (command.destination.w - text_w) * 0.5f;
                    y += (command.destination.h - text_h) * 0.5f;
                }
                renderer.draw_debug_text_colored_scaled(
                    x, y, command.text.c_str(), command.text_color, command.font_size / 8.0f);
            }
            continue;
        }
        if (command.fill_color.has_value()) {
            renderer.draw_rect(command.destination, *command.fill_color, true);
            continue;
        }
        if (!command.texture.present()) {
            continue;
        }
        auto* texture = static_cast<SDL_Texture*>(command.texture.native);
        if (options.rotation_deg != 0.0f && command.allow_rotation) {
            renderer.draw_texture_rotated(texture, command.destination, command.alpha,
                                          command.flip_x, command.flip_y,
                                          static_cast<double>(options.rotation_deg), 0.0f, 0.0f);
        } else if (command.tile) {
            draw_tiled(renderer, texture, command);
        } else if (command.has_source_rect) {
            renderer.draw_texture(texture, command.source_rect, command.destination, command.alpha,
                                  command.flip_x, command.flip_y);
        } else {
            renderer.draw_texture(texture, command.destination, command.alpha, command.flip_x,
                                  command.flip_y);
        }
    }
}

} // namespace d2engine
