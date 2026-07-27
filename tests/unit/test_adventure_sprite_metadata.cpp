#include <gtest/gtest.h>

#include "d2engine/app/adventure_visual_resources.hpp"
#include "d2engine/assets/asset_runtime.hpp"
#include "d2engine/assets/ff_asset_store.hpp"
#include "d2engine/assets/image_asset_key.hpp"
#include "d2engine/render/render_asset_runtime.hpp"

#include <d2res/rgba_buffer.hpp>

#include <SDL3/SDL.h>

#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <lodepng.h>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "tests/test_process.hpp"

#ifndef DISCIPLES2_GAME_ROOT
#define DISCIPLES2_GAME_ROOT ""
#endif

namespace fs = std::filesystem;

// ── RAII SDL_Cursor owner ──────────────────────────────────────────────
struct CursorOwner {
    SDL_Cursor* cursor = nullptr;
    ~CursorOwner() {
        if (cursor)
            SDL_DestroyCursor(cursor);
    }
    CursorOwner(const CursorOwner&) = delete;
    CursorOwner& operator=(const CursorOwner&) = delete;
    CursorOwner(CursorOwner&& o) noexcept : cursor(o.cursor) { o.cursor = nullptr; }
    CursorOwner& operator=(CursorOwner&& o) noexcept {
        if (this != &o) {
            if (cursor)
                SDL_DestroyCursor(cursor);
            cursor = o.cursor;
            o.cursor = nullptr;
        }
        return *this;
    }
};

// ── Minimal PNG ────────────────────────────────────────────────────────
static std::vector<uint8_t> make_1x1_png(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    const unsigned char        pixels[4] = {r, g, b, a};
    std::vector<unsigned char> out;
    unsigned                   err = lodepng::encode(out, pixels, 1, 1);
    if (err != 0)
        throw std::runtime_error("lodepng_encode");
    return {out.begin(), out.end()};
}

// ── LE write helpers ────────────────────────────────────────────────────
static void w32(std::vector<uint8_t>& b, int32_t v) {
    b.push_back(static_cast<uint8_t>(v));
    b.push_back(static_cast<uint8_t>(v >> 8));
    b.push_back(static_cast<uint8_t>(v >> 16));
    b.push_back(static_cast<uint8_t>(v >> 24));
}
static void w16(std::vector<uint8_t>& b, uint16_t v) {
    b.push_back(static_cast<uint8_t>(v));
    b.push_back(static_cast<uint8_t>(v >> 8));
}
static void w_fixed(std::vector<uint8_t>& b, std::string_view s, size_t width) {
    size_t start = b.size();
    b.insert(b.end(), s.begin(), s.end());
    if (b.size() - start > width)
        b.resize(start + width);
    while (b.size() - start < width)
        b.push_back(0);
}
static void w_cstr(std::vector<uint8_t>& b, std::string_view s) {
    b.insert(b.end(), s.begin(), s.end());
    b.push_back(0);
}
static std::vector<uint8_t> mqrc(int32_t id, const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> r;
    r.push_back('M');
    r.push_back('Q');
    r.push_back('R');
    r.push_back('C');
    w32(r, 0);
    w32(r, id);
    auto sz = static_cast<int32_t>(payload.size());
    w32(r, sz);
    w32(r, sz);
    w32(r, 1);
    w32(r, 0);
    r.insert(r.end(), payload.begin(), payload.end());
    return r;
}

struct PieceSpec {
    int ox, oy, w, h;
};
struct StaticSpriteSpec {
    std::string            sprite_name = "MYSTATIC";
    int32_t                png_id = 200, idx_id = 100, img_id = 101;
    int32_t                frame_w = 80, frame_h = 60;
    std::vector<PieceSpec> pieces = {{10, 20, 30, 25}};
    uint8_t                r = 0xFF, g = 0x00, b = 0x00;
};

