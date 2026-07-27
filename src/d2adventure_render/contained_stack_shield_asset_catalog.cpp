#include "contained_stack_shield_asset_catalog.hpp"

#include <algorithm>
#include <array>
#include <cctype>

namespace d2engine::adventure_render {

namespace {

[[nodiscard]] std::string upper_ascii(std::string_view value) {
    std::string result(value);
    std::ranges::transform(result, result.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    });
    return result;
}

[[nodiscard]] std::string settlement_name(d2runtime::AdventureSettlementKind settlement_kind) {
    return settlement_kind == d2runtime::AdventureSettlementKind::Village ? "Village" : "Capital";
}

[[nodiscard]] std::string expected_outer_logical_name(std::string_view                   race_id,
                                                      d2runtime::AdventureSettlementKind kind) {
    const auto race = upper_ascii(race_id);
    if (race == "G000RR0004") {
        if (kind != d2runtime::AdventureSettlementKind::Village) {
            return {};
        }
        return "G000RR8888SHLV8";
    }
    if (race == "G000RR0000" || race == "G000RR0001" || race == "G000RR0002" ||
        race == "G000RR0003" || race == "G000RR0005") {
        return race + (kind == d2runtime::AdventureSettlementKind::Village ? "SHLV8" : "SHLC8");
    }
    return {};
}

} // namespace

const ContainedStackShieldAsset& ContainedStackShieldAssetCatalog::resolve(
    std::string_view race_id, d2runtime::AdventureSettlementKind settlement_kind) const {
    const auto race = upper_ascii(race_id);
    const auto kind = settlement_name(settlement_kind);
    const auto target_outer_logical_name = expected_outer_logical_name(race, settlement_kind);

    if (target_outer_logical_name.empty()) {
        throw std::runtime_error("contained_stack_shield_unsupported race=" + race +
                                 " settlement=" + kind);
    }

    for (const auto& asset : assets) {
        if (asset.outer_logical_name == target_outer_logical_name) {
            return asset;
        }
    }

    throw std::runtime_error("contained_stack_shield_unsupported race=" + race +
                             " settlement=" + kind);
}

} // namespace d2engine::adventure_render
