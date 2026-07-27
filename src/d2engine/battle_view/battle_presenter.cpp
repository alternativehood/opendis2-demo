#include "battle_presenter.hpp"

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <variant>

namespace d2engine {
namespace {

bool commandless_success_event(const BattleVisualEvent& event) {
    return std::holds_alternative<ActorSelected>(event) ||
           std::holds_alternative<TargetSelected>(event) ||
           std::holds_alternative<TargetDamaged>(event) ||
           std::holds_alternative<AttackImpactCue>(event);
    // BattleEffectStarted removed — it has a real builder (build_battle_effect);
    // failed builds must not silently complete the envelope.
}

} // namespace

VisualEntityId BattlePresenter::selected_entity_id() const noexcept {
    return selection_.actor.has_value()
               ? scene_.visual_entity_for(*selection_.actor).value_or(VisualEntityId{})
               : VisualEntityId{};
}

const UnitAnimationRoleSet* BattlePresenter::selected_roles() const noexcept {
    const auto  entity_id = scene_.visual_entity_for(selected_unit_id());
    const auto* unit = entity_id.has_value() ? scene_.try_unit_by_id(*entity_id) : nullptr;
    return unit != nullptr ? unit_profiles_.roles(unit->visual_profile_id) : nullptr;
}

BattlePresenter::BattlePresenter(BattleScene& scene, UnitVisualProfileRegistry unit_profiles,
                                 UnitLifecycleVisualProfileRegistry lifecycle_profiles,
                                 BattleEffectRoleSet effects, std::size_t attacker_count)
    : scene_(scene), unit_profiles_(std::move(unit_profiles)),
      lifecycle_profiles_(std::move(lifecycle_profiles)), states_(scene_.units().size()),
      effects_(effects), attacker_count_(attacker_count),
      engine_(scene_, BattleScriptAssets{.unit_profiles = &unit_profiles_,
                                         .lifecycle_profiles = &lifecycle_profiles_,
                                         .effects = &effects_,
                                         .marker = &marker_large_animation_,
                                         .marker_small = &marker_small_animation_,
                                         .marker_large = &marker_large_animation_}) {
    unit_ids_.reserve(scene_.units().size());
    for (const auto& unit : scene_.units()) {
        unit_ids_.push_back(unit.unit_instance_id);
    }
    if (!unit_ids_.empty()) {
        selection_.actor = unit_ids_.front();
        selection_.target =
            attacker_count_ < unit_ids_.size() ? unit_ids_[attacker_count_] : unit_ids_.front();
    }
    for (std::size_t i = 0; i < states_.size(); ++i) {
        states_[i].unit_id = scene_.units()[i].unit_instance_id;
        states_[i].size_small = !scene_.units()[i].is_large;
    }
}

BattlePresenter::BattlePresenter(BattleScene& scene, UnitVisualProfileRegistry unit_profiles,
                                 UnitLifecycleVisualProfileRegistry lifecycle_profiles,
                                 std::size_t attacker_count, BattleEffectRoleSet effects)
    : BattlePresenter(scene, std::move(unit_profiles), std::move(lifecycle_profiles), effects,
                      attacker_count) {}

void BattlePresenter::apply_event_state(const BattleVisualEvent& event) {
    if (const auto* actor = std::get_if<ActorSelected>(&event); actor != nullptr) {
        selection_.actor = actor->selected;
    } else if (const auto* target = std::get_if<TargetSelected>(&event); target != nullptr) {
        selection_.target = target->selected;
    } else if (const auto* damaged = std::get_if<TargetDamaged>(&event);
               damaged != nullptr && damaged->current_hp.has_value()) {
        static_cast<void>(scene_.set_unit_hp(damaged->target, *damaged->current_hp));
    } else if (const auto* drained = std::get_if<LifeDrained>(&event); drained != nullptr) {
        if (drained->target_current_hp.has_value()) {
            static_cast<void>(scene_.set_unit_hp(drained->target, *drained->target_current_hp));
        }
        if (drained->source_current_hp.has_value()) {
            static_cast<void>(scene_.set_unit_hp(drained->source, *drained->source_current_hp));
        }
    } else if (const auto* healed = std::get_if<SourceHealed>(&event);
               healed != nullptr && healed->current_hp.has_value()) {
        static_cast<void>(scene_.set_unit_hp(healed->source, *healed->current_hp));
    } else if (const auto* target_killed = std::get_if<TargetKilled>(&event);
               target_killed != nullptr) {
        static_cast<void>(scene_.set_unit_hp(target_killed->target, 0));
    } else if (const auto* unit_killed = std::get_if<UnitKilled>(&event); unit_killed != nullptr) {
        static_cast<void>(scene_.set_unit_hp(unit_killed->target, 0));
    } else if (const auto* revived = std::get_if<UnitRevived>(&event);
               revived != nullptr && revived->current_hp.has_value()) {
        static_cast<void>(scene_.set_unit_hp(revived->target, *revived->current_hp));
    }
}

void BattlePresenter::submit_visual_event(BattleVisualEvent event) {
    apply_event_state(event);
    fire_step_cue(event);
    engine_.submit(std::move(event));
    engine_.update(0.0f);
    process_emitted_events();
    sync_states();
}

void BattlePresenter::start_step_envelopes(
    const std::vector<std::size_t>& indexes) { // NOLINT(misc-no-recursion)
    if (active_step_ == nullptr || indexes.empty()) {
        return;
    }
    std::vector<BattleVisualEvent> events;
    events.reserve(indexes.size());
    std::vector<std::size_t> ready;
    ready.reserve(indexes.size());
    for (const std::size_t index : indexes) {
        if (index >= step_execution_.envelope_states.size() ||
            step_execution_.envelope_states[index] !=
                BattleVisualStepExecution::EnvelopeState::Waiting) {
            continue;
        }
        const auto& envelope = active_step_->envelopes[index];
        apply_event_state(envelope.event);
        ready.push_back(index);
        events.push_back(envelope.event);
    }
    if (ready.empty()) {
        return;
    }
    const auto result = engine_.submit_batch(events);
    for (std::size_t i = 0; i < ready.size(); ++i) {
        const std::size_t index = ready[i];
        const auto&       envelope = active_step_->envelopes[index];
        std::erase(step_execution_.waiting_envelope_ids, envelope.id);
        if ((i < result.started.size() && result.started[i]) ||
            commandless_success_event(envelope.event)) {
            step_execution_.envelope_states[index] =
                BattleVisualStepExecution::EnvelopeState::Started;
            step_execution_.started_envelope_ids.push_back(envelope.id);
            continue;
        }
        if (envelope.optional) {
            step_execution_.envelope_states[index] =
                BattleVisualStepExecution::EnvelopeState::Skipped;
            step_execution_.skipped_optional_envelope_ids.push_back(envelope.id);
            step_execution_.diagnostics.push_back("optional envelope skipped: " + envelope.id);
        } else {
            step_execution_.envelope_states[index] =
                BattleVisualStepExecution::EnvelopeState::Failed;
            step_execution_.missing_required_envelope_ids.push_back(envelope.id);
            step_execution_.failed = true;
            step_execution_.diagnostics.push_back("missing required envelope asset: " +
                                                  envelope.id);
        }
    }
    process_emitted_events();
    sync_states();
}

BattleVisualStepExecution BattlePresenter::submit_visual_step(const BattleVisualStep& step) {
    active_step_storage_ = step;
    active_step_ = &*active_step_storage_;
    step_execution_ = {.step_id = step.id};
    step_execution_.envelope_states.assign(step.envelopes.size(),
                                           BattleVisualStepExecution::EnvelopeState::Waiting);
    std::vector<std::size_t> start_indexes;
    for (std::size_t i = 0; i < step.envelopes.size(); ++i) {
        const auto& envelope = step.envelopes[i];
        if (!envelope.at.event_id.has_value()) {
            start_indexes.push_back(i);
        } else {
            step_execution_.waiting_envelope_ids.push_back(envelope.id);
            step_execution_.diagnostics.push_back("waiting " + envelope.id + " for " +
                                                  *envelope.at.event_id + "." + envelope.at.cue);
            if (envelope.at.cue == "impact") {
                step_execution_.diagnostics.push_back(
                    "impact cue fallback: 50% attack duration for event " + *envelope.at.event_id);
            }
        }
    }
    start_step_envelopes(start_indexes);
    step_execution_.complete =
        !step_execution_.failed && step.complete == BattleVisualStepCompletion::Immediate;
    return step_execution_;
}

void BattlePresenter::finish_visual_step() {
    for (auto& state : step_execution_.envelope_states) {
        if (state == BattleVisualStepExecution::EnvelopeState::Started) {
            state = BattleVisualStepExecution::EnvelopeState::Completed;
        }
    }
    active_step_storage_.reset();
    active_step_ = nullptr;
    step_execution_.complete = true;
}

void BattlePresenter::cancel_visual_step() {
    active_step_storage_.reset();
    active_step_ = nullptr;
}

void BattlePresenter::update_sequence_delays(std::map<std::string, int> delays) {
    engine_.update_sequence_delays(std::move(delays));
}

void BattlePresenter::process_emitted_events() { // NOLINT(misc-no-recursion)
    for (;;) {
        auto emitted = engine_.drain_emitted_events();
        if (emitted.empty()) {
            return;
        }
        for (const auto& event : emitted) {
            fire_step_cue(event);
            if (active_step_ == nullptr) {
                for (auto resolved :
                     d2engine::DebugBattleOutcomeResolver::resolve(event, selection_)) {
                    engine_.submit(std::move(resolved));
                }
            }
        }
        engine_.update(0.0F);
    }
}

void BattlePresenter::fire_step_cue(const BattleVisualEvent& event) { // NOLINT(misc-no-recursion)
    if (active_step_ == nullptr) {
        return;
    }
    const auto* cue = std::get_if<AttackImpactCue>(&event);
    if (cue == nullptr) {
        return;
    }
    std::vector<std::size_t> ready;
    for (std::size_t i = 0; i < active_step_->envelopes.size(); ++i) {
        const auto& envelope = active_step_->envelopes[i];
        if (i >= step_execution_.envelope_states.size() ||
            step_execution_.envelope_states[i] !=
                BattleVisualStepExecution::EnvelopeState::Waiting ||
            !envelope.at.event_id.has_value() || envelope.at.cue != "impact") {
            continue;
        }
        const auto source =
            std::ranges::find_if(active_step_->envelopes, [&](const auto& candidate) {
                const auto* attack = std::get_if<AttackStarted>(&candidate.event);
                return candidate.id == *envelope.at.event_id && attack != nullptr &&
                       attack->attack_id == cue->attack_id;
            });
        if (source != active_step_->envelopes.end()) {
            ready.push_back(i);
        }
    }
    if (ready.empty()) {
        return;
    }
    step_execution_.fired_cues.emplace_back("impact");
    step_execution_.diagnostics.emplace_back("cue fired: impact");
    start_step_envelopes(ready);
}

bool BattlePresenter::visual_step_complete() const noexcept {
    if (step_execution_.failed) {
        return false;
    }
    if (step_execution_.complete) {
        return true;
    }
    return active_step_ != nullptr &&
           active_step_->complete == BattleVisualStepCompletion::AllRequiredTracksFinished &&
           step_execution_.missing_required_envelope_ids.empty() && !engine_.busy();
}

void BattlePresenter::on_unit_created(UnitInstanceId unit_id) {
    const bool was_empty = unit_ids_.empty();
    unit_ids_.push_back(unit_id);
    UnitPresenterState new_state{.unit_id = unit_id, .role = UnitPresenterState::Role::Idle};
    if (const auto eid = scene_.visual_entity_for(unit_id)) {
        if (const BattleUnit* u = scene_.try_unit_by_id(*eid)) {
            new_state.size_small = !u->is_large;
        }
    }
    states_.push_back(new_state);
    if (was_empty) {
        selection_.actor = unit_id;
        selection_.target = unit_id;
    } else if (!selection_.actor.has_value()) {
        selection_.actor = unit_id;
    }
}

void BattlePresenter::on_unit_retreated(UnitInstanceId unit_id) {
    std::erase(unit_ids_, unit_id);
    std::erase_if(states_, [unit_id](const UnitPresenterState& s) { return s.unit_id == unit_id; });
    if (selection_.actor == unit_id) {
        if (!unit_ids_.empty()) {
            selection_.actor = unit_ids_.front();
        } else {
            selection_.actor.reset();
        }
    }
    if (selection_.target == unit_id) {
        if (!unit_ids_.empty()) {
            selection_.target = unit_ids_.front();
        } else {
            selection_.target.reset();
        }
    }
}

UnitVisualProfileId BattlePresenter::add_visual_profile(UnitAnimationRoleSet roles) {
    return unit_profiles_.add(std::move(roles));
}

UnitLifecycleVisualProfileId
BattlePresenter::add_lifecycle_profile(std::string name, UnitLifecycleVisualProfile profile) {
    return lifecycle_profiles_.add(std::move(name), std::move(profile));
}

void BattlePresenter::set_marker_small_animation(AnimationSequence marker) {
    marker_small_animation_ = std::move(marker);
    marker_small_animation_.is_looping = true;
    has_marker_small_animation_ = true;
}

void BattlePresenter::set_marker_large_animation(AnimationSequence marker) {
    marker_large_animation_ = std::move(marker);
    marker_large_animation_.is_looping = true;
    has_marker_large_animation_ = true;
}

std::vector<BattleSelectableUnit> BattlePresenter::selectable_units() const {
    std::vector<BattleSelectableUnit> units;
    units.reserve(unit_ids_.size());
    for (const auto id : unit_ids_) {
        const auto  entity = scene_.visual_entity_for(id);
        const auto* unit = entity.has_value() ? scene_.try_unit_by_id(*entity) : nullptr;
        units.push_back(BattleSelectableUnit{
            .id = id, .selectable = unit != nullptr && unit->life_state != LifeVisualState::Dead});
    }
    return units;
}

// Private helper: shared preamble + clip application for preview_role_* methods.
void BattlePresenter::preview_role_impl(const UnitStateClip UnitAnimationRoleSet::* field,
                                        bool                                        looping) {
    const auto entity = scene_.visual_entity_for(selected_unit_id());
    if (!entity.has_value())
        return;
    const auto* unit = scene_.try_unit_by_id(*entity);
    if (unit == nullptr || unit->life_state != LifeVisualState::Alive)
        return;
    const auto* roles = selected_roles();
    if (roles == nullptr)
        return;
    const auto& clip = (roles->*field).clip;
    if (!clip.is_empty()) {
        static_cast<void>(
            scene_.replace_track_layered_clip(*entity, TrackKind::Base, clip, looping));
    }
}

void BattlePresenter::trigger_attack() {
    const auto  actor_id = selected_unit_id();
    const auto  target_id = selected_target_id();
    const auto  sel_entity = scene_.visual_entity_for(actor_id);
    const auto* sel = sel_entity.has_value() ? scene_.try_unit_by_id(*sel_entity) : nullptr;
    const bool  alive = sel != nullptr && sel->life_state == LifeVisualState::Alive;
    const auto  tgt_entity = scene_.visual_entity_for(target_id);
    const auto* tgt = tgt_entity.has_value() ? scene_.try_unit_by_id(*tgt_entity) : nullptr;
    if (!alive || tgt == nullptr || tgt->life_state != LifeVisualState::Alive || engine_.busy() ||
        selected_roles() == nullptr)
        return;
    submit_visual_event(AttackStarted{.attack_id = AttackInstanceId{next_attack_id_++},
                                      .source = actor_id,
                                      .targets = TargetSet::single(target_id)});
}

void BattlePresenter::cycle_actor() {
    const auto change =
        d2engine::BattleSelectionController::select_next_actor(selection_, selectable_units());
    if (change.actor.has_value()) {
        submit_visual_event(*change.actor);
    }
    if (change.target.has_value()) {
        submit_visual_event(*change.target);
    }
}

void BattlePresenter::cycle_target() {
    const auto event =
        d2engine::BattleSelectionController::select_next_target(selection_, selectable_units());
    if (event.has_value()) {
        submit_visual_event(*event);
    }
}

void BattlePresenter::preview_role_idle() {
    preview_role_impl(&UnitAnimationRoleSet::idle, true);
}
void BattlePresenter::preview_role_hit() {
    preview_role_impl(&UnitAnimationRoleSet::hit, false);
}
void BattlePresenter::preview_role_death() {
    preview_role_impl(&UnitAnimationRoleSet::death, false);
}
void BattlePresenter::preview_role_attack() {
    preview_role_impl(&UnitAnimationRoleSet::attack, false);
}

void BattlePresenter::trigger_heff() {
    const auto  actor_id = selected_unit_id();
    const auto  sel_entity = scene_.visual_entity_for(actor_id);
    const auto* sel = sel_entity.has_value() ? scene_.try_unit_by_id(*sel_entity) : nullptr;
    if (sel != nullptr && sel->life_state == LifeVisualState::Alive)
        submit_visual_event(CastEffectStarted{.caster = actor_id, .role = BattleEffectRole::Heff});
}

void BattlePresenter::trigger_tuch() {
    const auto  actor_id = selected_unit_id();
    const auto  sel_entity = scene_.visual_entity_for(actor_id);
    const auto* sel = sel_entity.has_value() ? scene_.try_unit_by_id(*sel_entity) : nullptr;
    if (sel != nullptr && sel->life_state == LifeVisualState::Alive)
        submit_visual_event(CastEffectStarted{.caster = actor_id, .role = BattleEffectRole::Tuch});
}

void BattlePresenter::step_debug_frame(int delta) {
    const auto entity = scene_.visual_entity_for(selected_unit_id());
    if (entity.has_value()) {
        static_cast<void>(d2engine::BattleDebugSceneController::step_frame(scene_, *entity, delta));
    }
}

void BattlePresenter::sync_states() {
    for (auto& state : states_) {
        const auto  entity_id = scene_.visual_entity_for(state.unit_id);
        const auto* unit = entity_id.has_value() ? scene_.try_unit_by_id(*entity_id) : nullptr;
        if (unit == nullptr) {
            continue;
        }
        switch (unit->life_state) {
        case LifeVisualState::Alive:
            state.role = UnitPresenterState::Role::Idle;
            break;
        case LifeVisualState::FadingOut:
            state.role = UnitPresenterState::Role::FadingOut;
            break;
        case LifeVisualState::Dead:
            state.role = UnitPresenterState::Role::Dead;
            break;
        case LifeVisualState::Reviving:
            state.role = UnitPresenterState::Role::Reviving;
            break;
        }
    }
}

void BattlePresenter::update(float delta_ms) {
    engine_.update(delta_ms);
    process_emitted_events();
    sync_states();
}

} // namespace d2engine
