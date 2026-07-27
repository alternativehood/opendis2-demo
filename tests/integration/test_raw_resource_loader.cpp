#include <gtest/gtest.h>
#include "d2engine/assets/asset_runtime.hpp"
#include "d2engine/assets/portrait_manifest.hpp"
#include "d2engine/assets/ff_asset_store.hpp"
#include "d2engine/assets/terrain_asset_catalog.hpp"
#include "d2engine/render/game_texture_cache.hpp"
#include "d2res/dbf_reader.hpp"
#include "d2res/mqdb.hpp"

#include <SDL3/SDL.h>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <map>
#include <optional>
#include <span>
#include <thread>
#include <vector>

#ifndef DISCIPLES2_GAME_ROOT
#define DISCIPLES2_GAME_ROOT ""
#endif

static const std::filesystem::path GAME_ROOT{[] {
    const char* env = std::getenv("DISCIPLES2_GAME_ROOT"); // NOLINT(concurrency-mt-unsafe)
    if (env && env[0] != '\0')
        return std::string{env};
    return std::string{DISCIPLES2_GAME_ROOT};
}()};
namespace fs = std::filesystem;

class FfAssetStoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!fs::is_directory(GAME_ROOT))
            GTEST_SKIP() << "Game root not found: " << GAME_ROOT;
        store_.emplace(GAME_ROOT);
    }
    std::optional<d2engine::FfAssetStore> store_;
};

TEST_F(FfAssetStoreTest, DecodeRawPng_KnownPortraitNonEmpty) {
    const auto buf = store_->raw_png("Imgs/Faces.ff", "G000UU0001FACE.PNG");
    ASSERT_NE(buf, nullptr);
    EXPECT_GT(buf->width, 0u);
    EXPECT_GT(buf->height, 0u);
    EXPECT_FALSE(buf->rgba.empty());
}

TEST_F(FfAssetStoreTest, DecodeRawPng_MissingRecordReturnsNull) {
    const auto buf = store_->raw_png("Imgs/Faces.ff", "DOES_NOT_EXIST.PNG");
    EXPECT_EQ(buf, nullptr);
}

TEST_F(FfAssetStoreTest, GrBorderRawContainerListsAndDecodesRecord) {
    const auto names = store_->record_names("Imgs/Grborder.ff");
    ASSERT_FALSE(names.empty());
    EXPECT_NE(std::ranges::find(names, "NE_01_00.PNG"), names.end());

    const auto png = store_->raw_png("Imgs/Grborder.ff", "NE_01_00.PNG");
    ASSERT_NE(png, nullptr);
    EXPECT_EQ(png->width, 64u);
    EXPECT_EQ(png->height, 32u);
}

TEST_F(FfAssetStoreTest, PrepareTexture_KnownSpriteHasRgbaAndTiming) {
    const auto names = store_->sprites_in("Imgs/Batunits.ff");
    if (names.empty())
        GTEST_SKIP() << "No sprites in Imgs/Batunits.ff";

    d2engine::AssetRuntime runtime{
        [&](const d2engine::ImageAssetKey& key) {
            auto pixels = std::make_shared<d2res::RgbaBuffer>(
                store_->decode_sprite(key.container_path, key.image_name));
            auto image = std::make_shared<d2engine::PreparedImage>(
                d2engine::PreparedImage{.key = key, .pixels = std::move(pixels)});
            return d2engine::PreparedImageResult{
                .key = key, .image = std::move(image), .success = true};
        },
        1};
    const auto result = runtime
                            .request_image({.container_path = "Imgs/Batunits.ff",
                                            .image_name = names.front(),
                                            .kind = d2engine::ImageAssetKind::ComposedSprite},
                                           d2engine::AssetPriority::Critical)
                            .get();

    ASSERT_TRUE(result.success) << result.error;
    EXPECT_EQ(result.key.container_path, "Imgs/Batunits.ff");
    ASSERT_NE(result.image, nullptr);
    ASSERT_NE(result.image->pixels, nullptr);
    EXPECT_GT(result.image->pixels->width, 0u);
    EXPECT_GT(result.image->pixels->height, 0u);
    EXPECT_FALSE(result.image->pixels->rgba.empty());
    EXPECT_GE(result.elapsed_ms, 0.0);
}

