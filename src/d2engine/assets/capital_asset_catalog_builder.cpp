#include "capital_asset_catalog_builder.hpp"

#include "ff_asset_store.hpp"

#include <d2adventure_render/terrain/capital_asset_catalog.hpp>

#include <array>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace d2engine {

namespace {

constexpr std::string_view kCapitalContainer = "Imgs/IsoAnim.ff";

struct CapitalMapping {
    std::string_view race_id;
    std::string_view active_animation_name;
    std::string_view ruined_animation_name;
};

constexpr std::array<CapitalMapping, 5> kCapitalMappings = {
    {{"g000rr0000", "G000FT0000HU0", "G000FT0000HUC0"},
     {"g000rr0001", "G000FT0000DWC0", {}},
     {"g000rr0002", "G000FT0000HE0", "G000FT0000HEC0"},
     {"g000rr0003", "G000FT0000UN0", "G000FT0000UNC0"},
     {"g000rr0005", "G000FT0000EL0", "G000FT0000ELC0"}}};

[[nodiscard]] std::string detail_message(std::string_view race_id, std::string_view animation,
                                         std::string_view state) {
    return "capital asset build failed race_id=" + std::string(race_id) +
           " state=" + std::string(state) + " animation=" + std::string(animation);
}

[[nodiscard]] adventure_render::AnimatedCapitalVisual build_visual(const FfAssetStore& store,
                                                                   std::string_view    race_id,
                                                                   std::string_view animation_name,
                                                                   std::string_view state) {
    const auto sequence = store.animation_metadata(kCapitalContainer, animation_name);
    if (sequence.frames.empty()) {
        throw std::runtime_error(detail_message(race_id, animation_name, state) +
                                 " reason=zero_frames");
    }
    if (sequence.canvas_foot_x < 0 || sequence.canvas_foot_y < 0) {
        throw std::runtime_error(detail_message(race_id, animation_name, state) +
                                 " reason=negative_canvas_foot");
    }
    if (sequence.native_canvas_w <= 0 || sequence.native_canvas_h <= 0) {
        throw std::runtime_error(detail_message(race_id, animation_name, state) +
                                 " reason=invalid_native_canvas");
    }

    adventure_render::AnimatedCapitalVisual visual;
    visual.container_path = kCapitalContainer;
    visual.logical_animation_name = animation_name;
    visual.canvas_foot_x = sequence.canvas_foot_x;
    visual.canvas_foot_y = sequence.canvas_foot_y;
    visual.animation_data.animation_name = animation_name;
    visual.animation_data.native_canvas_w = sequence.native_canvas_w;
    visual.animation_data.native_canvas_h = sequence.native_canvas_h;
    visual.animation_data.is_looping = true;
    visual.animation_data.timing_source =
        adventure_render::AdventureAnimationTimingSource::ProvisionalFallback;
    visual.animation_data.frames.reserve(sequence.frames.size());

    for (const auto& frame : sequence.frames) {
        const auto sprite_meta = store.sprite_metadata(kCapitalContainer, frame.image_name);
        if (sprite_meta.canvas_width <= 0 || sprite_meta.canvas_height <= 0) {
            throw std::runtime_error(detail_message(race_id, animation_name, state) + " frame=" +
                                     frame.image_name + " reason=invalid_canvas_dimensions");
        }
        if (sprite_meta.canvas_foot_x < 0 || sprite_meta.canvas_foot_y < 0) {
            throw std::runtime_error(detail_message(race_id, animation_name, state) +
                                     " frame=" + frame.image_name + " reason=negative_canvas_foot");
        }

        adventure_render::AdventureAnimationFrame af;
        af.record_name = frame.image_name;
        af.duration_ms = static_cast<int>(frame.duration_ms);
        af.canvas_width = sprite_meta.canvas_width;
        af.canvas_height = sprite_meta.canvas_height;
        visual.animation_data.frames.push_back(std::move(af));
    }

    return visual;
}

} // namespace

adventure_render::CapitalAssetCatalog build_capital_asset_catalog(const FfAssetStore& store) {
    adventure_render::CapitalAssetCatalog catalog;
    std::size_t                           ruined_count = 0;

    for (const auto& mapping : kCapitalMappings) {
        const auto active =
            build_visual(store, mapping.race_id, mapping.active_animation_name, "Active");

        adventure_render::CapitalVisualSet set;
        set.active = active;
        if (!mapping.ruined_animation_name.empty()) {
            set.ruined =
                build_visual(store, mapping.race_id, mapping.ruined_animation_name, "Ruined");
            ++ruined_count;
        }
        catalog.visuals.emplace(adventure_render::canonical_capital_race_id(mapping.race_id),
                                std::move(set));
    }

    if (catalog.visuals.size() != kCapitalMappings.size()) {
        throw std::runtime_error("capital asset catalog must contain exactly five mappings");
    }
    if (ruined_count != 4u) {
        throw std::runtime_error("capital asset catalog must contain exactly four ruined mappings");
    }

    return catalog;
}

} // namespace d2engine
