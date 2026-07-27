#include "battle_startup_texture_warmup.hpp"

#include "battle_animation_scripts.hpp"
#include "portrait_render_item.hpp"
#include "battle_scenario_data.hpp"
#include "battle_scenario_executor.hpp"
#include "battle_scenario_runtime.hpp"
#include "battle_visual_event.hpp"
#include "visual_effect_resolver.hpp"
#include "visual_visibility.hpp"

#include <algorithm>
#include <cstdint>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

namespace d2engine {
namespace {

constexpr std::size_t kNearFutureWarmupMs = 1000;

ImageAssetKey make_key(std::string container_path, std::string image_name) {
    return {.container_path = std::move(container_path),
            .image_name = std::move(image_name),
            .kind = ImageAssetKind::ComposedSprite,
            .postprocess = ImagePostprocess::DetectMagentaBorder};
}

void add_key(std::vector<BattleWarmupEntry>& entries, std::set<std::string>& seen,
             std::string container_path, std::string image_name, WarmupBucket bucket) {
    if (container_path.empty() || image_name.empty()) {
        return;
    }
    const std::string id =
        container_path + '\n' + image_name + '\n' + std::to_string(static_cast<int>(bucket));
    if (seen.insert(id).second) {
        entries.push_back(
            {.key = make_key(std::move(container_path), std::move(image_name)), .bucket = bucket});
    }
}

void add_texture(std::vector<BattleWarmupEntry>& entries, std::set<std::string>& seen,
                 WarmupBucket bucket, const std::string& container_path,
                 const std::string& image_name) {
    add_key(entries, seen, container_path, image_name, bucket);
}

void add_sequence(std::vector<BattleWarmupEntry>& entries, std::set<std::string>& seen,
                  const AnimationSequence& sequence, WarmupBucket first_frame_bucket) {
    std::size_t elapsed_ms = 0;
    for (std::size_t i = 0; i < sequence.frames.size(); ++i) {
        const auto&  frame = sequence.frames[i];
        WarmupBucket bucket = WarmupBucket::Later;
        if (i == 0 && first_frame_bucket == WarmupBucket::Critical) {
            bucket = WarmupBucket::Critical;
        } else if (elapsed_ms < kNearFutureWarmupMs) {
            bucket = WarmupBucket::NearFuture;
        }
        add_texture(entries, seen, bucket, sequence.container_path, frame.image_name);
        elapsed_ms += frame.duration_ms;
    }
}

void add_clip(std::vector<BattleWarmupEntry>& entries, std::set<std::string>& seen,
              const ClipPayload& clip, WarmupBucket first_frame_bucket) {
    if (const auto* sequence = std::get_if<AnimationSequence>(&clip); sequence != nullptr) {
        add_sequence(entries, seen, *sequence, first_frame_bucket);
    } else if (const auto* layered_ptr = std::get_if<const LayeredAnimationClip*>(&clip);
               layered_ptr != nullptr && *layered_ptr != nullptr) {
        const LayeredAnimationClip& layered = **layered_ptr;
        for (auto slot : LayeredAnimationClip::kDrawOrder) {
            const auto* seq = layer_sequence(layered, slot);
            if (seq != nullptr && !seq->frames.empty()) {
                add_sequence(entries, seen, *seq, first_frame_bucket);
            }
        }
    }
}

void add_command(std::vector<BattleWarmupEntry>& entries, std::set<std::string>& seen,
                 const CommandNode& node, WarmupBucket first_frame_bucket);

void add_visual_command(std::vector<BattleWarmupEntry>& entries, std::set<std::string>& seen,
                        const VisualCommand& command, WarmupBucket first_frame_bucket) {
    std::visit(
        [&](const auto& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, PlayClip>) {
                add_clip(entries, seen, value.clip, first_frame_bucket);
            } else if constexpr (std::is_same_v<T, SpawnEffect>) {
                if (value.track.layered_player.clip != nullptr &&
                    !value.track.layered_player.clip->is_empty()) {
                    for (auto slot : LayeredAnimationClip::kDrawOrder) {
                        const auto* seq = layer_sequence(*value.track.layered_player.clip, slot);
                        if (seq != nullptr && !seq->frames.empty()) {
                            add_sequence(entries, seen, *seq, first_frame_bucket);
                        }
                    }
                } else {
                    add_sequence(entries, seen, value.track.player.sequence(), first_frame_bucket);
                }
            }
        },
        command);
}

void add_command(std::vector<BattleWarmupEntry>& entries, std::set<std::string>& seen,
                 const CommandNode& node, WarmupBucket first_frame_bucket) {
    std::visit(
        [&](const auto& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, VisualCommand>) {
                add_visual_command(entries, seen, value, first_frame_bucket);
            } else if constexpr (std::is_same_v<T, Sequence> || std::is_same_v<T, Parallel>) {
                for (const auto& child : value.children) {
                    add_command(entries, seen, child, first_frame_bucket);
                }
            }
        },
        node.value);
}