TEST_F(FfAssetStoreTest, PrepareTexture_MissingSpriteReturnsStructuredFailure) {
    d2engine::AssetRuntime runtime{
        [&](const d2engine::ImageAssetKey& key) {
            try {
                auto pixels = std::make_shared<d2res::RgbaBuffer>(
                    store_->decode_sprite(key.container_path, key.image_name));
                auto image = std::make_shared<d2engine::PreparedImage>(
                    d2engine::PreparedImage{.key = key, .pixels = std::move(pixels)});
                return d2engine::PreparedImageResult{
                    .key = key, .image = std::move(image), .success = true};
            } catch (const std::exception& e) {
                return d2engine::PreparedImageResult{
                    .key = key, .success = false, .error = e.what()};
            }
        },
        1};

    const auto result = runtime
                            .request_image({.container_path = "Imgs/Batunits.ff",
                                            .image_name = "DOES_NOT_EXIST.PNG",
                                            .kind = d2engine::ImageAssetKind::ComposedSprite},
                                           d2engine::AssetPriority::Critical)
                            .get();

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error.empty());
    EXPECT_EQ(result.key.container_path, "Imgs/Batunits.ff");
    EXPECT_EQ(result.key.image_name, "DOES_NOT_EXIST.PNG");
}

TEST_F(FfAssetStoreTest, FacesInventory_ListsPortraitRecords) {
    const auto names = store_->record_names("Imgs/Faces.ff");
    ASSERT_FALSE(names.empty());

    const bool has_known_face =
        std::ranges::any_of(names, [](const std::string& n) { return n == "G000UU0001FACE.PNG"; });
    EXPECT_TRUE(has_known_face) << "G000UU0001FACE.PNG not found in Faces.ff";
}

TEST_F(FfAssetStoreTest, DecodeRawPng_FacebRecord_NonEmpty) {
    const auto buf = store_->raw_png("Imgs/Faces.ff", "G000UU0001FACEB.PNG");
    if (buf == nullptr)
        GTEST_SKIP() << "FACEB record G000UU0001FACEB.PNG not found in game data";
    EXPECT_GT(buf->width, 0u);
    EXPECT_GT(buf->height, 0u);
    EXPECT_FALSE(buf->rgba.empty());
}

TEST_F(FfAssetStoreTest, BuildPortraitManifest_PairsKnownUnit) {
    const std::filesystem::path globals_dir = GAME_ROOT / "Globals";
    ASSERT_TRUE(fs::is_directory(globals_dir)) << globals_dir;

    // Load Gunits.dbf
    const auto gunits_path = globals_dir / "Gunits.dbf";
    ASSERT_TRUE(fs::exists(gunits_path)) << gunits_path;

    std::ifstream f(gunits_path, std::ios::binary);
    ASSERT_TRUE(f) << "Cannot open " << gunits_path;
    std::vector<uint8_t>   data((std::istreambuf_iterator<char>(f)),
                                std::istreambuf_iterator<char>());
    d2res::DbfReader const reader(std::span<const uint8_t>(data.data(), data.size()));
    const auto             gunits_rows = reader.read_records();
    ASSERT_FALSE(gunits_rows.empty()) << "Gunits.dbf has no rows";

    const auto manifest = d2engine::build_portrait_manifest(*store_, gunits_rows);
    EXPECT_EQ(manifest.schema_version, 1);
    EXPECT_EQ(manifest.container, "Imgs/Faces.ff");
    ASSERT_FALSE(manifest.units.empty()) << "Portrait manifest has no units";

    // Find the known unit
    const auto it = std::ranges::find_if(manifest.units,
                                         [](const auto& e) { return e.unit_id == "g000uu0001"; });
    ASSERT_NE(it, manifest.units.end()) << "g000uu0001 not found in portrait manifest";
    EXPECT_FALSE(it->face_record_name.empty());
}

class GetRawCacheTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!fs::is_directory(GAME_ROOT))
            GTEST_SKIP() << "Game root not found: " << GAME_ROOT;

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
        store_.emplace(GAME_ROOT);
        cache_ = std::make_unique<d2engine::GameTextureCache>(renderer_, *store_);
        const auto names = store_->sprites_in("Imgs/Batunits.ff");
        if (!names.empty()) {
            valid_sprite_ = names.front();
        }
    }

    void TearDown() override {
        cache_.reset();
        store_.reset();
        if (renderer_ != nullptr)
            SDL_DestroyRenderer(renderer_);
        if (window_ != nullptr)
            SDL_DestroyWindow(window_);
        SDL_Quit();
    }

    SDL_Window*                                 window_ = nullptr;
    SDL_Renderer*                               renderer_ = nullptr;
    std::optional<d2engine::FfAssetStore>       store_;
    std::unique_ptr<d2engine::GameTextureCache> cache_;
    std::string                                 valid_sprite_;
};

TEST_F(GetRawCacheTest, GetRaw_CacheHitReturnsSamePointer) {
    SDL_Texture const* first = cache_->get_raw("Imgs/Faces.ff", "G000UU0001FACE.PNG");
    SDL_Texture const* second = cache_->get_raw("Imgs/Faces.ff", "G000UU0001FACE.PNG");
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first, second);
}

TEST_F(GetRawCacheTest, GetRaw_FacebRecord_Loads) {
    SDL_Texture const* texture = cache_->get_raw("Imgs/Faces.ff", "G000UU0001FACEB.PNG");
    if (texture == nullptr)
        GTEST_SKIP() << "FACEB record G000UU0001FACEB.PNG not found in game data";
    ASSERT_NE(texture, nullptr);

    SDL_Texture const* second = cache_->get_raw("Imgs/Faces.ff", "G000UU0001FACEB.PNG");
    EXPECT_EQ(texture, second);
}

TEST_F(GetRawCacheTest, GetFillsCacheAndSecondGetHits) {
    if (valid_sprite_.empty())
        GTEST_SKIP() << "No sprites in Imgs/Batunits.ff";

    SDL_Texture const* first = cache_->get("Imgs/Batunits.ff", valid_sprite_);
    ASSERT_NE(first, nullptr);

    SDL_Texture const* second = cache_->get("Imgs/Batunits.ff", valid_sprite_);
    EXPECT_EQ(first, second);

    const auto& stats = cache_->stats();
    EXPECT_EQ(stats.cache_misses, 1u);
    EXPECT_EQ(stats.lazy_render_misses, 1u);
    EXPECT_EQ(stats.cache_hits, 1u);
}

TEST_F(GetRawCacheTest, UploadPreparedFillsCacheAndGetHits) {
    d2engine::PreparedTextureFrame const frame{.container_path = "Imgs/Batunits.ff",
                                               .image_name = "PREPARED_TEST.PNG",
                                               .rgba = {255, 0, 255, 255},
                                               .width = 1,
                                               .height = 1};

    const auto uploaded = cache_->upload_prepared(frame);
    ASSERT_TRUE(uploaded.success) << uploaded.error;
    EXPECT_FALSE(uploaded.skipped_cache_hit);

    SDL_Texture const* texture = cache_->get("Imgs/Batunits.ff", "PREPARED_TEST.PNG");
    EXPECT_NE(texture, nullptr);

    const auto& stats = cache_->stats();
    EXPECT_EQ(stats.prepared_upload_requests, 1u);
    EXPECT_EQ(stats.prepared_upload_successes, 1u);
    EXPECT_EQ(stats.lazy_render_misses, 0u);
}

TEST_F(GetRawCacheTest, UploadPreparedDuplicateIsSkipped) {
    d2engine::PreparedTextureFrame const frame{.container_path = "Imgs/Batunits.ff",
                                               .image_name = "PREPARED_TEST.PNG",
                                               .rgba = {255, 0, 255, 255},
                                               .width = 1,
                                               .height = 1};

    ASSERT_TRUE(cache_->upload_prepared(frame).success);
    const auto duplicate = cache_->upload_prepared(frame);

    EXPECT_TRUE(duplicate.success);
    EXPECT_TRUE(duplicate.skipped_cache_hit);
    EXPECT_EQ(cache_->stats().prepared_upload_skipped_cache_hits, 1u);
}

