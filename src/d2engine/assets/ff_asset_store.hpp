#pragma once

#include "d2engine/animation/animation_sequence.hpp"
#include <d2adventure_render/image_store.hpp>
#include <d2adventure_render/adventure_render_types.hpp>
#include "d2res/rgba_buffer.hpp"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace d2res {
class MqdbContainer;
class ImageResourceDecoder;
class AnimationDecoder;
struct OptMaps;
} // namespace d2res

namespace d2engine {

// ── Usage statistics per record ──────────────────────────────────────────────
struct FfRecordUsage {
    std::uint64_t hits = 0;
    std::uint64_t loads = 0;
    std::uint64_t cache_hits = 0;
};

struct FfMissingRecordUsage {
    std::uint64_t hits = 0;
};

// ── Structured access report (read-only query API, not only log text) ────────
struct FfRecordAccessReport {
    std::string   record_name;
    std::uint64_t hits = 0;
    std::uint64_t loads = 0;
    std::uint64_t cache_hits = 0;
};

struct FfContainerAccessReport {
    std::string                       container_path;
    std::vector<FfRecordAccessReport> records;
    std::vector<FfRecordAccessReport> missing_requests;
    std::uint64_t                     total_hits = 0;
    std::uint64_t                     total_loads = 0;
    std::uint64_t                     cache_hits = 0;
};

struct FfAccessReport {
    std::vector<FfContainerAccessReport> containers;
};

// ── FfAssetStore: sole high-level owner/access for original .ff assets ───────
//
// Responsibilities:
//   - discover all .ff containers under game_root
//   - lazy-open MQDB on first container access
//   - build complete record-name inventory on first access
//   - central source cache (raw bytes + decoded RGBA)
//   - access statistics (hits / loads / cache_hits / missing)
//   - debug access dump
//
// Thread-safe: per-container synchronization. Multiple threads may safely
// query different containers concurrently.
class FfAssetStore final : public IImageStore {
public:
    explicit FfAssetStore(std::filesystem::path game_root);
    ~FfAssetStore() noexcept override;

    FfAssetStore(const FfAssetStore&) = delete;
    FfAssetStore& operator=(const FfAssetStore&) = delete;
    FfAssetStore(FfAssetStore&&) = delete;
    FfAssetStore& operator=(FfAssetStore&&) = delete;

    // ── Container inventory (metadata only, does NOT increment hits) ─────────

    // All discovered .ff containers (canonical relative paths).
    [[nodiscard]] std::vector<std::string> containers() const;

    // All record names in a container as a sorted vector.
    // Lazy-opens the container on first call.
    [[nodiscard]] std::vector<std::string> record_names(std::string_view container) const;

    // True if the record exists in the container inventory.
    // Lazy-opens the container on first call.
    [[nodiscard]] bool contains_record(std::string_view container, std::string_view record) const;

    // ── OPT-indexed sprite/animation queries ─────────────────────────────────

    // List all sprite names in a container (from OPT index).
    [[nodiscard]] std::vector<std::string> sprites_in(std::string_view container) const;

    // List all animation names in a container.
    [[nodiscard]] std::vector<std::string> animations_in(std::string_view container) const;

    // List animation names without logging warnings for expected missing -ANIMS.OPT.
    // THROWS if the container has -ANIMS.OPT but parsing fails (corrupt data).
    [[nodiscard]] std::vector<std::string>
    animations_in_if_present(std::string_view container) const;

    // Decode an OPT-indexed sprite. Returns decoded RGBA.
    // Throws std::runtime_error on failure.
    [[nodiscard]] d2res::RgbaBuffer decode_sprite(std::string_view container,
                                                  std::string_view sprite_name) const;

    // Dependency info for one logical sprite (metadata only, no I/O).
    // This is how the FF layer exposes "which physical record feeds which
    // logical sprite" without revealing OPT/PNG internals.
    struct ResolvedSpriteDependency {
        std::string              logical_name;
        std::string              physical_record_name; // actual MQRC record name
        const d2res::ImageFrame* frame = nullptr;      // null for raw-PNG
        const d2res::ImageBlock* block = nullptr;      // null for raw-PNG
        bool                     is_raw_png = false;   // true when no OPT
    };

    // Canvas metadata for a static logical composed sprite.
    // Derived from the authoritative OPT ImageFrame pieces.
    //
    // canvas_foot_x and canvas_foot_y are derived from the visible ImageFrame
    // piece bounding box (horizontal center of all pieces, bottom of all pieces).
    // They represent the visual footprint of the sprite content and are NOT
    // necessarily an authored semantic world pivot or canonical tile anchor.
    // Consumers that need tile-aligned placement (e.g. road overlays) must use
    // the tile geometry (e.g. cell_canvas_origin) rather than these foot values.
    struct SpriteMetadata {
        int32_t                               canvas_width = 0;
        int32_t                               canvas_height = 0;
        int32_t                               canvas_foot_x = 0;
        int32_t                               canvas_foot_y = 0;
        int32_t                               canvas_top_y = 0;
        adventure_render::CanvasContentBounds content_bounds;
    };

    // Return canvas metadata for a static logical sprite from OPT ImageFrame.
    // Uses the existing OPT index (same data as resolve_sprite_fast).
    // Throws std::runtime_error if the sprite is not found or has no OPT metadata.
    [[nodiscard]] SpriteMetadata sprite_metadata(std::string_view container,
                                                 std::string_view sprite_name) const;

    // Fast O(1) resolve using the prebuilt index. Requires initialize_container
    // to have completed. Returns nullopt for unresolvable names.
    [[nodiscard]] std::optional<ResolvedSpriteDependency>
    resolve_sprite_fast(std::string_view container, std::string_view sprite_name) const;

