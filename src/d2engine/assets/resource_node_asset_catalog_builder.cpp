#include "resource_node_asset_catalog_builder.hpp"

#include "ff_asset_store.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace d2engine {

using namespace adventure_render;

adventure_render::ResourceNodeAssetCatalog
build_resource_node_asset_catalog(const FfAssetStore& store) {
    adventure_render::ResourceNodeAssetCatalog catalog;

    // ── Gold Mine: static sprite in IsoCmon.ff ──────────────────────────
    {
        const auto meta = store.sprite_metadata("Imgs/IsoCmon.ff", "G000CR0000GL");
        if (meta.canvas_width <= 0 || meta.canvas_height <= 0) {
            throw std::runtime_error(
                "build_resource_node_asset_catalog: invalid GoldMine canvas dimensions "
                "(" +
                std::to_string(meta.canvas_width) + "x" + std::to_string(meta.canvas_height) + ")");
        }
        StaticResourceNodeVisual vis;
        vis.container_path = "Imgs/IsoCmon.ff";
        vis.logical_sprite = "G000CR0000GL";
        vis.canvas_width = meta.canvas_width;
        vis.canvas_height = meta.canvas_height;
        vis.canvas_foot_x = meta.canvas_foot_x;
        vis.canvas_foot_y = meta.canvas_foot_y;
        catalog.visuals[d2runtime::AdventureResourceKind::GoldMine] = std::move(vis);
    }

    // ── Mana resources: looping animations in IsoAnim.ff ────────────────
    struct ManaEntry {
        d2runtime::AdventureResourceKind kind;
        const char*                      animation_name;
    };

    const ManaEntry mana_entries[] = {
        {d2runtime::AdventureResourceKind::RedMana, "G000CR0000RD"},
        {d2runtime::AdventureResourceKind::YellowMana, "G000CR0000YE"},
        {d2runtime::AdventureResourceKind::OrangeMana, "G000CR0000RG"},
        {d2runtime::AdventureResourceKind::WhiteMana, "G000CR0000WH"},
        {d2runtime::AdventureResourceKind::BlueMana, "G000CR0000GR"},
    };

    for (const auto& entry : mana_entries) {
        const auto anim_meta = store.animation_metadata("Imgs/IsoAnim.ff", entry.animation_name);

        if (anim_meta.frames.empty()) {
            throw std::runtime_error(
                std::string("build_resource_node_asset_catalog: zero-frame animation ") +
                "Imgs/IsoAnim.ff/" + entry.animation_name);
        }

        if (anim_meta.canvas_foot_x < 0 || anim_meta.canvas_foot_y < 0) {
            throw std::runtime_error(
                std::string("build_resource_node_asset_catalog: negative foot for ") +
                "Imgs/IsoAnim.ff/" + entry.animation_name + " foot=(" +
                std::to_string(anim_meta.canvas_foot_x) + "," +
                std::to_string(anim_meta.canvas_foot_y) + ")");
        }

        AnimatedResourceNodeVisual vis;
        vis.container_path = "Imgs/IsoAnim.ff";
        vis.logical_animation = entry.animation_name;
        vis.canvas_foot_x = anim_meta.canvas_foot_x;
        vis.canvas_foot_y = anim_meta.canvas_foot_y;

        vis.animation_data.animation_name = entry.animation_name;
        vis.animation_data.native_canvas_w = anim_meta.native_canvas_w;
        vis.animation_data.native_canvas_h = anim_meta.native_canvas_h;
        vis.animation_data.is_looping = true;
        vis.animation_data.frames.reserve(anim_meta.frames.size());

        for (const auto& frame : anim_meta.frames) {
            const auto frame_meta = store.sprite_metadata("Imgs/IsoAnim.ff", frame.image_name);

            if (frame_meta.canvas_width <= 0 || frame_meta.canvas_height <= 0) {
                throw std::runtime_error(
                    std::string(
                        "build_resource_node_asset_catalog: invalid frame dimensions for ") +
                    "Imgs/IsoAnim.ff/" + entry.animation_name + " frame=" + frame.image_name +
                    " dim=(" + std::to_string(frame_meta.canvas_width) + "x" +
                    std::to_string(frame_meta.canvas_height) + ")");
            }

            AdventureAnimationFrame af;
            af.record_name = frame.image_name;
            af.duration_ms = static_cast<int>(frame.duration_ms);
            af.canvas_width = frame_meta.canvas_width;
            af.canvas_height = frame_meta.canvas_height;
            vis.animation_data.frames.push_back(std::move(af));
        }

        catalog.visuals[entry.kind] = std::move(vis);
    }

    if (catalog.visuals.size() != 6u) {
        throw std::runtime_error(
            "build_resource_node_asset_catalog: expected 6 resource kinds, got " +
            std::to_string(catalog.visuals.size()));
    }

    return catalog;
}

} // namespace d2engine
