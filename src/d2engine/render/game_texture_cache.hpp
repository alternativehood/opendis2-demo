#pragma once

#include "../assets/ff_asset_store.hpp"
#include "../assets/asset_runtime.hpp"
#include "prepared_texture_frame.hpp"
#include "sdl_texture.hpp"

#include <SDL3/SDL.h>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace d2engine {

// Caches SDL_Texture objects for decoded sprites, keyed by (container, sprite_name).
class GameTextureCache {
public:
    struct PreparedTextureUploadResult {
        std::string container_path;
        std::string image_name;
        bool        success = false;
        bool        skipped_cache_hit = false;
        std::string error;
        double      elapsed_ms = 0.0;
    };

    struct SlowTextureDiagnostic {
        std::string container_path;
        std::string image_name;
        std::string source;
        double      elapsed_ms = 0.0;
    };

    struct LazyRenderContext {
        std::size_t frame_number = 0;
        double      render_time_ms = 0.0;
        std::string script_step;
        std::string script_envelope;
    };

    struct LazyRenderMissDiagnostic {
        std::string container_path;
        std::string image_name;
        std::size_t frame_number = 0;
        double      render_time_ms = 0.0;
        double      load_ms = 0.0;
        std::string script_step;
        std::string script_envelope;
    };

    struct TextureCacheStats {
        std::uint64_t cache_hits = 0;
        std::uint64_t cache_misses = 0;
        std::uint64_t prepared_upload_requests = 0;
        std::uint64_t prepared_upload_successes = 0;
        std::uint64_t prepared_upload_failures = 0;
        std::uint64_t prepared_upload_skipped_cache_hits = 0;
        std::uint64_t lazy_render_misses = 0;
        std::uint64_t planned_asset_misses = 0;
        std::uint64_t strict_planned_asset_misses = 0;
        double        total_prepared_upload_ms = 0.0;
    };

    explicit GameTextureCache(SDL_Renderer* renderer);
    GameTextureCache(SDL_Renderer* renderer, const FfAssetStore& store);
    ~GameTextureCache();

    // Disable copy/move
    GameTextureCache(const GameTextureCache&) = delete;
    GameTextureCache& operator=(const GameTextureCache&) = delete;
    GameTextureCache(GameTextureCache&&) = delete;
    GameTextureCache& operator=(GameTextureCache&&) = delete;

    // Get or create texture for a sprite. Returns nullptr if not found.
    // Caches the result for subsequent calls.
    [[nodiscard]] SDL_Texture* get(const std::string& container_path,
                                   const std::string& sprite_name);

    [[nodiscard]] PreparedTextureUploadResult upload_prepared(const PreparedTextureFrame& frame);
    [[nodiscard]] PreparedTextureUploadResult upload_prepared(const PreparedImage& image);

    // Get or create texture from a raw PNG record in an OPT-less container (Faces.ff, Ground.ff).
    // Returns nullptr on failure; emits one info log on first miss. Never throws.
    [[nodiscard]] SDL_Texture* get_raw(const std::string& container_path,
                                       const std::string& record_name,
                                       bool               apply_magenta_key = false);

    // True when the texture is already uploaded and available from cache.
    [[nodiscard]] bool         is_cached(const std::string& container_path,
                                         const std::string& sprite_name) const;
    [[nodiscard]] bool         is_cached(const ImageAssetKey& key) const;
    [[nodiscard]] SDL_Texture* find(const ImageAssetKey& key) const;

    // List all loaded texture keys as "container/sprite" strings.
    [[nodiscard]] std::vector<std::string> all_keys() const;

    [[nodiscard]] const TextureCacheStats& stats() const noexcept { return stats_; }
    [[nodiscard]] const std::vector<LazyRenderMissDiagnostic>& lazy_render_misses() const noexcept {
        return lazy_render_misses_;
    }

    // Mark a key as planned: render will return null instead of lazy-decoding on miss.
    void mark_planned(const std::string& container_path, const std::string& image_name);
    void mark_planned(const ImageAssetKey& key);

    // Enable strict mode: any render-path get() miss for a non-cached key is a hard error.
    // No lazy decoding is performed. Sets stats_.strict_planned_asset_misses on each miss.
    void set_strict_planned_assets(bool enabled) noexcept { strict_planned_assets_ = enabled; }

    // Keys requested via get() during render (for plan-vs-render comparison).
    [[nodiscard]] std::size_t render_requested_key_count() const noexcept {
        return render_requested_keys_.size();
    }

    // Keys explicitly marked as planned.
    [[nodiscard]] std::size_t planned_key_count() const noexcept { return planned_.size(); }

    // Keys requested by render that were absent from cache (strict mode only).
    [[nodiscard]] const std::vector<std::string>& strict_miss_keys() const noexcept {
        return strict_miss_keys_;
    }

    void set_lazy_render_context(LazyRenderContext context);
    void clear_lazy_render_context();

    // Clear all cached textures.
    void clear();

private:
    struct TextureEntry {
        SdlTexture texture;
        int        width = 0;
        int        height = 0;
    };

    enum class LoadSource : std::uint8_t { LazyRender, PreparedUpload, Planned };

    struct LoadResult {
        SDL_Texture* texture = nullptr;
        bool         cache_hit = false;
        std::string  error;
        double       elapsed_ms = 0.0;
    };

    SDL_Renderer*                                 renderer_ = nullptr;
    const FfAssetStore*                           store_ = nullptr;
    std::unordered_map<std::string, TextureEntry> cache_;
    std::unordered_set<std::string>               failed_; // keys that failed decode, never retry
    std::unordered_set<std::string>               planned_;
    std::unordered_set<std::string>               render_requested_keys_;
    std::vector<std::string>                      strict_miss_keys_;
    bool                                          strict_planned_assets_ = false;
    TextureCacheStats                             stats_;
    std::vector<SlowTextureDiagnostic>            slow_textures_;
    std::vector<LazyRenderMissDiagnostic>         lazy_render_misses_;
    std::optional<LazyRenderContext>              lazy_render_context_;

    static ImageAssetKey composed_sprite_key(const std::string& container_path,
                                             const std::string& sprite_name);
    static std::string make_key(const std::string& container_path, const std::string& sprite_name);
    static std::string make_key(const ImageAssetKey& key);
    [[nodiscard]] LoadResult load(const std::string& container_path, const std::string& sprite_name,
                                  LoadSource source);
    void record_slow_texture(const std::string& container_path, const std::string& sprite_name,
                             LoadSource source, double elapsed_ms);
    void record_lazy_render_miss(const std::string& container_path, const std::string& sprite_name,
                                 double load_ms);
};

} // namespace d2engine
