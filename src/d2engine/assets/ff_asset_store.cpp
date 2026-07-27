#include "ff_asset_store.hpp"

#include "d2res/anim_decoder.hpp"
#include "d2res/image_decoder.hpp"
#include "d2res/mqdb.hpp"
#include "d2res/opt_maps.hpp"
#include "d2res/png_metadata.hpp"

#include <d2log/log.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <limits>
#include <ranges>
#include <sstream>
#include <utility>

namespace d2engine {

namespace {

auto kLog = d2log::get("d2.assets"); // NOLINT(cert-err58-cpp)

constexpr std::array<std::uint8_t, 12> kIendChunk = {0x00, 0x00, 0x00, 0x00, 0x49, 0x45,
                                                     0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82};

struct ScopedTimer {
    using Clock = std::chrono::steady_clock;
    Clock::time_point start;
    explicit ScopedTimer() : start(Clock::now()) {}
    [[nodiscard]] double elapsed_ms() const {
        return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    }
};

[[nodiscard]] bool ascii_iequals(std::string_view a, std::string_view b) {
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const unsigned char ca = static_cast<unsigned char>(a[i]);
        const unsigned char cb = static_cast<unsigned char>(b[i]);
        const auto          lower = [](unsigned char c) {
            return static_cast<unsigned char>(c >= 'A' && c <= 'Z' ? c + 0x20 : c);
        };
        if (lower(ca) != lower(cb))
            return false;
    }
    return true;
}

[[nodiscard]] std::string ascii_upper(std::string src) {
    for (char& c : src)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return src;
}

// Case-insensitively find an immediate subdirectory of root matching the given
// lowercase name (e.g. "imgs", "interf"). Returns the actual filesystem path
// with original casing, or empty path if not found.
std::filesystem::path find_asset_dir(const std::filesystem::path& root,
                                     std::string_view             name_lower) {
    if (!std::filesystem::is_directory(root))
        return {};
    for (const auto& entry : std::filesystem::directory_iterator(root)) {
        if (!entry.is_directory())
            continue;
        if (ascii_iequals(entry.path().filename().string(), name_lower)) {
            return entry.path();
        }
    }
    return {};
}

struct FrameCanvasMetadata {
    int32_t                               width = 0;
    int32_t                               height = 0;
    int32_t                               foot_x = 0;
    int32_t                               foot_y = 0;
    int32_t                               top_y = 0;
    adventure_render::CanvasContentBounds content_bounds;
};

[[nodiscard]] adventure_render::CanvasContentBounds
derive_animation_frame_content_bounds(const d2res::ImageFrame& frame) {
    if (frame.output_width <= 0 || frame.output_height <= 0) {
        throw std::runtime_error("animation_frame_invalid_output_dimensions frame=" + frame.name +
                                 " dimensions=" + std::to_string(frame.output_width) + "x" +
                                 std::to_string(frame.output_height));
    }
    if (frame.pieces.empty()) {
        return {};
    }

    int32_t min_x = frame.pieces[0].output_x;
    int32_t max_x = min_x + frame.pieces[0].width;
    int32_t min_y = frame.pieces[0].output_y;
    int32_t max_y = min_y + frame.pieces[0].height;
    for (std::size_t i = 1; i < frame.pieces.size(); ++i) {
        const auto& p = frame.pieces[i];
        min_x = std::min(min_x, p.output_x);
        max_x = std::max(max_x, p.output_x + p.width);
        min_y = std::min(min_y, p.output_y);
        max_y = std::max(max_y, p.output_y + p.height);
    }
    if (max_x <= min_x || max_y <= min_y) {
        throw std::runtime_error("animation_frame_invalid_content_bounds frame=" + frame.name);
    }
    return {.min_x = min_x, .min_y = min_y, .max_x = max_x, .max_y = max_y};
}

FrameCanvasMetadata derive_frame_canvas_metadata(const d2res::ImageFrame& frame) {
    if (frame.pieces.empty()) {
        throw std::runtime_error("ImageFrame.pieces is empty for '" + frame.name + "'");
    }
    if (frame.output_width <= 0 || frame.output_height <= 0) {
        throw std::runtime_error("ImageFrame.output dimensions invalid for '" + frame.name +
                                 "': " + std::to_string(frame.output_width) + "x" +
                                 std::to_string(frame.output_height));
    }
    int32_t min_x = frame.pieces[0].output_x;
    int32_t max_x = min_x + frame.pieces[0].width;
    int32_t min_y = frame.pieces[0].output_y;
    int32_t max_y = min_y + frame.pieces[0].height;
    for (std::size_t i = 1; i < frame.pieces.size(); ++i) {
        const auto& p = frame.pieces[i];
        min_x = std::min(min_x, p.output_x);
        max_x = std::max(max_x, p.output_x + p.width);
        min_y = std::min(min_y, p.output_y);
        max_y = std::max(max_y, p.output_y + p.height);
    }
    if (max_x <= min_x || max_y <= min_y) {
        throw std::runtime_error("ImageFrame content bounds invalid for '" + frame.name + "'");
    }
    return {.width = frame.output_width,
            .height = frame.output_height,
            .foot_x = (min_x + max_x) / 2,
            .foot_y = max_y,
            .top_y = min_y,
            .content_bounds = {.min_x = min_x, .min_y = min_y, .max_x = max_x, .max_y = max_y}};
}

void apply_first_animation_frame_canvas(AnimationSequence& seq, const d2res::ImageMap& image_map,
                                        const std::vector<std::string>& frame_names,
                                        bool                            stop_after_match) {
    for (const auto& fname : frame_names) {
        const std::string key = ascii_upper(fname);
        auto              blk_it = image_map.frame_name_to_block.find(key);
        if (blk_it == image_map.frame_name_to_block.end()) {
            continue;
        }
        const auto& blk = image_map.blocks[blk_it->second];
        bool        found = false;
        for (const auto& img_frame : blk.frames) {
            if (ascii_upper(img_frame.name) != key || img_frame.pieces.empty()) {
                continue;
            }
            seq.native_canvas_w = img_frame.output_width;
            seq.native_canvas_h = img_frame.output_height;
            if (!img_frame.pieces.empty()) {
                auto meta = derive_frame_canvas_metadata(img_frame);
                seq.canvas_foot_x = meta.foot_x;
                seq.canvas_foot_y = meta.foot_y;
                seq.canvas_top_y = meta.top_y;
            }
            found = true;
            break;
        }
        if (found || stop_after_match)
            break;
    }
}

} // namespace

