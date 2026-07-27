#include <gtest/gtest.h>

#include "d2engine/app/animation_time_controller.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>

namespace {

using d2::app::AnimationTimeController;

std::string debug_ui_source() {
    std::ifstream in(std::filesystem::path(OPENDIS2_SOURCE_DIR) /
                     "src/d2engine/app/debug_ui_renderer.cpp");
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::size_t occurrence_count(const std::string& text, const std::string& needle) {
    std::size_t count = 0;
    std::size_t position = 0;
    while ((position = text.find(needle, position)) != std::string::npos) {
        ++count;
        position += needle.size();
    }
    return count;
}

TEST(AnimationTimeController, StartsAndResetsAtRelativeDefault) {
    AnimationTimeController controller;
    EXPECT_FLOAT_EQ(controller.speed(), 1.0F);
    EXPECT_FLOAT_EQ(controller.effective_time_scale(),
                    AnimationTimeController::kBaseAnimationTimeScale);
    EXPECT_FALSE(controller.paused());
    controller.set_speed(2.0F);
    controller.set_paused(true);
    controller.reset();
    EXPECT_FLOAT_EQ(controller.speed(), 1.0F);
    EXPECT_FLOAT_EQ(controller.effective_time_scale(),
                    AnimationTimeController::kBaseAnimationTimeScale);
    EXPECT_FALSE(controller.paused());
}

TEST(AnimationTimeController, AppliesBaseScaleAndRelativeSpeed) {
    AnimationTimeController controller;
    constexpr float         kRealDeltaMs = 20.0F;
    EXPECT_FLOAT_EQ(controller.scale_delta_ms(kRealDeltaMs),
                    kRealDeltaMs * AnimationTimeController::kBaseAnimationTimeScale);
    controller.set_speed(0.5F);
    EXPECT_FLOAT_EQ(controller.scale_delta_ms(kRealDeltaMs),
                    kRealDeltaMs * AnimationTimeController::kBaseAnimationTimeScale * 0.5F);
    controller.set_speed(1.0F);
    EXPECT_FLOAT_EQ(controller.scale_delta_ms(kRealDeltaMs),
                    kRealDeltaMs * AnimationTimeController::kBaseAnimationTimeScale);
    controller.set_speed(2.0F);
    EXPECT_FLOAT_EQ(controller.scale_delta_ms(kRealDeltaMs),
                    kRealDeltaMs * AnimationTimeController::kBaseAnimationTimeScale * 2.0F);
    controller.set_paused(true);
    EXPECT_FLOAT_EQ(controller.scale_delta_ms(kRealDeltaMs), 0.0F);
    controller.set_paused(false);
    EXPECT_FLOAT_EQ(controller.scale_delta_ms(kRealDeltaMs),
                    kRealDeltaMs * AnimationTimeController::kBaseAnimationTimeScale * 2.0F);
}

TEST(AnimationTimeController, RejectsInvalidValuesAndClampsFiniteSpeeds) {
    AnimationTimeController controller;
    controller.set_speed(-1.0F);
    EXPECT_EQ(controller.speed(), AnimationTimeController::kMinimumSpeed);
    controller.set_speed(10.0F);
    EXPECT_EQ(controller.speed(), AnimationTimeController::kMaximumSpeed);
    EXPECT_THROW(controller.set_speed(std::numeric_limits<float>::quiet_NaN()),
                 std::invalid_argument);
    EXPECT_THROW(controller.set_speed(std::numeric_limits<float>::infinity()),
                 std::invalid_argument);
    EXPECT_THROW(static_cast<void>(controller.scale_delta_ms(-1.0F)), std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(controller.scale_delta_ms(std::numeric_limits<float>::infinity())),
        std::invalid_argument);
}

TEST(AnimationTimeController, DebugPanelUsesRelativeSpeedControls) {
    const std::string source = debug_ui_source();
    const std::string reset_label = "Reset to default";
    EXPECT_NE(source.find(reset_label), std::string::npos);
    EXPECT_EQ(source.find(reset_label, source.find(reset_label) + reset_label.size()),
              std::string::npos);
    EXPECT_EQ(source.find("Production default"), std::string::npos);
    EXPECT_EQ(source.find("Reset to production default"), std::string::npos);
    EXPECT_EQ(source.find("Production default:"), std::string::npos);
    EXPECT_NE(source.find("##animation_speed"), std::string::npos);
    EXPECT_EQ(source.find("SliderFloat(\"Animation speed\""), std::string::npos);
}

TEST(AnimationTimeController, DebugToolsUsesOneWindowAndContentOnlySections) {
    const std::string source = debug_ui_source();
    EXPECT_EQ(occurrence_count(source, "ImGui::Begin(\"Debug Tools\""), 1U);
    EXPECT_EQ(source.find("ImGui::Begin(\"Tree Layout Editor\""), std::string::npos);
    EXPECT_EQ(source.find("ImGui::Begin(\"Animation Time\""), std::string::npos);
    EXPECT_EQ(source.find("ImGui::Begin(\"Render Output\""), std::string::npos);
    EXPECT_EQ(source.find("ImGui::Begin(\"Battle Tuning\""), std::string::npos);
    EXPECT_EQ(occurrence_count(source, "ImGui::Begin("), 1U);
    EXPECT_EQ(occurrence_count(source, "ImGui::End()"), 1U);
    EXPECT_NE(source.find("ImGui::CollapsingHeader(\"Tree Layout Editor\","), std::string::npos);
    EXPECT_NE(source.find("ImGui::CollapsingHeader(\"Animation Time\","), std::string::npos);
    EXPECT_EQ(occurrence_count(source, "ImGui::CollapsingHeader(\"Audio Preview\","), 1U);
    EXPECT_NE(source.find("ImGui::CollapsingHeader(\"Render Output\","), std::string::npos);
    EXPECT_NE(source.find("ImGui::CollapsingHeader(\"Battle Tuning\","), std::string::npos);
    EXPECT_EQ(occurrence_count(source, "ImGui::PushID("),
              occurrence_count(source, "ImGui::PopID()"));
}

TEST(AnimationTimeController, DebugToolsMovementSectionUsesAdventureScreenCommands) {
    const std::string source = debug_ui_source();
    EXPECT_EQ(occurrence_count(source, "ImGui::CollapsingHeader(\"Movement\","), 1U);
    EXPECT_NE(source.find("dynamic_cast<AdventureScreen*>"), std::string::npos);
    EXPECT_NE(source.find("Reset move points"), std::string::npos);
    EXPECT_NE(source.find("Free move points"), std::string::npos);
    EXPECT_NE(source.find("Free move points = 1048576"), std::string::npos);
    EXPECT_NE(source.find("Follow unit"), std::string::npos);
    EXPECT_NE(source.find("Stop following"), std::string::npos);
    EXPECT_NE(source.find("debug_reset_selected_stack_movement_points"), std::string::npos);
    EXPECT_NE(source.find("debug_grant_selected_stack_free_movement_points"), std::string::npos);
    EXPECT_NE(source.find("ImGui::BeginDisabled()"), std::string::npos);
    EXPECT_NE(source.find("ImGui::PushID(\"movement\")"), std::string::npos);
}

TEST(AnimationTimeController, AudioPreviewStaysDebugOnly) {
    const std::string source = debug_ui_source();
    EXPECT_EQ(source.find("ImGui::Begin(\"Audio Preview\""), std::string::npos);
    EXPECT_NE(source.find("render_audio_preview_contents("), std::string::npos);
    EXPECT_NE(source.find("DebugSoundCatalog&"), std::string::npos);
    EXPECT_NE(source.find("debug_audio_preview.play_preview"), std::string::npos);
    EXPECT_NE(source.find("debug_audio_preview.stop_preview"), std::string::npos);
    EXPECT_NE(source.find("Backend: SDL3_mixer"), std::string::npos);
}

} // namespace