static void write_static_sprite_ff(const fs::path& path, const StaticSpriteSpec& s) {
    std::vector<uint8_t> images;
    images.push_back(0);
    w16(images, 255);
    w32(images, 1);
    w32(images, 1);
    for (int i = 0; i < 256; ++i) {
        images.push_back(0);
        images.push_back(0);
        images.push_back(0);
        images.push_back(0);
    }
    w32(images, 1);
    w_cstr(images, s.sprite_name);
    auto pc = static_cast<int32_t>(s.pieces.size());
    w32(images, pc);
    w32(images, s.frame_w);
    w32(images, s.frame_h);
    for (const auto& p : s.pieces) {
        w32(images, p.ox);
        w32(images, p.oy);
        w32(images, 0);
        w32(images, 0);
        w32(images, p.w);
        w32(images, p.h);
    }
    auto img_sz = static_cast<int32_t>(images.size());

    // INDEX: only image entry, no animation
    std::vector<uint8_t> idx;
    w32(idx, 1);
    w32(idx, s.png_id);
    w_cstr(idx, s.sprite_name);
    w32(idx, 0);
    w32(idx, img_sz);

    // No ANIMS payload at all

    auto png = make_1x1_png(s.r, s.g, s.b, 255);

    std::vector<uint8_t> nt;
    w32(nt, 4);
    w_fixed(nt, s.sprite_name, 256);
    w32(nt, s.idx_id);
    w_fixed(nt, s.sprite_name + ".PNG", 256);
    w32(nt, s.png_id);
    w_fixed(nt, "-INDEX.OPT", 256);
    w32(nt, s.idx_id);
    w_fixed(nt, "-IMAGES.OPT", 256);
    w32(nt, s.img_id);

    std::vector<uint8_t> mff = {'M', 'F', 'F', 0, 0, 0, 0, 0};

    auto r1 = mqrc(1, mff);
    auto r2 = mqrc(2, nt);
    auto ri = mqrc(s.idx_id, idx);
    auto rm = mqrc(s.img_id, images);
    auto rp = mqrc(s.png_id, png);

    std::vector<uint8_t> file;
    file.push_back('M');
    file.push_back('Q');
    file.push_back('D');
    file.push_back('B');
    w32(file, 0);
    w32(file, 5);
    w32(file, 0);
    w32(file, 0);
    w32(file, 0);
    w32(file, 0);
    file.insert(file.end(), r1.begin(), r1.end());
    file.insert(file.end(), r2.begin(), r2.end());
    file.insert(file.end(), ri.begin(), ri.end());
    file.insert(file.end(), rm.begin(), rm.end());
    file.insert(file.end(), rp.begin(), rp.end());

    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(file.data()),
              static_cast<std::streamsize>(file.size()));
}

// ── Self-contained FfAssetStore fixture (no game data needed) ──────────
class StaticSpriteMetadataTest : public ::testing::Test {
protected:
    fs::path temp_root_;
    fs::path ff_path_;

    void SetUp() override {
        static std::atomic<int> seq{0};
        temp_root_ = fs::temp_directory_path() /
                     ("opendis2_static_sprite_test_" + std::to_string(test_support::process_id()) +
                      "_" + std::to_string(seq.fetch_add(1)));
        fs::create_directories(temp_root_ / "Imgs");
        ff_path_ = temp_root_ / "Imgs" / "Teststat.ff";

        StaticSpriteSpec spec{
            .sprite_name = "MYSTATIC", .frame_w = 80, .frame_h = 60, .pieces = {{10, 20, 30, 25}}};
        write_static_sprite_ff(ff_path_, spec);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(temp_root_, ec);
    }
};

// ── Tests ───────────────────────────────────────────────────────────────

TEST_F(StaticSpriteMetadataTest, SucceedsWithoutAnimationNamespace) {
    d2engine::FfAssetStore store(temp_root_);
    // Verify no animation exists with this name
    auto anims = store.animations_in("Imgs/Teststat.ff");
    bool found = false;
    for (const auto& a : anims) {
        if (a == "MYSTATIC") {
            found = true;
        }
    }
    EXPECT_FALSE(found) << "MYSTATIC must NOT be in animation namespace";

    // sprite_metadata must succeed anyway
    auto meta = store.sprite_metadata("Imgs/Teststat.ff", "MYSTATIC");
    EXPECT_EQ(meta.canvas_width, 80);
    EXPECT_EQ(meta.canvas_height, 60);
}

TEST_F(StaticSpriteMetadataTest, AsymmetricSinglePieceGeometry) {
    d2engine::FfAssetStore store(temp_root_);
    auto                   meta = store.sprite_metadata("Imgs/Teststat.ff", "MYSTATIC");
    // Piece at (10,20) size (30,25) in 80x60 canvas
    EXPECT_EQ(meta.canvas_width, 80);
    EXPECT_EQ(meta.canvas_height, 60);
    EXPECT_EQ(meta.canvas_foot_x, (10 + 10 + 30) / 2); // (min_x + max_x)/2 = (10 + 40)/2 = 25
    EXPECT_EQ(meta.canvas_foot_y, 20 + 25);            // max_y = 45
    EXPECT_EQ(meta.canvas_top_y, 20);                  // min_y
}

TEST_F(StaticSpriteMetadataTest, MultiPieceGeometry) {
    StaticSpriteSpec spec{.sprite_name = "MULTI",
                          .frame_w = 100,
                          .frame_h = 80,
                          .pieces = {{0, 0, 40, 30}, {50, 10, 30, 50}}};
    auto             ff_path = temp_root_ / "Imgs" / "Testmulti.ff";
    write_static_sprite_ff(ff_path, spec);

    d2engine::FfAssetStore store(temp_root_);
    auto                   meta = store.sprite_metadata("Imgs/Testmulti.ff", "MULTI");
    EXPECT_EQ(meta.canvas_width, 100);
    EXPECT_EQ(meta.canvas_height, 80);
    // Piece 1: (0,0)-(40,30), Piece 2: (50,10)-(80,60)
    // min_x=0, max_x=80, min_y=0, max_y=60
    EXPECT_EQ(meta.canvas_foot_x, (0 + 80) / 2); // 40
    EXPECT_EQ(meta.canvas_foot_y, 60);           // max_y
    EXPECT_EQ(meta.canvas_top_y, 0);             // min_y
}

