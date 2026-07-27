#include "AdventureActorAnimationResolver.hpp"

#include <cctype>
#include <string>

namespace d2runtime {

namespace {

std::string upper(std::string_view s) {
    std::string out(s);
    for (auto& c : out)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return out;
}

std::string dir_suffix(AdventureIsoDirection d) {
    return std::to_string(static_cast<int>(d));
}

std::string boat_move_main_animation(const std::string& race_id, AdventureIsoDirection d) {
    return upper(race_id) + "BTMV" + dir_suffix(d);
}

std::string boat_move_shadow_animation(const std::string& race_id, AdventureIsoDirection d) {
    return upper(race_id) + "SBTM" + dir_suffix(d);
}

std::string boat_idle_main_animation(const std::string& race_id, AdventureIsoDirection d) {
    return upper(race_id) + "BOAT" + dir_suffix(d);
}

std::string boat_idle_shadow_animation(const std::string& race_id, AdventureIsoDirection d) {
    return upper(race_id) + "SBOA" + dir_suffix(d);
}

std::string unit_idle_main_animation(const std::string& unit_type_id, AdventureIsoDirection d) {
    return upper(unit_type_id) + "STOP" + dir_suffix(d);
}

std::string unit_idle_shadow_animation(const std::string& unit_type_id, AdventureIsoDirection d) {
    return upper(unit_type_id) + "SSTO" + dir_suffix(d);
}

std::string unit_move_main_animation(const std::string& unit_type_id, AdventureIsoDirection d) {
    return upper(unit_type_id) + "MOVE" + dir_suffix(d);
}

std::string unit_move_shadow_animation(const std::string& unit_type_id, AdventureIsoDirection d) {
    return upper(unit_type_id) + "SMOV" + dir_suffix(d);
}

} // namespace

std::optional<AdventureAnimationIdentity> AdventureActorAnimationResolver::resolve(
    const AdventureActorPresentation& presentation, AdventureActorAnimationRole role,
    AdventureActorAnimationLayer layer, const std::string& unit_type_id, const std::string& race_id,
    AdventureIsoDirection direction) const {
    constexpr std::string_view kContainer = "Imgs/Isounit.ff";

    if (presentation.kind == AdventureActorPresentationKind::Boat) {
        switch (role) {
        case AdventureActorAnimationRole::Idle:
            switch (layer) {
            case AdventureActorAnimationLayer::Main:
                return AdventureAnimationIdentity{.container_path = std::string(kContainer),
                                                  .logical_animation_name =
                                                      boat_idle_main_animation(race_id, direction)};
            case AdventureActorAnimationLayer::Shadow:
                return AdventureAnimationIdentity{
                    .container_path = std::string(kContainer),
                    .logical_animation_name = boat_idle_shadow_animation(race_id, direction)};
            }
            break;
        case AdventureActorAnimationRole::Move:
            switch (layer) {
            case AdventureActorAnimationLayer::Main:
                return AdventureAnimationIdentity{.container_path = std::string(kContainer),
                                                  .logical_animation_name =
                                                      boat_move_main_animation(race_id, direction)};
            case AdventureActorAnimationLayer::Shadow:
                return AdventureAnimationIdentity{
                    .container_path = std::string(kContainer),
                    .logical_animation_name = boat_move_shadow_animation(race_id, direction)};
            }
            break;
        }
        return std::nullopt;
    }

    switch (role) {
    case AdventureActorAnimationRole::Idle:
        switch (layer) {
        case AdventureActorAnimationLayer::Main:
            return AdventureAnimationIdentity{
                .container_path = std::string(kContainer),
                .logical_animation_name = unit_idle_main_animation(unit_type_id, direction)};
        case AdventureActorAnimationLayer::Shadow:
            return AdventureAnimationIdentity{
                .container_path = std::string(kContainer),
                .logical_animation_name = unit_idle_shadow_animation(unit_type_id, direction)};
        }
        break;
    case AdventureActorAnimationRole::Move:
        switch (layer) {
        case AdventureActorAnimationLayer::Main:
            return AdventureAnimationIdentity{
                .container_path = std::string(kContainer),
                .logical_animation_name = unit_move_main_animation(unit_type_id, direction)};
        case AdventureActorAnimationLayer::Shadow:
            return AdventureAnimationIdentity{
                .container_path = std::string(kContainer),
                .logical_animation_name = unit_move_shadow_animation(unit_type_id, direction)};
        }
        break;
    }
    return std::nullopt;
}

} // namespace d2runtime
