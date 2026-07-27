#include <gtest/gtest.h>

#include "d2engine/app/upscale_controller.hpp"
#include "d2engine/render/upscale_settings.hpp"

namespace d2engine {

TEST(UpscaleSettings, DefaultsToFsr1WithConfiguredSharpness) {
    const UpscaleSettings settings;
    EXPECT_EQ(settings.mode, UpscaleMode::Fsr1);
    EXPECT_FLOAT_EQ(settings.fsr_sharpness, 0.70F);
}

TEST(UpscaleSettings, ModeNamesAreStable) {
    EXPECT_EQ(upscale_mode_name(UpscaleMode::Linear), "Linear (Legacy)");
    EXPECT_EQ(upscale_mode_name(UpscaleMode::Nearest), "Nearest");
    EXPECT_EQ(upscale_mode_name(UpscaleMode::PixelArt), "Pixel Art");
    EXPECT_EQ(upscale_mode_name(UpscaleMode::Fsr1), "FSR 1");
}

TEST(UpscaleSettings, SharpnessIsClampedToSupportedRange) {
    UpscaleSettings settings{.fsr_sharpness = -2.0F};
    settings.clamp();
    EXPECT_FLOAT_EQ(settings.fsr_sharpness, 0.0F);

    settings.fsr_sharpness = 2.0F;
    settings.clamp();
    EXPECT_FLOAT_EQ(settings.fsr_sharpness, 1.0F);
}

TEST(UpscaleSettings, FsrIsEffectiveWhenOutputIsAnUpscale) {
    EXPECT_EQ(resolve_effective_upscale_mode(UpscaleMode::Fsr1, {640, 480}, {1280, 960}),
              UpscaleMode::Fsr1);
}

TEST(UpscaleSettings, FsrFallsBackForDownscaleOrOneToOneOutput) {
    EXPECT_EQ(resolve_effective_upscale_mode(UpscaleMode::Fsr1, {640, 480}, {639, 960}),
              UpscaleMode::Linear);
    EXPECT_EQ(resolve_effective_upscale_mode(UpscaleMode::Fsr1, {640, 480}, {640, 480}),
              UpscaleMode::Linear);
    EXPECT_EQ(resolve_effective_upscale_mode(UpscaleMode::Fsr1, {640, 480}, {1280, 479}),
              UpscaleMode::Linear);
}

TEST(UpscaleController, ChangesPropagateToRequestedSettings) {
    UpscaleController controller;
    controller.set_mode(UpscaleMode::Fsr1);
    controller.set_fsr_sharpness(0.75F);

    EXPECT_EQ(controller.settings().mode, UpscaleMode::Fsr1);
    EXPECT_FLOAT_EQ(controller.settings().fsr_sharpness, 0.75F);
}

TEST(UpscaleController, ResetRestoresDefaults) {
    UpscaleController controller;
    controller.set_mode(UpscaleMode::Fsr1);
    controller.set_fsr_sharpness(1.0F);
    controller.reset();

    EXPECT_EQ(controller.settings().mode, UpscaleMode::Fsr1);
    EXPECT_FLOAT_EQ(controller.settings().fsr_sharpness, 0.70F);
}

} // namespace d2engine
