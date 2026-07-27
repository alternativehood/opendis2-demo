#pragma once

#include "battle_animation_engine.hpp"
#include "battle_debug_scene_controller.hpp"
#include "battle_effect_role_set.hpp"
#include "battle_scene.hpp"
#include "battle_selection_controller.hpp"
#include "battle_visual_profile_registry.hpp"
#include "battle_visual_step.hpp"
#include "battle_visual_step_execution.hpp"
#include "debug_battle_outcome_resolver.hpp"
#include "i_battle_presentation_sink.hpp"
#include "unit_animation_role_set.hpp"
#include "unit_lifecycle_visual_profile.hpp"

#include "../animation/animation_sequence.hpp"

#include <vector>

namespace d2engine {

struct UnitPresenterState {
    enum class Role : std::uint8_t { Idle, Hit, FadingOut, Dead, Reviving };
    UnitInstanceId unit_id;
    Role           role = Role::Idle;
    bool           size_small = true;
};

class BattlePresenter : public IBattlePresentationSink {
public:
    BattlePresenter(BattleScene& scene, UnitVisualProfileRegistry unit_profiles,
                    UnitLifecycleVisualProfileRegistry lifecycle_profiles,
                    BattleEffectRoleSet effects, std::size_t attacker_count);

    explicit BattlePresenter(BattleScene& scene, UnitVisualProfileRegistry unit_profiles,
                             UnitLifecycleVisualProfileRegistry lifecycle_profiles,
                             std::size_t attacker_count, BattleEffectRoleSet effects = {});

    void update(float delta_ms);
    // Domain-level action methods (called by app layer; no keyboard enum here)
    void                      trigger_attack();
    void                      cycle_actor();
    void                      cycle_target();
    void                      preview_role_idle();
    void                      preview_role_hit();
    void                      preview_role_death();
    void                      preview_role_attack();
    void                      trigger_heff();
    void                      trigger_tuch();
    void                      step_debug_frame(int delta);
    void                      submit_visual_event(BattleVisualEvent event);
    BattleVisualStepExecution submit_visual_step(const BattleVisualStep& step) override;
    void                      finish_visual_step() override;
    void                      cancel_visual_step() override;
    void                      set_marker_small_animation(AnimationSequence marker);
    void                      set_marker_large_animation(AnimationSequence marker);
    [[nodiscard]] const AnimationSequence* marker_animation() const noexcept {
        return has_marker_large_animation_ ? &marker_large_animation_ : nullptr;
    }
    [[nodiscard]] const AnimationSequence* marker_small_animation() const noexcept {
        return has_marker_small_animation_ ? &marker_small_animation_ : nullptr;
    }
    [[nodiscard]] const AnimationSequence* marker_large_animation() const noexcept {
        return has_marker_large_animation_ ? &marker_large_animation_ : nullptr;
    }
    void                         update_sequence_delays(std::map<std::string, int> delays);
    [[nodiscard]] UnitInstanceId selected_unit_id() const noexcept {
        return selection_.actor.value_or(UnitInstanceId{});
    }
    [[nodiscard]] UnitInstanceId selected_target_id() const noexcept {
        return selection_.target.value_or(UnitInstanceId{});
    }
    [[nodiscard]] VisualEntityId              selected_entity_id() const noexcept;
    [[nodiscard]] const UnitAnimationRoleSet* selected_roles() const noexcept;
    [[nodiscard]] BattleAnimationInspection   inspection() const { return engine_.inspection(); }
    [[nodiscard]] const BattleVisualStepExecution& visual_step_execution() const noexcept override {
        return step_execution_;
    }
    [[nodiscard]] bool has_active_visual_step() const noexcept override {
        return active_step_ != nullptr;
    }
    [[nodiscard]] bool visual_step_complete() const noexcept override;
    [[nodiscard]] bool visual_step_failed() const noexcept override {
        return step_execution_.failed;
    }
    [[nodiscard]] bool animation_busy() const noexcept override { return engine_.busy(); }

    // Dynamic unit lifecycle support (IBattlePresentationSink implementation)
    void                         on_unit_created(UnitInstanceId unit_id) override;
    void                         on_unit_retreated(UnitInstanceId unit_id) override;
    UnitVisualProfileId          add_visual_profile(UnitAnimationRoleSet roles) override;
    UnitLifecycleVisualProfileId add_lifecycle_profile(std::string                name,
                                                       UnitLifecycleVisualProfile profile) override;

    // Const access satisfies IBattlePresentationSink::unit_profiles()
    [[nodiscard]] const UnitVisualProfileRegistry& unit_profiles() const override {
        return unit_profiles_;
    }

    // Mutable accessors for app layer (not part of IBattlePresentationSink)
    UnitVisualProfileRegistry&          unit_profiles() { return unit_profiles_; }
    UnitLifecycleVisualProfileRegistry& lifecycle_profiles() { return lifecycle_profiles_; }
    BattleEffectRoleSet&                effects() { return effects_; }

private:
    [[nodiscard]] std::vector<BattleSelectableUnit> selectable_units() const;
    void preview_role_impl(const UnitStateClip UnitAnimationRoleSet::* field, bool looping);
    void apply_event_state(const BattleVisualEvent& event);
    void sync_states();
    void process_emitted_events();
    void fire_step_cue(const BattleVisualEvent& event);
    void start_step_envelopes(const std::vector<std::size_t>& indexes);

    BattleScene&                       scene_;
    UnitVisualProfileRegistry          unit_profiles_;
    UnitLifecycleVisualProfileRegistry lifecycle_profiles_;
    std::vector<UnitPresenterState>    states_;
    std::vector<UnitInstanceId>        unit_ids_;
    BattleEffectRoleSet                effects_;
    AnimationSequence                  marker_large_animation_;
    AnimationSequence                  marker_small_animation_;
    bool                               has_marker_large_animation_ = false;
    bool                               has_marker_small_animation_ = false;
    std::size_t                        attacker_count_ = 0;
    BattleSelectionModel               selection_;
    std::uint32_t                      next_attack_id_ = 1;
    BattleAnimationEngine              engine_;
    std::optional<BattleVisualStep>    active_step_storage_;
    const BattleVisualStep*            active_step_ = nullptr;
    BattleVisualStepExecution          step_execution_;
};

} // namespace d2engine
