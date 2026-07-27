#pragma once

#include "battle_ids.hpp"
#include "battle_visual_profile_registry.hpp"
#include "battle_visual_step.hpp"
#include "battle_visual_step_execution.hpp"
#include "unit_animation_role_set.hpp"
#include "unit_lifecycle_visual_profile.hpp"

#include <string>

namespace d2engine {

class IBattlePresentationSink {
public:
    IBattlePresentationSink() = default;
    virtual ~IBattlePresentationSink() = default;

    IBattlePresentationSink(const IBattlePresentationSink&) = delete;
    IBattlePresentationSink& operator=(const IBattlePresentationSink&) = delete;
    IBattlePresentationSink(IBattlePresentationSink&&) = delete;
    IBattlePresentationSink& operator=(IBattlePresentationSink&&) = delete;

    virtual UnitVisualProfileId add_visual_profile(UnitAnimationRoleSet roles) = 0;
    virtual UnitLifecycleVisualProfileId
    add_lifecycle_profile(std::string name, UnitLifecycleVisualProfile profile) = 0;
    [[nodiscard]] virtual const UnitVisualProfileRegistry& unit_profiles() const = 0;

    virtual void on_unit_created(UnitInstanceId unit_id) = 0;
    virtual void on_unit_retreated(UnitInstanceId unit_id) = 0;

    virtual BattleVisualStepExecution submit_visual_step(const BattleVisualStep& step) = 0;
    virtual void                      finish_visual_step() = 0;
    virtual void                      cancel_visual_step() = 0;
    [[nodiscard]] virtual bool        has_active_visual_step() const = 0;
    [[nodiscard]] virtual bool        visual_step_complete() const = 0;
    [[nodiscard]] virtual bool        visual_step_failed() const = 0;
    [[nodiscard]] virtual bool        animation_busy() const = 0;
    [[nodiscard]] virtual const BattleVisualStepExecution& visual_step_execution() const = 0;
};

} // namespace d2engine
