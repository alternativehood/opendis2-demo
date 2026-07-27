#pragma once

#include "../animation/animation_sequence.hpp"
#include "battle_effect_clip.hpp"
#include "unit_state_clip.hpp"

#include <string_view>

namespace d2engine {

// Abstract interface for resolving semantic animation requests into clips.
// Hides raw container paths (e.g., "Imgs/Batunits.ff") from the battle engine core.
// Implementations may use RawResourceLoader, asset packages, or pre-built fake data.
class IAnimationCatalog {
public:
    IAnimationCatalog() = default;
    virtual ~IAnimationCatalog() = default;

    IAnimationCatalog(const IAnimationCatalog&) = delete;
    IAnimationCatalog& operator=(const IAnimationCatalog&) = delete;
    IAnimationCatalog(IAnimationCatalog&&) = delete;
    IAnimationCatalog& operator=(IAnimationCatalog&&) = delete;

    // Resolve a unit-specific animation clip (single A1 layer, legacy shim).
    // Returns an empty sequence (zero frames) if the clip is not available.
    [[nodiscard]] virtual AnimationSequence
    unit_clip(std::string_view unit_type, std::string_view role, char direction) const = 0;

    // Resolve a global battle effect clip (e.g., faction death, revive).
    // Returns an empty sequence (zero frames) if the effect is not available.
    [[nodiscard]] virtual AnimationSequence battle_effect(std::string_view role) const = 0;

    // Resolve a short image-frame sequence (e.g., DEAD_HUMAN_LA00/01) with OPT layout metadata.
    // Returns an empty sequence when the frames are not available.
    [[nodiscard]] virtual AnimationSequence image_sequence(std::string_view base_name) const {
        static_cast<void>(base_name);
        return {};
    }

    // Resolve all three layers (S1/A1/A2) for a unit state animation.
    // action is the base name without the trailing 'A' suffix (e.g., "IDLE", "HHIT", "HMOV",
    // "STIL").
    [[nodiscard]] virtual UnitStateClip unit_state_bundle(std::string_view unit_type,
                                                          std::string_view action,
                                                          char             direction) const = 0;

    // Resolve all three layers for a battle effect (HEFF, TUCH).
    // effect_family is e.g. "HEFF" or "TUCH".
    [[nodiscard]] virtual BattleEffectClip battle_effect_bundle(std::string_view unit_type,
                                                                std::string_view effect_family,
                                                                char direction) const = 0;
};

} // namespace d2engine