// ── Construction / destruction ───────────────────────────────────────────────

FfAssetStore::FfAssetStore(std::filesystem::path game_root) : game_root_(std::move(game_root)) {
    if (!std::filesystem::is_directory(game_root_)) {
        throw std::runtime_error("Game root is not a directory: " + game_root_.string());
    }

    const auto imgs_dir = find_asset_dir(game_root_, "imgs");
    if (imgs_dir.empty()) {
        throw std::runtime_error("Imgs/ directory not found in: " + game_root_.string());
    }

    int  count = 0;
    auto scan_dir = [&](const std::filesystem::path& dir, std::string_view prefix) {
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (!entry.is_regular_file())
                continue;
            if (!ascii_iequals(entry.path().extension().string(), ".ff"))
                continue;

            const auto filename = entry.path().filename().string();
            const auto canonical = canonical_ff_container_key(std::string(prefix) + "/" + filename);

            // Detect case-collision
            if (containers_.contains(canonical)) {
                const auto& existing = containers_[canonical]->full_path;
                throw std::runtime_error("Duplicate container identity: '" + canonical +
                                         "' maps to both '" + existing.string() + "' and '" +
                                         entry.path().string() + "'");
            }

            auto state = std::make_unique<ContainerState>();
            state->full_path = entry.path(); // preserve actual filesystem casing
            containers_.emplace(canonical, std::move(state));
            ++count;
        }
    };

    scan_dir(imgs_dir, "Imgs");

    const auto interf_dir = find_asset_dir(game_root_, "interf");
    if (!interf_dir.empty()) {
        scan_dir(interf_dir, "Interf");
    }

    if (containers_.empty()) {
        throw std::runtime_error("No .ff containers found in " + imgs_dir.string());
    }

    kLog->info("discovered containers={}", count);
}

FfAssetStore::~FfAssetStore() noexcept {
    try {
        if (dumped_.load(std::memory_order_acquire) == FfAccessDumpState::None) {
            if (kLog->should_log(spdlog::level::debug)) {
                dump_access_report(FfAccessDumpMode::Summary);
            }
        }
    } catch (...) {
    }
}

// ── Container discovery ──────────────────────────────────────────────────────

std::vector<std::string> FfAssetStore::containers() const {
    std::lock_guard          lock(registry_mtx_);
    std::vector<std::string> result;
    result.reserve(containers_.size());
    for (const auto& [path, _] : containers_) {
        result.push_back(path);
    }
    std::ranges::sort(result);
    return result;
}

// ── Canonical logical container identity ────────────────────────────────────
//
// Produces a platform-independent, case-insensitive logical key.
// NOT a filesystem path — never use the result to open files.

std::string FfAssetStore::canonical_ff_container_key(std::string_view path) {
    std::string result;
    result.reserve(path.size());
    for (char ch : path) {
        const auto uc = static_cast<unsigned char>(ch);
        if (uc == '\\') {
            result.push_back('/');
        } else if (uc >= 'A' && uc <= 'Z') {
            result.push_back(static_cast<char>(uc + 0x20)); // lowercase
        } else {
            result.push_back(static_cast<char>(uc));
        }
    }
    return result;
}

// ── Container lookup (case-insensitive canonical key) ───────────────────────

