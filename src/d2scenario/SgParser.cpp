#include "SgParser.hpp"
#include "SgRecordReader.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace d2scenario {
namespace {

constexpr std::size_t kBlockDataSize = 128;
constexpr int         kTilesPerBlockX = 4;
constexpr int         kTilesPerBlockY = 8;

std::string cp1251_to_utf8(std::span<const uint8_t> src) {
    static const char32_t cp1251_table[128] = {
        0x0402, 0x0403, 0x201A, 0x0453, 0x201E, 0x2026, 0x2020, 0x2021, 0x20AC, 0x2030, 0x0409,
        0x2039, 0x040A, 0x040C, 0x040B, 0x040F, 0x0452, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022,
        0x2013, 0x2014, 0x0000, 0x2122, 0x0459, 0x203A, 0x045A, 0x045C, 0x045B, 0x045F, 0x00A0,
        0x040E, 0x045E, 0x0408, 0x00A4, 0x0490, 0x00A6, 0x00A7, 0x0401, 0x00A9, 0x0404, 0x00AB,
        0x00AC, 0x00AD, 0x00AE, 0x0407, 0x00B0, 0x00B1, 0x0406, 0x0456, 0x0491, 0x00B5, 0x00B6,
        0x00B7, 0x0451, 0x2116, 0x0454, 0x00BB, 0x0458, 0x0405, 0x0455, 0x0457, 0x0410, 0x0411,
        0x0412, 0x0413, 0x0414, 0x0415, 0x0416, 0x0417, 0x0418, 0x0419, 0x041A, 0x041B, 0x041C,
        0x041D, 0x041E, 0x041F, 0x0420, 0x0421, 0x0422, 0x0423, 0x0424, 0x0425, 0x0426, 0x0427,
        0x0428, 0x0429, 0x042A, 0x042B, 0x042C, 0x042D, 0x042E, 0x042F, 0x0430, 0x0431, 0x0432,
        0x0433, 0x0434, 0x0435, 0x0436, 0x0437, 0x0438, 0x0439, 0x043A, 0x043B, 0x043C, 0x043D,
        0x043E, 0x043F, 0x0440, 0x0441, 0x0442, 0x0443, 0x0444, 0x0445, 0x0446, 0x0447, 0x0448,
        0x0449, 0x044A, 0x044B, 0x044C, 0x044D, 0x044E, 0x044F,
    };

    std::string result;
    result.reserve(src.size() * 2);

    for (auto c : src) {
        if (c < 0x80) {
            result += static_cast<char>(c);
        } else {
            char32_t cp = cp1251_table[c - 0x80];
            if (cp < 0x80) {
                result += static_cast<char>(static_cast<uint8_t>(cp));
            } else if (cp < 0x800) {
                result += static_cast<char>(0xC0 | (cp >> 6));
                result += static_cast<char>(0x80 | (cp & 0x3F));
            } else {
                result += static_cast<char>(0xE0 | (cp >> 12));
                result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                result += static_cast<char>(0x80 | (cp & 0x3F));
            }
        }
    }
    return result;
}

constexpr std::array<uint8_t, 4> kWhat = {'W', 'H', 'A', 'T'};
constexpr std::array<uint8_t, 9> kEndObject = {'E', 'N', 'D', 'O', 'B', 'J', 'E', 'C', 'T'};
constexpr std::array<uint8_t, 9> kBegObject = {'B', 'E', 'G', 'O', 'B', 'J', 'E', 'C', 'T'};

bool starts_with_g000(const std::string& s) {
    return s.size() >= 4 && s[0] == 'G' && s[1] == '0' && s[2] == '0' && s[3] == '0';
}

std::string bytes_to_ascii(std::span<const char> bytes) {
    return std::string(bytes.begin(), bytes.end());
}

class MidUnitReader {
public:
    explicit MidUnitReader(const std::vector<uint8_t>& rec) : rec_(rec), limit_(rec.size()) {
        auto end_it = std::search(rec_.begin(), rec_.end(), kEndObject.begin(), kEndObject.end());
        if (end_it != rec_.end())
            limit_ = static_cast<std::size_t>(std::distance(rec_.begin(), end_it));

        SgRecordReader envelope(rec_);
        object_id_ = envelope.read_object_id();
        pos_ = body_start_after_envelope_id();
    }

    [[nodiscard]] SgMidUnitWire read() {
        SgMidUnitWire wire;
        wire.object_id = object_id_;

        require_remaining(1, "leading_zero");
        if (rec_[pos_] != 0) {
            throw std::runtime_error("MidUnit expected structural leading 0x00 byte");
        }
        ++pos_;

        require_marker("UNIT_ID");
        wire.unit_id = read_lp_string("UNIT_ID");

        require_marker("TYPE");
        wire.type_id = read_lp_string("TYPE");

        require_marker("LEVEL");
        wire.level = read_i32("LEVEL");

        require_remaining(10, "inner_unit_id");
        std::memcpy(wire.inner_unit_id.data(), rec_.data() + pos_, wire.inner_unit_id.size());
        pos_ += wire.inner_unit_id.size();

        const auto modifier_count = read_u32_raw("modifier_count");
        const auto remaining = limit_ - pos_;
        if (modifier_count > 1024 || modifier_count > remaining / 13) {
            throw std::runtime_error("MidUnit modifier_count impossible: " +
                                     std::to_string(modifier_count));
        }
        wire.modifier_ids.reserve(static_cast<std::size_t>(modifier_count));
        for (std::uint32_t i = 0; i < modifier_count; ++i) {
            require_marker("MODIF_ID");
            wire.modifier_ids.push_back(read_lp_string("MODIF_ID"));
        }

        require_marker("CREATION");
        wire.creation = read_i32("CREATION");

        require_marker("NAME_TXT");
        wire.name_text_raw = read_lp_payload("NAME_TXT");
        wire.name_text = SgParser::decode_cp1251(wire.name_text_raw);

        require_marker("TRANSF");
        wire.transformed = read_u8("TRANSF");

        if (at_marker("DYNLEVEL")) {
            require_marker("DYNLEVEL");
            wire.has_dynamic_level = true;
            wire.dynamic_level = read_u8("DYNLEVEL");
        }

        if (!at_marker("HP")) {
            if (wire.has_dynamic_level && pos_ + 3 <= limit_ && rec_[pos_] == 0 &&
                rec_[pos_ + 1] == 0 && rec_[pos_ + 2] == 0 && pos_ + 3 + 2 <= limit_ &&
                std::memcmp(rec_.data() + pos_ + 3, "HP", 2) == 0) {
                wire.pre_hp_padding = {0, 0, 0};
                pos_ += 3;
            } else {
                throw std::runtime_error("MidUnit has unknown bytes before HP");
            }
        }

        require_marker("HP");
        wire.hp = read_i32("HP");

        require_marker("XP");
        wire.xp = read_i32("XP");

        if (pos_ != limit_) {
            throw std::runtime_error("MidUnit has trailing bytes before ENDOBJECT");
        }

        return wire;
    }

private:
    const std::vector<uint8_t>& rec_;
    std::size_t                 pos_ = 0;
    std::size_t                 limit_ = 0;
    std::string                 object_id_;

    [[nodiscard]] std::size_t body_start_after_envelope_id() const {
        auto beg_it = std::search(rec_.begin(), rec_.end(), kBegObject.begin(), kBegObject.end());
        if (beg_it == rec_.end())
            return 0;
        return static_cast<std::size_t>(std::distance(rec_.begin(), beg_it)) + kBegObject.size();
    }

    [[nodiscard]] bool at_marker(std::string_view marker) const {
        return pos_ + marker.size() <= limit_ &&
               std::memcmp(rec_.data() + pos_, marker.data(), marker.size()) == 0;
    }

    void require_marker(std::string_view marker) {
        if (!at_marker(marker))
            throw std::runtime_error("MidUnit expected marker " + std::string(marker));
        pos_ += marker.size();
    }

    void require_remaining(std::size_t count, std::string_view what) const {
        if (pos_ + count > limit_)
            throw std::runtime_error("MidUnit truncated while reading " + std::string(what));
    }

    [[nodiscard]] std::uint32_t read_le32_raw(std::size_t offset) const {
        if (offset + 4 > limit_)
            throw std::runtime_error("MidUnit truncated while reading u32");
        return static_cast<std::uint32_t>(rec_[offset]) |
               (static_cast<std::uint32_t>(rec_[offset + 1]) << 8) |
               (static_cast<std::uint32_t>(rec_[offset + 2]) << 16) |
               (static_cast<std::uint32_t>(rec_[offset + 3]) << 24);
    }

    [[nodiscard]] std::uint32_t read_u32_raw(std::string_view what) {
        require_remaining(4, what);
        const auto value = read_le32_raw(pos_);
        pos_ += 4;
        return value;
    }