TEST_F(GetRawCacheTest, GetMissIsCountedAsLazyRenderMiss) {
    if (valid_sprite_.empty())
        GTEST_SKIP() << "No sprites in Imgs/Batunits.ff";
    SDL_Texture const* texture = cache_->get("Imgs/Batunits.ff", valid_sprite_);
    ASSERT_NE(texture, nullptr);

    const auto& stats = cache_->stats();
    EXPECT_EQ(stats.cache_misses, 1u);
    EXPECT_EQ(stats.lazy_render_misses, 1u);
}

TEST_F(GetRawCacheTest, GetMissingSpriteThrowsStructuredError) {
    EXPECT_THROW((void)cache_->get("Imgs/Batunits.ff", "DOES_NOT_EXIST.PNG"), std::runtime_error);

    EXPECT_EQ(cache_->stats().cache_misses, 1u);
    EXPECT_EQ(cache_->stats().lazy_render_misses, 1u);
}

// ── Synthetic case-insensitive FF discovery regression tests ──────────────────

namespace {

[[nodiscard]] std::string ascii_lower(std::string_view src) {
    std::string out;
    out.reserve(src.size());
    for (char ch : src) {
        const auto uc = static_cast<unsigned char>(ch);
        out.push_back(static_cast<char>(uc >= 'A' && uc <= 'Z' ? uc + 0x20 : uc));
    }
    return out;
}

[[nodiscard]] std::string ascii_upper(std::string_view src) {
    std::string out;
    out.reserve(src.size());
    for (char ch : src) {
        const auto uc = static_cast<unsigned char>(ch);
        out.push_back(static_cast<char>(uc >= 'a' && uc <= 'z' ? uc - 0x20 : uc));
    }
    return out;
}

[[nodiscard]] fs::path find_any_ff_file(const fs::path& dir) {
    if (!fs::is_directory(dir))
        return {};
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (entry.is_regular_file()) {
            const auto ext = ascii_lower(entry.path().extension().string());
            if (ext == ".ff")
                return entry.path();
        }
    }
    return {};
}

} // namespace

class FfCaseDiscoveryTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!fs::is_directory(GAME_ROOT))
            GTEST_SKIP() << "Game root not found: " << GAME_ROOT;
        source_ff_ = find_any_ff_file(GAME_ROOT / "Imgs");
        if (source_ff_.empty())
            GTEST_SKIP() << "No .ff file found in Imgs/ to copy";
    }
    void TearDown() override {
        if (!tmp_root_.empty() && fs::exists(tmp_root_))
            fs::remove_all(tmp_root_);
    }
    fs::path source_ff_;
    fs::path tmp_root_;
};

TEST_F(FfCaseDiscoveryTest, MixedCaseAssetDirectory) {
    tmp_root_ = fs::temp_directory_path() / "opendis2_case_discovery_mixed";
    const auto asset_dir = tmp_root_ / "IMGS";
    fs::create_directories(asset_dir);
    fs::copy_file(source_ff_, asset_dir / source_ff_.filename(),
                  fs::copy_options::overwrite_existing);

    d2engine::FfAssetStore store(tmp_root_);
    const auto             containers = store.containers();

    const std::string expected_key = "imgs/" + ascii_lower(source_ff_.filename().string());
    EXPECT_TRUE(std::ranges::find(containers, expected_key) != containers.end())
        << "Expected logical key: " << expected_key;
}

TEST_F(FfCaseDiscoveryTest, UppercaseExtension) {
    tmp_root_ = fs::temp_directory_path() / "opendis2_case_discovery_ext";
    const auto asset_dir = tmp_root_ / "Imgs";
    fs::create_directories(asset_dir);

    const auto dst_name = source_ff_.filename().stem().string() + ".FF";
    fs::copy_file(source_ff_, asset_dir / dst_name, fs::copy_options::overwrite_existing);

    d2engine::FfAssetStore store(tmp_root_);
    const auto             containers = store.containers();

    const std::string expected_key = "imgs/" + ascii_lower(source_ff_.stem().string()) + ".ff";
    EXPECT_TRUE(std::ranges::find(containers, expected_key) != containers.end())
        << "Expected logical key: " << expected_key;
}

