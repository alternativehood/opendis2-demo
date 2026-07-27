#pragma once
#include "layered_animation_clip.hpp"
#include <string>
#include <utility>

namespace d2engine {

// Extract the direction character from a resolved animation name.
// Names have the format: G000<unit_type><role_suffix><variant><direction>00
// The direction character is at position name.size() - 3.
[[nodiscard]] inline char extract_direction_from_name(const std::string& name) {
    return name.size() >= 3 ? name[name.size() - 3] : '\0';
}

struct BattleEffectClip {
    LayeredAnimationClip clip;
    std::string          family;
    char                 direction_or_variant = 'A'; // actual resolved direction after fallback
    char                 requested_direction = 'A';  // original direction requested

    [[nodiscard]] bool is_empty() const noexcept { return clip.is_empty(); }

    // suspicious = S1 present, A1 absent, family=="HEFF" (Erhog case)
    [[nodiscard]] bool is_suspicious() const noexcept {
        return clip.s1.has_value() && !clip.a1.has_value() && family == "HEFF";
    }
};

} // namespace d2engine