TEST_F(StaticSpriteMetadataTest, MissingSpriteFailsExplicitly) {
    d2engine::FfAssetStore store(temp_root_);
    EXPECT_THROW(
        { static_cast<void>(store.sprite_metadata("Imgs/Teststat.ff", "DOES_NOT_EXIST")); },
        std::runtime_error);
}

// ── Game-data-backed integration tests ─────────────────────────────────
static fs::path game_root() {
    const char* env = std::getenv("DISCIPLES2_GAME_ROOT"); // NOLINT
    return env != nullptr && env[0] != '\0' ? fs::path(env) : fs::path(DISCIPLES2_GAME_ROOT);
}
static bool has_game_data() {
    return !game_root().empty() && fs::is_directory(game_root());
}

class SelectVisualLoaderTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!has_game_data())
            GTEST_SKIP() << "No DISCIPLES2_GAME_ROOT";
        if (!SDL_Init(SDL_INIT_VIDEO))
            GTEST_SKIP() << "SDL_Init: " << SDL_GetError();
        window_ = SDL_CreateWindow("sel-test", 1, 1, SDL_WINDOW_HIDDEN);
        if (!window_) {
            SDL_Quit();
            GTEST_SKIP() << "window: " << SDL_GetError();
        }
        renderer_ = SDL_CreateRenderer(window_, nullptr);
        if (!renderer_) {
            SDL_DestroyWindow(window_);
            SDL_Quit();
            GTEST_SKIP() << "renderer: " << SDL_GetError();
        }
        assets_ = std::make_unique<d2engine::AssetRuntime>(game_root(), 1);
        render_assets_ = std::make_unique<d2engine::RenderAssetRuntime>(renderer_, *assets_);
        loader_ =
            std::make_unique<d2engine::AdventureVisualResourcesLoader>(*assets_, *render_assets_);
    }
    void TearDown() override {
        loader_.reset();
        render_assets_.reset();
        assets_.reset();
        if (renderer_)
            SDL_DestroyRenderer(renderer_);
        if (window_)
            SDL_DestroyWindow(window_);
        SDL_Quit();
    }
    // Destroy cursors between loads to prevent leaks
    static void destroy_cursors(d2engine::LoadedAdventureVisualResources& r) {
        if (r.cursors.default_cursor) {
            SDL_DestroyCursor(r.cursors.default_cursor);
            r.cursors.default_cursor = nullptr;
        }
        if (r.cursors.select_unit) {
            SDL_DestroyCursor(r.cursors.select_unit);
            r.cursors.select_unit = nullptr;
        }
    }
    SDL_Window*                                               window_ = nullptr;
    SDL_Renderer*                                             renderer_ = nullptr;
    std::unique_ptr<d2engine::AssetRuntime>                   assets_;
    std::unique_ptr<d2engine::RenderAssetRuntime>             render_assets_;
    std::unique_ptr<d2engine::AdventureVisualResourcesLoader> loader_;
};

TEST_F(SelectVisualLoaderTest, LoadResolvesAllFiveVisuals) {
    auto resources = loader_->load();
    EXPECT_NE(resources.cursors.default_cursor, nullptr);
    EXPECT_NE(resources.cursors.select_unit, nullptr);
    for (const auto* sel : {&resources.world.select_selected, &resources.world.select_no,
                            &resources.world.select_yes}) {
        EXPECT_EQ(sel->key.kind, d2engine::ImageAssetKind::ComposedSprite);
        EXPECT_GT(sel->src_width, 0);
        EXPECT_GT(sel->src_height, 0);
    }
    destroy_cursors(resources);
}

TEST_F(SelectVisualLoaderTest, SelectTexturesInCacheAfterLoad) {
    auto  resources = loader_->load();
    auto& cache = render_assets_->textures();
    for (const auto& key : {resources.world.select_selected.key, resources.world.select_no.key,
                            resources.world.select_yes.key}) {
        EXPECT_NE(cache.find(key), nullptr) << key.container_path << "/" << key.image_name;
    }
    destroy_cursors(resources);
}

TEST_F(SelectVisualLoaderTest, CacheHitAcceptedAsSuccess) {
    auto r1 = loader_->load();
    destroy_cursors(r1);

    auto  r2 = loader_->load();
    auto& cache = render_assets_->textures();
    for (const auto& key :
         {r2.world.select_selected.key, r2.world.select_no.key, r2.world.select_yes.key}) {
        EXPECT_NE(cache.find(key), nullptr);
    }
    destroy_cursors(r2);
}

TEST_F(SelectVisualLoaderTest, LoadReturnsSelectVisualsAsComposedSprite) {
    auto r = loader_->load();
    EXPECT_EQ(r.world.select_selected.key.kind, d2engine::ImageAssetKind::ComposedSprite);
    EXPECT_EQ(r.world.select_no.key.kind, d2engine::ImageAssetKind::ComposedSprite);
    EXPECT_EQ(r.world.select_yes.key.kind, d2engine::ImageAssetKind::ComposedSprite);
    destroy_cursors(r);
}