TEST_F(FfCaseDiscoveryTest, MixedCaseFile) {
    tmp_root_ = fs::temp_directory_path() / "opendis2_case_discovery_file";
    const auto asset_dir = tmp_root_ / "iMgS";
    fs::create_directories(asset_dir);

    const std::string mixed_name = "gRbOrDeR.Ff";
    fs::copy_file(source_ff_, asset_dir / mixed_name, fs::copy_options::overwrite_existing);

    d2engine::FfAssetStore store(tmp_root_);
    const auto             containers = store.containers();

    const std::string expected_key = "imgs/grborder.ff";
    EXPECT_TRUE(std::ranges::find(containers, expected_key) != containers.end())
        << "Expected logical key: " << expected_key;
}

TEST_F(FfCaseDiscoveryTest, CaseInsensitiveLogicalLookup) {
    tmp_root_ = fs::temp_directory_path() / "opendis2_case_lookup";
    const auto asset_dir = tmp_root_ / "Imgs";
    fs::create_directories(asset_dir);
    fs::copy_file(source_ff_, asset_dir / source_ff_.filename(),
                  fs::copy_options::overwrite_existing);

    d2engine::FfAssetStore store(tmp_root_);

    // All these logical requests must resolve to the same underlying container
    const std::string expected_key = "imgs/" + ascii_lower(source_ff_.filename().string());

    EXPECT_NO_THROW({ (void)store.record_names(expected_key); });
    EXPECT_NO_THROW(
        { (void)store.record_names("IMGS/" + ascii_upper(source_ff_.filename().string())); });
    EXPECT_NO_THROW({ (void)store.record_names("Imgs/" + source_ff_.filename().string()); });
}

TEST_F(FfCaseDiscoveryTest, PhysicalPathPreservesCase) {
    tmp_root_ = fs::temp_directory_path() / "opendis2_case_physical";
    const auto asset_dir = tmp_root_ / "iMgS";
    fs::create_directories(asset_dir);

    const std::string mixed_name = "gRbOrDeR.Ff";
    const auto        dst = asset_dir / mixed_name;
    fs::copy_file(source_ff_, dst, fs::copy_options::overwrite_existing);

    d2engine::FfAssetStore store(tmp_root_);

    // record_names forces lazy-open; if the physical path was reconstructed from
    // lowercase logical ID, the open would fail on case-sensitive filesystems.
    const auto names = store.record_names("imgs/grborder.ff");
    EXPECT_FALSE(names.empty()) << "Container open failed — physical path likely "
                                   "reconstructed from lowercase logical ID";
}

TEST_F(FfCaseDiscoveryTest, CaseCollisionDetected) {
    tmp_root_ = fs::temp_directory_path() / "opendis2_case_collision";
    const auto asset_dir = tmp_root_ / "Imgs";
    fs::create_directories(asset_dir);

    const auto lower_name = ascii_lower(source_ff_.filename().string());
    const auto upper_name = source_ff_.stem().string() + ".FF";

    fs::copy_file(source_ff_, asset_dir / lower_name, fs::copy_options::overwrite_existing);

    // On case-sensitive filesystems we can create a second file with different casing.
    // On case-insensitive filesystems the second copy will overwrite the first.
    // Detect filesystem capability by comparing the paths.
    bool case_sensitive_fs = true;
    try {
        fs::copy_file(source_ff_, asset_dir / upper_name, fs::copy_options::overwrite_existing);
        case_sensitive_fs = !fs::equivalent(asset_dir / lower_name, asset_dir / upper_name);
    } catch (...) {
        case_sensitive_fs = false;
    }

    if (!case_sensitive_fs) {
        GTEST_SKIP() << "Filesystem is case-insensitive; collision test not applicable";
    }

    // With both files present, FfAssetStore should reject the duplicate logical identity.
    try {
        d2engine::FfAssetStore store(tmp_root_);
        FAIL() << "Expected duplicate container identity error";
    } catch (const std::runtime_error& e) {
        const std::string msg = e.what();
        EXPECT_TRUE(msg.find("Duplicate") != std::string::npos ||
                    msg.find("duplicate") != std::string::npos)
            << "Error message: " << msg;
    }
}

// ── First-access deadlock regression tests ──────────────────────────────────