void add_scenario_assets(std::vector<BattleWarmupEntry>& entries, std::set<std::string>& seen,
                         const BattleScenarioWarmupInput& input,
                         const BattleScriptContext*       script_context) {
    if (script_context == nullptr) {
        return;
    }
    if (input.scenario == nullptr) {
        return;
    }
    if (input.runtime == nullptr) {
        return;
    }

    const auto seq_it = std::ranges::find_if(
        input.scenario->sequences, [&](const auto& s) { return s.id == input.sequence_id; });
    if (seq_it == input.scenario->sequences.end()) {
        return;
    }
    const auto& sequence = *seq_it;

    VisualEffectResolver const resolver(script_context->scene,
                                        script_context->assets.unit_profiles);

    for (std::size_t i = input.start_step_index; i < sequence.steps.size(); ++i) {
        const bool first_unplayed_step = (i == input.start_step_index);
        for (const auto& envelope : sequence.steps[i].envelopes) {
            if (std::holds_alternative<ScenarioUnitCreated>(envelope.event) ||
                std::holds_alternative<ScenarioUnitRetreated>(envelope.event)) {
                continue;
            }

            try {
                const auto visual_event = BattleScenarioExecutor::resolve_event(
                    envelope.event, *input.runtime, envelope.id, sequence.steps[i].id,
                    input.sequence_id, "");

                const auto expanded = resolver.expand(visual_event);
                for (const auto& ev : expanded) {
                    const auto command = BattleAnimationScripts::build(ev, *script_context);
                    if (command.has_value()) {
                        add_command(entries, seen, *command,
                                    first_unplayed_step ? WarmupBucket::Critical
                                                        : WarmupBucket::NearFuture);
                    }
                }
            } catch (const std::exception& e) {
                // Logged elsewhere; skip failing events gracefully
                static_cast<void>(e);
            }
        }
    }
}

} // namespace

BattleWarmupPlan build_battle_startup_texture_warmup_plan(
    const BattleRenderSnapshot& snapshot, const BattleRenderOptions& options,
    const BattleScriptContext* script_context, const BattleScenarioWarmupInput* scenario_input) {
    BattleWarmupPlan      plan;
    std::set<std::string> seen;

    if (options.draw_background) {
        add_texture(plan.entries, seen, WarmupBucket::Critical, options.ground_container,
                    options.ground_image);
        add_texture(plan.entries, seen, WarmupBucket::Critical, options.terrain_container,
                    options.terrain_image);
        for (const auto& overlay : options.terrain_overlay_images) {
            add_texture(plan.entries, seen, WarmupBucket::Critical, options.terrain_container,
                        overlay);
        }
        if (options.terrain_image.empty()) {
            plan.diagnostics.emplace_back("missing required battle background image");
        }
        if (options.ground_image.empty()) {
            plan.diagnostics.emplace_back("missing required ground background image");
        }
    }
    if (options.draw_frame) {
        add_texture(plan.entries, seen, WarmupBucket::Critical, options.frame_container,
                    options.frame_image);
        if (options.frame_image.empty()) {
            plan.diagnostics.emplace_back("missing required battle frame image");
        }
    }
    if (options.draw_unit_groups) {
        add_texture(plan.entries, seen, WarmupBucket::Critical, options.frame_container,
                    options.left_unit_group_image);
        add_texture(plan.entries, seen, WarmupBucket::Critical, options.frame_container,
                    options.right_unit_group_image);
        if (options.tree_layout != nullptr) {
            for (const auto& [path, node] : options.tree_layout->entries()) {
                if (node.asset.has_value() && path.starts_with("/ui/")) {
                    add_texture(plan.entries, seen, WarmupBucket::Critical, options.frame_container,
                                *node.asset);
                }
            }
        }
        add_texture(plan.entries, seen, WarmupBucket::Critical, kDeadMaskContainer,
                    kDeadMaskLargeName);
        add_texture(plan.entries, seen, WarmupBucket::Critical, kDeadMaskContainer,
                    kDeadMaskSmallName);
    }

    for (const auto& entity : snapshot.entities) {
        for (const auto& track : entity.tracks) {
            if (track.visibility != TrackVisibility::Visible) {
                continue;
            }
            // Current frame is critical (needed immediately for first render)
            add_texture(plan.entries, seen, WarmupBucket::Critical, track.container_path,
                        track.current_frame_name);
            // Full animation sequence covers track progression during playback
            if (!track.sequence.frames.empty()) {
                add_sequence(plan.entries, seen, track.sequence, WarmupBucket::NearFuture);
            }
        }
    }

    if (scenario_input != nullptr) {
        add_scenario_assets(plan.entries, seen, *scenario_input, script_context);
    }
    return plan;
}

} // namespace d2engine
