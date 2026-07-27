#include "render_graph_asset_collector.hpp"

#include "image_asset_key.hpp"

#include <d2adventure_render/adventure_render_types.hpp>
#include <d2adventure_render/render_graph.hpp>

#include <set>
#include <string>
#include <vector>

namespace d2engine {

namespace {

void collect_primitive_assets(const adventure_render::PreparedAdventureRenderPrimitive& prim,
                              std::set<std::string>& seen_ids, std::vector<ImageAssetKey>& keys) {
    if (prim.container_path.empty())
        return;

    auto add_asset = [&](const std::string& record) {
        if (record.empty())
            return;
        const std::string asset_id = prim.container_path + "/" + record;
        if (!seen_ids.insert(asset_id).second)
            return;
        keys.push_back(make_world_composed_sprite_key(prim.container_path, record));
    };

    if (prim.animation.has_value()) {
        for (const auto& frame : prim.animation->frames)
            add_asset(frame.record_name);
    } else {
        add_asset(prim.record_name);
    }
}

void collect_primitive_initial_assets(
    const adventure_render::PreparedAdventureRenderPrimitive& prim, std::set<std::string>& seen_ids,
    std::vector<ImageAssetKey>& keys) {
    if (prim.container_path.empty())
        return;

    auto add_asset = [&](const std::string& record) {
        if (record.empty())
            return;
        const std::string asset_id = prim.container_path + "/" + record;
        if (!seen_ids.insert(asset_id).second)
            return;
        keys.push_back(make_world_composed_sprite_key(prim.container_path, record));
    };

    if (prim.animation.has_value()) {
        // Only the first frame is critical for initial display
        if (!prim.animation->frames.empty())
            add_asset(prim.animation->frames[0].record_name);
    } else {
        add_asset(prim.record_name);
    }
}

void collect_primitive_remaining_animation_assets(
    const adventure_render::PreparedAdventureRenderPrimitive& prim,
    std::set<std::string>& initial_ids, std::vector<ImageAssetKey>& keys) {
    if (prim.container_path.empty() || !prim.animation.has_value())
        return;

    auto add_remaining = [&](const std::string& record) {
        if (record.empty())
            return;
        const std::string asset_id = prim.container_path + "/" + record;
        if (initial_ids.contains(asset_id))
            return;
        if (!initial_ids.insert(asset_id).second)
            return;
        keys.push_back(make_world_composed_sprite_key(prim.container_path, record));
    };

    for (std::size_t i = 1; i < prim.animation->frames.size(); ++i)
        add_remaining(prim.animation->frames[i].record_name);
}

void collect_phase_initial(
    const std::vector<adventure_render::PreparedAdventureRenderPrimitive>& phase,
    std::set<std::string>& seen_ids, std::vector<ImageAssetKey>& keys) {
    for (const auto& prim : phase)
        collect_primitive_initial_assets(prim, seen_ids, keys);
}

void collect_phase_remaining(
    const std::vector<adventure_render::PreparedAdventureRenderPrimitive>& phase,
    std::set<std::string>& initial_ids, std::vector<ImageAssetKey>& keys) {
    for (const auto& prim : phase)
        collect_primitive_remaining_animation_assets(prim, initial_ids, keys);
}

} // namespace

std::vector<ImageAssetKey>
collect_adventure_render_asset_keys(const adventure_render::PreparedAdventureRenderGraph& graph) {
    std::set<std::string>      seen_ids;
    std::vector<ImageAssetKey> keys;

    auto collect_phase =
        [&](const std::vector<adventure_render::PreparedAdventureRenderPrimitive>& phase) {
            for (const auto& prim : phase)
                collect_primitive_assets(prim, seen_ids, keys);
        };

    collect_phase(graph.ground_overlay);
    collect_phase(graph.world);
    collect_phase(graph.world_overlay);
    collect_phase(graph.fog);
    collect_phase(graph.ui_overlay);

    return keys;
}

std::vector<ImageAssetKey>
collect_adventure_initial_asset_keys(const adventure_render::PreparedAdventureRenderGraph& graph) {
    std::set<std::string>      seen_ids;
    std::vector<ImageAssetKey> keys;

    collect_phase_initial(graph.ground_overlay, seen_ids, keys);
    collect_phase_initial(graph.world, seen_ids, keys);
    collect_phase_initial(graph.world_overlay, seen_ids, keys);
    collect_phase_initial(graph.fog, seen_ids, keys);
    collect_phase_initial(graph.ui_overlay, seen_ids, keys);

    return keys;
}

std::vector<ImageAssetKey> collect_adventure_remaining_animation_asset_keys(
    const adventure_render::PreparedAdventureRenderGraph& graph) {
    // Pre-populate initial_ids with all static + frame-0 keys so they are
    // excluded from the remaining set.
    std::set<std::string>      initial_ids;
    std::vector<ImageAssetKey> keys;

    auto seed_initial =
        [&](const std::vector<adventure_render::PreparedAdventureRenderPrimitive>& phase) {
            std::vector<ImageAssetKey> dummy;
            collect_phase_initial(phase, initial_ids, dummy);
        };
    seed_initial(graph.ground_overlay);
    seed_initial(graph.world);
    seed_initial(graph.world_overlay);
    seed_initial(graph.fog);
    seed_initial(graph.ui_overlay);

    collect_phase_remaining(graph.ground_overlay, initial_ids, keys);
    collect_phase_remaining(graph.world, initial_ids, keys);
    collect_phase_remaining(graph.world_overlay, initial_ids, keys);
    collect_phase_remaining(graph.fog, initial_ids, keys);
    collect_phase_remaining(graph.ui_overlay, initial_ids, keys);

    return keys;
}

} // namespace d2engine