TEST_F(FfAssetStoreTest, FirstRecordNamesAccessCompletes) {
    auto future = std::async(std::launch::async,
                             [this]() { return store_->record_names("Imgs/Grborder.ff"); });
    ASSERT_EQ(future.wait_for(std::chrono::seconds(5)), std::future_status::ready)
        << "record_names first access timed out (deadlock?)";
    auto names = future.get();
    EXPECT_FALSE(names.empty());
}

TEST_F(FfAssetStoreTest, FirstContainsRecordAccessCompletes) {
    auto future = std::async(std::launch::async, [this]() {
        return store_->contains_record("Imgs/Grborder.ff", "NE_01_00.PNG");
    });
    ASSERT_EQ(future.wait_for(std::chrono::seconds(5)), std::future_status::ready)
        << "contains_record first access timed out (deadlock?)";
    EXPECT_TRUE(future.get());
}

TEST_F(FfAssetStoreTest, FirstRawRecordAccessCompletes) {
    auto future = std::async(std::launch::async, [this]() {
        return store_->raw_record("Imgs/Grborder.ff", "NE_01_00.PNG");
    });
    ASSERT_EQ(future.wait_for(std::chrono::seconds(5)), std::future_status::ready)
        << "raw_record first access timed out (deadlock?)";
    EXPECT_NE(future.get(), nullptr);
}

TEST_F(FfAssetStoreTest, FirstRawPngAccessCompletes) {
    auto future = std::async(std::launch::async, [this]() {
        return store_->raw_png("Imgs/Grborder.ff", "NE_01_00.PNG");
    });
    ASSERT_EQ(future.wait_for(std::chrono::seconds(5)), std::future_status::ready)
        << "raw_png first access timed out (deadlock?)";
    EXPECT_NE(future.get(), nullptr);
}

TEST_F(FfAssetStoreTest, FirstRawPngDimensionsAccessCompletes) {
    auto future = std::async(std::launch::async, [this]() {
        return store_->raw_png("Imgs/Grborder.ff", "NE_01_00.PNG");
    });
    ASSERT_EQ(future.wait_for(std::chrono::seconds(5)), std::future_status::ready)
        << "raw_png first access timed out (deadlock?)";
    const auto png = future.get();
    ASSERT_NE(png, nullptr);
    EXPECT_EQ(png->width, 64u);
    EXPECT_EQ(png->height, 32u);
}

TEST_F(FfAssetStoreTest, FirstPrewarmCompletes) {
    auto future = std::async(std::launch::async, [this]() { store_->prewarm("Imgs/Grborder.ff"); });
    ASSERT_EQ(future.wait_for(std::chrono::seconds(5)), std::future_status::ready)
        << "prewarm first access timed out (deadlock?)";
    future.get();
}

TEST_F(FfAssetStoreTest, ConcurrentFirstAccessInitializesOnce) {
    std::vector<std::future<void>> futures;
    futures.reserve(4);
    for (int i = 0; i < 4; ++i) {
        futures.push_back(std::async(std::launch::async, [this, i]() {
            if (i % 3 == 0) {
                auto names = store_->record_names("Imgs/Grborder.ff");
                EXPECT_FALSE(names.empty());
            } else if (i % 3 == 1) {
                auto png = store_->raw_png("Imgs/Grborder.ff", "NE_01_00.PNG");
                ASSERT_NE(png, nullptr);
                EXPECT_EQ(png->width, 64u);
                EXPECT_EQ(png->height, 32u);
            } else {
                EXPECT_TRUE(store_->contains_record("Imgs/Grborder.ff", "NE_01_00.PNG"));
            }
        }));
    }
    for (auto& f : futures) {
        ASSERT_EQ(f.wait_for(std::chrono::seconds(10)), std::future_status::ready)
            << "Concurrent first access timed out (deadlock?)";
        f.get();
    }
}

TEST_F(FfAssetStoreTest, ConcurrentAccessReportVsInitialization) {
    std::atomic<bool>              go{false};
    std::vector<std::future<void>> futures;

    futures.push_back(std::async(std::launch::async, [this, &go]() {
        while (!go.load()) {
            std::this_thread::yield();
        }
        auto names = store_->record_names("Imgs/Grborder.ff");
        EXPECT_FALSE(names.empty());
    }));

    futures.push_back(std::async(std::launch::async, [this, &go]() {
        while (!go.load()) {
            std::this_thread::yield();
        }
        auto report = store_->access_report();
        // Report may or may not include the container depending on race outcome;
        // the invariant is that it must not crash or deadlock.
        (void)report;
    }));

    go.store(true);
    for (auto& f : futures) {
        ASSERT_EQ(f.wait_for(std::chrono::seconds(10)), std::future_status::ready)
            << "Concurrent access_report vs initialization timed out";
        f.get();
    }
}