    [[nodiscard]] std::int32_t read_i32(std::string_view what) {
        return static_cast<std::int32_t>(read_u32_raw(what));
    }

    [[nodiscard]] std::uint8_t read_u8(std::string_view what) {
        require_remaining(1, what);
        return rec_[pos_++];
    }

    [[nodiscard]] std::string read_lp_string(std::string_view what) {
        return SgParser::decode_cp1251(read_lp_payload(what));
    }

    [[nodiscard]] std::vector<std::uint8_t> read_lp_payload(std::string_view what) {
        const auto len = read_u32_raw(what);
        if (len == 0 || len > 200000 || pos_ + static_cast<std::size_t>(len) > limit_)
            throw std::runtime_error("MidUnit invalid LP string length for " + std::string(what));
        std::span<const uint8_t> raw(rec_.data() + pos_, static_cast<std::size_t>(len));
        pos_ += static_cast<std::size_t>(len);
        if (raw.back() != 0) {
            throw std::runtime_error("MidUnit LP string lacks terminating NUL for " +
                                     std::string(what));
        }
        return {raw.begin(), raw.end()};
    }
};

SgMidUnitWire parse_mid_unit_wire_record(const std::vector<uint8_t>& rec) {
    return MidUnitReader(rec).read();
}

SgUnit to_semantic_unit(const SgMidUnitWire& wire) {
    SgUnit u;
    u.id = wire.unit_id;
    u.type_id = wire.type_id;
    u.level_raw_i32 = wire.level;
    u.modifier_ids = wire.modifier_ids;
    u.creation = wire.creation;
    u.name = wire.name_text;
    u.transformed = wire.transformed;
    if (wire.has_dynamic_level)
        u.dynamic_level = wire.dynamic_level;
    u.hp = wire.hp;
    u.xp = wire.xp;
    return u;
}

} // anonymous namespace

SgParser::SgParser(std::span<const uint8_t> data) : data_(data) {}

void SgParser::add_warning(const std::string& msg) {
    warnings_.push_back(msg);
}

void SgParser::validate_signature() {
    constexpr std::string_view kMidFileSignature = "MidFile";
    if (data_.size() < kMidFileSignature.size()) {
        throw std::runtime_error("File too small to contain .sg signature, size=" +
                                 std::to_string(data_.size()));
    }
    if (data_.size() >= kSignature.size() &&
        std::string_view(reinterpret_cast<const char*>(data_.data()), kSignature.size()) ==
            kSignature) {
        return;
    }
    if (std::string_view(reinterpret_cast<const char*>(data_.data()), kMidFileSignature.size()) ==
        kMidFileSignature) {
        return;
    }
    std::string actual(reinterpret_cast<const char*>(data_.data()),
                       std::min<std::size_t>(data_.size(), kSignature.size()));
    throw std::runtime_error("Invalid .sg signature, got '" + actual + "'");
}

std::string SgParser::short_class_name(const std::string& full_class) {
    auto pos = full_class.find("AVC");
    if (pos == std::string::npos)
        return full_class;
    auto at_pos = full_class.find("@@", pos);
    if (at_pos == std::string::npos)
        return full_class.substr(pos + 3);
    return full_class.substr(pos + 3, at_pos - pos - 3);
}

std::string SgParser::decode_cp1251(std::span<const uint8_t> bytes) {
    while (!bytes.empty() && bytes.back() == 0)
        bytes = bytes.first(bytes.size() - 1);
    if (bytes.empty())
        return {};
    return cp1251_to_utf8(bytes);
}

uint32_t SgParser::read_le32(const std::vector<uint8_t>& data, std::size_t offset) {
    if (offset + 4 > data.size())
        return 0;
    return static_cast<uint32_t>(data[offset]) | (static_cast<uint32_t>(data[offset + 1]) << 8) |
           (static_cast<uint32_t>(data[offset + 2]) << 16) |
           (static_cast<uint32_t>(data[offset + 3]) << 24);
}

bool SgParser::has_field_exact(const std::vector<uint8_t>& rec, const std::string& key) const {
    SgRecordReader reader(rec);
    return reader.has_field_exact(key);
}

std::string SgParser::read_string_field(const std::vector<uint8_t>& rec,
                                        const std::string&          key) const {
    SgRecordReader reader(rec);
    auto           raw = reader.read_string_exact(key);
    if (raw.empty())
        return {};
    return decode_cp1251(
        std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(raw.data()), raw.size()));
}

int SgParser::read_int_field(const std::vector<uint8_t>& rec, const std::string& key) const {
    SgRecordReader reader(rec);
    return reader.read_int32_exact(key);
}

bool SgParser::read_bool_field(const std::vector<uint8_t>& rec, const std::string& key) const {
    SgRecordReader reader(rec);
    return reader.read_bool_exact(key);
}

std::vector<uint8_t> SgParser::read_bytes_field(const std::vector<uint8_t>& rec,
                                                const std::string&          key) const {
    SgRecordReader reader(rec);
    return reader.read_bytes_exact(key);
}

std::vector<std::string> SgParser::read_all_string_fields(const std::vector<uint8_t>& rec,
                                                          const std::string&          key) const {
    SgRecordReader           reader(rec);
    auto                     raw_results = reader.read_all_string_fields(key);
    std::vector<std::string> decoded;
    decoded.reserve(raw_results.size());
    for (const auto& r : raw_results) {
        if (r.empty()) {
            decoded.emplace_back();
        } else {
            decoded.push_back(decode_cp1251(
                std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(r.data()), r.size())));
        }
    }
    return decoded;
}

std::vector<int> SgParser::read_all_int_fields(const std::vector<uint8_t>& rec,
                                               const std::string&          key) const {
    SgRecordReader reader(rec);
    return reader.read_all_int_fields(key);
}

// ============================================================================
// CLASSIFICATION
// ============================================================================

SgObjectClassification SgParser::classify_object(const std::string& short_cls) const {
    static const std::set<std::string> s_parsed_classes = {
        "ScenarioInfo",  "MidPlayer",         "MidSubRace",      "MidUnit",
        "MidStack",      "Capital",           "MidVillage",      "MidSiteMerchant",
        "MidSiteMercs",  "MidSiteTrainer",    "MidSiteMage",     "MidRuin",
        "MidBag",        "MidLocation",       "MidEvent",        "MidItem",
        "MidLandmark",   "MidRoad",           "MidCrystal",      "MidgardMapBlock",
        "MidgardMap",    "MidStackTemplate",  "MidDiplomacy",    "MidTalismanCharges",
        "MidgardMapFog", "PlayerKnownSpells", "PlayerBuildings", "MidgardPlan",
        "MidMountains",  "MidScenVariables",  "TurnSummary",
    };

    static const std::set<std::string> s_verified_empty = {
        "MidSpellCast",
        "MidSpellEffects",
        "MidStackDestroyed",
        "MidQuestLog",
    };

    if (s_parsed_classes.count(short_cls))
        return SgObjectClassification::Parsed;
    if (s_verified_empty.count(short_cls))
        return SgObjectClassification::VerifiedEmptyInitialState;
    return SgObjectClassification::Unknown;
}

// ============================================================================
// PARSING IMPLEMENTATIONS
// ============================================================================

SgScenarioInfo SgParser::parse_scenario_info(const std::vector<uint8_t>& rec) {
    SgScenarioInfo info;
    info.id = read_string_field(rec, "INFO_ID");
    info.name = read_string_field(rec, "NAME");
    info.creator = read_string_field(rec, "CREATOR");
    info.briefing = read_string_field(rec, "BRIEFING");
    info.description = read_string_field(rec, "DESC");
    info.campaign = read_string_field(rec, "CAMPAIGN");
    info.map_size = read_int_field(rec, "MAP_SIZE");
    info.map_seed = read_int_field(rec, "MAP_SEED");
    info.current_turn = read_int_field(rec, "CUR_TURN");
    info.max_unit = read_int_field(rec, "MAX_UNIT");
    info.max_spell = read_int_field(rec, "MAX_SPELL");
    info.max_leader = read_int_field(rec, "MAX_LEADER");
    info.max_city = read_int_field(rec, "MAX_CITY");
    info.diff_scenario = read_int_field(rec, "DIFFSCEN");
    info.diff_game = read_int_field(rec, "DIFFGAME");
    info.suggested_level = read_int_field(rec, "SUGG_LVL");
    info.brief_long = read_all_string_fields(rec, "BRIEFLONG");
    info.debunk_win = read_all_string_fields(rec, "DEBUNKW");
    info.debunk_loss = read_string_field(rec, "DEBUNKL");
    return info;
}

