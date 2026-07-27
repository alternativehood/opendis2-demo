#pragma once

#include <string>
#include <unordered_set>

namespace d2engine {

struct LayerTriple {
    std::string s1; // empty = absent
    std::string a1;
    std::string a2;
};

class AnimationRoleResolver {
public:
    explicit AnimationRoleResolver(std::unordered_set<std::string> known);

    [[nodiscard]] std::string resolve(const std::string& unit_type, const std::string& role_suffix,
                                      char direction) const;

    [[nodiscard]] LayerTriple resolve_layers(const std::string& unit_type,
                                             const std::string& role_suffix, char direction) const;

private:
    std::unordered_set<std::string> known_;
};

} // namespace d2engine