FfAssetStore::ContainerState* FfAssetStore::find_container(std::string_view container) const {
    const auto      key = canonical_ff_container_key(container);
    std::lock_guard lock(registry_mtx_);
    const auto      it = containers_.find(key);
    return it == containers_.end() ? nullptr : it->second.get();
}

FfAssetStore::ContainerState& FfAssetStore::ensure_container(std::string_view container) const {
    auto* state = find_container(container);
    if (state == nullptr) {
        throw std::runtime_error("Container not found: " + std::string(container));
    }
    return *state;
}

// ── Lazy container initialization (builds inventory) ─────────────────────────
//
// INVARIANT: callers must NOT hold ContainerState::mtx when calling this
// function. std::call_once provides its own synchronization so that exactly
// one thread performs the MQDB open + inventory build.
//
// Publication happens under state.mtx so that access_report() (which also
// acquires state.mtx) never observes partially initialized state.

void FfAssetStore::initialize_container(ContainerState&  state,
                                        std::string_view container_key) const {
    std::call_once(state.init_once, [&] {
        try {
            ScopedTimer timer;
            kLog->debug("ff_container_init_begin container={} physical_path={}", container_key,
                        state.full_path.string());

            // Step 1: expensive I/O and parsing without lock
            auto mqdb =
                std::make_unique<d2res::MqdbContainer>(d2res::MqdbContainer::open(state.full_path));
            const double open_ms = timer.elapsed_ms();
            kLog->debug("ff_container_opened container={} duration_ms={:.2f}", container_key,
                        open_ms);

            auto record_names = mqdb->names();
            std::ranges::sort(record_names);
            const double inventory_ms = timer.elapsed_ms();
            kLog->debug("ff_container_inventory_built container={} records={} duration_ms={:.2f}",
                        container_key, record_names.size(), inventory_ms);

            std::unordered_set<std::string> record_name_index;
            record_name_index.reserve(record_names.size());
            std::unordered_map<std::string, FfRecordUsage> record_usage;
            for (const auto& name : record_names) {
                record_name_index.insert(name);
                record_usage[name] = FfRecordUsage{};
            }

            std::unique_ptr<d2res::OptMaps>              opt_maps;
            std::unique_ptr<d2res::ImageResourceDecoder> decoder;
            bool                                         has_opt = false;
            try {
                opt_maps = std::make_unique<d2res::OptMaps>(d2res::parse_image_opt_maps(*mqdb));
                decoder = std::make_unique<d2res::ImageResourceDecoder>(*mqdb, *opt_maps);
                has_opt = true;
            } catch (const std::exception& e) {
                kLog->debug("no_opt_maps container={} reason={}", container_key, e.what());
            }
            const double opt_ms = timer.elapsed_ms();
            kLog->debug("ff_container_opt_maps_end container={} has_opt={} duration_ms={:.2f}",
                        container_key, has_opt, opt_ms);

            // Step 2: build derived indexes LOCALLY, before publication.
            //
            // LIFETIME INVARIANT: resolve_index stores pointers into *opt_maps
            // (ImageFrame, ImageBlock). Those pointers refer to objects owned by
            // the heap-allocated OptMaps instance. Moving the unique_ptr
            // transfers ownership without relocating the pointee, so the
            // pointers remain valid for the lifetime of ContainerState.
            // ensure_anim_maps() later mutates opt_maps->anim_map but never
            // touches image_map.blocks or image_map.frame_name_to_block —
            // frame/block pointers stay stable.

            std::unordered_map<int32_t, std::string> id_to_record_name;
            for (const auto& rec : mqdb->records()) {
                if (!rec.name.empty()) {
                    id_to_record_name[rec.id] = rec.name;
                }
            }

            std::unordered_map<std::string, ResolvedSpriteDependency> resolve_index;
            if (has_opt) {
                resolve_index.reserve(opt_maps->index_map.entries.size());
                for (const auto& entry : opt_maps->index_map.entries) {
                    if (entry.is_animation()) {
                        continue;
                    }
                    const std::string key = ascii_upper(entry.name);

                    const auto blk_it = opt_maps->image_map.frame_name_to_block.find(key);
                    if (blk_it == opt_maps->image_map.frame_name_to_block.end())
                        continue;
                    const auto& block = opt_maps->image_map.blocks[blk_it->second];

                    const d2res::ImageFrame* frame_ptr = nullptr;
                    for (const auto& f : block.frames) {
                        if (ascii_upper(f.name) == key) {
                            frame_ptr = &f;
                            break;
                        }
                    }
                    if (frame_ptr == nullptr)
                        continue;

                    std::string physical_name;
                    {
                        auto id_it = id_to_record_name.find(entry.id);
                        if (id_it != id_to_record_name.end()) {
                            physical_name = id_it->second;
                        }
                    }

                    if (!physical_name.empty()) {
                        resolve_index[key] = ResolvedSpriteDependency{.logical_name = entry.name,
                                                                      .physical_record_name =
                                                                          std::move(physical_name),
                                                                      .frame = frame_ptr,
                                                                      .block = &block,
                                                                      .is_raw_png = false};
                        continue;
                    }

                    if (record_name_index.contains(entry.name)) {
                        resolve_index[key] =
                            ResolvedSpriteDependency{.logical_name = entry.name,
                                                     .physical_record_name = entry.name,
                                                     .frame = frame_ptr,
                                                     .block = &block,
                                                     .is_raw_png = false};
                        continue;
                    }

                    const std::string with_png = entry.name + ".PNG";
                    if (record_name_index.contains(ascii_upper(with_png))) {
                        resolve_index[key] =
                            ResolvedSpriteDependency{.logical_name = entry.name,
                                                     .physical_record_name = with_png,
                                                     .frame = frame_ptr,
                                                     .block = &block,
                                                     .is_raw_png = false};
                        continue;
                    }
                }

                // Also add raw-PNG entries: any record that wasn't resolved
                // above but exists in the container can be served as raw PNG.
                for (const auto& rec_name : record_names) {
                    const std::string norm = ascii_upper(rec_name);
                    if (resolve_index.contains(norm))
                        continue;
                    if (!norm.ends_with(".PNG"))
                        continue;
                    const std::string logical = norm.substr(0, norm.size() - 4);
                    if (!resolve_index.contains(logical)) {
                        resolve_index[logical] =
                            ResolvedSpriteDependency{.logical_name = logical,
                                                     .physical_record_name = rec_name,
                                                     .frame = nullptr,
                                                     .block = nullptr,
                                                     .is_raw_png = true};
                    }
                    if (!resolve_index.contains(norm)) {
                        resolve_index[norm] =
                            ResolvedSpriteDependency{.logical_name = norm,
                                                     .physical_record_name = rec_name,
                                                     .frame = nullptr,
                                                     .block = nullptr,
                                                     .is_raw_png = true};
                    }
                }
            }

            // Step 3: atomic publication under state.mtx.
            // All local work is done; publish the fully-constructed state
            // so that resolve_index pointers are already stable.
            {
                std::lock_guard lock(state.mtx);
                state.mqdb = std::move(mqdb);
                state.record_names = std::move(record_names);
                state.record_name_index = std::move(record_name_index);
                state.record_usage = std::move(record_usage);
                state.opt_maps = std::move(opt_maps);
                state.decoder = std::move(decoder);
                state.id_to_record_name = std::move(id_to_record_name);
                state.resolve_index = std::move(resolve_index);
                state.ever_accessed = true;
            }

            kLog->debug("ff_container_init_end container={} duration_ms={:.2f}", container_key,
                        timer.elapsed_ms());
        } catch (const std::exception& e) {
            throw std::runtime_error("Failed to open container '" + std::string(container_key) +
                                     "': " + e.what());
        }
    });
}