SgPlayer SgParser::parse_player(const std::vector<uint8_t>& rec) {
    SgPlayer pl;
    pl.id = read_string_field(rec, "PLAYER_ID");
    pl.name = read_string_field(rec, "NAME_TXT");
    pl.description = read_string_field(rec, "DESC_TXT");
    pl.lord_id = read_string_field(rec, "LORD_ID");
    pl.race_id = read_string_field(rec, "RACE_ID");
    pl.fog_id = read_string_field(rec, "FOG_ID");
    pl.known_id = read_string_field(rec, "KNOWN_ID");
    pl.builds_id = read_string_field(rec, "BUILDS_ID");
    pl.face = read_int_field(rec, "FACE");
    pl.is_human = read_bool_field(rec, "IS_HUMAN");
    pl.bank = read_string_field(rec, "BANK");
    pl.spell_bank = read_string_field(rec, "SPELL_BANK");
    pl.attitude = read_bool_field(rec, "ATTITUDE");
    pl.always_ai = read_bool_field(rec, "ALWAYSAI");
    return pl;
}

SgSubRace SgParser::parse_subrace(const std::vector<uint8_t>& rec) {
    SgSubRace sr;
    sr.id = read_string_field(rec, "SUBRACE_ID");
    sr.subrace = read_int_field(rec, "SUBRACE");
    sr.player_id = read_string_field(rec, "PLAYER_ID");
    sr.number = read_int_field(rec, "NUMBER");
    sr.name_txt = read_string_field(rec, "NAME_TXT");
    sr.banner = read_int_field(rec, "BANNER");
    return sr;
}

SgStack SgParser::parse_stack(const std::vector<uint8_t>& rec) {
    SgStack s;
    s.id = read_string_field(rec, "STACK_ID");
    s.group_id = read_string_field(rec, "GROUP_ID");

    // UNIT_n = stack member at member index n (n = 0..5)
    // POS_n, read separately as cell-indexed: POS[formation_cell] = member_index
    for (int i = 0; i < 6; ++i) {
        std::string key = "UNIT_" + std::to_string(i);
        std::string uid = read_string_field(rec, key);
        s.units.push_back(uid.empty() ? "G000000000" : uid);
    }
    for (int i = 0; i < 6; ++i) {
        std::string pos_key = "POS_" + std::to_string(i);
        s.positions.push_back(read_int_field(rec, pos_key));
    }

    s.owner = read_string_field(rec, "OWNER");
    s.leader_id = read_string_field(rec, "LEADER_ID");
    SgRecordReader reader(rec);
    s.leader_alive = reader.read_uint8_exact("LEADR_ALIV");
    s.pos_x = read_int_field(rec, "POS_X");
    s.pos_y = read_int_field(rec, "POS_Y");
    s.move = read_int_field(rec, "MOVE");
    s.morale = read_int_field(rec, "MORALE");
    s.inside = read_string_field(rec, "INSIDE");
    s.subrace = read_string_field(rec, "SUBRACE");
    s.invisible = read_bool_field(rec, "INVISIBLE");
    s.ai_ignore = read_bool_field(rec, "AI_IGNORE");
    s.order = read_int_field(rec, "ORDER");
    s.ai_order = read_int_field(rec, "AIORDER");
    s.ai_priority = read_int_field(rec, "AIPRIORITY");
    s.create_level = read_int_field(rec, "CREAT_LVL");
    s.battles_won = read_int_field(rec, "NBBATTLE");
    s.banner = read_string_field(rec, "BANNER");
    s.tome = read_string_field(rec, "TOME");
    s.battle1 = read_string_field(rec, "BATTLE1");
    s.battle2 = read_string_field(rec, "BATTLE2");
    s.artifact1 = read_string_field(rec, "ARTIFACT1");
    s.artifact2 = read_string_field(rec, "ARTIFACT2");
    s.boots = read_string_field(rec, "BOOTS");
    s.facing = read_int_field(rec, "FACING");
    return s;
}

SgCityOrVillage SgParser::parse_city(const std::vector<uint8_t>& rec, const std::string& kind) {
    SgCityOrVillage c;
    c.kind = kind;
    c.id = read_string_field(rec, "CITY_ID");
    c.name = read_string_field(rec, "NAME_TXT");
    c.description = read_string_field(rec, "DESC_TXT");
    c.owner = read_string_field(rec, "OWNER");
    c.subrace = read_string_field(rec, "SUBRACE");
    c.stack = read_string_field(rec, "STACK");
    c.pos_x = read_int_field(rec, "POS_X");
    c.pos_y = read_int_field(rec, "POS_Y");
    c.group_id = read_string_field(rec, "GROUP_ID");
    c.ai_priority = read_int_field(rec, "AIPRIORITY");
    c.size = read_int_field(rec, "SIZE");
    c.item_ids = read_all_string_fields(rec, "ITEM_ID");

    for (int i = 0; i < 6; ++i) {
        std::string key = "UNIT_" + std::to_string(i);
        std::string uid = read_string_field(rec, key);
        c.unit_ids.push_back(uid.empty() ? "G000000000" : uid);
    }
    for (int i = 0; i < 6; ++i) {
        std::string pos_key = "POS_" + std::to_string(i);
        c.positions.push_back(read_int_field(rec, pos_key));
    }
    return c;
}

SgSite SgParser::parse_site(const std::vector<uint8_t>& rec, const std::string& kind) {
    SgSite s;
    s.kind = kind;
    s.id = read_string_field(rec, "SITE_ID");
    s.title = read_string_field(rec, "TXT_TITLE");
    s.description = read_string_field(rec, "TXT_DESC");
    s.image_iso = read_int_field(rec, "IMG_ISO");
    s.image_interface = read_int_field(rec, "IMG_INTF");
    s.pos_x = read_int_field(rec, "POS_X");
    s.pos_y = read_int_field(rec, "POS_Y");
    s.visitor = read_string_field(rec, "VISITER");
    s.ai_priority = read_int_field(rec, "AIPRIORITY");

    if (kind == "MidSiteMerchant") {
        s.buy_armor = read_string_field(rec, "BUY_ARMOR");
        s.buy_jewel = read_string_field(rec, "BUY_JEWEL");
        s.buy_weapon = read_string_field(rec, "BUY_WEAPON");
        s.buy_banner = read_string_field(rec, "BUY_BANNER");
        s.buy_potion = read_string_field(rec, "BUY_POTION");
        s.buy_scroll = read_string_field(rec, "BUY_SCROLL");
        s.buy_wand = read_string_field(rec, "BUY_WAND");
        s.buy_value = read_int_field(rec, "BUY_VALUE");
        s.qty_item = read_int_field(rec, "QTY_ITEM");
        s.items = read_all_string_fields(rec, "ITEM_ID");
        if (s.items.empty()) {
            std::string single = read_string_field(rec, "ITEM");
            if (!single.empty())
                s.items.push_back(single);
        }
        s.missions = read_all_string_fields(rec, "MISSION");
    }

    if (kind == "MidSiteMercs") {
        s.qty_unit = read_int_field(rec, "QTY_UNIT");
        s.units.clear();
        for (int i = 0; i < 6; ++i) {
            std::string unit_key = "UNIT_" + std::to_string(i);
            std::string uid = read_string_field(rec, unit_key);
            if (!uid.empty() && uid != "G000000000")
                s.units.push_back(uid);
        }
    }

    if (kind == "MidSiteMage") {
        s.qty_spell = read_int_field(rec, "QTY_SPELL");
        s.spells = read_all_string_fields(rec, "SPELL");
    }

    return s;
}

SgRuin SgParser::parse_ruin(const std::vector<uint8_t>& rec) {
    SgRuin r;
    r.id = read_string_field(rec, "RUIN_ID");
    r.title = read_string_field(rec, "TITLE");
    r.description = read_string_field(rec, "DESC");
    r.image = read_int_field(rec, "IMAGE");
    r.pos_x = read_int_field(rec, "POS_X");
    r.pos_y = read_int_field(rec, "POS_Y");
    r.cash = read_string_field(rec, "CASH");
    r.item = read_string_field(rec, "ITEM");
    r.looter = read_string_field(rec, "LOOTER");
    r.ai_priority = read_int_field(rec, "AIPRIORITY");

    for (int i = 0; i < 6; ++i) {
        std::string key = "UNIT_" + std::to_string(i);
        std::string uid = read_string_field(rec, key);
        r.unit_ids.push_back(uid.empty() ? "G000000000" : uid);
    }
    for (int i = 0; i < 6; ++i) {
        std::string pos_key = "POS_" + std::to_string(i);
        r.positions.push_back(read_int_field(rec, pos_key));
    }
    return r;
}