    // Decode an OPT-indexed animation sequence.
    // Throws std::runtime_error on failure.
    [[nodiscard]] AnimationSequence decode_animation(std::string_view container,
                                                     std::string_view anim_name) const;

    // Return animation metadata WITHOUT decoding frames/content.
    // Uses parsed OPT/ANIMS metadata only. Fast enough for bulk catalog build.
    // Throws std::runtime_error on failure.
    [[nodiscard]] AnimationSequence animation_metadata(std::string_view container,
                                                       std::string_view anim_name) const;

    // Access parsed OPT maps for a container. Returns nullptr if none.
    [[nodiscard]] const d2res::OptMaps* container_maps(std::string_view container) const;

    // Pre-open a container on the calling thread (for later thread-safe use).
    void prewarm(std::string_view container) const;

    // ── Raw record / PNG access (hits-incrementing) ──────────────────────────

    // Read raw record payload bytes. Returns immutable shared copy.
    // Returns nullptr on missing record or container error.
    [[nodiscard]] std::shared_ptr<const std::vector<std::uint8_t>>
    raw_record(std::string_view container, std::string_view record) const;

    // Decode a raw PNG record. Returns immutable shared decoded buffer.
    // The same physical decoded object is returned on repeated calls.
    // Returns nullptr on missing or decode failure.
    [[nodiscard]] std::shared_ptr<const d2res::RgbaBuffer>
    raw_png(std::string_view container, std::string_view record) const override;

    // Explicit mutable copy (rarely needed; prefer raw_png for immutable access).
    [[nodiscard]] std::optional<d2res::RgbaBuffer>
    copy_raw_png(std::string_view container, std::string_view record) const override;

    // ── Access report / debug dump ───────────────────────────────────────────

    enum class FfAccessDumpMode { Summary, Full };

    enum class FfAccessDumpState { None, Summary, Full };

    // Structured read-only report. Does NOT log.
    [[nodiscard]] FfAccessReport access_report() const;

    // Log debug dump at DEBUG level. Idempotent (dumps only once by default).
    // Summary: container summary + missing records only.
    // Full:    container summary + every record (including hits=0) + missing records.
    void dump_access_report(FfAccessDumpMode mode = FfAccessDumpMode::Summary) const;

private:
    // ── Internal types ───────────────────────────────────────────────────────

    struct CachedRawBytes {
        std::shared_ptr<const std::vector<std::uint8_t>> bytes;
        bool                                             loaded = false;
    };

    struct CachedDecodedPng {
        std::shared_ptr<const d2res::RgbaBuffer> buffer;
        bool                                     loaded = false;
    };

    struct ContainerState {
        std::filesystem::path  full_path;
        mutable std::once_flag init_once;
        bool                   ever_accessed = false;

        // MQDB + OPT
        std::unique_ptr<d2res::MqdbContainer>        mqdb;
        std::unique_ptr<d2res::OptMaps>              opt_maps;
        std::unique_ptr<d2res::ImageResourceDecoder> decoder;

        // Animation cache
        mutable std::once_flag                                     anim_init_once;
        mutable std::unordered_map<std::string, AnimationSequence> anim_cache;

        // Complete inventory (built once on init)
        std::vector<std::string>        record_names;
        std::unordered_set<std::string> record_name_index;

        // Zero-initialized for every existing record
        std::unordered_map<std::string, FfRecordUsage> record_usage;

        // Requested names that do not exist
        std::unordered_map<std::string, FfMissingRecordUsage> missing_usage;

        // MQRC record ID → physical record name map (built during init)
        std::unordered_map<int32_t, std::string> id_to_record_name;

        // O(1) logical-name → ResolvedSpriteDependency index.
        // Built during initialize_container to avoid repeated 1258× OPT linear scans.
        // Key = ascii_upper(normalized logical name).
        // Stores ResolvedSpriteDependency by value (all pointers refer to opt_maps
        // which lives in this same ContainerState and is never destroyed).
        // Absent key means the logical name is unresolvable.
        std::unordered_map<std::string, ResolvedSpriteDependency> resolve_index;

        // Caches
        mutable std::unordered_map<std::string, CachedRawBytes>   raw_bytes_cache;
        mutable std::unordered_map<std::string, CachedDecodedPng> decoded_png_cache;

        // Synchronization (protects post-initialization mutable caches/statistics)
        mutable std::mutex mtx;
    };

    // ── Registry ─────────────────────────────────────────────────────────────

    std::filesystem::path                                                    game_root_;
    mutable std::mutex                                                       registry_mtx_;
    mutable std::unordered_map<std::string, std::unique_ptr<ContainerState>> containers_;
    mutable std::atomic<FfAccessDumpState> dumped_{FfAccessDumpState::None};

    // ── Internal helpers ─────────────────────────────────────────────────────

    [[nodiscard]] ContainerState* find_container(std::string_view container) const;
    [[nodiscard]] ContainerState& ensure_container(std::string_view container) const;
    void initialize_container(ContainerState& state, std::string_view container_key) const;
    void ensure_anim_maps(ContainerState& state, std::string_view container_key) const;

    // Canonical logical container identity: lowercase, forward-slash separators.
    // Used as the key for containers_ map. NOT a filesystem path.
    static std::string               canonical_ff_container_key(std::string_view path);
    static bool                      has_iend_chunk(std::span<const std::uint8_t> payload);
    static std::vector<std::uint8_t> ensure_iend(std::vector<std::uint8_t> payload);
};

} // namespace d2engine