void FfAssetStore::ensure_anim_maps(ContainerState& state, std::string_view container_key) const {
    if (!state.mqdb || !state.opt_maps) {
        throw std::runtime_error("Container '" + std::string(container_key) + "' is not open");
    }

    std::call_once(state.anim_init_once, [&] {
        try {
            const auto anims_rec = state.mqdb->find_by_name("-ANIMS.OPT");
            if (!anims_rec.has_value()) {
                throw std::runtime_error("container has no -ANIMS.OPT record");
            }

            const auto               anims_entry = state.mqdb->read_record(anims_rec->index);
            std::vector<std::string> anim_names;
            for (const auto& entry : state.opt_maps->index_map.entries) {
                if (entry.is_animation()) {
                    anim_names.push_back(entry.name);
                }
            }

            state.opt_maps->anim_map = d2res::AnimsParser::parse(
                std::span<const uint8_t>(anims_entry.payload), anim_names);
        } catch (const std::exception& e) {
            std::ostringstream msg;
            msg << "Failed to parse animation maps for container '" << container_key
                << "': " << e.what();
            throw std::runtime_error(msg.str());
        }
    });
}

// ── Inventory queries (metadata, do NOT increment hits) ──────────────────────

std::vector<std::string> FfAssetStore::record_names(std::string_view container) const {
    auto& state = ensure_container(container);
    initialize_container(state, container);
    std::lock_guard lock(state.mtx);
    state.ever_accessed = true;
    return state.record_names;
}

bool FfAssetStore::contains_record(std::string_view container, std::string_view record) const {
    auto& state = ensure_container(container);
    initialize_container(state, container);
    std::lock_guard lock(state.mtx);
    state.ever_accessed = true;
    return state.record_name_index.contains(std::string(record));
}

// ── OPT-indexed sprite / animation queries ───────────────────────────────────

std::vector<std::string> FfAssetStore::sprites_in(std::string_view container) const {
    auto& state = ensure_container(container);
    initialize_container(state, container);
    std::lock_guard lock(state.mtx);
    state.ever_accessed = true;
    if (!state.decoder) {
        return {};
    }
    return state.decoder->list_images();
}

