#include "game_texture_cache.hpp"
#include "../../d2res/rgba_buffer.hpp"

#include <d2log/log.hpp>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace d2engine {
namespace {

auto kLog = d2log::get("d2.texture"); // NOLINT(cert-err58-cpp)

} // namespace

GameTextureCache::GameTextureCache(SDL_Renderer* renderer) : renderer_(renderer) {}

GameTextureCache::GameTextureCache(SDL_Renderer* renderer, const FfAssetStore& store)
    : renderer_(renderer), store_(&store) {}

GameTextureCache::~GameTextureCache() {
    const auto        started = std::chrono::steady_clock::now();
    const std::size_t tex_count = cache_.size();
    cache_.clear();
    const double ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started)
            .count();
    kLog->debug("shutdown_timing stage=game_texture_cache_clear textures={} duration_ms={:.1f}",
                tex_count, ms);
}

SDL_Texture* GameTextureCache::get(const std::string& container_path,
                                   const std::string& sprite_name) {
    const std::string key = make_key(container_path, sprite_name);
    render_requested_keys_.insert(key);
    const auto        typed_key = composed_sprite_key(container_path, sprite_name);
    const std::string typed_cache_key = make_key(typed_key);

    {
        const auto it = cache_.find(key);
        if (it != cache_.end()) {
            ++stats_.cache_hits;
            return it->second.texture.get();
        }
        const auto typed_it = cache_.find(typed_cache_key);
        if (typed_it != cache_.end()) {
            ++stats_.cache_hits;
            return typed_it->second.texture.get();
        }
    }

    // In strict mode: any cache miss is a hard diagnostic failure — no lazy decode.
    if (strict_planned_assets_) {
        ++stats_.strict_planned_asset_misses;
        strict_miss_keys_.push_back(key);
        record_lazy_render_miss(container_path, sprite_name, 0.0);
        kLog->error("strict_miss key={}", key);
        return nullptr;
    }

    // Planned but not yet uploaded: return null without decode to avoid blocking render path
    if (planned_.contains(key) || planned_.contains(typed_cache_key)) {
        ++stats_.planned_asset_misses;
        record_lazy_render_miss(container_path, sprite_name, 0.0);
        kLog->debug("planned_not_ready key={}", key);
        return nullptr;
    }

    LoadResult const loaded = load(container_path, sprite_name, LoadSource::LazyRender);
    if ((loaded.texture == nullptr) && !loaded.error.empty()) {
        throw std::runtime_error(loaded.error);
    }
    return loaded.texture;
}

void GameTextureCache::mark_planned(const std::string& container_path,
                                    const std::string& image_name) {
    planned_.insert(make_key(container_path, image_name));
}

void GameTextureCache::mark_planned(const ImageAssetKey& key) {
    planned_.insert(make_key(key));
}

GameTextureCache::PreparedTextureUploadResult
GameTextureCache::upload_prepared(const PreparedTextureFrame& frame) {
    const auto started = std::chrono::steady_clock::now();
    ++stats_.prepared_upload_requests;

    PreparedTextureUploadResult result{.container_path = frame.container_path,
                                       .image_name = frame.image_name};
    const std::string           key = make_key(frame.container_path, frame.image_name);
    if (cache_.contains(key)) {
        ++stats_.prepared_upload_skipped_cache_hits;
        result.success = true;
        result.skipped_cache_hit = true;
        return result;
    }
    if (frame.width == 0 || frame.height == 0 || frame.rgba.empty()) {
        ++stats_.prepared_upload_failures;
        result.error = "prepared texture frame is empty";
        return result;
    }

    const int  pitch = static_cast<int>(frame.width) * 4;
    SdlTexture texture =
        create_sdl_texture(renderer_, static_cast<int>(frame.width), static_cast<int>(frame.height),
                           frame.rgba.data(), pitch);
    result.elapsed_ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started)
            .count();
    stats_.total_prepared_upload_ms += result.elapsed_ms;
    if (!texture) {
        ++stats_.prepared_upload_failures;
        result.error = "failed to create SDL texture for prepared frame: " + key;
        return result;
    }

    cache_[key] = TextureEntry{.texture = std::move(texture),
                               .width = static_cast<int>(frame.width),
                               .height = static_cast<int>(frame.height)};
    ++stats_.prepared_upload_successes;
    result.success = true;
    record_slow_texture(frame.container_path, frame.image_name, LoadSource::PreparedUpload,
                        result.elapsed_ms);
    return result;
}

