#include <gtest/gtest.h>
#include "d2engine/assets/asset_runtime.hpp"
#include "d2engine/render/game_texture_cache.hpp"
#include "d2engine/render/texture_cache.hpp"
#include "d2asset/asset_database.hpp"

#include <SDL3/SDL.h>
#include <lodepng.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>

#include "tests/test_process.hpp"

namespace fs = std::filesystem;
using namespace d2engine;

class TextureCacheTest : public ::testing::Test {
protected:
    void SetUp() override {
        static std::size_t sequence = 0;
        root_ = fs::temp_directory_path() /
                ("d2texture_test_" + std::to_string(test_support::process_id()) + "_" +
                 std::to_string(++sequence));
        std::error_code ec;
        fs::remove_all(root_, ec);
        fs::create_directories(root_, ec);

        // Create a real 1x1 PNG atlas sheet
        fs::create_directories(root_ / "atlas");
        std::vector<uint8_t> const rgba = {255, 0, 255, 255};
        std::vector<uint8_t>       png_data;
        const unsigned             err = lodepng::encode(png_data, rgba, 1, 1);
        ASSERT_EQ(err, 0u);
        std::ofstream png_file(root_ / "atlas" / "atlas_000.png", std::ios::binary);
        png_file.write(reinterpret_cast<const char*>(png_data.data()),
                       static_cast<std::streamsize>(png_data.size()));
        png_file.close();

        // Create game_manifest.json
        nlohmann::json manifest = {
            {"asset_schema_version", 1},
            {"containers",
             nlohmann::json::array({{{"container_id", "test/test.ff"},
                                     {"path", "Test/Test.ff"},
                                     {"content_kinds", nlohmann::json::array({"images"})}}})},
            {"assets", nlohmann::json::array({{{"asset_id", "test/test.ff/test_sprite"},
                                               {"logical_name", "test_sprite"},
                                               {"type", "image"},
                                               {"container_id", "test/test.ff"},
                                               {"path", "atlas/test_sprite.json"}},
                                              {{"asset_id", "test/test.ff/atlas"},
                                               {"logical_name", "Atlas"},
                                               {"type", "atlas"},
                                               {"container_id", "test/test.ff"},
                                               {"path", "atlas/atlas.json"}}})},
            {"warnings", nlohmann::json::array()}};

        std::ofstream manifest_file(root_ / "game_manifest.json");
        manifest_file << manifest.dump(2) << '\n';
        manifest_file.close();

        // Create image sidecar
        std::ofstream image_sidecar(root_ / "atlas" / "test_sprite.json");
        image_sidecar << "{}\n";
        image_sidecar.close();

        // Create atlas sidecar
        nlohmann::json const atlas_sidecar = {
            {"source_container", "test/test.ff"},
            {"max_sheet_size", 64},
            {"sheet_count", 1},
            {"total_sprites", 1},
            {"skipped_sprites", 0},
            {"entries", nlohmann::json::array({{{"name", "test_sprite"},
                                                {"sheet", 0},
                                                {"x", 0},
                                                {"y", 0},
                                                {"w", 1},
                                                {"h", 1}}})}};

        std::ofstream atlas_file(root_ / "atlas" / "atlas.json");
        atlas_file << atlas_sidecar.dump(2) << '\n';
        atlas_file.close();

        // Create animation sidecar
        nlohmann::json const anim_sidecar = {
            {"name", "Idle"},
            {"frame_count", 1},
            {"source_container", "test/test.ff"},
            {"frames",
             nlohmann::json::array(
                 {{{"index", 0}, {"logical_name", "test_sprite"}, {"width", 1}, {"height", 1}}})},
            {"frame_delay_ms", 100}};

        std::ofstream anim_file(root_ / "atlas" / "anim.json");
        anim_file << anim_sidecar.dump(2) << '\n';
        anim_file.close();

        // Add animation asset to manifest
        manifest["assets"].push_back({{"asset_id", "test/test.ff/anim"},
                                      {"logical_name", "Idle"},
                                      {"type", "animation"},
                                      {"container_id", "test/test.ff"},
                                      {"path", "atlas/anim.json"}});
        std::ofstream manifest_file2(root_ / "game_manifest.json");
        manifest_file2 << manifest.dump(2) << '\n';
        manifest_file2.close();

        // Initialize SDL
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            GTEST_SKIP() << "SDL_Init failed: " << SDL_GetError();
        }
        window_ = SDL_CreateWindow("test", 1, 1, SDL_WINDOW_HIDDEN);
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
        std::error_code ec;
        fs::remove_all(root_, ec);
    }

    fs::path      root_;
    SDL_Window*   window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
};

TEST_F(TextureCacheTest, ConstructsAndLoadsFirstAtlas) {
    RuntimeAssetContext const assets(root_);
    TextureCache const        cache(renderer_, assets);
    ASSERT_FALSE(cache.all_atlas_ids().empty());
    EXPECT_NE(cache.get(cache.all_atlas_ids()[0]), nullptr);
}

TEST_F(TextureCacheTest, CacheHitReturnsSameTexture) {
    RuntimeAssetContext const assets(root_);
    TextureCache const        cache(renderer_, assets);

    SDL_Texture const* tex1 = cache.get("test/test.ff/atlas/sheet0");
    SDL_Texture const* tex2 = cache.get("test/test.ff/atlas/sheet0");
    EXPECT_NE(tex1, nullptr);
    EXPECT_EQ(tex1, tex2);
}

TEST_F(TextureCacheTest, GetUnknownAtlasReturnsNull) {
    RuntimeAssetContext const assets(root_);
    TextureCache const        cache(renderer_, assets);

    EXPECT_EQ(cache.get("nonexistent"), nullptr);
}

TEST_F(TextureCacheTest, GameTextureCacheReusesUploadedTextureAcrossLogicalConsumers) {
    GameTextureCache cache(renderer_);
    ImageAssetKey    key{.container_path = "Imgs/Interf.ff",
                         .image_name = "BUTTON.PNG",
                         .kind = ImageAssetKind::RawPng};
    auto             pixels = std::make_shared<d2res::RgbaBuffer>();
    pixels->width = 1;
    pixels->height = 1;
    pixels->rgba = {20, 40, 60, 255};
    PreparedImage image{.key = key, .pixels = std::move(pixels)};

    const auto adventure_upload = cache.upload_prepared(image);
    ASSERT_TRUE(adventure_upload.success) << adventure_upload.error;
    SDL_Texture* adventure_texture = cache.find(key);
    ASSERT_NE(adventure_texture, nullptr);

    EXPECT_TRUE(cache.is_cached(key));
    EXPECT_EQ(cache.find(key), adventure_texture);
    const auto battle_upload = cache.upload_prepared(image);
    EXPECT_TRUE(battle_upload.success);
    EXPECT_TRUE(battle_upload.skipped_cache_hit);
    EXPECT_EQ(cache.find(key), adventure_texture);
    EXPECT_EQ(cache.stats().prepared_upload_successes, 1u);
    EXPECT_EQ(cache.stats().prepared_upload_skipped_cache_hits, 1u);
}
