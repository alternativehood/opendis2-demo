#pragma once

#include "MapCellCoord.hpp"

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace d2runtime {

enum class AdventureSiteKind : std::uint8_t {
    Mage = 0,
    Merchant = 1,
    Mercenary = 2,
    Trainer = 3,
};

struct AdventureMerchantSiteData {
    std::string              buy_armor;
    std::string              buy_jewel;
    std::string              buy_weapon;
    std::string              buy_banner;
    std::string              buy_potion;
    std::string              buy_scroll;
    std::string              buy_wand;
    int                      buy_value = 0;
    int                      declared_item_count = 0;
    std::vector<std::string> item_ids;
    std::vector<std::string> mission_ids;
};

struct AdventureMercenarySiteData {
    int                      declared_unit_count = 0;
    std::vector<std::string> unit_ids;
};

struct AdventureMageSiteData {
    int                      declared_spell_count = 0;
    std::vector<std::string> spell_ids;
};

struct AdventureTrainerSiteData {};

using AdventureSitePayload = std::variant<AdventureMerchantSiteData, AdventureMercenarySiteData,
                                          AdventureMageSiteData, AdventureTrainerSiteData>;

struct AdventureSite {
    std::string               id;
    AdventureSiteKind         kind = AdventureSiteKind::Mage;
    std::string               title;
    std::string               description;
    int                       image_iso = 0;
    int                       image_interface = 0;
    MapCellCoord              position;
    std::string               visitor_id;
    int                       ai_priority = 0;
    std::vector<MapCellCoord> footprint;
    AdventureSitePayload      payload;
};

} // namespace d2runtime