std::vector<std::string> FfAssetStore::animations_in(std::string_view container) const {
    auto& state = ensure_container(container);
    initialize_container(state, container);
    {
        std::lock_guard lock(state.mtx);
        state.ever_accessed = true;
        if (!state.decoder) {
            return {};
        }
    }
    try {
        ensure_anim_maps(state, container);
        const d2res::AnimationDecoder anim_decoder(*state.mqdb, *state.opt_maps);
        return anim_decoder.list_animations();
    } catch (const std::exception& e) {
        kLog->warn("no_animations container={} error={}", container, e.what());
        return {};
    }
}

std::vector<std::string> FfAssetStore::animations_in_if_present(std::string_view container) const {
    auto& state = ensure_container(container);
    initialize_container(state, container);
    {
        std::lock_guard lock(state.mtx);
        state.ever_accessed = true;
        if (!state.decoder || !state.mqdb) {
            return {};
        }
        // Normal absence of -ANIMS.OPT → silently return empty.
        if (!state.mqdb->find_by_name("-ANIMS.OPT").has_value()) {
            return {};
        }
    }
    // Container has -ANIMS.OPT — parse it.  Failures throw.
    ensure_anim_maps(state, container);
    const d2res::AnimationDecoder anim_decoder(*state.mqdb, *state.opt_maps);
    return anim_decoder.list_animations();
}

d2res::RgbaBuffer FfAssetStore::decode_sprite(std::string_view container,
                                              std::string_view sprite_name) const {
    auto& state = ensure_container(container);
    initialize_container(state, container);
    const std::string sprite_key(sprite_name);

    // Try OPT-indexed path
    d2res::ImageResourceDecoder* decoder = nullptr;
    {
        std::lock_guard lock(state.mtx);
        state.ever_accessed = true;
        decoder = state.decoder.get();
    }
    if (decoder != nullptr) {
        try {
            const auto        decoded = decoder->decode_image(sprite_key);
            d2res::RgbaBuffer buf;
            buf.width = decoded.width;
            buf.height = decoded.height;
            buf.rgba = decoded.rgba;

            {
                std::lock_guard lock(state.mtx);
                const auto      usage_it = state.record_usage.find(sprite_key);
                if (usage_it != state.record_usage.end()) {
                    usage_it->second.hits += 1;
                } else {
                    state.missing_usage[sprite_key].hits += 1;
                }
            }

            return buf;
        } catch (const std::exception&) {
            // Fall through to raw record lookup
        }
    }

    // Fall back to raw PNG record
    {
        auto decoded = copy_raw_png(container, sprite_name);
        if (decoded.has_value() && !decoded->rgba.empty()) {
            return std::move(*decoded);
        }
    }

    throw std::runtime_error("decode_sprite failed: sprite='" + std::string(sprite_name) +
                             "' container='" + std::string(container) + "'");
}

std::optional<FfAssetStore::ResolvedSpriteDependency>
FfAssetStore::resolve_sprite_fast(std::string_view container, std::string_view sprite_name) const {
    auto& state = ensure_container(container);
    initialize_container(state, container);

    const std::string key = ascii_upper(std::string(sprite_name));
    std::lock_guard   lock(state.mtx);
    state.ever_accessed = true;

    // First try OPT-composed index
    {
        auto it = state.resolve_index.find(key);
        if (it != state.resolve_index.end()) {
            return it->second;
        }
    }

    // Fallback: try as raw PNG record directly
    const std::string with_png = key + ".PNG";
    if (state.record_name_index.contains(key)) {
        return ResolvedSpriteDependency{.logical_name = std::string(sprite_name),
                                        .physical_record_name = key,
                                        .frame = nullptr,
                                        .block = nullptr,
                                        .is_raw_png = true};
    }
    if (state.record_name_index.contains(with_png)) {
        return ResolvedSpriteDependency{.logical_name = std::string(sprite_name),
                                        .physical_record_name = with_png,
                                        .frame = nullptr,
                                        .block = nullptr,
                                        .is_raw_png = true};
    }

    return std::nullopt;
}

FfAssetStore::SpriteMetadata FfAssetStore::sprite_metadata(std::string_view container,
                                                           std::string_view sprite_name) const {
    auto dep = resolve_sprite_fast(container, sprite_name);
    if (!dep.has_value() || dep->is_raw_png || dep->frame == nullptr) {
        throw std::runtime_error("sprite_metadata: no OPT metadata for " + std::string(container) +
                                 "/" + std::string(sprite_name));
    }
    const auto& frame = *dep->frame;
    auto        meta = derive_frame_canvas_metadata(frame);
    return SpriteMetadata{.canvas_width = meta.width,
                          .canvas_height = meta.height,
                          .canvas_foot_x = meta.foot_x,
                          .canvas_foot_y = meta.foot_y,
                          .canvas_top_y = meta.top_y,
                          .content_bounds = meta.content_bounds};
}

