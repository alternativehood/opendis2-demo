#include <d2engine/app/adventure_interaction_mask.hpp>
#include <d2engine/app/adventure_movement_visual_plan.hpp>
#include <d2engine/assets/asset_runtime.hpp>
#include <d2engine/render/render_asset_runtime.hpp>

#include <gtest/gtest.h>

#include <SDL3/SDL.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <mutex>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "tests/test_process.hpp"

namespace {
using d2engine::ImageAssetKey;
using d2engine::ImageAssetKind;
using d2engine::PreparedImage;
using d2engine::PreparedImageResult;
using d2engine::adventure_render::AdventureActorVisualLayer;

using MovementVisualPlanPreloadSignature = void (*)(d2engine::AdventureMovementVisualPlan&,
                                                    d2engine::AssetRuntime&,
                                                    d2engine::RenderAssetRuntime&);
static_assert(std::is_same_v<decltype(static_cast<MovementVisualPlanPreloadSignature>(
                                 &d2engine::preload_adventure_movement_visual_plan)),
                             MovementVisualPlanPreloadSignature>);

AdventureActorVisualLayer layer(std::string animation, std::initializer_list<std::string> frames,
                                int width = 2, int height = 2) {
    AdventureActorVisualLayer result{.container_path = "Imgs/Synthetic.ff",
                                     .logical_animation_name = std::move(animation),
                                     .native_canvas_w = width,
                                     .native_canvas_h = height};
    for (auto& record : frames)
        result.frames.push_back(
            {.record_name = std::move(record), .canvas_width = width, .canvas_height = height});
    return result;
}

PreparedImageResult decoded(std::string record, int width, int height, int opaque_x = 0,
                            int opaque_y = 0) {
    auto pixels = std::make_shared<d2res::RgbaBuffer>();
    pixels->width = static_cast<std::uint32_t>(width);
    pixels->height = static_cast<std::uint32_t>(height);
    pixels->rgba.resize(static_cast<std::size_t>(width * height * 4));
    const auto index = static_cast<std::size_t>((opaque_y * width + opaque_x) * 4 + 3);
    pixels->rgba[index] = 255;
    auto image = std::make_shared<PreparedImage>(PreparedImage{
        .key = ImageAssetKey{"Imgs/Synthetic.ff", record, ImageAssetKind::ComposedSprite},
        .pixels = pixels});
    return PreparedImageResult{.key = image->key, .image = image, .success = true};
}
} // namespace

TEST(AdventureMovementVisualPlan, OneFrameIdleBuildsMask) {
    const auto mask = d2engine::build_actor_layer_interaction_mask(
        layer("STOP0", {"IDLE0"}), std::vector<PreparedImageResult>{decoded("IDLE0", 2, 2)});
    ASSERT_NE(mask, nullptr);
    EXPECT_EQ(mask->width, 2);
    EXPECT_TRUE(mask->opaque(0, 0));
}

TEST(AdventureMovementVisualPlan, MultiFrameIdleUsesUnionIncludingLaterPixels) {
    const auto mask = d2engine::build_actor_layer_interaction_mask(
        layer("STOP0", {"IDLE0", "IDLE1"}),
        std::vector<PreparedImageResult>{decoded("IDLE0", 2, 2, 0, 0),
                                         decoded("IDLE1", 2, 2, 1, 1)});
    ASSERT_NE(mask, nullptr);
    EXPECT_TRUE(mask->opaque(0, 0));
    EXPECT_TRUE(mask->opaque(1, 1));
}

