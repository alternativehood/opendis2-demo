#include <gtest/gtest.h>

#include "d2engine/app/adventure_loading_screen.hpp"
#include "d2engine/assets/asset_runtime.hpp"
#include "d2engine/assets/ff_asset_store.hpp"
#include "d2engine/render/renderer2d.hpp"
#include "opendis2/adventure_startup_screen.hpp"

#include <SDL3/SDL.h>

#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <string>
#include <stdexcept>
#include <vector>

namespace d2engine {
namespace {

[[nodiscard]] std::string read_source_file(const std::string& rel_path) {
    const auto    path = std::filesystem::path(OPENDIS2_SOURCE_DIR) / rel_path;
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("cannot open source file: " + path.string());
    }
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

class LoadingScreenFixture : public ::testing::Test {
protected:
    void SetUp() override {
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            GTEST_SKIP() << "SDL_Init failed: " << SDL_GetError();
        }
        window_ = SDL_CreateWindow("loading-screen-test", 4, 4, SDL_WINDOW_HIDDEN);
        if (window_ == nullptr) {
            SDL_Quit();
            GTEST_SKIP() << "SDL_CreateWindow failed: " << SDL_GetError();
        }
        renderer_ = SDL_CreateRenderer(window_, nullptr);
        if (renderer_ == nullptr) {
            SDL_DestroyWindow(window_);
            SDL_Quit();
            GTEST_SKIP() << "SDL_CreateRenderer failed: " << SDL_GetError();
        }
    }

    void TearDown() override {
        if (renderer_ != nullptr) {
            SDL_DestroyRenderer(renderer_);
        }
        if (window_ != nullptr) {
            SDL_DestroyWindow(window_);
        }
        SDL_Quit();
    }

    SDL_Window*   window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
};

class RecordingRenderer2D final : public Renderer2D {
public:
    RecordingRenderer2D() : Renderer2D(nullptr) {}

    void clear(Color c) override { clear_color_ = c; }

    void draw_texture(SDL_Texture* tex, Rect src, Rect dst, float alpha = 1.0f, bool flip_h = false,
                      bool flip_v = false) override {
        textured_calls_.push_back({tex, src, dst, alpha, flip_h, flip_v});
    }

    void draw_texture(SDL_Texture* tex, Rect dst, float alpha = 1.0f, bool flip_h = false,
                      bool flip_v = false) override {
        textured_calls_.push_back({tex, Rect{0.0f, 0.0f, 0.0f, 0.0f}, dst, alpha, flip_h, flip_v});
    }

    void draw_rect(Rect rect, Color c, bool filled = true) override {
        rect_calls_.push_back({rect, c, filled});
    }

    struct TextureCall {
        SDL_Texture* tex = nullptr;
        Rect         src{};
        Rect         dst{};
        float        alpha = 1.0f;
        bool         flip_h = false;
        bool         flip_v = false;
    };

    struct RectCall {
        Rect  rect{};
        Color color{};
        bool  filled = true;
    };