// ── Terrain catalog scope tests ───────────────────────────────────────────────

TEST_F(FfAssetStoreTest, BuildGroundBorderCatalogTouchesOnlyRequiredContainers) {
    d2engine::TerrainAssetCatalogBuilder builder;
    auto                                 catalog = builder.build_ground_border(*store_);

    EXPECT_FALSE(catalog.ground_textures.empty());
    EXPECT_FALSE(catalog.border_assets.empty());
    EXPECT_TRUE(catalog.terrain_overlays.empty());
    EXPECT_TRUE(catalog.static_assets.empty());

    auto                  report = store_->access_report();
    std::set<std::string> accessed;
    for (const auto& c : report.containers) {
        accessed.insert(c.container_path);
    }

    EXPECT_TRUE(accessed.contains("imgs/ground.ff"));
    EXPECT_TRUE(accessed.contains("imgs/grborder.ff"));
    EXPECT_FALSE(accessed.contains("imgs/isoterrn.ff"))
        << "IsoTerrn.ff should not be accessed for ground+border catalog";
    EXPECT_FALSE(accessed.contains("imgs/isostill.ff"))
        << "IsoStill.ff should not be accessed for ground+border catalog";
    EXPECT_FALSE(accessed.contains("imgs/isocmon.ff"))
        << "IsoCmon.ff should not be accessed for ground+border catalog";
}

TEST_F(FfAssetStoreTest, AnimationMetadataMatchesDecodeAnimation) {
    const std::vector<std::string> containers = {"Imgs/Batunits.ff", "Imgs/Capital.ff",
                                                 "Imgs/IsoTerrn.ff"};
    for (const auto& container : containers) {
        const auto anims = store_->animations_in(container);
        if (!anims.empty()) {
            const auto meta = store_->animation_metadata(container, anims.front());
            const auto full = store_->decode_animation(container, anims.front());

            EXPECT_EQ(meta.name, full.name);
            EXPECT_EQ(meta.frames.size(), full.frames.size());
            EXPECT_EQ(meta.native_canvas_w, full.native_canvas_w);
            EXPECT_EQ(meta.native_canvas_h, full.native_canvas_h);
            EXPECT_EQ(meta.container_path, full.container_path);
            return;
        }
    }
    GTEST_SKIP() << "No animations found in any tested container";
}

// ── Access dump behavior tests ────────────────────────────────────────────────

TEST_F(FfAssetStoreTest, FullDumpContainsEveryInventoryRecord) {
    // Access one known record to create non-zero hits
    (void)store_->raw_png("Imgs/Grborder.ff", "NE_01_00.PNG");

    const auto report = store_->access_report();
    auto       grborder = std::ranges::find_if(
        report.containers, [](const auto& c) { return c.container_path == "imgs/grborder.ff"; });
    ASSERT_NE(grborder, report.containers.end());

    // Every inventory record must appear, including unused ones
    EXPECT_FALSE(grborder->records.empty());
    auto ne01 = std::ranges::find_if(grborder->records,
                                     [](const auto& r) { return r.record_name == "NE_01_00.PNG"; });
    ASSERT_NE(ne01, grborder->records.end());
    EXPECT_EQ(ne01->hits, 1u);
    EXPECT_EQ(ne01->loads, 1u);

    // At least one unused record must have hits=0
    const auto zero_hits =
        std::ranges::count_if(grborder->records, [](const auto& r) { return r.hits == 0; });
    EXPECT_GT(zero_hits, 0);
}

TEST_F(FfAssetStoreTest, FullDumpDeterministicOrder) {
    const auto report = store_->access_report();
    for (const auto& container : report.containers) {
        EXPECT_TRUE(std::ranges::is_sorted(container.records, {},
                                           &d2engine::FfRecordAccessReport::record_name));
        EXPECT_TRUE(std::ranges::is_sorted(container.missing_requests, {},
                                           &d2engine::FfRecordAccessReport::record_name));
    }
}