SgBag SgParser::parse_bag(const std::vector<uint8_t>& rec) {
    SgBag b;
    b.id = read_string_field(rec, "BAG_ID");
    b.pos_x = read_int_field(rec, "POS_X");
    b.pos_y = read_int_field(rec, "POS_Y");
    b.image = read_int_field(rec, "IMAGE");
    b.cash = read_string_field(rec, "CASH");
    b.items = read_all_string_fields(rec, "ITEM_ID");
    if (b.items.empty()) {
        std::string single = read_string_field(rec, "ITEM");
        if (!single.empty())
            b.items.push_back(single);
    }
    b.looter = read_string_field(rec, "LOOTER");
    b.ai_priority = read_int_field(rec, "AIPRIORITY");
    return b;
}

SgLocation SgParser::parse_location(const std::vector<uint8_t>& rec) {
    SgLocation loc;
    loc.id = read_string_field(rec, "LOC_ID");
    loc.pos_x = read_int_field(rec, "POS_X");
    loc.pos_y = read_int_field(rec, "POS_Y");
    loc.name = read_string_field(rec, "NAME_TXT");
    loc.radius = read_int_field(rec, "RADIUS");
    return loc;
}

SgEvent SgParser::parse_event(const std::vector<uint8_t>& rec) {
    SgEvent e;
    e.id = read_string_field(rec, "ID");
    e.name = read_string_field(rec, "NAME_TXT");
    e.enabled = read_bool_field(rec, "ENABLED");
    e.occur_once = read_bool_field(rec, "OCCUR_ONCE");
    e.chance = read_int_field(rec, "CHANCE");
    e.order = read_int_field(rec, "ORDER");
    e.cond_qty = read_int_field(rec, "COND_QTY");
    e.effect_qty = read_int_field(rec, "EFFECT_QTY");
    e.frequency = read_int_field(rec, "FREQUENCY");
    e.locations = read_all_string_fields(rec, "ID_LOC");
    e.players = read_all_string_fields(rec, "PLAYER");
    e.popup_texts = read_all_string_fields(rec, "POPUP_TXT");
    e.category_values = read_all_int_fields(rec, "CATEGORY");
    e.num_values = read_all_int_fields(rec, "NUM");
    return e;
}

SgItem SgParser::parse_item(const std::vector<uint8_t>& rec) {
    SgItem it;
    it.id = read_string_field(rec, "ITEM_ID");
    it.type = read_string_field(rec, "ITEM_TYPE");
    return it;
}

SgLandmark SgParser::parse_landmark(const std::vector<uint8_t>& rec) {
    SgLandmark lm;
    lm.id = read_string_field(rec, "LMARK_ID");
    if (lm.id.empty())
        lm.id = read_string_field(rec, "LANDMARK_ID");
    lm.pos_x = read_int_field(rec, "POS_X");
    lm.pos_y = read_int_field(rec, "POS_Y");
    lm.type = read_string_field(rec, "TYPE");
    lm.map_gfx_id = read_string_field(rec, "MAP_GFX");
    if (lm.map_gfx_id.empty())
        lm.map_gfx_id = read_string_field(rec, "IMAGE");
    if (lm.map_gfx_id.empty()) {
        int image_val = read_int_field(rec, "IMAGE");
        if (image_val != 0)
            lm.map_gfx_id = "IMG:" + std::to_string(image_val);
    }
    lm.image = read_string_field(rec, "IMAGE");
    lm.name = read_string_field(rec, "NAME_TXT");
    return lm;
}

SgRoad SgParser::parse_road(const std::vector<uint8_t>& rec) {
    SgRoad rd;
    rd.id = read_string_field(rec, "ROAD_ID");
    rd.index = read_int_field(rec, "INDEX");
    rd.variant = read_int_field(rec, "VAR");
    rd.pos_x = read_int_field(rec, "POS_X");
    rd.pos_y = read_int_field(rec, "POS_Y");
    return rd;
}

SgCrystal SgParser::parse_crystal(const std::vector<uint8_t>& rec) {
    SgCrystal cr;
    cr.id = read_string_field(rec, "CRYSTAL_ID");
    cr.pos_x = read_int_field(rec, "POS_X");
    cr.pos_y = read_int_field(rec, "POS_Y");
    cr.resource = read_int_field(rec, "RESOURCE");
    cr.type = read_string_field(rec, "TYPE");
    if (cr.type.empty())
        cr.type = read_string_field(rec, "CRYSTAL_TYPE");
    cr.owner = read_string_field(rec, "OWNER");
    cr.ai_priority = read_int_field(rec, "AIPRIORITY");
    return cr;
}

SgMapBlock SgParser::parse_map_block(const std::vector<uint8_t>& rec, const std::string& oid_str) {
    SgMapBlock mb;
    mb.id = oid_str;
    mb.raw_data = read_bytes_field(rec, "BLOCKDATA");

    if (mb.raw_data.size() >= kBlockDataSize) {
        mb.raw_data.resize(kBlockDataSize);
        for (std::size_t i = 0; i < kBlockDataSize / 4; ++i) {
            mb.values.push_back(read_le32(mb.raw_data, i * 4));
        }
    }

    if (oid_str.size() >= 4) {
        try {
            std::string xs = oid_str.substr(oid_str.size() - 4, 2);
            std::string ys = oid_str.substr(oid_str.size() - 2);
            mb.grid_x = static_cast<int>(std::stoul(xs, nullptr, 16));
            mb.grid_y = static_cast<int>(std::stoul(ys, nullptr, 16));
        } catch (...) {
            mb.grid_x = -1;
            mb.grid_y = -1;
        }
    }
    return mb;
}

SgTerrainGrid SgParser::reconstruct_terrain(const std::vector<SgMapBlock>& blocks, int map_size) {
    SgTerrainGrid grid;
    grid.width = map_size;
    grid.height = map_size;
    grid.tiles.assign(static_cast<std::size_t>(map_size),
                      std::vector<uint32_t>(static_cast<std::size_t>(map_size), 0));

    for (const auto& block : blocks) {
        if (block.grid_x < 0 || block.grid_y < 0 ||
            block.values.size() != kTilesPerBlockX * kTilesPerBlockY)
            continue;
        for (int i = 0; i < kTilesPerBlockX; ++i) {
            for (int j = 0; j < kTilesPerBlockY; ++j) {
                int tile_x = block.grid_x + i;
                int tile_y = block.grid_y + j;
                if (tile_x >= 0 && tile_x < map_size && tile_y >= 0 && tile_y < map_size) {
                    grid.tiles[static_cast<std::size_t>(tile_y)][static_cast<std::size_t>(tile_x)] =
                        block.values[static_cast<std::size_t>(i * kTilesPerBlockY + j)];
                }
            }
        }
    }

    return grid;
}

// ============================================================================
// SEMANTIC PARSERS (formerly recognized-unparsed classes)
// ============================================================================

SgStackTemplate SgParser::parse_mid_stack_template(const std::vector<uint8_t>& rec) {
    SgStackTemplate st;
    st.id = read_string_field(rec, "TEMPLATE_ID");
    if (st.id.empty())
        st.id = read_string_field(rec, "STACK_TMPL_ID");

    st.owner = read_string_field(rec, "OWNER");
    st.leader = read_string_field(rec, "LEADER");
    st.leader_level = read_int_field(rec, "LEADER_LVL");
    st.name_txt = read_string_field(rec, "NAME_TXT");
    st.subrace = read_string_field(rec, "SUBRACE");
    st.order = read_int_field(rec, "ORDER");
    st.order_target = read_string_field(rec, "ORDER_TARG");
    st.use_facing = read_bool_field(rec, "USE_FACING");
    st.facing = read_int_field(rec, "FACING");
    st.ai_priority = read_int_field(rec, "AIPRIORITY");
    st.pos_x = read_int_field(rec, "POS_X");
    st.pos_y = read_int_field(rec, "POS_Y");
    st.modifier_id = read_string_field(rec, "MODIF_ID");
    st.unit_pos = read_int_field(rec, "UNIT_POS");

    for (int i = 0; i < 6; ++i) {
        SgStackTemplateUnit ut;
        std::string         unit_key = "UNIT_" + std::to_string(i);
        ut.unit_id = read_string_field(rec, unit_key);
        if (ut.unit_id.empty() && ut.unit_id != "G000000000") {
            // Try UNIT_0, UNIT_1, etc. as standalone keys
            continue;
        }
        std::string lvl_key = std::to_string(i) + "_LVL";
        ut.level = read_int_field(rec, lvl_key);
        if (ut.level == 0) {
            ut.level = read_int_field(rec, "UNIT_" + std::to_string(i) + "_LVL");
        }
        std::string pos_key = "POS_" + std::to_string(i);
        ut.position = read_int_field(rec, pos_key);
        if (!ut.unit_id.empty() && ut.unit_id != "G000000000") {
            st.units.push_back(ut);
        }
    }

    return st;
}