TEST(AdventureMovementVisualPlan, MissingOrInvalidDecodedFrameThrows) {
    const auto idle = layer("STOP0", {"IDLE0"});
    EXPECT_THROW(static_cast<void>(d2engine::build_actor_layer_interaction_mask(
                     idle, std::span<const PreparedImageResult>{})),
                 std::runtime_error);
    auto failed = decoded("IDLE0", 2, 2);
    failed.success = false;
    EXPECT_THROW(static_cast<void>(d2engine::build_actor_layer_interaction_mask(
                     idle, std::vector<PreparedImageResult>{failed})),
                 std::runtime_error);
    const auto null_key =
        ImageAssetKey{"Imgs/Synthetic.ff", "IDLE0", ImageAssetKind::ComposedSprite};
    auto null_image =
        std::make_shared<PreparedImage>(PreparedImage{.key = null_key, .pixels = nullptr});
    const auto null_pixels =
        PreparedImageResult{.key = null_key, .image = std::move(null_image), .success = true};
    EXPECT_THROW(static_cast<void>(d2engine::build_actor_layer_interaction_mask(
                     idle, std::vector<PreparedImageResult>{null_pixels})),
                 std::runtime_error);
    auto undersized_pixels = std::make_shared<d2res::RgbaBuffer>();
    undersized_pixels->width = 2;
    undersized_pixels->height = 2;
    undersized_pixels->rgba.resize(3);
    const auto undersized_key =
        ImageAssetKey{"Imgs/Synthetic.ff", "IDLE0", ImageAssetKind::ComposedSprite};
    auto undersized_image = std::make_shared<PreparedImage>(
        PreparedImage{.key = undersized_key, .pixels = undersized_pixels});
    const auto undersized = PreparedImageResult{
        .key = undersized_key, .image = std::move(undersized_image), .success = true};
    EXPECT_THROW(static_cast<void>(d2engine::build_actor_layer_interaction_mask(
                     idle, std::vector<PreparedImageResult>{undersized})),
                 std::runtime_error);
}

TEST(AdventureMovementVisualPlan, DimensionMismatchThrows) {
    const auto idle = layer("STOP0", {"IDLE0"}, 3, 3);
    EXPECT_THROW(static_cast<void>(d2engine::build_actor_layer_interaction_mask(
                     idle, std::vector<PreparedImageResult>{decoded("IDLE0", 2, 2)})),
                 std::runtime_error);
}

TEST(AdventureMovementVisualPlan, ShadowAndMoveLayersAreNotUsedByIdleHelper) {
    const auto idle = layer("STOP0", {"IDLE0"});
    const auto mask = d2engine::build_actor_layer_interaction_mask(
        idle, std::vector<PreparedImageResult>{decoded("IDLE0", 2, 2)});
    EXPECT_EQ(mask->width, 2);
    EXPECT_EQ(mask->height, 2);
}

namespace {

class AdventureMovementVisualPlanPreloadTest : public ::testing::Test {
protected:
    void SetUp() override {
        root_ = std::filesystem::temp_directory_path() /
                ("d2_movement_visual_plan_" + std::to_string(test_support::process_id()));
        std::error_code error;
        std::filesystem::remove_all(root_, error);
        std::filesystem::create_directories(root_ / "Imgs");
        std::ofstream dummy(root_ / "Imgs" / "synthetic.ff", std::ios::binary);
        dummy.put('\0');

        if (!SDL_Init(SDL_INIT_VIDEO))
            GTEST_SKIP() << "SDL video unavailable: " << SDL_GetError();
        window_ = SDL_CreateWindow("movement visual plan", 2, 2, SDL_WINDOW_HIDDEN);
        if (window_ == nullptr)
            GTEST_SKIP() << "SDL window unavailable: " << SDL_GetError();
        renderer_ = SDL_CreateRenderer(window_, nullptr);
        if (renderer_ == nullptr)
            GTEST_SKIP() << "SDL renderer unavailable: " << SDL_GetError();

        auto store = std::make_unique<d2engine::FfAssetStore>(root_);
        assets_ = std::make_unique<d2engine::AssetRuntime>(
            std::move(store), [this](const ImageAssetKey& key) { return decode(key); }, 1);
        render_assets_ = std::make_unique<d2engine::RenderAssetRuntime>(renderer_, *assets_);
    }

    void TearDown() override {
        render_assets_.reset();
        assets_.reset();
        if (renderer_ != nullptr)
            SDL_DestroyRenderer(renderer_);
        if (window_ != nullptr)
            SDL_DestroyWindow(window_);
        SDL_Quit();
        std::error_code error;
        std::filesystem::remove_all(root_, error);
    }