TEST_F(FfAssetStoreTest, DumpAccessReportIdempotent) {
    // Calling dump multiple times must not crash or throw
    store_->dump_access_report(d2engine::FfAssetStore::FfAccessDumpMode::Summary);
    store_->dump_access_report(d2engine::FfAssetStore::FfAccessDumpMode::Summary);
    store_->dump_access_report(d2engine::FfAssetStore::FfAccessDumpMode::Full);
    store_->dump_access_report(d2engine::FfAssetStore::FfAccessDumpMode::Full);
    SUCCEED();
}

TEST_F(FfAssetStoreTest, FullDumpIncludesUnusedNeWa) {
    // Access exactly one NE and one WA record
    (void)store_->raw_png("Imgs/Grborder.ff", "NE_01_00.PNG");
    (void)store_->raw_png("Imgs/Grborder.ff", "WA_01_00.PNG");

    const auto report = store_->access_report();
    auto       grborder = std::ranges::find_if(
        report.containers, [](const auto& c) { return c.container_path == "imgs/grborder.ff"; });
    ASSERT_NE(grborder, report.containers.end());

    auto ne_used = std::ranges::find_if(
        grborder->records, [](const auto& r) { return r.record_name == "NE_01_00.PNG"; });
    auto ne_unused = std::ranges::find_if(
        grborder->records, [](const auto& r) { return r.record_name == "NE_01_01.PNG"; });
    auto wa_used = std::ranges::find_if(
        grborder->records, [](const auto& r) { return r.record_name == "WA_01_00.PNG"; });
    auto wa_unused = std::ranges::find_if(
        grborder->records, [](const auto& r) { return r.record_name == "WA_01_01.PNG"; });

    ASSERT_NE(ne_used, grborder->records.end());
    ASSERT_NE(ne_unused, grborder->records.end());
    ASSERT_NE(wa_used, grborder->records.end());
    ASSERT_NE(wa_unused, grborder->records.end());

    EXPECT_EQ(ne_used->hits, 1u);
    EXPECT_EQ(ne_unused->hits, 0u);
    EXPECT_EQ(wa_used->hits, 1u);
    EXPECT_EQ(wa_unused->hits, 0u);
}

TEST_F(FfAssetStoreTest, SummaryDumpOnlySummary) {
    // Summary dump should not crash and should produce a valid access_report
    store_->dump_access_report(d2engine::FfAssetStore::FfAccessDumpMode::Summary);
    const auto report = store_->access_report();
    // Report may be empty if no container was ever accessed; that's fine.
    // The invariant is that the call succeeds and is idempotent.
    (void)report;
    SUCCEED();
}

TEST_F(FfAssetStoreTest, StackInfoPopupResolvesThroughComposedSprite) {
    const auto sprites = store_->sprites_in("Interf/Interf.ff");
    const auto it = std::ranges::find(sprites, std::string{"_PG0500IX"});
    if (it == sprites.end())
        GTEST_SKIP() << "Popup sprite _PG0500IX not found in Interf.ff";

    const auto resolved = store_->resolve_sprite_fast("Interf/Interf.ff", "_PG0500IX");
    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(resolved->logical_name, "_PG0500IX");
    EXPECT_FALSE(resolved->physical_record_name.empty());
    EXPECT_FALSE(resolved->is_raw_png)
        << "_PG0500IX must resolve through OPT metadata, not as a raw PNG";

    if (resolved->frame != nullptr) {
        EXPECT_GT(resolved->frame->output_width, 0);
        EXPECT_GT(resolved->frame->output_height, 0);
    }

    const auto decoded = store_->decode_sprite("Interf/Interf.ff", "_PG0500IX");
    EXPECT_GT(decoded.width, 0u);
    EXPECT_GT(decoded.height, 0u);
    EXPECT_FALSE(decoded.rgba.empty());

    if (resolved->frame != nullptr) {
        EXPECT_LE(decoded.width, resolved->frame->output_width);
        EXPECT_LE(decoded.height, resolved->frame->output_height);
    }
}