SgMidScenVariables SgParser::parse_mid_scen_variables(const std::vector<uint8_t>& rec) {
    SgMidScenVariables sv;
    sv.id = read_string_field(rec, "SCENVAR_ID");
    if (sv.id.empty())
        sv.id = read_string_field(rec, "VAR_ID");

    auto        names = read_all_string_fields(rec, "NAME");
    auto        values = read_all_string_fields(rec, "VALUE");
    auto        values2 = read_all_string_fields(rec, "VALUE2");
    std::size_t count = names.size();
    for (std::size_t i = 0; i < count; ++i) {
        SgNameValuePair pair;
        pair.name = names[i];
        pair.value = (i < values.size()) ? values[i] : "";
        pair.value2 = (i < values2.size()) ? values2[i] : "";
        sv.variables.push_back(pair);
    }

    if (sv.variables.empty()) {
        auto int_vals = read_all_int_fields(rec, "INTVAL");
        for (std::size_t i = 0; i < int_vals.size(); ++i) {
            SgNameValuePair pair;
            pair.name = "INTVAL[" + std::to_string(i) + "]";
            pair.value = std::to_string(int_vals[i]);
            sv.variables.push_back(pair);
        }
    }

    return sv;
}

SgMidDiplomacy SgParser::parse_mid_diplomacy(const std::vector<uint8_t>& rec) {
    SgMidDiplomacy d;

    // All three are INT fields (key + 4-byte value), not strings.
    // RACE_1, RACE_2 are integer race indexes.
    // RELATION is an integer relation value (typically 0-3).
    auto race1_vals = read_all_int_fields(rec, "RACE_1");
    auto race2_vals = read_all_int_fields(rec, "RACE_2");
    auto relations = read_all_int_fields(rec, "RELATION");

    std::size_t count = std::min({race1_vals.size(), race2_vals.size(), relations.size()});
    for (std::size_t i = 0; i < count; ++i) {
        SgDiplomacyRelation rel;
        rel.race1 = "RACE_I:" + std::to_string(race1_vals[i]);
        rel.race2 = "RACE_I:" + std::to_string(race2_vals[i]);
        rel.relation = relations[i];
        d.relations.push_back(rel);
    }

    return d;
}

SgMidTalismanCharges SgParser::parse_mid_talisman_charges(const std::vector<uint8_t>& rec) {
    SgMidTalismanCharges tc;

    auto        item_ids = read_all_string_fields(rec, "ID_TALIS");
    auto        charges = read_all_int_fields(rec, "CHARGES");
    std::size_t count = std::min(item_ids.size(), charges.size());
    for (std::size_t i = 0; i < count; ++i) {
        SgTalismanCharge ch;
        ch.item_id = item_ids[i];
        ch.charges = charges[i];
        tc.charges.push_back(ch);
    }
    return tc;
}

SgMidgardPlan SgParser::parse_midgard_plan(const std::vector<uint8_t>& rec) {
    SgMidgardPlan plan;
    plan.id = read_string_field(rec, "PLAN_ID");
    if (plan.id.empty())
        plan.id = read_string_field(rec, "MIDGARDPLAN_ID");

    auto        elements = read_all_string_fields(rec, "ELEMENT");
    auto        pos_xs = read_all_int_fields(rec, "POS_X");
    auto        pos_ys = read_all_int_fields(rec, "POS_Y");
    std::size_t count = elements.size();
    for (std::size_t i = 0; i < count; ++i) {
        SgPlanEntry entry;
        entry.element = elements[i];
        entry.pos_x = (i < pos_xs.size()) ? pos_xs[i] : 0;
        entry.pos_y = (i < pos_ys.size()) ? pos_ys[i] : 0;
        plan.entries.push_back(entry);
    }

    return plan;
}

SgMidMountains SgParser::parse_mid_mountains(const std::vector<uint8_t>& rec) {
    SgMidMountains mt;

    std::size_t start_pos = 0;
    for (std::size_t i = 0; i + kBegObject.size() <= rec.size(); ++i) {
        if (std::memcmp(rec.data() + i, kBegObject.data(), kBegObject.size()) == 0) {
            start_pos = i + kBegObject.size();
            break;
        }
    }

    std::size_t scan_pos = start_pos;

    if (scan_pos + 15 > rec.size())
        return mt;
    if (rec[scan_pos] == 0)
        scan_pos += 1;
    scan_pos += 10;
    if (scan_pos + 4 > rec.size())
        return mt;
    scan_pos += 4;

    const uint8_t* id_mount_key = reinterpret_cast<const uint8_t*>("ID_MOUNT");
    const uint8_t* size_x_key = reinterpret_cast<const uint8_t*>("SIZE_X");
    const uint8_t* size_y_key = reinterpret_cast<const uint8_t*>("SIZE_Y");
    const uint8_t* pos_x_key = reinterpret_cast<const uint8_t*>("POS_X");
    const uint8_t* pos_y_key = reinterpret_cast<const uint8_t*>("POS_Y");
    const uint8_t* race_key = reinterpret_cast<const uint8_t*>("RACE");

    while (scan_pos + 8 + 4 <= rec.size()) {

        if (std::memcmp(rec.data() + scan_pos, id_mount_key, 8) != 0)
            break;
        if (scan_pos > start_pos && rec[scan_pos - 1] == '_')
            break;

        SgMountainEntry entry;
        scan_pos += 8;
        if (scan_pos + 4 > rec.size())
            break;
        entry.id_mount = static_cast<int>(read_le32(rec, scan_pos));
        scan_pos += 4;

        if (scan_pos + 6 + 4 > rec.size() || std::memcmp(rec.data() + scan_pos, size_x_key, 6) != 0)
            break;
        scan_pos += 6;
        entry.size_x = static_cast<int>(read_le32(rec, scan_pos));
        scan_pos += 4;

        if (scan_pos + 6 + 4 > rec.size() || std::memcmp(rec.data() + scan_pos, size_y_key, 6) != 0)
            break;
        scan_pos += 6;
        entry.size_y = static_cast<int>(read_le32(rec, scan_pos));
        scan_pos += 4;

        if (scan_pos + 5 + 4 > rec.size() || std::memcmp(rec.data() + scan_pos, pos_x_key, 5) != 0)
            break;
        scan_pos += 5;
        entry.pos_x = static_cast<int>(read_le32(rec, scan_pos));
        scan_pos += 4;

        if (scan_pos + 5 + 4 > rec.size() || std::memcmp(rec.data() + scan_pos, pos_y_key, 5) != 0)
            break;
        scan_pos += 5;
        entry.pos_y = static_cast<int>(read_le32(rec, scan_pos));
        scan_pos += 4;

        // IMAGE as int32
        {
            bool        as_string = false;
            std::size_t check_pos = scan_pos + 5;
            if (check_pos + 4 + 4 + 4 <= rec.size()) {
                uint32_t    img_len = read_le32(rec, check_pos);
                std::size_t after_data = check_pos + 4 + img_len;
                if (img_len > 0 && img_len < 500 && after_data + 4 <= rec.size() &&
                    std::memcmp(rec.data() + after_data, race_key, 4) == 0) {
                    as_string = true;
                }
            }
            scan_pos += 5;
            if (as_string) {
                uint32_t img_len = read_le32(rec, scan_pos);
                if (img_len > 0 && img_len < 2000 && scan_pos + 4 + img_len <= rec.size()) {
                    entry.image = static_cast<int>(read_le32(rec, scan_pos + 4));
                }
                scan_pos += 4 + img_len;
            } else {
                entry.image = static_cast<int>(read_le32(rec, scan_pos));
                scan_pos += 4;
            }
        }

        // RACE as int32
        {
            bool        as_string = false;
            std::size_t check_pos = scan_pos;
            if (check_pos + 4 + 4 + 4 <= rec.size()) {
                if (std::memcmp(rec.data() + check_pos, race_key, 4) == 0) {
                    uint32_t    race_len = read_le32(rec, check_pos + 4);
                    std::size_t after_data = check_pos + 4 + 4 + race_len;
                    if (race_len > 0 && race_len < 500 && after_data + 8 <= rec.size() &&
                        std::memcmp(rec.data() + after_data, id_mount_key, 8) == 0) {
                        as_string = true;
                    }
                }
            }
            if (as_string) {
                scan_pos += 4;
                uint32_t race_len = read_le32(rec, scan_pos);
                if (race_len > 0 && race_len < 2000 && scan_pos + 4 + race_len <= rec.size()) {
                    entry.race = static_cast<int>(read_le32(rec, scan_pos + 4));
                }
                scan_pos += 4 + race_len;
            } else if (scan_pos + 4 + 4 <= rec.size() &&
                       std::memcmp(rec.data() + scan_pos, race_key, 4) == 0) {
                scan_pos += 4;
                entry.race = static_cast<int>(read_le32(rec, scan_pos));
                scan_pos += 4;
            }
        }

        mt.entries.push_back(entry);
    }

    return mt;
}