    PreparedImageResult decode(const ImageAssetKey& key) {
        ++decode_counts_[key];
        if (key == failed_key_)
            return PreparedImageResult{.key = key, .success = false, .error = failure_};

        auto pixels = std::make_shared<d2res::RgbaBuffer>();
        pixels->width = 2;
        pixels->height = 2;
        pixels->rgba.assign(16, 0);
        pixels->rgba[3] = 255;
        auto image =
            std::make_shared<PreparedImage>(PreparedImage{.key = key, .pixels = std::move(pixels)});
        return PreparedImageResult{.key = key, .image = std::move(image), .success = true};
    }

    [[nodiscard]] std::size_t decode_count(const ImageAssetKey& key) const {
        const auto it = decode_counts_.find(key);
        return it == decode_counts_.end() ? 0 : it->second;
    }

    void reset_decode_counts() { decode_counts_.clear(); }

    void cache(const ImageAssetKey& key) {
        auto pixels = std::make_shared<d2res::RgbaBuffer>();
        pixels->width = 2;
        pixels->height = 2;
        pixels->rgba.assign(16, 0);
        pixels->rgba[3] = 255;
        const PreparedImage image{.key = key, .pixels = std::move(pixels)};
        ASSERT_TRUE(render_assets_->textures().upload_prepared(image).success);
    }

    [[nodiscard]] ImageAssetKey key(std::string record) const {
        return ImageAssetKey{"Imgs/Synthetic.ff", std::move(record),
                             ImageAssetKind::ComposedSprite};
    }

    [[nodiscard]] d2engine::AdventureMovementVisualPlan plan(std::string          stack_id,
                                                             const ImageAssetKey& move_key,
                                                             const ImageAssetKey& idle_key,
                                                             std::size_t segment_count = 1) const {
        d2engine::AdventureMovementVisualPlan result;
        result.stack_id = std::move(stack_id);
        result.route.start = {0, 0};
        result.route.destination = {static_cast<int>(segment_count), 0};
        for (std::size_t i = 0; i < segment_count; ++i) {
            d2engine::AdventureMovementSegmentVisual segment;
            segment.route_step_index = i;
            segment.from = {static_cast<int>(i), 0};
            segment.to = {static_cast<int>(i + 1), 0};
            segment.move_visual.body = layer("MOVE0", {move_key.image_name});
            segment.idle_visual_at_destination.body = layer("STOP0", {idle_key.image_name});
            result.segments.push_back(std::move(segment));
        }
        return result;
    }

