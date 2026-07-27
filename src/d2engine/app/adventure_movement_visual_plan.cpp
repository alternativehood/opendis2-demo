#include "adventure_movement_visual_plan.hpp"
#include "../assets/adventure_actor_visual_conversion.hpp"
#include "../assets/iso_actor_visual_resolver.hpp"
#include "../assets/image_asset_key.hpp"
#include "../assets/asset_runtime.hpp"
#include "adventure_interaction_mask.hpp"
#include "../render/render_asset_runtime.hpp"
#include <d2log/log.hpp>
#include <d2runtime/AdventureMovementDirection.hpp>
#include <algorithm>
#include <exception>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>
namespace d2engine {

namespace {
auto kLog = d2log::get("d2.movement"); // NOLINT(cert-err58-cpp)

void collect_layer(const AdventureMovementVisualPlan& plan, std::size_t segment_index,
                   std::string_view                                   layer_name,
                   const adventure_render::AdventureActorVisualLayer& layer,
                   std::vector<ImageAssetKey>&                        keys,
                   std::unordered_map<ImageAssetKey, std::string>&    contexts) {
    for (const auto& frame : layer.frames) {
        ImageAssetKey key{layer.container_path, frame.record_name, ImageAssetKind::ComposedSprite};
        keys.push_back(key);
        contexts.emplace(key,
                         "stack=" + plan.stack_id + " segment=" + std::to_string(segment_index) +
                             " animation=" + layer.logical_animation_name +
                             " layer=" + std::string(layer_name) +
                             " container=" + layer.container_path + " frame=" + frame.record_name);
    }
}

void collect_banner(const AdventureMovementVisualPlan& plan, std::vector<ImageAssetKey>& keys,
                    std::unordered_map<ImageAssetKey, std::string>& contexts) {
    const ImageAssetKey key{plan.banner_asset.container_path, plan.banner_asset.record_name,
                            ImageAssetKind::ComposedSprite};
    keys.push_back(key);
    contexts.emplace(key, "stack=" + plan.stack_id +
                              " banner_index=" + std::to_string(plan.banner_index) +
                              " container=" + plan.banner_asset.container_path +
                              " frame=" + plan.banner_asset.record_name);
}

bool image_key_less(const ImageAssetKey& lhs, const ImageAssetKey& rhs) {
    if (lhs.container_path != rhs.container_path)
        return lhs.container_path < rhs.container_path;
    if (lhs.image_name != rhs.image_name)
        return lhs.image_name < rhs.image_name;
    if (lhs.kind != rhs.kind)
        return lhs.kind < rhs.kind;
    return lhs.postprocess < rhs.postprocess;
}

void sort_unique(std::vector<ImageAssetKey>& keys) {
    std::sort(keys.begin(), keys.end(), image_key_less);
    keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
}

std::string result_context(const AdventureMovementVisualPlan& plan, const ImageAssetKey& key,
                           const std::unordered_map<ImageAssetKey, std::string>& contexts) {
    const auto it = contexts.find(key);
    if (it != contexts.end())
        return it->second;
    return "stack=" + plan.stack_id + " container=" + key.container_path +
           " frame=" + key.image_name;
}
} // namespace

void preload_adventure_movement_visual_plan(AdventureMovementVisualPlan& plan, AssetRuntime& assets,
                                            RenderAssetRuntime& render_assets) {
    if (plan.segments.empty())
        throw std::invalid_argument("movement_visual_preload_empty_plan stack=" + plan.stack_id);
    std::vector<ImageAssetKey>                     all_texture_keys;
    std::vector<ImageAssetKey>                     idle_body_decode_keys;
    std::vector<ImageAssetKey>                     ordinary_texture_keys;
    std::unordered_map<ImageAssetKey, std::string> contexts;
    for (std::size_t i = 0; i < plan.segments.size(); ++i) {
        const auto& segment = plan.segments[i];
        collect_layer(plan, i, "move_body", segment.move_visual.body, all_texture_keys, contexts);
        if (segment.move_visual.shadow)
            collect_layer(plan, i, "move_shadow", *segment.move_visual.shadow, all_texture_keys,
                          contexts);
        collect_layer(plan, i, "idle_body", segment.idle_visual_at_destination.body,
                      all_texture_keys, contexts);
        collect_layer(plan, i, "idle_body", segment.idle_visual_at_destination.body,
                      idle_body_decode_keys, contexts);
        if (segment.idle_visual_at_destination.shadow)
            collect_layer(plan, i, "idle_shadow", *segment.idle_visual_at_destination.shadow,
                          all_texture_keys, contexts);
    }
    collect_banner(plan, all_texture_keys, contexts);
    sort_unique(all_texture_keys);
    sort_unique(idle_body_decode_keys);
    if (all_texture_keys.empty() || idle_body_decode_keys.empty())
        throw std::runtime_error("movement_visual_preload_no_frames stack=" + plan.stack_id);
    const auto idle_gpu_cache_hits = static_cast<std::size_t>(std::count_if(
        idle_body_decode_keys.begin(), idle_body_decode_keys.end(),
        [&render_assets](const auto& key) { return render_assets.textures().is_cached(key); }));
    const auto idle_body_key_set = std::unordered_set<ImageAssetKey>(idle_body_decode_keys.begin(),
                                                                     idle_body_decode_keys.end());
    for (const auto& key : all_texture_keys) {
        if (!idle_body_key_set.contains(key))
            ordinary_texture_keys.push_back(key);
    }
    sort_unique(ordinary_texture_keys);

    auto idle_body_batch = assets.request_batch(idle_body_decode_keys, AssetPriority::Critical,
                                                "AdventureMovementActorIdleMasks");
    idle_body_batch.wait();
    if (idle_body_batch.handles().size() != idle_body_decode_keys.size())
        throw std::runtime_error("movement_visual_idle_decode_handle_count_mismatch stack=" +
                                 plan.stack_id);

    std::vector<PreparedImageResult> idle_body_results;
    idle_body_results.reserve(idle_body_batch.handles().size());
    std::unordered_set<ImageAssetKey> seen_idle_keys;
    std::vector<bool>                 idle_result_consumed;
    auto                              cleanup_idle_results = [&] {
        for (std::size_t i = 0; i < idle_body_results.size(); ++i) {
            if (i < idle_result_consumed.size() && idle_result_consumed[i])
                continue;
            auto& result = idle_body_results[i];
            if (!result.success || result.image == nullptr)
                continue;
            try {
                assets.release_prepared_if_matches(result.key, result.image);
            } catch (...) {
            }
            result.image.reset();
        }
    };
    try {
        for (const auto& handle : idle_body_batch.handles()) {
            auto       result = handle.get();
            const auto context = result_context(plan, result.key, contexts);
            if (!idle_body_key_set.contains(result.key))
                throw std::runtime_error("movement_visual_idle_decode_unexpected_key " + context);
            if (!seen_idle_keys.insert(result.key).second)
                throw std::runtime_error("movement_visual_idle_decode_duplicate_key " + context);
            if (!result.success || result.image == nullptr || result.image->pixels == nullptr)
                throw std::runtime_error("movement_visual_idle_decode_failed " + context +
                                         " error=" + result.error);
            idle_body_results.push_back(std::move(result));
        }

        kLog->debug("adventure_movement_idle_masks stack={} unique_idle_frames={} segments={} "
                    "gpu_cache_hits={} decoded={}",
                    plan.stack_id, idle_body_decode_keys.size(), plan.segments.size(),
                    idle_gpu_cache_hits, idle_body_results.size());

        idle_result_consumed.assign(idle_body_results.size(), false);
        for (std::size_t i = 0; i < plan.segments.size(); ++i) {
            auto& segment = plan.segments[i];
            segment.idle_interaction_mask_at_destination = build_actor_layer_interaction_mask(
                segment.idle_visual_at_destination.body, idle_body_results);
            if (!segment.idle_interaction_mask_at_destination)
                throw std::logic_error("movement_visual_preload_missing_idle_mask stack=" +
                                       plan.stack_id + " segment=" + std::to_string(i));
        }
        for (std::size_t i = 0; i < idle_body_results.size(); ++i) {
            const bool uploaded = render_assets.upload_one_and_release(idle_body_results[i]);
            idle_result_consumed[i] = true;
            if (!uploaded)
                throw std::runtime_error("movement_visual_idle_upload_failed " +
                                         result_context(plan, idle_body_results[i].key, contexts));
        }
    } catch (...) {
        const auto original = std::current_exception();
        cleanup_idle_results();
        std::rethrow_exception(original);
    }

    const auto ordinary_batch = render_assets.request_textures(
        ordinary_texture_keys, AssetPriority::Critical, "AdventureMovementActorVisuals");
    ordinary_batch.wait();
    const auto ordinary_upload = render_assets.upload_ready(ordinary_batch);
    if (ordinary_upload.failed != 0)
        throw std::runtime_error("movement_visual_preload_upload_failed stack=" + plan.stack_id);
    for (const auto& key : all_texture_keys) {
        if (render_assets.textures().find(key) == nullptr)
            throw std::runtime_error("movement_visual_preload_missing_cache " +
                                     result_context(plan, key, contexts));
    }
}

AdventureMovementVisualPlan AdventureMovementVisualPlanBuilder::build(
    const d2runtime::AdventureWorldState&     world,
    const d2game::AdventureMovementExecution& execution) const {
    const auto* stack = world.find_stack(execution.stack_id);
    if (!stack || !d2runtime::is_stack_on_adventure_map(*stack))
        throw std::runtime_error("movement_visual_missing_stack id=" + execution.stack_id);
    const auto* subrace = world.find_subrace(stack->subrace);
    if (subrace == nullptr)
        throw std::runtime_error("movement_visual_missing_subrace id=" + execution.stack_id);
    const auto& banner_asset = banner_catalog_->resolve_banner(subrace->banner);
    AdventureStackActorRequestResolver req(*game_data_);
    IsoActorVisualResolver             resolver(*catalog_, *game_data_);
    AdventureMovementVisualPlan        out{
        execution.stack_id, execution.route, banner_asset, subrace->banner, {}};
    for (std::size_t i = 0; i < execution.route.steps.size(); ++i) {
        const auto& step = execution.route.steps[i];
        const auto  from = i == 0 ? execution.route.start : execution.route.steps[i - 1].cell;
        const auto direction = d2runtime::adventure_direction_for_delta(step.delta_x, step.delta_y);
        auto       move_req = req.resolve_for(world, *stack, step.cell, direction,
                                              d2runtime::AdventureActorAnimationRole::Move);
        auto       idle_req = req.resolve_for(world, *stack, step.cell, direction,
                                              d2runtime::AdventureActorAnimationRole::Idle);
        const auto move = resolver.resolve(move_req);
        const auto idle = resolver.resolve(idle_req);
        if (!move || !idle)
            throw std::runtime_error("movement_visual_unresolved stack=" + execution.stack_id +
                                     " step=" + std::to_string(i));
        out.segments.push_back({i, from, step.cell, direction, to_adventure_actor_visual(*move),
                                to_adventure_actor_visual(*idle), nullptr});
    }
    return out;
}
} // namespace d2engine