SgMapFog SgParser::parse_midgard_map_fog(const std::vector<uint8_t>& rec) {
    SgMapFog fog;

    std::size_t start_pos = 0;
    for (std::size_t i = 0; i + kBegObject.size() <= rec.size(); ++i) {
        if (std::memcmp(rec.data() + i, kBegObject.data(), kBegObject.size()) == 0) {
            start_pos = i + kBegObject.size();
            break;
        }
    }

    // Skip raw payload header: [sep][obj_id][4-byte count]
    std::size_t scan_pos = start_pos;
    if (scan_pos + 15 > rec.size())
        return fog;
    scan_pos += 1;  // skip separator null
    scan_pos += 10; // skip repeated obj_id
    scan_pos += 4;  // skip count

    while (scan_pos + 5 + 4 + 3 + 4 <= rec.size()) {
        if (std::memcmp(rec.data() + scan_pos, "POS_Y", 5) != 0)
            break;
        if (scan_pos > start_pos && rec[scan_pos - 1] == '_')
            break;

        scan_pos += 5;
        int pos_y = static_cast<int>(read_le32(rec, scan_pos));
        scan_pos += 4;

        if (scan_pos + 3 > rec.size() || std::memcmp(rec.data() + scan_pos, "FOG", 3) != 0)
            break;
        scan_pos += 3;

        if (scan_pos + 4 > rec.size())
            break;
        uint32_t fog_len = read_le32(rec, scan_pos);
        if (fog_len > 0 && fog_len < 100000 && scan_pos + 4 + fog_len <= rec.size()) {
            SgFogRow row;
            row.pos_y = pos_y;
            row.raw_bytes.assign(rec.begin() + static_cast<std::ptrdiff_t>(scan_pos + 4),
                                 rec.begin() + static_cast<std::ptrdiff_t>(scan_pos + 4 + fog_len));

            if (fog.bytes_per_row == 0)
                fog.bytes_per_row = static_cast<int>(fog_len);

            fog.rows.push_back(std::move(row));
        }
        scan_pos += 4 + fog_len;
    }

    fog.map_height_tiles = static_cast<int>(fog.rows.size());
    fog.map_width_tiles = fog.bytes_per_row * 8;

    for (const auto& row : fog.rows)
        fog.fog_data.insert(fog.fog_data.end(), row.raw_bytes.begin(), row.raw_bytes.end());

    if (!fog.fog_data.empty())
        fog.encoding_hypothesis = "row-based packed bits (POS_Y + FOG pairs)";

    return fog;
}

SgPlayerKnownSpells SgParser::parse_player_known_spells(const std::vector<uint8_t>& rec) {
    SgPlayerKnownSpells ks;
    ks.id = read_string_field(rec, "SPELLSET_ID");
    if (ks.id.empty())
        ks.id = read_string_field(rec, "KNOWNSPELL_ID");
    ks.player_id = read_string_field(rec, "PLAYER_ID");
    ks.spell_ids = read_all_string_fields(rec, "SPELL_ID");
    if (ks.spell_ids.empty())
        ks.spell_ids = read_all_string_fields(rec, "SPELL");
    return ks;
}

SgPlayerBuildings SgParser::parse_player_buildings(const std::vector<uint8_t>& rec) {
    SgPlayerBuildings pb;
    pb.id = read_string_field(rec, "BUILDINGS_ID");
    if (pb.id.empty())
        pb.id = read_string_field(rec, "PLAYERBUILD_ID");
    pb.player_id = read_string_field(rec, "PLAYER_ID");
    pb.build_data = read_bytes_field(rec, "BUILDDATA");
    if (pb.build_data.empty()) {
        // Try to find BUILD/BUILD_ID sequential fields
        auto build_ids = read_all_string_fields(rec, "BUILD_ID");
        auto builds = read_all_string_fields(rec, "BUILD");
        for (const auto& b : build_ids) {
            pb.build_data.insert(pb.build_data.end(), b.begin(), b.end());
            pb.build_data.push_back(0);
        }
        for (const auto& b : builds) {
            pb.build_data.insert(pb.build_data.end(), b.begin(), b.end());
            pb.build_data.push_back(0);
        }
    }
    return pb;
}

SgTurnSummary SgParser::parse_turn_summary(const std::vector<uint8_t>& rec) {
    SgTurnSummary ts;
    ts.id = read_string_field(rec, "SUMMARY_ID");
    if (ts.id.empty())
        ts.id = read_string_field(rec, "TURNSUM_ID");
    ts.turn = read_int_field(rec, "TURN");
    return ts;
}

// ============================================================================
// GLOBAL ID PROVENANCE COLLECTION
// ============================================================================

bool SgParser::has_semantic_content(const std::vector<uint8_t>&     rec,
                                    const std::vector<std::string>& expected_fields) const {
    for (const auto& field : expected_fields) {
        if (has_field_exact(rec, field))
            return true;
    }
    return false;
}