    std::filesystem::path                          root_;
    SDL_Window*                                    window_ = nullptr;
    SDL_Renderer*                                  renderer_ = nullptr;
    std::unique_ptr<d2engine::AssetRuntime>        assets_;
    std::unique_ptr<d2engine::RenderAssetRuntime>  render_assets_;
    std::unordered_map<ImageAssetKey, std::size_t> decode_counts_;
    ImageAssetKey                                  failed_key_;
    std::string                                    failure_;
};

TEST_F(AdventureMovementVisualPlanPreloadTest, GpuCachedIdleBodyIsStillDecodedForMask) {
    const auto move = key("MOVE0");
    const auto idle = key("STOP0");
    cache(idle);
    auto movement_plan = plan("SYNTHETIC_STACK", move, idle);

    EXPECT_NO_THROW(
        d2engine::preload_adventure_movement_visual_plan(movement_plan, *assets_, *render_assets_));
    EXPECT_EQ(decode_count(idle), 1);
    ASSERT_NE(movement_plan.segments[0].idle_interaction_mask_at_destination, nullptr);
    EXPECT_TRUE(movement_plan.segments[0].idle_interaction_mask_at_destination->opaque(0, 0));
    EXPECT_NE(render_assets_->textures().find(idle), nullptr);
}

TEST_F(AdventureMovementVisualPlanPreloadTest, GpuCachedStop7WhpRegression) {
    const ImageAssetKey idle{"Imgs/Isounit.ff", "WHP", ImageAssetKind::ComposedSprite};
    const ImageAssetKey move{"Imgs/Isounit.ff", "G000UU5129MOVE7", ImageAssetKind::ComposedSprite};
    cache(idle);
    auto movement_plan = plan("S143KC0012", move, idle);
    movement_plan.segments[0].idle_visual_at_destination.body.container_path = "Imgs/Isounit.ff";
    movement_plan.segments[0].idle_visual_at_destination.body.frames[0].record_name = "WHP";
    movement_plan.segments[0].move_visual.body.container_path = "Imgs/Isounit.ff";
    movement_plan.segments[0].move_visual.body.frames[0].record_name = "G000UU5129MOVE7";
    EXPECT_NO_THROW(
        d2engine::preload_adventure_movement_visual_plan(movement_plan, *assets_, *render_assets_));
    EXPECT_NE(movement_plan.segments[0].idle_interaction_mask_at_destination, nullptr);
}

TEST_F(AdventureMovementVisualPlanPreloadTest, GpuCachedStop0VepRegression) {
    const ImageAssetKey idle{"Imgs/Isounit.ff", "VEP", ImageAssetKind::ComposedSprite};
    const ImageAssetKey move{"Imgs/Isounit.ff", "G000UU5129MOVE0", ImageAssetKind::ComposedSprite};
    cache(idle);
    auto movement_plan = plan("S143KC0012", move, idle);
    movement_plan.segments[0].idle_visual_at_destination.body.container_path = "Imgs/Isounit.ff";
    movement_plan.segments[0].idle_visual_at_destination.body.frames[0].record_name = "VEP";
    movement_plan.segments[0].move_visual.body.container_path = "Imgs/Isounit.ff";
    movement_plan.segments[0].move_visual.body.frames[0].record_name = "G000UU5129MOVE0";
    EXPECT_NO_THROW(
        d2engine::preload_adventure_movement_visual_plan(movement_plan, *assets_, *render_assets_));
}

TEST_F(AdventureMovementVisualPlanPreloadTest, SharedIdleFrameAcrossSegmentsDecodesOnce) {
    const auto move = key("MOVE0");
    const auto idle = key("STOP0");
    auto       movement_plan = plan("SYNTHETIC_STACK", move, idle, 2);

    EXPECT_NO_THROW(
        d2engine::preload_adventure_movement_visual_plan(movement_plan, *assets_, *render_assets_));
    EXPECT_EQ(decode_count(idle), 1);
    EXPECT_NE(movement_plan.segments[0].idle_interaction_mask_at_destination, nullptr);
    EXPECT_NE(movement_plan.segments[1].idle_interaction_mask_at_destination, nullptr);
}

TEST_F(AdventureMovementVisualPlanPreloadTest, NonCachedIdleFrameIsDecodedUploadedAndMasked) {
    const auto move = key("MOVE0");
    const auto idle = key("STOP0");
    auto       movement_plan = plan("SYNTHETIC_STACK", move, idle);

    EXPECT_NO_THROW(
        d2engine::preload_adventure_movement_visual_plan(movement_plan, *assets_, *render_assets_));
    EXPECT_EQ(decode_count(idle), 1);
    EXPECT_NE(movement_plan.segments[0].idle_interaction_mask_at_destination, nullptr);
    EXPECT_NE(render_assets_->textures().find(idle), nullptr);
    EXPECT_EQ(assets_->prepared_resident_stats().count, 0);
}

TEST_F(AdventureMovementVisualPlanPreloadTest, CachedMoveFrameDoesNotForceCpuDecode) {
    const auto move = key("MOVE0");
    const auto idle = key("STOP0");
    cache(move);
    auto movement_plan = plan("SYNTHETIC_STACK", move, idle);

    ASSERT_NO_THROW(
        d2engine::preload_adventure_movement_visual_plan(movement_plan, *assets_, *render_assets_));
    EXPECT_EQ(decode_count(move), 0);
    EXPECT_EQ(decode_count(idle), 1);
}

TEST_F(AdventureMovementVisualPlanPreloadTest, IdleDecodeFailureIncludesExactContext) {
    failed_key_ = {"Imgs/Isounit.ff", "WHP", ImageAssetKind::ComposedSprite};
    failure_ = "synthetic decoder failure";
    const ImageAssetKey move{"Imgs/Isounit.ff", "G000UU5129MOVE7", ImageAssetKind::ComposedSprite};
    auto                movement_plan = plan("S143KC0012", move, failed_key_);
    movement_plan.segments[0].idle_visual_at_destination.body.container_path = "Imgs/Isounit.ff";
    movement_plan.segments[0].idle_visual_at_destination.body.logical_animation_name =
        "G000UU5129STOP7";
    movement_plan.segments[0].idle_visual_at_destination.body.frames[0].record_name = "WHP";
    movement_plan.segments[0].move_visual.body.container_path = "Imgs/Isounit.ff";
    movement_plan.segments[0].move_visual.body.frames[0].record_name = "G000UU5129MOVE7";

    try {
        d2engine::preload_adventure_movement_visual_plan(movement_plan, *assets_, *render_assets_);
        FAIL() << "expected preload failure";
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("S143KC0012"), std::string::npos);
        EXPECT_NE(message.find("G000UU5129STOP7"), std::string::npos);
        EXPECT_NE(message.find("Imgs/Isounit.ff"), std::string::npos);
        EXPECT_NE(message.find("WHP"), std::string::npos);
        EXPECT_NE(message.find("synthetic decoder failure"), std::string::npos);
    }
}