    Color                    clear_color_{};
    std::vector<TextureCall> textured_calls_;
    std::vector<RectCall>    rect_calls_;
};

TEST(AdventureStartupArchitecture, LauncherUsesLoadingScreenFirst) {
    const auto launcher = read_source_file("src/opendis2/adventure_launcher.cpp");
    EXPECT_NE(launcher.find("AdventureStartupScreen"), std::string::npos);
    EXPECT_NE(launcher.find("start_with_screen(std::move(loading_screen))"), std::string::npos);
    EXPECT_EQ(launcher.find("make_adventure_screen"), std::string::npos);
    EXPECT_EQ(launcher.find("prepare_full_map("), std::string::npos);
}

TEST(AdventureStartupArchitecture, StartupScreenDefersHeavyWorkUntilAfterFirstFrame) {
    const auto startup = read_source_file("src/opendis2/adventure_startup_screen.cpp");
    EXPECT_NE(startup.find("adventure_loading_screen entered"), std::string::npos);
    EXPECT_NE(startup.find("loading_background loaded"), std::string::npos);
    EXPECT_NE(startup.find("adventure_loading_screen first_frame_rendered"), std::string::npos);
    EXPECT_NE(startup.find("if (!rendered_once_)"), std::string::npos);
    EXPECT_NE(startup.find("decode_sprite(\"Interf/Interf.ff\", \"LOADING\")"), std::string::npos);
    EXPECT_EQ(startup.find("raw_png(\"Interf/Interf.ff\""), std::string::npos);
    EXPECT_EQ(startup.find("decode_sprite(\"Interf/Interf.ff\", \"ImageMap#175#000\")"),
              std::string::npos);
    EXPECT_NE(startup.find("loading_background loaded source=Interf/Interf.ff logical=LOADING"),
              std::string::npos);
    EXPECT_NE(startup.find("physical=LOADING_D2ELF.PNG size={}x{} composed=true"),
              std::string::npos);
    EXPECT_EQ(startup.find("LOADING_D2_ELF.PNG"), std::string::npos);
    EXPECT_NE(startup.find("std::move(scenario_->outcome.session)"), std::string::npos);
    EXPECT_NE(startup.find("std::move(session)"), std::string::npos);
    EXPECT_EQ(startup.find("&store]"), std::string::npos);
    EXPECT_NE(startup.find("auto& store = app.screen_config_store();"), std::string::npos);
    EXPECT_NE(startup.find("app_.ensure_shared_runtime_initialized();"), std::string::npos);
    EXPECT_NE(startup.find("adventure_startup stage=ParseScenario"), std::string::npos);
    EXPECT_NE(startup.find("request_batch(source_assets"), std::string::npos);
    EXPECT_NE(startup.find("\"AdventureTerrain\""), std::string::npos);
}

TEST(AdventureStartupArchitecture, ProductionBuildsTypedRuinCatalog) {
    const auto startup = read_source_file("src/opendis2/adventure_startup_screen.cpp");
    EXPECT_NE(startup.find("build_ruin_asset_catalog(app.asset_runtime().store())"),
              std::string::npos);
    EXPECT_EQ(startup.find("RuinAssetCatalog ruin_catalog;"), std::string::npos);
    const auto engine_cmake = read_source_file("src/d2engine/CMakeLists.txt");
    EXPECT_NE(engine_cmake.find("assets/ruin_asset_catalog_builder.cpp"), std::string::npos);
}

TEST(AdventureStartupArchitecture, AdventureScreenNoLongerOwnsLoadingStartupState) {
    const auto screen = read_source_file("src/d2engine/app/adventure_screen.hpp");
    EXPECT_NE(
        screen.find(
            "AdventureScreen(AppRuntimeContext& runtime, std::unique_ptr<d2game::GameSession>"),
        std::string::npos);
    EXPECT_NE(screen.find("std::unique_ptr<d2game::GameSession>"), std::string::npos);
    EXPECT_EQ(screen.find("d2game::GameSession&"), std::string::npos);
    EXPECT_EQ(screen.find("startup_phase_"), std::string::npos);
    EXPECT_EQ(screen.find("loading_background_"), std::string::npos);
    EXPECT_EQ(screen.find("loading_screen_"), std::string::npos);
    EXPECT_EQ(screen.find("initial_asset_batch_"), std::string::npos);
    EXPECT_EQ(screen.find("advance_startup"), std::string::npos);
    const auto startup = read_source_file("src/opendis2/adventure_startup_screen.cpp");
    const auto startup_header = read_source_file("src/opendis2/adventure_startup_screen.hpp");
    EXPECT_NE(startup_header.find("initial_preload"), std::string::npos);
    EXPECT_NE(startup_header.find("animation_preload"), std::string::npos);
    EXPECT_NE(startup.find("UploadAnimationAssets"), std::string::npos);
    EXPECT_NE(startup.find("AdventureWorldAnimations"), std::string::npos);
    EXPECT_EQ(startup.find("AdventureWorldDeferred"), std::string::npos);
    const auto screen_source = read_source_file("src/d2engine/app/adventure_screen.cpp");
    EXPECT_EQ(screen_source.find("adventure_deferred_assets"), std::string::npos);
    EXPECT_EQ(screen_source.find("begin_incremental_preload"), std::string::npos);
    EXPECT_EQ(screen_source.find("AdventureWorldAnimations"), std::string::npos);
}

TEST(AdventureStartupLoadingScreen, ProgressIsMonotonic) {
    AdventureLoadingScreen screen(nullptr);
    EXPECT_FLOAT_EQ(screen.progress(), 0.0f);
    screen.set_progress(0.05f);
    EXPECT_FLOAT_EQ(screen.progress(), 0.05f);
    screen.set_progress(0.20f);
    EXPECT_FLOAT_EQ(screen.progress(), 0.20f);
    screen.set_progress(0.55f);
    EXPECT_FLOAT_EQ(screen.progress(), 0.55f);
    screen.set_progress(0.90f);
    EXPECT_FLOAT_EQ(screen.progress(), 0.90f);
    screen.set_progress(1.0f);
    EXPECT_FLOAT_EQ(screen.progress(), 1.0f);
}

TEST_F(LoadingScreenFixture, NullBackgroundRendersWithoutCrashing) {
    AdventureLoadingScreen screen(nullptr);
    Renderer2D             r2d(renderer_);
    screen.render(r2d, 1416, 852);
}

TEST_F(LoadingScreenFixture, RenderBackgroundFitsContainAndCentersInWidescreenViewport) {
    SDL_Texture* texture =
        SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, 800, 600);
    ASSERT_NE(texture, nullptr) << SDL_GetError();

