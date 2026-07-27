#pragma once

#include "app_runtime_context.hpp"
#include "screen.hpp"
#include "stack_info_asset_plan.hpp"
#include "stack_info_screen_input.hpp"
#include "stack_inspection.hpp"

#include "../assets/image_asset_key.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace d2engine {

class StackInfoScreen final : public Screen {
public:
    StackInfoScreen(AppRuntimeContext& runtime, TreeLayout tree_layout, std::string config_source,
                    StackInspectionModel stack_model, std::function<void()> request_close);
    ~StackInfoScreen() override;

    StackInfoScreen(const StackInfoScreen&) = delete;
    StackInfoScreen& operator=(const StackInfoScreen&) = delete;
    StackInfoScreen(StackInfoScreen&&) = delete;
    StackInfoScreen& operator=(StackInfoScreen&&) = delete;

    std::string_view                name() const override { return "StackInfoScreen"; }
    [[nodiscard]] ScreenStackPolicy stack_policy() const noexcept override {
        return {.render_below = true};
    }

    void on_enter() override;
    bool handle_input(const InputEvent& event) override;
    void update(const d2::app::ScreenUpdateContext& context) override;
    void render(Renderer2D& renderer) override;

    [[nodiscard]] static std::vector<std::string> required_layout_nodes();

    AppRuntimeContext&    runtime_;
    StackInspectionModel  stack_model_;
    std::function<void()> request_close_;

    StackInfoAssetPlan asset_plan_;
    bool               assets_planned_ = false;
};

} // namespace d2engine
