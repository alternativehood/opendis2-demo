#include "animation_role_resolver.hpp"

#include "animation_role.hpp"

namespace d2engine {

AnimationRoleResolver::AnimationRoleResolver(std::unordered_set<std::string> known)
    : known_(std::move(known)) {}

std::string AnimationRoleResolver::resolve(const std::string& unit_type,
                                           const std::string& role_suffix, char direction) const {
    auto name = AnimationNames::build(unit_type, role_suffix, 1, direction);
    if (known_.contains(name)) {
        return name;
    }
    // Defender falls back to attacker variant
    if (direction == 'D') {
        auto fallback = AnimationNames::build(unit_type, role_suffix, 1, 'A');
        if (known_.contains(fallback)) {
            return fallback;
        }
    }
    // Some roles (TUCHA, HEFFA) ship a direction-agnostic 'B' variant
    auto both = AnimationNames::build(unit_type, role_suffix, 1, 'B');
    if (known_.contains(both)) {
        return both;
    }
    return {};
}

LayerTriple AnimationRoleResolver::resolve_layers(const std::string& unit_type,
                                                  const std::string& role_suffix,
                                                  char               direction) const {
    LayerTriple result;

    // A1: existing resolve with variant 1
    result.a1 = resolve(unit_type, role_suffix, direction);

    // S1: replace trailing 'A' with 'S' in role_suffix
    if (!role_suffix.empty() && role_suffix.back() == 'A') {
        const std::string s1_suffix = role_suffix.substr(0, role_suffix.size() - 1) + 'S';
        result.s1 = resolve(unit_type, s1_suffix, direction);
    }

    // A2: same role_suffix but variant 2, same D→A→B fallback as A1/S1
    auto a2_name = AnimationNames::build(unit_type, role_suffix, 2, direction);
    if (known_.contains(a2_name)) {
        result.a2 = a2_name;
    } else {
        if (direction == 'D') {
            auto a2_a = AnimationNames::build(unit_type, role_suffix, 2, 'A');
            if (known_.contains(a2_a)) {
                result.a2 = a2_a;
                return result;
            }
        }
        auto a2_b = AnimationNames::build(unit_type, role_suffix, 2, 'B');
        if (known_.contains(a2_b)) {
            result.a2 = a2_b;
        }
    }

    return result;
}

} // namespace d2engine
