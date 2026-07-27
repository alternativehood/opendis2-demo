#pragma once

#include <d2adventure_render/adventure_render_types.hpp>

#include <cctype>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

namespace d2engine::adventure_render {

enum class CapitalVisualState {
    Active,
    Ruined,
};

struct AnimatedCapitalVisual {
    std::string            container_path;
    std::string            logical_animation_name;
    int                    canvas_foot_x = 0;
    int                    canvas_foot_y = 0;
    AdventureAnimationData animation_data;
};

inline std::string canonical_capital_race_id(std::string_view race_id) {
    std::string canonical(race_id);
    for (char& ch : canonical) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return canonical;
}

struct CapitalVisualSet {
    AnimatedCapitalVisual                active;
    std::optional<AnimatedCapitalVisual> ruined;
};

struct CapitalAssetCatalog {
    std::unordered_map<std::string, CapitalVisualSet> visuals;

    [[nodiscard]] const AnimatedCapitalVisual& resolve(std::string_view   race_id,
                                                       CapitalVisualState state) const {
        const auto key = canonical_capital_race_id(race_id);
        if (key.empty()) {
            throw std::runtime_error("capital asset mapping missing for empty race_id");
        }
        const auto it = visuals.find(key);
        if (it == visuals.end()) {
            throw std::runtime_error("capital asset mapping missing for race_id=" +
                                     std::string(race_id) + " canonical=" + key);
        }
        switch (state) {
        case CapitalVisualState::Active:
            return it->second.active;
        case CapitalVisualState::Ruined:
            if (!it->second.ruined.has_value()) {
                throw std::runtime_error(
                    "capital_ruined_visual_unavailable race_id=" + std::string(race_id) +
                    " canonical=" + key + " state=Ruined");
            }
            return *it->second.ruined;
        }
        throw std::runtime_error("capital asset mapping missing for race_id=" +
                                 std::string(race_id) + " canonical=" + key);
    }
};

} // namespace d2engine::adventure_render