TEST_F(AdventureMovementVisualPlanPreloadTest, AllIdlePreparedResultsAreReleased) {
    auto movement_plan = plan("SYNTHETIC_STACK", key("MOVE0"), key("STOP0"));
    ASSERT_NO_THROW(
        d2engine::preload_adventure_movement_visual_plan(movement_plan, *assets_, *render_assets_));
    EXPECT_EQ(assets_->prepared_resident_stats().count, 0);
}

TEST_F(AdventureMovementVisualPlanPreloadTest, GpuResidencyDoesNotChangeIdleDecodeCount) {
    const auto move = key("MOVE0");
    const auto idle = key("STOP0");
    auto       first_plan = plan("SYNTHETIC_STACK", move, idle);
    ASSERT_NO_THROW(
        d2engine::preload_adventure_movement_visual_plan(first_plan, *assets_, *render_assets_));
    EXPECT_EQ(decode_count(idle), 1);

    reset_decode_counts();
    cache(idle);
    auto second_plan = plan("SYNTHETIC_STACK", move, idle);
    ASSERT_NO_THROW(
        d2engine::preload_adventure_movement_visual_plan(second_plan, *assets_, *render_assets_));
    EXPECT_EQ(decode_count(idle), 1);
}

TEST(AdventureMovementVisualPlan, PreloadSourceContractUsesDirectIdleRuntimeRequest) {
    const auto source_root = std::filesystem::path(OPENDIS2_SOURCE_DIR);
    const auto read_source = [](const std::filesystem::path& path) {
        std::ifstream file(path);
        return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    };
    const auto header =
        read_source(source_root / "src/d2engine/app/adventure_movement_visual_plan.hpp");
    const auto implementation =
        read_source(source_root / "src/d2engine/app/adventure_movement_visual_plan.cpp");
    const auto screen = read_source(source_root / "src/d2engine/app/adventure_screen.cpp");

    EXPECT_NE(header.find("AssetRuntime&"), std::string::npos);
    EXPECT_NE(header.find("RenderAssetRuntime&"), std::string::npos);
    EXPECT_NE(implementation.find("assets.request_batch("), std::string::npos);
    EXPECT_NE(implementation.find("idle_body_decode_keys"), std::string::npos);
    EXPECT_NE(implementation.find("ordinary_texture_keys"), std::string::npos);
    EXPECT_NE(implementation.find("render_assets.request_textures("), std::string::npos);
    EXPECT_NE(screen.find("runtime_.assets"), std::string::npos);
    EXPECT_NE(screen.find("runtime_.render_assets"), std::string::npos);
}

} // namespace