GameTextureCache::PreparedTextureUploadResult
GameTextureCache::upload_prepared(const PreparedImage& image) {
    const auto started = std::chrono::steady_clock::now();
    ++stats_.prepared_upload_requests;

    PreparedTextureUploadResult result{.container_path = image.key.container_path,
                                       .image_name = image.key.image_name};
    const std::string           key = make_key(image.key);
    if (cache_.contains(key)) {
        ++stats_.prepared_upload_skipped_cache_hits;
        result.success = true;
        result.skipped_cache_hit = true;
        return result;
    }
    if (image.pixels == nullptr || image.pixels->width == 0 || image.pixels->height == 0 ||
        image.pixels->rgba.empty()) {
        ++stats_.prepared_upload_failures;
        result.error = "prepared image is empty";
        return result;
    }

    const int  pitch = static_cast<int>(image.pixels->width) * 4;
    SdlTexture texture = create_sdl_texture(renderer_, static_cast<int>(image.pixels->width),
                                            static_cast<int>(image.pixels->height),
                                            image.pixels->rgba.data(), pitch);
    result.elapsed_ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started)
            .count();
    stats_.total_prepared_upload_ms += result.elapsed_ms;
    if (!texture) {
        ++stats_.prepared_upload_failures;
        result.error = "failed to create SDL texture for prepared image: " + key;
        return result;
    }

    cache_[key] = TextureEntry{.texture = std::move(texture),
                               .width = static_cast<int>(image.pixels->width),
                               .height = static_cast<int>(image.pixels->height)};
    ++stats_.prepared_upload_successes;
    result.success = true;
    record_slow_texture(image.key.container_path, image.key.image_name, LoadSource::PreparedUpload,
                        result.elapsed_ms);
    return result;
}

GameTextureCache::LoadResult GameTextureCache::load(const std::string& container_path,
                                                    const std::string& sprite_name,
                                                    LoadSource         source) {
    const auto        started = std::chrono::steady_clock::now();
    const std::string key = make_key(container_path, sprite_name);
    const auto        it = cache_.find(key);
    if (it != cache_.end()) {
        ++stats_.cache_hits;
        return {.texture = it->second.texture.get(), .cache_hit = true};
    }
    ++stats_.cache_misses;
    if (source == LoadSource::LazyRender) {
        ++stats_.lazy_render_misses;
    }

    // Decode and create texture
    d2res::RgbaBuffer decoded;
    try {
        if (store_ == nullptr) {
            return {.error = "texture cache has no source asset store"};
        }
        decoded = store_->decode_sprite(container_path, sprite_name);
    } catch (const std::exception& e) {
        std::ostringstream msg;
        msg << "Failed to load texture for '" << key << "': " << e.what();
        return {.error = msg.str(),
                .elapsed_ms = std::chrono::duration<double, std::milli>(
                                  std::chrono::steady_clock::now() - started)
                                  .count()};
    }

    if (decoded.width == 0 || decoded.height == 0) {
        std::ostringstream msg;
        msg << "Decoded empty texture for '" << key << "'";
        return {.error = msg.str(),
                .elapsed_ms = std::chrono::duration<double, std::milli>(
                                  std::chrono::steady_clock::now() - started)
                                  .count()};
    }

    // Safety net: run border heuristic on all decoded sprites to catch
    // assets with missing/wrong opacity metadata.
    if (d2res::detect_magenta_key_border(decoded)) {
        d2res::apply_magenta_key_to_rgba(decoded);
    }

    const int  pitch = static_cast<int>(decoded.width) * 4;
    SdlTexture texture =
        create_sdl_texture(renderer_, static_cast<int>(decoded.width),
                           static_cast<int>(decoded.height), decoded.rgba.data(), pitch);

    if (!texture) {
        std::ostringstream msg;
        msg << "Failed to create SDL texture for '" << key << "'";
        return {.error = msg.str(),
                .elapsed_ms = std::chrono::duration<double, std::milli>(
                                  std::chrono::steady_clock::now() - started)
                                  .count()};
    }

    SDL_Texture* raw_ptr = texture.get();
    cache_[key] = TextureEntry{.texture = std::move(texture),
                               .width = static_cast<int>(decoded.width),
                               .height = static_cast<int>(decoded.height)};
    const double elapsed_ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started)
            .count();
    record_slow_texture(container_path, sprite_name, source, elapsed_ms);
    if (source == LoadSource::LazyRender) {
        record_lazy_render_miss(container_path, sprite_name, elapsed_ms);
        kLog->debug("lazy_render_miss key={} elapsed_ms={}", key, elapsed_ms);
    }

    // cppcheck-suppress returnDanglingLifetime ; texture is moved into cache, raw_ptr remains valid
    return {.texture = raw_ptr, .elapsed_ms = elapsed_ms};
}