    AdventureLoadingScreen screen(texture);
    RecordingRenderer2D    recorder;
    screen.render(recorder, 1280, 720);

    ASSERT_FALSE(recorder.textured_calls_.empty());
    const auto& background_call = recorder.textured_calls_.front();
    EXPECT_FLOAT_EQ(background_call.src.x, 0.0f);
    EXPECT_FLOAT_EQ(background_call.src.y, 0.0f);
    EXPECT_FLOAT_EQ(background_call.src.w, 800.0f);
    EXPECT_FLOAT_EQ(background_call.src.h, 600.0f);
    EXPECT_FLOAT_EQ(background_call.dst.x, 160.0f);
    EXPECT_FLOAT_EQ(background_call.dst.y, 0.0f);
    EXPECT_FLOAT_EQ(background_call.dst.w, 960.0f);
    EXPECT_FLOAT_EQ(background_call.dst.h, 720.0f);

    for (const auto& call : recorder.rect_calls_) {
        EXPECT_GE(call.rect.x, 0.0f);
        EXPECT_GE(call.rect.y, 0.0f);
        EXPECT_LE(call.rect.x + call.rect.w, 1280.0f);
        EXPECT_LE(call.rect.y + call.rect.h, 720.0f);
    }

    SDL_DestroyTexture(texture);
}

TEST(AdventureStartupBackground, RealAssetDecodesFromGameRootIfPresent) {
    const char* root_env = std::getenv("DISCIPLES2_GAME_ROOT");
    if (root_env == nullptr || std::string(root_env).empty()) {
        GTEST_SKIP() << "DISCIPLES2_GAME_ROOT not set";
    }

    const std::filesystem::path root_path(root_env);
    if (!std::filesystem::is_directory(root_path)) {
        GTEST_SKIP() << "DISCIPLES2_GAME_ROOT is not a directory: " << root_path;
    }

    FfAssetStore store{root_path};
    const auto   composed = store.decode_sprite("Interf/Interf.ff", "LOADING");
    EXPECT_EQ(composed.width, 800u);
    EXPECT_EQ(composed.height, 600u);
    EXPECT_FALSE(composed.rgba.empty());

    const std::size_t total_pixels = static_cast<std::size_t>(composed.width) * composed.height;
    std::size_t       magenta_pixels = 0;
    for (std::size_t i = 0; i + 3 < composed.rgba.size(); i += 4) {
        if (composed.rgba[i] == 255 && composed.rgba[i + 1] == 0 && composed.rgba[i + 2] == 255) {
            ++magenta_pixels;
        }
    }
    EXPECT_LT(magenta_pixels, total_pixels / 4);
}

} // namespace
} // namespace d2engine
