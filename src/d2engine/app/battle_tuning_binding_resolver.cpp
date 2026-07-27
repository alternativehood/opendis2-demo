#include "../battle_view/battle_debug_binding.hpp"

#include "battle_tuning_state.hpp"

namespace d2engine {

bool BattleTuningBindingResolver::can_resolve(const TuningBinding& binding) const {
    return to_config_binding(binding).has_value();
}

std::optional<TunablePropertyValue>
BattleTuningBindingResolver::read_property(const TuningBinding& binding) const {
    const auto config = to_config_binding(binding);
    if (!config.has_value()) {
        return std::nullopt;
    }
    return state_.placement(*config);
}

bool BattleTuningBindingResolver::write_property(const TuningBinding&        binding,
                                                 const TunablePropertyValue& value) {
    const auto config = to_config_binding(binding);
    return config.has_value() && state_.set_placement(*config, value);
}

} // namespace d2engine