void SgParser::collect_global_id_usages(const SgParseResult&          parse_result,
                                        std::vector<SgGlobalIdUsage>& usages) const {
    // ── Temporary debt ──────────────────────────────────────────────────
    // This is a manual collector that must be kept in sync with the parser
    // as new scenario classes are added. Each field on each semantic struct
    // that can contain a G* ID must be listed here.
    //
    // Long-term fix: derive global ID usages automatically from the field
    // schema or from strong ID types at read time (reference provenance).
    // See architecture discussion in docs/architecture/game_data_registry.md.
    // ────────────────────────────────────────────────────────────────────
    const auto& scenario = parse_result.scenario;
    auto        add = [&](const std::string& value, const std::string& oid, const std::string& cls,
                          const std::string& field, int px, int py) {
        if (!starts_with_g000(value))
            return;
        SgGlobalIdUsage u;
        u.value.value = value;
        u.object_id.value = oid;
        u.class_name = cls;
        u.field_name = field;
        u.pos_x = px;
        u.pos_y = py;
        usages.push_back(u);
    };

    // Players
    for (const auto& pl : scenario.players) {
        add(pl.lord_id, pl.id, "MidPlayer", "LORD_ID", -1, -1);
        add(pl.race_id, pl.id, "MidPlayer", "RACE_ID", -1, -1);
        add(pl.fog_id, pl.id, "MidPlayer", "FOG_ID", -1, -1);
        add(pl.known_id, pl.id, "MidPlayer", "KNOWN_ID", -1, -1);
        add(pl.builds_id, pl.id, "MidPlayer", "BUILDS_ID", -1, -1);
    }

    // SubRaces
    for (const auto& sr : scenario.subraces) {
        add(sr.player_id, sr.id, "MidSubRace", "PLAYER_ID", -1, -1);
    }

    // Units
    for (const auto& u : scenario.units) {
        add(u.type_id, u.id, "MidUnit", "TYPE", -1, -1);
    }

    // Stacks
    for (const auto& s : scenario.stacks) {
        add(s.owner, s.id, "MidStack", "OWNER", s.pos_x, s.pos_y);
        add(s.leader_id, s.id, "MidStack", "LEADER_ID", s.pos_x, s.pos_y);
        add(s.inside, s.id, "MidStack", "INSIDE", s.pos_x, s.pos_y);
        add(s.subrace, s.id, "MidStack", "SUBRACE", s.pos_x, s.pos_y);
        add(s.banner, s.id, "MidStack", "BANNER", s.pos_x, s.pos_y);
        add(s.tome, s.id, "MidStack", "TOME", s.pos_x, s.pos_y);
        add(s.battle1, s.id, "MidStack", "BATTLE1", s.pos_x, s.pos_y);
        add(s.battle2, s.id, "MidStack", "BATTLE2", s.pos_x, s.pos_y);
        add(s.artifact1, s.id, "MidStack", "ARTIFACT1", s.pos_x, s.pos_y);
        add(s.artifact2, s.id, "MidStack", "ARTIFACT2", s.pos_x, s.pos_y);
        add(s.boots, s.id, "MidStack", "BOOTS", s.pos_x, s.pos_y);
        for (const auto& uid : s.units)
            add(uid, s.id, "MidStack", "UNIT_N", s.pos_x, s.pos_y);
    }

    // Cities
    for (const auto& c : scenario.cities) {
        add(c.owner, c.id, c.kind, "OWNER", c.pos_x, c.pos_y);
        add(c.subrace, c.id, c.kind, "SUBRACE", c.pos_x, c.pos_y);
        add(c.stack, c.id, c.kind, "STACK", c.pos_x, c.pos_y);
        add(c.group_id, c.id, c.kind, "GROUP_ID", c.pos_x, c.pos_y);
        for (const auto& uid : c.unit_ids)
            add(uid, c.id, c.kind, "UNIT_N", c.pos_x, c.pos_y);
        for (const auto& iid : c.item_ids)
            add(iid, c.id, c.kind, "ITEM_ID", c.pos_x, c.pos_y);
    }

    // Ruins
    for (const auto& r : scenario.ruins) {
        add(r.item, r.id, "MidRuin", "ITEM", r.pos_x, r.pos_y);
        add(r.looter, r.id, "MidRuin", "LOOTER", r.pos_x, r.pos_y);
        for (const auto& uid : r.unit_ids)
            add(uid, r.id, "MidRuin", "UNIT_N", r.pos_x, r.pos_y);
    }

    // Bags
    for (const auto& b : scenario.bags) {
        add(b.looter, b.id, "MidBag", "LOOTER", b.pos_x, b.pos_y);
        for (const auto& iid : b.items)
            add(iid, b.id, "MidBag", "ITEM_ID", b.pos_x, b.pos_y);
    }

    // Events
    for (const auto& e : scenario.events) {
        for (const auto& loc : e.locations)
            add(loc, e.id, "MidEvent", "ID_LOC", -1, -1);
        for (const auto& plr : e.players)
            add(plr, e.id, "MidEvent", "PLAYER", -1, -1);
    }

    // Landmarks
    for (const auto& lm : scenario.landmarks) {
        add(lm.type, lm.id, "MidLandmark", "TYPE", lm.pos_x, lm.pos_y);
        add(lm.map_gfx_id, lm.id, "MidLandmark", "MAP_GFX", lm.pos_x, lm.pos_y);
    }

    // Crystals
    for (const auto& cr : scenario.crystals) {
        add(cr.owner, cr.id, "MidCrystal", "OWNER", cr.pos_x, cr.pos_y);
    }

    // Stack Templates
    for (const auto& st : scenario.stack_templates) {
        add(st.owner, st.id, "MidStackTemplate", "OWNER", st.pos_x, st.pos_y);
        add(st.leader, st.id, "MidStackTemplate", "LEADER", st.pos_x, st.pos_y);
        add(st.subrace, st.id, "MidStackTemplate", "SUBRACE", st.pos_x, st.pos_y);
        add(st.order_target, st.id, "MidStackTemplate", "ORDER_TARG", st.pos_x, st.pos_y);
        add(st.modifier_id, st.id, "MidStackTemplate", "MODIF_ID", st.pos_x, st.pos_y);
        for (const auto& ut : st.units)
            add(ut.unit_id, st.id, "MidStackTemplate", "UNIT_N", st.pos_x, st.pos_y);
    }

    // Diplomacy
    for (const auto& d : scenario.diplomacy) {
        for (const auto& rel : d.relations) {
            add(rel.race1, d.id, "MidDiplomacy", "RACE_1", -1, -1);
            add(rel.race2, d.id, "MidDiplomacy", "RACE_2", -1, -1);
        }
    }

    // Talisman Charges
    for (const auto& tc : scenario.talisman_charges) {
        for (const auto& ch : tc.charges)
            add(ch.item_id, tc.id, "MidTalismanCharges", "ITEM_ID", -1, -1);
    }

    // Plans
    for (const auto& plan : scenario.plans) {
        for (const auto& entry : plan.entries)
            add(entry.element, plan.id, "MidgardPlan", "ELEMENT", entry.pos_x, entry.pos_y);
    }

    // Sites
    for (const auto& s : scenario.sites) {
        add(s.visitor, s.id, s.kind, "VISITER", s.pos_x, s.pos_y);
        for (const auto& iid : s.items)
            add(iid, s.id, s.kind, "ITEM_ID", s.pos_x, s.pos_y);
        for (const auto& mid : s.missions)
            add(mid, s.id, s.kind, "MISSION", s.pos_x, s.pos_y);
        for (const auto& uid : s.units)
            add(uid, s.id, s.kind, "UNIT_N", s.pos_x, s.pos_y);
        for (const auto& sp : s.spells)
            add(sp, s.id, s.kind, "SPELL", s.pos_x, s.pos_y);
    }

    // Known Spells
    for (const auto& ks : scenario.known_spells) {
        for (const auto& sp : ks.spell_ids)
            add(sp, ks.id, "PlayerKnownSpells", "SPELL_ID", -1, -1);
    }

    // Map Fog
    for (const auto& fog : scenario.map_fogs) {
        add(fog.player_id, fog.id, "MidgardMapFog", "PLAYER_ID", -1, -1);
    }

    // Buildings
    for (const auto& pb : scenario.buildings) {
        add(pb.player_id, pb.id, "PlayerBuildings", "PLAYER_ID", -1, -1);
    }
}

// ============================================================================
// Main parse entry point
// ============================================================================

