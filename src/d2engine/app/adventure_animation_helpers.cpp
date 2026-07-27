#include "adventure_animation_helpers.hpp"

#include <stdexcept>
#include <string>

namespace d2engine {

AnimationSequence
adventure_animation_data_to_sequence(std::string_view                                container_path,
                                     const adventure_render::AdventureAnimationData& anim_data) {
    AnimationSequence seq;
    seq.name = anim_data.animation_name;
    seq.container_path = std::string(container_path);
    seq.native_canvas_w = anim_data.native_canvas_w;
    seq.native_canvas_h = anim_data.native_canvas_h;
    seq.is_looping = anim_data.is_looping;
    seq.frames.reserve(anim_data.frames.size());
    for (const auto& f : anim_data.frames) {
        seq.frames.push_back({.image_name = f.record_name,
                              .index = seq.frames.size(),
                              .duration_ms = static_cast<std::size_t>(f.duration_ms)});
    }
    return seq;
}

adventure_render::StableRenderId
adventure_animation_player_id(const adventure_render::PreparedAdventureRenderPrimitive& primitive) {
    return primitive.animation_sync_source_id.value_or(primitive.stable_id);
}

AdventureAnimationPlayerMap
build_adventure_animation_players(const adventure_render::PreparedAdventureRenderGraph& graph) {
    using namespace adventure_render;

    AdventureAnimationPlayerMap players;

    // Collect all phase primitives into one view
    auto process_phase = [&](const std::vector<PreparedAdventureRenderPrimitive>& prims) {
        for (const auto& prim : prims) {
            if (!prim.animation.has_value())
                continue;

            if (!prim.animation_sync_source_id.has_value()) {
                // Pass 1: clock owner
                if (players.contains(prim.stable_id)) {
                    std::string msg = "duplicate_animation_player stable_id=";
                    msg += std::to_string(prim.stable_id);
                    msg += " label=" + prim.debug_label;
                    throw std::runtime_error(msg);
                }
                auto seq =
                    adventure_animation_data_to_sequence(prim.container_path, *prim.animation);
                AnimationPlayer player(std::move(seq));
                player.play();
                players.emplace(prim.stable_id, std::move(player));
            }
        }
    };

    process_phase(graph.ground_overlay);
    process_phase(graph.world);
    process_phase(graph.world_overlay);
    process_phase(graph.fog);
    process_phase(graph.ui_overlay);

    // Pass 2: synchronized followers
    auto process_followers = [&](const std::vector<PreparedAdventureRenderPrimitive>& prims) {
        for (const auto& prim : prims) {
            if (!prim.animation.has_value())
                continue;

            if (!prim.animation_sync_source_id.has_value())
                continue;

            const StableRenderId source_id = *prim.animation_sync_source_id;

            // Find the source primitive
            const PreparedAdventureRenderPrimitive* source_prim = nullptr;
            auto find_source = [&](const std::vector<PreparedAdventureRenderPrimitive>& p) {
                for (const auto& sp : p) {
                    if (sp.stable_id == source_id) {
                        return &sp;
                    }
                }
                return static_cast<const PreparedAdventureRenderPrimitive*>(nullptr);
            };
            source_prim = find_source(graph.ground_overlay);
            if (!source_prim)
                source_prim = find_source(graph.world);
            if (!source_prim)
                source_prim = find_source(graph.world_overlay);
            if (!source_prim)
                source_prim = find_source(graph.fog);
            if (!source_prim)
                source_prim = find_source(graph.ui_overlay);

            if (source_prim == nullptr) {
                std::string msg = "missing_sync_source stable_id=";
                msg += std::to_string(prim.stable_id);
                msg += " label=" + prim.debug_label;
                msg += " source_id=" + std::to_string(source_id);
                msg += " animation=" + prim.animation->animation_name;
                throw std::runtime_error(msg);
            }

            if (!source_prim->animation.has_value()) {
                std::string msg = "static_sync_source stable_id=";
                msg += std::to_string(prim.stable_id);
                msg += " label=" + prim.debug_label;
                msg += " source_id=" + std::to_string(source_id);
                msg += " source_no_animation";
                msg += " animation=" + prim.animation->animation_name;
                throw std::runtime_error(msg);
            }

            auto source_player_it = players.find(source_id);
            if (source_player_it == players.end()) {
                std::string msg = "missing_sync_source stable_id=";
                msg += std::to_string(prim.stable_id);
                msg += " label=" + prim.debug_label;
                msg += " source_id=" + std::to_string(source_id);
                msg += " animation=" + prim.animation->animation_name;
                throw std::runtime_error(msg);
            }

            // Validate synchronization topology
            const auto& src_anim = *source_prim->animation;
            const auto& fol_anim = *prim.animation;

            if (src_anim.frames.size() != fol_anim.frames.size()) {
                std::string msg = "sync_frame_count_mismatch follower_id=";
                msg += std::to_string(prim.stable_id);
                msg += " follower_label=" + prim.debug_label;
                msg += " follower_anim=" + fol_anim.animation_name;
                msg += " follower_frames=" + std::to_string(fol_anim.frames.size());
                msg += " source_id=" + std::to_string(source_id);
                msg += " source_label=" + source_prim->debug_label;
                msg += " source_anim=" + src_anim.animation_name;
                msg += " source_frames=" + std::to_string(src_anim.frames.size());
                throw std::runtime_error(msg);
            }

            if (src_anim.is_looping != fol_anim.is_looping) {
                std::string msg = "sync_looping_mismatch follower_id=";
                msg += std::to_string(prim.stable_id);
                msg += " follower_label=" + prim.debug_label;
                msg += " follower_anim=" + fol_anim.animation_name;
                msg += " source_id=" + std::to_string(source_id);
                msg += " source_label=" + source_prim->debug_label;
                msg += " source_anim=" + src_anim.animation_name;
                throw std::runtime_error(msg);
            }

            if (src_anim.timing_source != fol_anim.timing_source) {
                std::string msg = "sync_timing_source_mismatch follower_id=";
                msg += std::to_string(prim.stable_id);
                msg += " follower_label=" + prim.debug_label;
                msg += " follower_anim=" + fol_anim.animation_name;
                msg += " source_id=" + std::to_string(source_id);
                msg += " source_label=" + source_prim->debug_label;
                msg += " source_anim=" + src_anim.animation_name;
                throw std::runtime_error(msg);
            }

            for (std::size_t fi = 0; fi < src_anim.frames.size(); ++fi) {
                if (src_anim.frames[fi].duration_ms != fol_anim.frames[fi].duration_ms) {
                    std::string msg = "sync_duration_mismatch follower_id=";
                    msg += std::to_string(prim.stable_id);
                    msg += " follower_label=" + prim.debug_label;
                    msg += " follower_anim=" + fol_anim.animation_name;
                    msg += " frame=" + std::to_string(fi);
                    msg += " follower_duration=" + std::to_string(fol_anim.frames[fi].duration_ms);
                    msg += " source_id=" + std::to_string(source_id);
                    msg += " source_label=" + source_prim->debug_label;
                    msg += " source_anim=" + src_anim.animation_name;
                    msg += " source_duration=" + std::to_string(src_anim.frames[fi].duration_ms);
                    throw std::runtime_error(msg);
                }
            }
        }
    };

    process_followers(graph.ground_overlay);
    process_followers(graph.world);
    process_followers(graph.world_overlay);
    process_followers(graph.fog);
    process_followers(graph.ui_overlay);

    return players;
}

} // namespace d2engine