SDL_Texture* GameTextureCache::get_raw(const std::string& container_path,
                                       const std::string& record_name, bool apply_magenta_key) {
    const ImageAssetKey asset_key{.container_path = container_path,
                                  .image_name = record_name,
                                  .kind = ImageAssetKind::RawPng,
                                  .postprocess = apply_magenta_key ? ImagePostprocess::MagentaKey
                                                                   : ImagePostprocess::None};
    const std::string   key = make_key(asset_key);
    const auto          it = cache_.find(key);
    if (it != cache_.end()) {
        return it->second.texture.get();
    }

    // Negative cache — skip if record was already confirmed missing
    if (failed_.contains(key)) {
        return nullptr;
    }

    if (store_ == nullptr) {
        failed_.insert(key);
        kLog->warn("get_raw_no_store key={}", key);
        return nullptr;
    }

    auto pixels = store_->raw_png(container_path, record_name);
    if (pixels == nullptr) {
        failed_.insert(key);
        kLog->warn("get_raw_missing key={}", key);
        return nullptr;
    }

    d2res::RgbaBuffer        copied;
    const d2res::RgbaBuffer* upload_pixels = pixels.get();
    if (apply_magenta_key) {
        copied = *pixels;
        d2res::apply_magenta_key_to_rgba(copied);
        upload_pixels = &copied;
    }

    SdlTexture texture = create_sdl_texture(
        renderer_, static_cast<int>(upload_pixels->width), static_cast<int>(upload_pixels->height),
        upload_pixels->rgba.data(), static_cast<int>(upload_pixels->width) * 4);
    if (!texture) {
        failed_.insert(key);
        return nullptr;
    }

    SDL_Texture* raw_ptr = texture.get();
    cache_[key] = TextureEntry{.texture = std::move(texture),
                               .width = static_cast<int>(upload_pixels->width),
                               .height = static_cast<int>(upload_pixels->height)};

    // cppcheck-suppress returnDanglingLifetime ; texture moved into cache, raw_ptr stays valid
    return raw_ptr;
}

bool GameTextureCache::is_cached(const std::string& container_path,
                                 const std::string& sprite_name) const {
    return cache_.contains(make_key(container_path, sprite_name)) ||
           cache_.contains(make_key(composed_sprite_key(container_path, sprite_name)));
}

bool GameTextureCache::is_cached(const ImageAssetKey& key) const {
    return cache_.contains(make_key(key));
}

SDL_Texture* GameTextureCache::find(const ImageAssetKey& key) const {
    const auto it = cache_.find(make_key(key));
    return it == cache_.end() ? nullptr : it->second.texture.get();
}

std::vector<std::string> GameTextureCache::all_keys() const {
    std::vector<std::string> result;
    result.reserve(cache_.size());
    for (const auto& [key, _] : cache_) {
        result.push_back(key);
    }
    return result;
}

void GameTextureCache::clear() {
    cache_.clear();
    planned_.clear();
    failed_.clear();
    stats_ = {};
    slow_textures_.clear();
    lazy_render_misses_.clear();
    lazy_render_context_.reset();
}

std::string GameTextureCache::make_key(const std::string& container_path,
                                       const std::string& sprite_name) {
    return container_path + "/" + sprite_name;
}

ImageAssetKey GameTextureCache::composed_sprite_key(const std::string& container_path,
                                                    const std::string& sprite_name) {
    return {.container_path = container_path,
            .image_name = sprite_name,
            .kind = ImageAssetKind::ComposedSprite,
            .postprocess = ImagePostprocess::DetectMagentaBorder};
}

std::string GameTextureCache::make_key(const ImageAssetKey& key) {
    return to_string(key);
}

void GameTextureCache::record_slow_texture(const std::string& container_path,
                                           const std::string& sprite_name, LoadSource source,
                                           double elapsed_ms) {
    slow_textures_.push_back(
        {.container_path = container_path,
         .image_name = sprite_name,
         .source = source == LoadSource::PreparedUpload ? "prepared-upload" : "lazy-render",
         .elapsed_ms = elapsed_ms});
    std::ranges::sort(slow_textures_, [](const auto& lhs, const auto& rhs) {
        return lhs.elapsed_ms > rhs.elapsed_ms;
    });
    if (slow_textures_.size() > 10) {
        slow_textures_.resize(10);
    }
}

void GameTextureCache::set_lazy_render_context(LazyRenderContext context) {
    lazy_render_context_ = std::move(context);
}

void GameTextureCache::clear_lazy_render_context() {
    lazy_render_context_.reset();
}

void GameTextureCache::record_lazy_render_miss(const std::string& container_path,
                                               const std::string& sprite_name, double load_ms) {
    LazyRenderMissDiagnostic diagnostic{.container_path = container_path,
                                        .image_name = sprite_name,
                                        .render_time_ms = load_ms,
                                        .load_ms = load_ms};
    if (lazy_render_context_.has_value()) {
        diagnostic.frame_number = lazy_render_context_->frame_number;
        if (lazy_render_context_->render_time_ms > 0.0) {
            diagnostic.render_time_ms = lazy_render_context_->render_time_ms;
        }
        diagnostic.script_step = lazy_render_context_->script_step;
        diagnostic.script_envelope = lazy_render_context_->script_envelope;
    }
    lazy_render_misses_.push_back(std::move(diagnostic));
}

} // namespace d2engine
