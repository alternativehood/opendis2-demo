#include "stack_info_screen.hpp"

#include "../render/game_texture_cache.hpp"
#include "../render/render_asset_runtime.hpp"
#include "../render/renderer2d.hpp"
#include "../render/text/text_layout_types.hpp"

#include <d2log/log.hpp>

#include <array>
#include <string>

namespace d2engine {

namespace {

auto                   kLog = d2log::get("d2.stack_info"); // NOLINT
inline constexpr auto* kLabelBattlesWon = "Battles won:";

SDL_Texture* find_texture(GameTextureCache& textures, const std::optional<ImageAssetKey>& key) {
    if (!key.has_value() || key->container_path.empty())
        return nullptr;
    return textures.find(*key);
}

void draw_texture_at(Renderer2D& renderer, SDL_Texture* tex, Screen& screen,
                     std::string_view layout_path) {
    if (tex == nullptr)
        return;
    auto rect = screen.layout_rect(layout_path);
    if (rect.w > 0 && rect.h > 0)
        renderer.draw_texture(tex, rect);
}

} // namespace

StackInfoScreen::StackInfoScreen(AppRuntimeContext& runtime, TreeLayout tree_layout,
                                 std::string config_source, StackInspectionModel stack_model,
                                 std::function<void()> request_close)
    : Screen(std::move(tree_layout), std::move(config_source)), runtime_(runtime),
      stack_model_(std::move(stack_model)), request_close_(std::move(request_close)) {}

StackInfoScreen::~StackInfoScreen() = default;

std::vector<std::string> StackInfoScreen::required_layout_nodes() {
    return {"/stack_info",
            "/stack_info/background",
            "/stack_info/leader/portrait",
            "/stack_info/leader/name",
            "/stack_info/leader/race",
            "/stack_info/leader/battles_won",
            "/stack_info/formation/slot_0/portrait",
            "/stack_info/formation/slot_0/name",
            "/stack_info/formation/slot_0/frame",
            "/stack_info/formation/slot_1/portrait",
            "/stack_info/formation/slot_1/name",
            "/stack_info/formation/slot_1/frame",
            "/stack_info/formation/slot_2/portrait",
            "/stack_info/formation/slot_2/name",
            "/stack_info/formation/slot_2/frame",
            "/stack_info/formation/slot_3/portrait",
            "/stack_info/formation/slot_3/name",
            "/stack_info/formation/slot_3/frame",
            "/stack_info/formation/slot_4/portrait",
            "/stack_info/formation/slot_4/name",
            "/stack_info/formation/slot_4/frame",
            "/stack_info/formation/slot_5/portrait",
            "/stack_info/formation/slot_5/name",
            "/stack_info/formation/slot_5/frame",
            "/stack_info/formation/large_row_0/portrait",
            "/stack_info/formation/large_row_0/name",
            "/stack_info/formation/large_row_0/frame",
            "/stack_info/formation/large_row_1/portrait",
            "/stack_info/formation/large_row_1/name",
            "/stack_info/formation/large_row_1/frame",
            "/stack_info/formation/large_row_2/portrait",
            "/stack_info/formation/large_row_2/name",
            "/stack_info/formation/large_row_2/frame"};
}

void StackInfoScreen::on_enter() {
    kLog->info("stack_info_screen opened stack_id={}", stack_model_.id);
    if (assets_planned_)
        return;

    asset_plan_ = plan_stack_info_assets(stack_model_, runtime_.portraits, runtime_.game_data);

    if (!asset_plan_.interface_assets.empty()) {
        static_cast<void>(runtime_.render_assets.request_textures(
            asset_plan_.interface_assets, AssetPriority::Critical, "stack_info"));
    }

    assets_planned_ = true;
}

bool StackInfoScreen::handle_input(const InputEvent& event) {
    auto action = StackInfoScreenInputHandler::handle(event);
    if (!action.has_value())
        return false;

    if (std::get_if<StackInfoCancel>(&*action)) {
        if (request_close_) {
            request_close_();
        }
        return true;
    }

    return false;
}

void StackInfoScreen::update(const d2::app::ScreenUpdateContext& /*context*/) {}

void StackInfoScreen::render(Renderer2D& renderer) {
    auto& textures = runtime_.render_assets.textures();

    // Popup background
    auto bg_tex = textures.find(asset_plan_.popup_background);
    draw_texture_at(renderer, bg_tex, *this, "/stack_info/background");

    // Leader info text boxes
    const UnitInspectionModel* leader = nullptr;
    for (const auto& member : stack_model_.members) {
        if (member.is_leader) {
            leader = &member;
            break;
        }
    }

    if (leader != nullptr) {
        std::string leader_name;
        if (leader->instance_resolved && !leader->custom_name.empty()) {
            leader_name = leader->custom_name;
        } else if (leader->definition_resolved && leader->definition.has_value()) {
            leader_name = leader->definition->name;
        } else {
            leader_name = leader->type_id;
        }

        auto name_rect = layout_rect("/stack_info/leader/name");
        renderer.draw_text_box({name_rect,
                                leader_name,
                                {.color{.r = 0, .g = 0, .b = 0, .a = 255},
                                 .align = TextAlign::Center,
                                 .overflow = TextOverflowMode::Ellipsis}});

        if (!stack_model_.faction_name.empty()) {
            auto race_rect = layout_rect("/stack_info/leader/race");
            renderer.draw_text_box({race_rect,
                                    stack_model_.faction_name,
                                    {.color{.r = 0, .g = 0, .b = 0, .a = 255},
                                     .align = TextAlign::Center,
                                     .overflow = TextOverflowMode::Ellipsis}});
        }

        const std::string battles_text =
            std::string{kLabelBattlesWon} + " " + std::to_string(stack_model_.battles_won);
        auto battles_rect = layout_rect("/stack_info/leader/battles_won");
        renderer.draw_text_box({battles_rect,
                                battles_text,
                                {.color{.r = 0, .g = 0, .b = 0, .a = 255},
                                 .align = TextAlign::Center,
                                 .overflow = TextOverflowMode::Ellipsis}});
    }

    // Leader portrait from Events.ff
    auto leader_tex = find_texture(textures, asset_plan_.leader_portrait);
    draw_texture_at(renderer, leader_tex, *this, "/stack_info/leader/portrait");

    // Formation rendering: three explicit passes.

    // — Determine which rows contain a large unit —
    std::array<bool, 3> row_has_large = {};
    for (const auto& pp : asset_plan_.planned_portraits) {
        if (pp.is_large) {
            const auto row = pp.formation_cell / 2;
            if (row >= 0 && row < 3)
                row_has_large[static_cast<std::size_t>(row)] = true;
        }
    }

    // PASS 1 — portraits (behind frames)
    for (const auto& pp : asset_plan_.planned_portraits) {
        if (pp.layout_path.empty())
            continue;
        SDL_Texture* tex = nullptr;
        if (!pp.key.container_path.empty())
            tex = textures.find(pp.key);
        if (tex == nullptr)
            continue;
        const auto portrait_path = pp.layout_path + "/portrait";
        draw_texture_at(renderer, tex, *this, portrait_path);
    }

    // PASS 2 — frames (above portraits)
    for (std::size_t cell = 0; cell < 6; ++cell) {
        const auto row = static_cast<int>(cell) / 2;
        if (row_has_large[static_cast<std::size_t>(row)]) {
            // For large-unit rows, draw the large frame once per row
            if (cell % 2 == 0) {
                const auto lr_path =
                    "/stack_info/formation/large_row_" + std::to_string(row) + "/frame";
                auto* frame_tex = textures.find(asset_plan_.large_frame);
                draw_texture_at(renderer, frame_tex, *this, lr_path);
            }
        } else {
            // For normal rows, draw small frame for every cell (including empty)
            const auto slot_path = "/stack_info/formation/slot_" + std::to_string(cell) + "/frame";
            auto*      frame_tex = textures.find(asset_plan_.small_frame);
            draw_texture_at(renderer, frame_tex, *this, slot_path);
        }
    }

    // PASS 3 — names (above frames)
    for (const auto& pp : asset_plan_.planned_portraits) {
        if (pp.layout_path.empty() || pp.display_name.empty())
            continue;
        const auto name_path = pp.layout_path + "/name";
        auto       name_rect = layout_rect(name_path);
        if (name_rect.w <= 0 || name_rect.h <= 0)
            continue;
        renderer.draw_text_box({name_rect,
                                pp.display_name,
                                {.color{.r = 0, .g = 0, .b = 0, .a = 255},
                                 .align = TextAlign::Center,
                                 .overflow = TextOverflowMode::Ellipsis}});
    }
}

} // namespace d2engine