SgParseResult SgParser::parse() {
    SgParseResult result;
    validate_signature();

    std::map<std::string, std::vector<SgObjectIndexEntry>> class_entries;
    std::size_t                                            search_pos = 0;

    while (true) {
        if (search_pos >= data_.size())
            break;
        auto search_start = data_.begin() + static_cast<std::ptrdiff_t>(search_pos);
        auto what_it = std::search(search_start, data_.end(), kWhat.begin(), kWhat.end());
        if (what_it == data_.end())
            break;

        std::size_t what_offset = static_cast<std::size_t>(std::distance(data_.begin(), what_it));
        auto end_it = std::search(what_it, data_.end(), kEndObject.begin(), kEndObject.end());
        if (end_it == data_.end())
            break;

        std::size_t end_offset =
            static_cast<std::size_t>(std::distance(data_.begin(), end_it)) + kEndObject.size();

        std::size_t cls_off = what_offset + 4;
        if (cls_off + 4 > data_.size())
            break;
        uint32_t cls_len = 0;
        std::memcpy(&cls_len, data_.data() + cls_off, 4);
        std::string full_class;
        if (cls_len > 0 && cls_len < 200 &&
            cls_off + 4 + static_cast<std::size_t>(cls_len) <= data_.size()) {
            full_class.assign(reinterpret_cast<const char*>(data_.data() + cls_off + 4),
                              static_cast<std::size_t>(cls_len));
        }

        std::string short_cls = short_class_name(full_class);

        auto                 obj_id_span = data_.subspan(what_offset, end_offset - what_offset);
        std::vector<uint8_t> rec(obj_id_span.begin(), obj_id_span.end());
        std::string          oid_str;
        for (std::size_t i = 0; i + 10 <= rec.size(); ++i) {
            if (std::memcmp(rec.data() + i, "OBJ_ID", 6) == 0) {
                std::size_t val_off = i + 6;
                if (val_off + 4 <= rec.size()) {
                    uint32_t id_len = 0;
                    std::memcpy(&id_len, rec.data() + val_off, 4);
                    if (id_len > 0 && id_len < 128 && val_off + 4 + id_len <= rec.size()) {
                        oid_str.assign(reinterpret_cast<const char*>(rec.data() + val_off + 4),
                                       id_len);
                        while (!oid_str.empty() && oid_str.back() == '\0')
                            oid_str.pop_back();
                    }
                }
                break;
            }
        }

        SgObjectClassification cls = classify_object(short_cls);

        SgObjectIndexEntry entry;
        entry.offset = what_offset;
        entry.length = end_offset - what_offset;
        entry.class_name = short_cls;
        entry.obj_id = oid_str;
        entry.classification = cls;
        result.object_index.push_back(entry);
        class_entries[short_cls].push_back(entry);

        SgRawObject raw;
        raw.offset = what_offset;
        raw.length = end_offset - what_offset;
        raw.class_name = short_cls;
        raw.obj_id = oid_str;
        raw.raw_bytes.assign(rec.begin(), rec.end());
        result.raw_objects.push_back(raw);

        search_pos = end_offset;
    }

    auto get_recs = [&](const std::string& cls) -> std::vector<std::vector<uint8_t>> {
        std::vector<std::vector<uint8_t>> recs;
        auto                              it = class_entries.find(cls);
        if (it == class_entries.end())
            return recs;
        for (const auto& entry : it->second) {
            auto span = data_.subspan(entry.offset, entry.length);
            recs.emplace_back(span.begin(), span.end());
        }
        return recs;
    };

    auto& scenario = result.scenario;

    // ScenarioInfo
    for (const auto& rec : get_recs("ScenarioInfo"))
        scenario.info = parse_scenario_info(rec);

    // Players
    for (const auto& rec : get_recs("MidPlayer"))
        scenario.players.push_back(parse_player(rec));

    // SubRaces
    for (const auto& rec : get_recs("MidSubRace"))
        scenario.subraces.push_back(parse_subrace(rec));

    // Units
    for (const auto& rec : get_recs("MidUnit")) {
        auto wire = parse_mid_unit_wire_record(rec);
        if (!wire.object_id.empty() && wire.object_id != wire.unit_id) {
            add_warning("MidUnit OBJ_ID != UNIT_ID: OBJ_ID=" + wire.object_id +
                        " UNIT_ID=" + wire.unit_id);
        }
        const auto inner_id = bytes_to_ascii(wire.inner_unit_id);
        if (wire.unit_id != inner_id) {
            add_warning("MidUnit UNIT_ID != inner_unit_id: UNIT_ID=" + wire.unit_id +
                        " inner_unit_id=" + inner_id);
        }
        result.unit_wires.push_back(wire);
        scenario.units.push_back(to_semantic_unit(wire));
    }

    // Stacks
    for (const auto& rec : get_recs("MidStack"))
        scenario.stacks.push_back(parse_stack(rec));

    // Cities / Capitals / Villages
    for (const auto& rec : get_recs("Capital"))
        scenario.cities.push_back(parse_city(rec, "Capital"));
    for (const auto& rec : get_recs("MidVillage"))
        scenario.cities.push_back(parse_city(rec, "MidVillage"));

    // Ruins
    for (const auto& rec : get_recs("MidRuin"))
        scenario.ruins.push_back(parse_ruin(rec));

    // Bags
    for (const auto& rec : get_recs("MidBag"))
        scenario.bags.push_back(parse_bag(rec));

    // Locations
    for (const auto& rec : get_recs("MidLocation"))
        scenario.locations.push_back(parse_location(rec));

    // Events
    for (const auto& rec : get_recs("MidEvent"))
        scenario.events.push_back(parse_event(rec));

    // Items
    for (const auto& rec : get_recs("MidItem"))
        scenario.items.push_back(parse_item(rec));

    // Landmarks
    for (const auto& rec : get_recs("MidLandmark"))
        scenario.landmarks.push_back(parse_landmark(rec));

    // Roads
    for (const auto& rec : get_recs("MidRoad"))
        scenario.roads.push_back(parse_road(rec));

    // Crystals
    for (const auto& rec : get_recs("MidCrystal"))
        scenario.crystals.push_back(parse_crystal(rec));

    // Map blocks
    for (const auto& entry : class_entries["MidgardMapBlock"]) {
        auto                 span = data_.subspan(entry.offset, entry.length);
        std::vector<uint8_t> rec(span.begin(), span.end());
        scenario.map.blocks.push_back(parse_map_block(rec, entry.obj_id));
    }

    // Map
    if (class_entries.count("MidgardMap")) {
        for (const auto& entry : class_entries["MidgardMap"]) {
            scenario.map.id = entry.obj_id;
        }
    }

    // Reconstruct terrain
    int map_size = scenario.info.map_size;
    if (map_size > 0 && !scenario.map.blocks.empty()) {
        scenario.map.terrain = reconstruct_terrain(scenario.map.blocks, map_size);
    }

    // Semantic parsers — pass object ID from index entry
    auto get_recs_with_ids =
        [&](const std::string& cls) -> std::vector<std::pair<std::vector<uint8_t>, std::string>> {
        std::vector<std::pair<std::vector<uint8_t>, std::string>> recs;
        auto                                                      it = class_entries.find(cls);
        if (it == class_entries.end())
            return recs;
        for (const auto& entry : it->second) {
            auto span = data_.subspan(entry.offset, entry.length);
            recs.emplace_back(std::vector<uint8_t>(span.begin(), span.end()), entry.obj_id);
        }
        return recs;
    };

    // Sites
    auto parse_site_kind = [&](const std::string& cls, const std::string& kind) {
        for (const auto& [rec, oid] : get_recs_with_ids(cls)) {
            auto s = parse_site(rec, kind);
            if (s.id.empty())
                s.id = oid;
            scenario.sites.push_back(s);
        }
    };
    parse_site_kind("MidSiteMerchant", "MidSiteMerchant");
    parse_site_kind("MidSiteMercs", "MidSiteMercs");
    parse_site_kind("MidSiteTrainer", "MidSiteTrainer");
    parse_site_kind("MidSiteMage", "MidSiteMage");

    for (const auto& [rec, oid] : get_recs_with_ids("MidStackTemplate")) {
        auto st = parse_mid_stack_template(rec);
        if (st.id.empty())
            st.id = oid;
        scenario.stack_templates.push_back(st);
    }

    for (const auto& [rec, oid] : get_recs_with_ids("MidScenVariables")) {
        auto sv = parse_mid_scen_variables(rec);
        if (sv.id.empty())
            sv.id = oid;
        scenario.scen_variables.push_back(sv);
    }

    for (const auto& [rec, oid] : get_recs_with_ids("MidDiplomacy")) {
        auto d = parse_mid_diplomacy(rec);
        if (d.id.empty())
            d.id = oid;
        scenario.diplomacy.push_back(d);
    }

    for (const auto& [rec, oid] : get_recs_with_ids("MidTalismanCharges")) {
        auto tc = parse_mid_talisman_charges(rec);
        if (tc.id.empty())
            tc.id = oid;
        scenario.talisman_charges.push_back(tc);
    }

    for (const auto& [rec, oid] : get_recs_with_ids("MidgardPlan")) {
        auto p = parse_midgard_plan(rec);
        if (p.id.empty())
            p.id = oid;
        scenario.plans.push_back(p);
    }

    for (const auto& [rec, oid] : get_recs_with_ids("MidMountains")) {
        auto mt = parse_mid_mountains(rec);
        if (mt.id.empty())
            mt.id = oid;
        scenario.mountains.push_back(mt);
    }

    for (const auto& [rec, oid] : get_recs_with_ids("MidgardMapFog")) {
        auto f = parse_midgard_map_fog(rec);
        if (f.id.empty())
            f.id = oid;
        scenario.map_fogs.push_back(f);
    }

    for (const auto& [rec, oid] : get_recs_with_ids("PlayerKnownSpells")) {
        if (!has_semantic_content(
                rec, {"SPELL_ID", "SPELL", "PLAYER_ID", "SPELLSET_ID", "KNOWNSPELL_ID"})) {
            for (auto& entry : result.object_index) {
                if (entry.obj_id == oid && entry.class_name == "PlayerKnownSpells") {
                    entry.classification = SgObjectClassification::VerifiedEmptyInitialState;
                    break;
                }
            }
            continue;
        }
        auto ks = parse_player_known_spells(rec);
        if (ks.id.empty())
            ks.id = oid;
        scenario.known_spells.push_back(ks);
    }

    for (const auto& [rec, oid] : get_recs_with_ids("PlayerBuildings")) {
        if (!has_semantic_content(rec, {"BUILD_ID", "BUILD", "PLAYER_ID", "BUILDDATA",
                                        "BUILDINGS_ID", "PLAYERBUILD_ID"})) {
            for (auto& entry : result.object_index) {
                if (entry.obj_id == oid && entry.class_name == "PlayerBuildings") {
                    entry.classification = SgObjectClassification::VerifiedEmptyInitialState;
                    break;
                }
            }
            continue;
        }
        auto pb = parse_player_buildings(rec);
        if (pb.id.empty())
            pb.id = oid;
        scenario.buildings.push_back(pb);
    }

    for (const auto& [rec, oid] : get_recs_with_ids("TurnSummary")) {
        auto ts = parse_turn_summary(rec);
        if (ts.id.empty())
            ts.id = oid;
        scenario.turn_summaries.push_back(ts);
    }

    // Verified-empty classes: classified directly, not pushed to semantic vectors.
    // They appear only in verified_empty_objects (via object_index classification).
    // For full diagnostic completeness they also get hull vectors in result.
    for (const auto& [rec, oid] : get_recs_with_ids("MidQuestLog")) {
        SgQuestLog ql;
        ql.id = oid;
        result.quest_logs.push_back(ql);
    }

    for (const auto& [rec, oid] : get_recs_with_ids("MidSpellCast")) {
        SgSpellCast sc;
        sc.id = oid;
        result.spell_casts.push_back(sc);
    }

    for (const auto& [rec, oid] : get_recs_with_ids("MidSpellEffects")) {
        SgSpellEffects se;
        se.id = oid;
        result.spell_effects.push_back(se);
    }

    for (const auto& [rec, oid] : get_recs_with_ids("MidStackDestroyed")) {
        SgStackDestroyed sd;
        sd.id = oid;
        result.stacks_destroyed.push_back(sd);
    }

    // Populate classification-based maps
    for (const auto& entry : result.object_index) {
        switch (entry.classification) {
        case SgObjectClassification::Parsed:
            result.parsed_objects[entry.class_name].push_back(entry);
            break;
        case SgObjectClassification::VerifiedEmptyInitialState:
            result.verified_empty_objects[entry.class_name].push_back(entry);
            break;
        case SgObjectClassification::Unknown:
            result.unknown_objects[entry.class_name].push_back(entry);
            break;
        }
    }

    // Collect global ID provenance — accesses result.scenario for semantic data
    collect_global_id_usages(result, result.global_id_usages);

    result.file_path = "";
    result.file_size = data_.size();
    result.parse_warnings = warnings_;

    return result;
}

} // namespace d2scenario