AnimationSequence FfAssetStore::decode_animation(std::string_view container,
                                                 std::string_view anim_name) const {
    auto& state = ensure_container(container);
    initialize_container(state, container);

    const std::string anim_key(anim_name);
    {
        std::lock_guard lock(state.mtx);
        state.ever_accessed = true;
        if (!state.decoder) {
            throw std::runtime_error("Container '" + std::string(container) +
                                     "' is not a valid image container");
        }
        auto cache_it = state.anim_cache.find(anim_key);
        if (cache_it != state.anim_cache.end()) {
            return cache_it->second;
        }
    }

    try {
        ensure_anim_maps(state, container);
        const d2res::AnimationDecoder anim_decoder(*state.mqdb, *state.opt_maps);
        const auto                    frame_names = anim_decoder.list_animation_frames(anim_name);

        AnimationSequence seq;
        seq.name = anim_key;
        seq.container_path = std::string(container);
        seq.is_looping = false;

        apply_first_animation_frame_canvas(seq, state.opt_maps->image_map, frame_names, false);

        for (std::size_t i = 0; i < frame_names.size(); ++i) {
            const auto dep = resolve_sprite_fast(container, frame_names[i]);
            if (!dep.has_value()) {
                throw std::runtime_error("animation_frame_unresolved animation=" + anim_key +
                                         " frame=" + frame_names[i]);
            }
            if (dep->is_raw_png) {
                throw std::runtime_error("animation_frame_raw_png_not_supported animation=" +
                                         anim_key + " frame=" + frame_names[i]);
            }
            if (dep->frame == nullptr) {
                throw std::runtime_error("animation_frame_missing_opt_metadata animation=" +
                                         anim_key + " frame=" + frame_names[i]);
            }
            AnimationFrame frame;
            frame.image_name = frame_names[i];
            frame.index = i;
            frame.duration_ms = 100;
            frame.content_bounds = derive_animation_frame_content_bounds(*dep->frame);
            seq.frames.push_back(frame);
        }

        {
            std::lock_guard lock(state.mtx);
            state.anim_cache[anim_key] = seq;
        }
        return seq;
    } catch (const std::exception& e) {
        std::ostringstream msg;
        msg << "Failed to decode animation '" << anim_name << "' from container '" << container
            << "': " << e.what();
        throw std::runtime_error(msg.str());
    }
}

AnimationSequence FfAssetStore::animation_metadata(std::string_view container,
                                                   std::string_view anim_name) const {
    auto& state = ensure_container(container);
    initialize_container(state, container);

    {
        std::lock_guard lock(state.mtx);
        state.ever_accessed = true;
        if (!state.decoder) {
            throw std::runtime_error("Container '" + std::string(container) +
                                     "' is not a valid image container");
        }
    }

    const std::string anim_key(anim_name);

    try {
        ensure_anim_maps(state, container);
        const d2res::AnimationDecoder anim_decoder(*state.mqdb, *state.opt_maps);
        const auto                    frame_names = anim_decoder.list_animation_frames(anim_name);

        AnimationSequence seq;
        seq.name = anim_key;
        seq.container_path = std::string(container);
        seq.is_looping = false;

        apply_first_animation_frame_canvas(seq, state.opt_maps->image_map, frame_names, true);

        seq.frames.reserve(frame_names.size());
        for (std::size_t i = 0; i < frame_names.size(); ++i) {
            const auto dep = resolve_sprite_fast(container, frame_names[i]);
            if (!dep.has_value()) {
                throw std::runtime_error("animation_frame_unresolved animation=" + anim_key +
                                         " frame=" + frame_names[i]);
            }
            if (dep->is_raw_png) {
                throw std::runtime_error("animation_frame_raw_png_not_supported animation=" +
                                         anim_key + " frame=" + frame_names[i]);
            }
            if (dep->frame == nullptr) {
                throw std::runtime_error("animation_frame_missing_opt_metadata animation=" +
                                         anim_key + " frame=" + frame_names[i]);
            }
            AnimationFrame frame;
            frame.image_name = frame_names[i];
            frame.index = i;
            frame.duration_ms = 100;
            frame.content_bounds = derive_animation_frame_content_bounds(*dep->frame);
            seq.frames.push_back(frame);
        }

        return seq;
    } catch (const std::exception& e) {
        std::ostringstream msg;
        msg << "Failed to get animation metadata '" << anim_name << "' from container '"
            << container << "': " << e.what();
        throw std::runtime_error(msg.str());
    }
}

const d2res::OptMaps* FfAssetStore::container_maps(std::string_view container) const {
    auto& state = ensure_container(container);
    initialize_container(state, container);
    std::lock_guard lock(state.mtx);
    state.ever_accessed = true;
    return state.opt_maps.get();
}

void FfAssetStore::prewarm(std::string_view container) const {
    auto& state = ensure_container(container);
    initialize_container(state, container);
}

