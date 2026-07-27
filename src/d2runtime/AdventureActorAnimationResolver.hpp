#pragma once

#include "AdventureIsoDirection.hpp"
#include "AdventureStackPresentationResolver.hpp"

#include <optional>
#include <string>

namespace d2runtime {

enum class AdventureActorAnimationRole : uint8_t { Idle, Move };

enum class AdventureActorAnimationLayer : std::uint8_t {
    Main,
    Shadow,
};

struct AdventureAnimationIdentity {
    std::string container_path;
    std::string logical_animation_name;
};

class AdventureActorAnimationResolver {
public:
    [[nodiscard]] std::optional<AdventureAnimationIdentity>
    resolve(const AdventureActorPresentation& presentation, AdventureActorAnimationRole role,
            AdventureActorAnimationLayer layer, const std::string& unit_type_id,
            const std::string& race_id, AdventureIsoDirection direction) const;
};

} // namespace d2runtime