// ── Raw record access (hits-incrementing) ────────────────────────────────────

bool FfAssetStore::has_iend_chunk(std::span<const std::uint8_t> payload) {
    if (payload.size() < kIendChunk.size())
        return false;
    return std::equal(kIendChunk.begin(), kIendChunk.end(), payload.end() - kIendChunk.size());
}

std::vector<std::uint8_t> FfAssetStore::ensure_iend(std::vector<std::uint8_t> payload) {
    if (!has_iend_chunk(payload)) {
        payload.insert(payload.end(), kIendChunk.begin(), kIendChunk.end());
    }
    return payload;
}

std::shared_ptr<const std::vector<std::uint8_t>>
FfAssetStore::raw_record(std::string_view container, std::string_view record) const {
    auto&             state = ensure_container(container);
    const std::string record_key(record);

    initialize_container(state, container);

    // Phase 1: check cache / existence under lock
    {
        std::lock_guard lock(state.mtx);
        state.ever_accessed = true;

        if (!state.record_name_index.contains(record_key)) {
            state.missing_usage[record_key].hits += 1;
            return nullptr;
        }

        auto cache_it = state.raw_bytes_cache.find(record_key);
        if (cache_it != state.raw_bytes_cache.end() && cache_it->second.loaded) {
            state.record_usage[record_key].hits += 1;
            state.record_usage[record_key].cache_hits += 1;
            return cache_it->second.bytes;
        }
    }

    // Phase 2: expensive I/O without lock
    std::shared_ptr<const std::vector<std::uint8_t>> loaded;
    try {
        const auto entry = state.mqdb->read_name(record_key);
        auto       payload = ensure_iend(entry.payload);
        loaded = std::make_shared<const std::vector<std::uint8_t>>(std::move(payload));
    } catch (const std::exception& e) {
        kLog->error("raw_record_failed record={} container={} error={}", record_key, container,
                    e.what());
    }

    // Phase 3: publish under lock (check again in case another thread won the race)
    {
        std::lock_guard lock(state.mtx);

        if (!loaded) {
            state.missing_usage[record_key].hits += 1;
            return nullptr;
        }

        auto cache_it = state.raw_bytes_cache.find(record_key);
        if (cache_it != state.raw_bytes_cache.end() && cache_it->second.loaded) {
            state.record_usage[record_key].hits += 1;
            state.record_usage[record_key].cache_hits += 1;
            return cache_it->second.bytes;
        }

        state.raw_bytes_cache[record_key] = {loaded, true};
        state.record_usage[record_key].hits += 1;
        state.record_usage[record_key].loads += 1;
        return loaded;
    }
}

std::shared_ptr<const d2res::RgbaBuffer> FfAssetStore::raw_png(std::string_view container,
                                                               std::string_view record) const {
    auto&             state = ensure_container(container);
    const std::string record_key(record);

    initialize_container(state, container);

    // Phase 1: check cache / existence under lock
    {
        std::lock_guard lock(state.mtx);
        state.ever_accessed = true;

        auto cache_it = state.decoded_png_cache.find(record_key);
        if (cache_it != state.decoded_png_cache.end() && cache_it->second.loaded) {
            state.record_usage[record_key].hits += 1;
            state.record_usage[record_key].cache_hits += 1;
            return cache_it->second.buffer;
        }

        if (!state.record_name_index.contains(record_key)) {
            state.missing_usage[record_key].hits += 1;
            return nullptr;
        }
    }

    // Phase 2: expensive decode without lock
    std::shared_ptr<d2res::RgbaBuffer>               decoded;
    std::shared_ptr<const std::vector<std::uint8_t>> raw_cached;
    bool                                             ok = false;
    try {
        const auto entry = state.mqdb->read_name(record_key);
        auto       payload = ensure_iend(entry.payload);

        auto buffer = d2res::decode_base_png(std::span<const std::uint8_t>(payload));
        decoded = std::make_shared<d2res::RgbaBuffer>(std::move(buffer));
        ok = true;
        raw_cached = std::make_shared<const std::vector<std::uint8_t>>(std::move(payload));
    } catch (const std::exception& e) {
        kLog->error("raw_png_failed record={} container={} error={}", record_key, container,
                    e.what());
    }

    // Phase 3: publish under lock
    {
        std::lock_guard lock(state.mtx);

        if (!ok) {
            state.missing_usage[record_key].hits += 1;
            return nullptr;
        }

        auto cache_it = state.decoded_png_cache.find(record_key);
        if (cache_it != state.decoded_png_cache.end() && cache_it->second.loaded) {
            state.record_usage[record_key].hits += 1;
            state.record_usage[record_key].cache_hits += 1;
            return cache_it->second.buffer;
        }

        if (raw_cached) {
            state.raw_bytes_cache[record_key] = {raw_cached, true};
        }
        state.decoded_png_cache[record_key] = {decoded, true};
        state.record_usage[record_key].hits += 1;
        state.record_usage[record_key].loads += 1;
        return decoded;
    }
}

std::optional<d2res::RgbaBuffer> FfAssetStore::copy_raw_png(std::string_view container,
                                                            std::string_view record) const {
    auto decoded = raw_png(container, record);
    if (decoded == nullptr) {
        return std::nullopt;
    }
    return *decoded;
}

// ── Access report ────────────────────────────────────────────────────────────

FfAccessReport FfAssetStore::access_report() const {
    FfAccessReport report;

    std::lock_guard          reg_lock(registry_mtx_);
    std::vector<std::string> sorted_keys;
    sorted_keys.reserve(containers_.size());
    for (const auto& [key, _] : containers_) {
        sorted_keys.push_back(key);
    }
    std::ranges::sort(sorted_keys);

    for (const auto& key : sorted_keys) {
        auto* state_ptr = containers_.at(key).get();

        std::lock_guard lock(state_ptr->mtx);
        if (!state_ptr->ever_accessed) {
            continue;
        }

        FfContainerAccessReport container_report;
        container_report.container_path = key;

        // Records sorted by name
        std::vector<std::string> sorted_records;
        sorted_records.reserve(state_ptr->record_usage.size());
        for (const auto& [name, _] : state_ptr->record_usage) {
            sorted_records.push_back(name);
        }
        std::ranges::sort(sorted_records);

        for (const auto& name : sorted_records) {
            const auto&          usage = state_ptr->record_usage.at(name);
            FfRecordAccessReport rec_report;
            rec_report.record_name = name;
            rec_report.hits = usage.hits;
            rec_report.loads = usage.loads;
            rec_report.cache_hits = usage.cache_hits;
            container_report.records.push_back(std::move(rec_report));
            container_report.total_hits += usage.hits;
            container_report.total_loads += usage.loads;
            container_report.cache_hits += usage.cache_hits;
        }

        // Missing records sorted
        std::vector<std::string> sorted_missing;
        sorted_missing.reserve(state_ptr->missing_usage.size());
        for (const auto& [name, _] : state_ptr->missing_usage) {
            sorted_missing.push_back(name);
        }
        std::ranges::sort(sorted_missing);

        for (const auto& name : sorted_missing) {
            FfRecordAccessReport miss_report;
            miss_report.record_name = name;
            miss_report.hits = state_ptr->missing_usage.at(name).hits;
            miss_report.loads = 0;
            miss_report.cache_hits = 0;
            container_report.missing_requests.push_back(std::move(miss_report));
        }

        report.containers.push_back(std::move(container_report));
    }

    return report;
}

void FfAssetStore::dump_access_report(FfAccessDumpMode mode) const {
    // State machine: prevent duplicates, allow Summary -> Full upgrade.
    auto expected = FfAccessDumpState::None;
    auto desired =
        (mode == FfAccessDumpMode::Full) ? FfAccessDumpState::Full : FfAccessDumpState::Summary;
    if (!dumped_.compare_exchange_strong(expected, desired, std::memory_order_acq_rel)) {
        if (expected == FfAccessDumpState::Full) {
            return; // Already dumped Full; nothing more to do.
        }
        if (expected == FfAccessDumpState::Summary && mode == FfAccessDumpMode::Summary) {
            return; // Already dumped Summary; do not duplicate.
        }
        // expected == Summary && mode == Full: proceed to emit Full details.
        // We do NOT attempt CAS again because another thread may also upgrade.
        // Accept rare duplicate log lines in that race.
        dumped_.store(FfAccessDumpState::Full, std::memory_order_release);
    }

    if (!kLog->should_log(spdlog::level::debug)) {
        return;
    }

    kLog->debug("ff_access_dump_begin");

    const auto report = access_report();
    for (const auto& container_report : report.containers) {
        const std::uint64_t used_records = static_cast<std::uint64_t>(std::ranges::count_if(
            container_report.records, [](const auto& r) { return r.hits > 0; }));

        kLog->debug("ff_container_summary container={} records={} used_records={} "
                    "zero_hit_records={} total_hits={} total_loads={} cache_hits={} "
                    "missing_requests={}",
                    container_report.container_path, container_report.records.size(), used_records,
                    container_report.records.size() - used_records, container_report.total_hits,
                    container_report.total_loads, container_report.cache_hits,
                    container_report.missing_requests.size());

        if (mode == FfAccessDumpMode::Full) {
            for (const auto& rec : container_report.records) {
                kLog->debug("ff_record container={} record={} hits={} loads={} cache_hits={}",
                            container_report.container_path, rec.record_name, rec.hits, rec.loads,
                            rec.cache_hits);
            }

            for (const auto& miss : container_report.missing_requests) {
                kLog->debug("ff_missing container={} record={} hits={}",
                            container_report.container_path, miss.record_name, miss.hits);
            }
        }
    }

    kLog->debug("ff_access_dump_end");
}

} // namespace d2engine
