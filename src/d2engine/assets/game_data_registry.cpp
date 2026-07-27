#include "game_data_registry.hpp"

#include "d2res/dbf_reader.hpp"

#include <d2log/log.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace d2engine {

namespace {

auto kLog = d2log::get("d2.gamedata"); // NOLINT(cert-err58-cpp)

// ── helpers ──────────────────────────────────────────────────────────────────

std::vector<uint8_t> read_file(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        throw std::runtime_error("cannot open: " + path.string());
    }
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

int safe_int(const std::map<std::string, std::string>& row, const std::string& key,
             int fallback = 0) {
    auto it = row.find(key);
    if (it == row.end() || it->second.empty()) {
        return fallback;
    }
    try {
        return std::stoi(it->second);
    } catch (...) {
        return fallback;
    }
}

bool is_true(const std::map<std::string, std::string>& row, const std::string& key) {
    auto it = row.find(key);
    if (it == row.end()) {
        return false;
    }
    const char c = it->second.empty() ? '\0' : it->second[0];
    return (c == 'T' || c == 't' || c == 'Y' || c == 'y');
}

std::string lower(std::string s) {
    std::ranges::transform(s, s.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string canonical_optional_attack_id(std::string_view raw) {
    auto id = lower(std::string(raw));
    if (id.empty() || id == "g000000000") {
        return {};
    }
    return id;
}

std::string field(const std::map<std::string, std::string>& row, const std::string& key) {
    auto it = row.find(key);
    return (it != row.end()) ? it->second : std::string{};
}

// Try to load a DBF table; on failure print to stderr and return empty vector.
std::vector<std::map<std::string, std::string>> load_dbf(const std::filesystem::path& globals_dir,
                                                         const std::string&           filename) {
    const auto path = globals_dir / filename;
    try {
        auto                   data = read_file(path);
        d2res::DbfReader const reader(std::span<const uint8_t>(data.data(), data.size()));
        return reader.read_records();
    } catch (const std::exception& e) {
        kLog->error("load_failed file={} error={}", filename, e.what());
        return {};
    }
}

// ── L-table loaders ───────────────────────────────────────────────────────────

std::unordered_map<int, AttackClass>
build_attack_class_map(const std::filesystem::path& globals_dir) {
    const auto                           rows = load_dbf(globals_dir, "LattC.dbf");
    std::unordered_map<int, AttackClass> m;
    for (const auto& row : rows) {
        const int         id = safe_int(row, "ID");
        const std::string txt = field(row, "TEXT");
        AttackClass       cls = AttackClass::Unknown;
        if (txt == "L_DAMAGE") {
            cls = AttackClass::Damage;
        } else if (txt == "L_DRAIN") {
            cls = AttackClass::Drain;
        } else if (txt == "L_PARALYZE") {
            cls = AttackClass::Paralyze;
        } else if (txt == "L_HEAL") {
            cls = AttackClass::Heal;
        } else if (txt == "L_FEAR") {
            cls = AttackClass::Fear;
        } else if (txt == "L_BOOST_DAMAGE") {
            cls = AttackClass::BoostDamage;
        } else if (txt == "L_PETRIFY") {
            cls = AttackClass::Petrify;
        } else if (txt == "L_LOWER_DAMAGE") {
            cls = AttackClass::LowerDamage;
        } else if (txt == "L_LOWER_INITIATIVE") {
            cls = AttackClass::LowerInitiative;
        } else if (txt == "L_POISON") {
            cls = AttackClass::Poison;
        } else if (txt == "L_FROSTBITE") {
            cls = AttackClass::Frostbite;
        } else if (txt == "L_REVIVE") {
            cls = AttackClass::Revive;
        } else if (txt == "L_DRAIN_OVERFLOW") {
            cls = AttackClass::DrainOverflow;
        } else if (txt == "L_CURE") {
            cls = AttackClass::Cure;
        } else if (txt == "L_SUMMON") {
            cls = AttackClass::Summon;
        } else if (txt == "L_DRAIN_LEVEL") {
            cls = AttackClass::DrainLevel;
        } else if (txt == "L_GIVE_ATTACK") {
            cls = AttackClass::GiveAttack;
        } else if (txt == "L_DOPPELGANGER") {
            cls = AttackClass::Doppelganger;
        } else if (txt == "L_TRANSFORM_SELF") {
            cls = AttackClass::TransformSelf;
        } else if (txt == "L_TRANSFORM_OTHER") {
            cls = AttackClass::TransformOther;
        } else if (txt == "L_BLISTER") {
            cls = AttackClass::Blister;
        } else if (txt == "L_BESTOW_WARDS") {
            cls = AttackClass::BestowWards;
        } else if (txt == "L_SHATTER") {
            cls = AttackClass::Shatter;
        }
        m[id] = cls;
    }
    return m;
}

std::unordered_map<int, AttackSource>
build_attack_source_map(const std::filesystem::path& globals_dir) {
    const auto                            rows = load_dbf(globals_dir, "LattS.dbf");
    std::unordered_map<int, AttackSource> m;
    for (const auto& row : rows) {
        const int         id = safe_int(row, "ID");
        const std::string txt = field(row, "TEXT");
        AttackSource      src = AttackSource::Unknown;
        if (txt == "L_WEAPON") {
            src = AttackSource::Weapon;
        } else if (txt == "L_MIND") {
            src = AttackSource::Mind;
        } else if (txt == "L_LIFE") {
            src = AttackSource::Life;
        } else if (txt == "L_DEATH") {
            src = AttackSource::Death;
        } else if (txt == "L_FIRE") {
            src = AttackSource::Fire;
        } else if (txt == "L_WATER") {
            src = AttackSource::Water;
        } else if (txt == "L_EARTH") {
            src = AttackSource::Earth;
        } else if (txt == "L_AIR") {
            src = AttackSource::Air;
        }
        m[id] = src;
    }
    return m;
}

std::unordered_map<int, AttackReach>
build_attack_reach_map(const std::filesystem::path& globals_dir) {
    const auto                           rows = load_dbf(globals_dir, "LAttR.dbf");
    std::unordered_map<int, AttackReach> m;
    for (const auto& row : rows) {
        const int         id = safe_int(row, "ID");
        const std::string txt = field(row, "TEXT");
        AttackReach       rch = AttackReach::Unknown;
        if (txt == "L_ALL") {
            rch = AttackReach::All;
        } else if (txt == "L_ANY") {
            rch = AttackReach::Any;
        } else if (txt == "L_ADJACENT") {
            rch = AttackReach::Adjacent;
        }
        m[id] = rch;
    }
    return m;
}

std::unordered_map<int, UnitBranch>
build_unit_branch_map(const std::filesystem::path& globals_dir) {
    const auto                          rows = load_dbf(globals_dir, "LunitB.dbf");
    std::unordered_map<int, UnitBranch> m;
    for (const auto& row : rows) {
        const int         id = safe_int(row, "ID");
        const std::string txt = field(row, "TEXT");
        UnitBranch        br = UnitBranch::Unknown;
        if (txt == "L_FIGHTER") {
            br = UnitBranch::Fighter;
        } else if (txt == "L_ARCHER") {
            br = UnitBranch::Archer;
        } else if (txt == "L_MAGE") {
            br = UnitBranch::Mage;
        } else if (txt == "L_SPECIAL") {
            br = UnitBranch::Special;
        } else if (txt == "L_SIDESHOW") {
            br = UnitBranch::Sideshow;
        } else if (txt == "L_HERO") {
            br = UnitBranch::Hero;
        } else if (txt == "L_NOBLE") {
            br = UnitBranch::Noble;
        } else if (txt == "L_SUMMON") {
            br = UnitBranch::Summon;
        }
        m[id] = br;
    }
    return m;
}

std::unordered_map<int, UnitCategory>
build_unit_category_map(const std::filesystem::path& globals_dir) {
    const auto                            rows = load_dbf(globals_dir, "LunitC.dbf");
    std::unordered_map<int, UnitCategory> m;
    for (const auto& row : rows) {
        const int         id = safe_int(row, "ID");
        const std::string txt = field(row, "TEXT");
        UnitCategory      cat = UnitCategory::Unknown;
        if (txt == "L_SOLDIER") {
            cat = UnitCategory::Soldier;
        } else if (txt == "L_NOBLE") {
            cat = UnitCategory::Noble;
        } else if (txt == "L_LEADER") {
            cat = UnitCategory::Leader;
        } else if (txt == "L_SUMMON") {
            cat = UnitCategory::Summon;
        } else if (txt == "L_ILLUSION") {
            cat = UnitCategory::Illusion;
        } else if (txt == "L_NEUTRAL_LEADER") {
            cat = UnitCategory::NeutralLeader;
        } else if (txt == "L_NEUTRAL_SOLDIER") {
            cat = UnitCategory::NeutralSoldier;
        } else if (txt == "L_GUARDIAN") {
            cat = UnitCategory::Guardian;
        }
        m[id] = cat;
    }
    return m;
}

std::unordered_map<int, std::string>
build_death_anim_map(const std::filesystem::path& globals_dir) {
    const auto                           rows = load_dbf(globals_dir, "LDthAnim.dbf");
    std::unordered_map<int, std::string> m;
    for (const auto& row : rows) {
        const int id = safe_int(row, "ID");
        m[id] = field(row, "TEXT");
    }
    return m;
}

} // namespace

// ── static ────────────────────────────────────────────────────────────────────

std::string GameDataRegistry::bones_sprite_base(std::string_view /*l_xxx*/, bool size_small) {
    // Stub: ignores l_xxx and maps all units to human corpse OPT image base names.
    // Real names are DEAD_HUMAN_SA00/SA01 (small) and DEAD_HUMAN_LA00/LA01 (large) in Battle.ff
    // OPT.
    return size_small ? "DEAD_HUMAN_SA" : "DEAD_HUMAN_LA";
}

std::string GameDataRegistry::death_battle_ff_name(std::string_view l_xxx, bool size_small) {
    if (l_xxx == "L_HUMAN")
        return "DEATH_HUMAN_S13";
    if (l_xxx == "L_HERETIC")
        return "DEATH_HERETIC_S13";
    if (l_xxx == "L_DWARF")
        return "DEATH_DWARF_S15";
    if (l_xxx == "L_UNDEAD")
        return "DEATH_UNDEAD_S15";
    if (l_xxx == "L_NEUTRAL")
        return "DEATH_NEUTRAL_S10";
    if (l_xxx == "L_DRAGON")
        return "DEATH_DRAGON_S15";
    if (l_xxx == "L_GHOST")
        return "DEATH_GHOST_S15";
    if (l_xxx == "L_ELF")
        return size_small ? "DEATH_ELF_S15" : "DEATH_ELF_L15";
    return "DEATH_HUMAN_S13";
}

// ── constructor ───────────────────────────────────────────────────────────────

GameDataRegistry::GameDataRegistry(const std::filesystem::path& globals_dir) {
    // ── Step 1: L-tables ──────────────────────────────────────────────────────
    const auto attack_class_map = build_attack_class_map(globals_dir);
    const auto attack_source_map = build_attack_source_map(globals_dir);
    const auto attack_reach_map = build_attack_reach_map(globals_dir);
    const auto unit_branch_map = build_unit_branch_map(globals_dir);
    const auto unit_category_map = build_unit_category_map(globals_dir);
    const auto death_anim_map = build_death_anim_map(globals_dir);

    // ── Step 2: Text ──────────────────────────────────────────────────────────
    for (const auto& row : load_dbf(globals_dir, "Tglobal.dbf")) {
        const std::string id = field(row, "TXT_ID");
        if (!id.empty()) {
            text_map_[id] = field(row, "TEXT");
        }
    }

    // ── Step 3: Attacks ───────────────────────────────────────────────────────
    for (const auto& row : load_dbf(globals_dir, "Gattacks.dbf")) {
        AttackDef def;
        def.attack_id = lower(field(row, "ATT_ID"));
        {
            const std::string ntxt = field(row, "NAME_TXT");
            def.name = text_map_.contains(ntxt) ? text_map_.at(ntxt) : std::string{};
        }
        {
            const std::string dtxt = field(row, "DESC_TXT");
            def.description = text_map_.contains(dtxt) ? text_map_.at(dtxt) : std::string{};
        }
        const int cls_id = safe_int(row, "CLASS");
        const int src_id = safe_int(row, "SOURCE");
        const int rch_id = safe_int(row, "REACH");
        {
            auto it = attack_class_map.find(cls_id);
            def.attack_class = (it != attack_class_map.end()) ? it->second : AttackClass::Unknown;
        }
        {
            auto it = attack_source_map.find(src_id);
            def.source = (it != attack_source_map.end()) ? it->second : AttackSource::Unknown;
        }
        {
            auto it = attack_reach_map.find(rch_id);
            def.reach = (it != attack_reach_map.end()) ? it->second : AttackReach::Unknown;
        }
        def.initiative = safe_int(row, "INITIATIVE");
        def.damage = safe_int(row, "QTY_DAM");
        def.heal = safe_int(row, "QTY_HEAL");
        def.power = safe_int(row, "POWER");
        def.infinite = is_true(row, "INFINITE");
        def.crit_hit = is_true(row, "CRIT_HIT");
        def.alt_attack_id = lower(field(row, "ALT_ATTACK"));
        for (const auto& wk : {"WARD1", "WARD2", "WARD3", "WARD4"}) {
            const std::string w = lower(field(row, wk));
            if (!w.empty()) {
                def.ward_ids.push_back(w);
            }
        }
        if (!def.attack_id.empty()) {
            attack_by_id_[def.attack_id] = attacks_.size();
            attacks_.push_back(std::move(def));
        }
    }

    // ── Step 4: Dynamic upgrades ──────────────────────────────────────────────
    for (const auto& row : load_dbf(globals_dir, "GDynUpgr.dbf")) {
        DynUpgradeDef def;
        def.upgrade_id = lower(field(row, "UPGRADE_ID"));
        def.hit_points = safe_int(row, "HIT_POINT");
        def.armor = safe_int(row, "ARMOR");
        def.regen = safe_int(row, "REGEN");
        def.damage = safe_int(row, "DAMAGE");
        def.heal = safe_int(row, "HEAL");
        def.power = safe_int(row, "POWER");
        def.xp_killed = safe_int(row, "XP_KILLED");
        def.xp_next = safe_int(row, "XP_NEXT");
        def.initiative = safe_int(row, "INITIATIVE");
        def.move = safe_int(row, "MOVE");
        def.negotiate = safe_int(row, "NEGOTIATE");
        def.enroll_cost = parse_resource_cost(field(row, "ENROLL_C"));
        def.revive_cost = parse_resource_cost(field(row, "REVIVE_C"));
        def.heal_cost = parse_resource_cost(field(row, "HEAL_C"));
        def.training_cost = parse_resource_cost(field(row, "TRAINING_C"));
        if (!def.upgrade_id.empty()) {
            upgrade_by_id_[def.upgrade_id] = dyn_upgrades_.size();
            dyn_upgrades_.push_back(std::move(def));
        }
    }

    // ── Step 5: Races ─────────────────────────────────────────────────────────
    for (const auto& row : load_dbf(globals_dir, "Grace.dbf")) {
        RaceDef def;
        def.race_id = lower(field(row, "RACE_ID"));
        {
            const std::string ntxt = field(row, "NAME_TXT");
            def.name = text_map_.contains(ntxt) ? text_map_.at(ntxt) : std::string{};
        }
        def.race_type = safe_int(row, "RACE_TYPE");
        def.playable = is_true(row, "PLAYABLE");
        def.regen_heal = safe_int(row, "REGEN_H");
        def.guardian_unit_id = lower(field(row, "GUARDIAN"));
        for (const auto& lk : {"LEADER_1", "LEADER_2", "LEADER_3", "LEADER_4"}) {
            const std::string v = lower(field(row, lk));
            if (!v.empty()) {
                def.leader_unit_ids.push_back(v);
            }
        }
        for (const auto& lk : {"SOLDIER_1", "SOLDIER_2", "SOLDIER_3", "SOLDIER_4", "SOLDIER_5"}) {
            const std::string v = lower(field(row, lk));
            if (!v.empty()) {
                def.soldier_unit_ids.push_back(v);
            }
        }
        if (!def.race_id.empty()) {
            race_by_id_[def.race_id] = races_.size();
            race_by_type_[def.race_type] = races_.size();
            races_.push_back(std::move(def));
        }
    }

    // ── Step 6: Units ─────────────────────────────────────────────────────────
    for (const auto& row : load_dbf(globals_dir, "Gunits.dbf")) {
        UnitDef def;
        def.unit_id = lower(field(row, "UNIT_ID"));
        def.race_id = lower(field(row, "RACE_ID"));
        def.subrace = safe_int(row, "SUBRACE");
        def.level = safe_int(row, "LEVEL", 1);
        def.hit_points = safe_int(row, "HIT_POINT");
        def.armor = safe_int(row, "ARMOR");
        def.regen = safe_int(row, "REGEN");
        def.move = safe_int(row, "MOVE");
        def.scout = safe_int(row, "SCOUT");
        def.leadership = safe_int(row, "LEADERSHIP");
        def.negotiate = safe_int(row, "NEGOTIATE");
        def.xp_killed = safe_int(row, "XP_KILLED");
        def.xp_next = safe_int(row, "XP_NEXT");
        def.size_small = is_true(row, "SIZE_SMALL");
        def.sex_male = is_true(row, "SEX_M");
        def.atck_twice = is_true(row, "ATCK_TWICE");
        def.water_only = is_true(row, "WATER_ONLY");

        const int cat_id = safe_int(row, "UNIT_CAT");
        const int branch_id = safe_int(row, "BRANCH");
        {
            auto it = unit_category_map.find(cat_id);
            def.category = (it != unit_category_map.end()) ? it->second : UnitCategory::Unknown;
        }
        {
            auto it = unit_branch_map.find(branch_id);
            def.branch = (it != unit_branch_map.end()) ? it->second : UnitBranch::Unknown;
        }

        const std::string name_txt = field(row, "NAME_TXT");
        const std::string desc_txt = field(row, "DESC_TXT");
        const std::string abil_txt = field(row, "ABIL_TXT");
        def.name = text_map_.contains(name_txt) ? text_map_.at(name_txt) : std::string{};
        def.description = text_map_.contains(desc_txt) ? text_map_.at(desc_txt) : std::string{};
        def.ability_text = text_map_.contains(abil_txt) ? text_map_.at(abil_txt) : std::string{};

        def.enroll_cost = parse_resource_cost(field(row, "ENROLL_C"));
        def.enroll_building_cost = parse_resource_cost(field(row, "ENROLL_B"));
        def.revive_cost = parse_resource_cost(field(row, "REVIVE_C"));
        def.heal_cost = parse_resource_cost(field(row, "HEAL_C"));
        def.training_cost = parse_resource_cost(field(row, "TRAINING_C"));

        def.primary_attack_id = canonical_optional_attack_id(field(row, "ATTACK_ID"));
        def.secondary_attack_id = canonical_optional_attack_id(field(row, "ATTACK2_ID"));
        def.prev_unit_id = lower(field(row, "PREV_ID"));
        def.base_unit_id = lower(field(row, "BASE_UNIT"));
        def.upgrade_building_id = lower(field(row, "UPGRADE_B"));
        def.dyn_upg1_id = lower(field(row, "DYN_UPG1"));
        def.dyn_upg2_id = lower(field(row, "DYN_UPG2"));
        def.dyn_upg_level = safe_int(row, "DYN_UPG_LV");

        def.death_anim_id = safe_int(row, "DEATH_ANIM", 1);
        {
            auto it = death_anim_map.find(def.death_anim_id);
            def.death_anim_label = (it != death_anim_map.end()) ? it->second : "L_HUMAN";
        }
        def.death_battle_ff_animation = death_battle_ff_name(def.death_anim_label, def.size_small);
        def.bones_battle_ff_sprite_base = bones_sprite_base(def.death_anim_label, def.size_small);

        if (!def.unit_id.empty()) {
            unit_by_id_[def.unit_id] = units_.size();
            units_.push_back(std::move(def));
        }
    }

    // ── Step 7: Cross-link attacks and upgrades into UnitDef ─────────────────
    for (auto& u : units_) {
        if (!u.primary_attack_id.empty()) {
            auto it = attack_by_id_.find(u.primary_attack_id);
            if (it != attack_by_id_.end()) {
                u.primary_attack = &attacks_[it->second];
            }
        }
        if (!u.secondary_attack_id.empty()) {
            auto it = attack_by_id_.find(u.secondary_attack_id);
            if (it != attack_by_id_.end()) {
                u.secondary_attack = &attacks_[it->second];
            }
        }
        if (!u.dyn_upg1_id.empty()) {
            auto it = upgrade_by_id_.find(u.dyn_upg1_id);
            if (it != upgrade_by_id_.end()) {
                u.dyn_upg1 = &dyn_upgrades_[it->second];
            }
        }
        if (!u.dyn_upg2_id.empty()) {
            auto it = upgrade_by_id_.find(u.dyn_upg2_id);
            if (it != upgrade_by_id_.end()) {
                u.dyn_upg2 = &dyn_upgrades_[it->second];
            }
        }
    }

    // ── Step 8: GMabi — native abilities ──────────────────────────────────────
    for (const auto& row : load_dbf(globals_dir, "GMabi.dbf")) {
        const std::string uid = lower(field(row, "UNIT_ID"));
        auto              it = unit_by_id_.find(uid);
        if (it != unit_by_id_.end()) {
            units_[it->second].native_ability_ids.push_back(safe_int(row, "M_ABILITY"));
        }
    }

    // ── Step 9: Gimmu — native immunities ────────────────────────────────────
    for (const auto& row : load_dbf(globals_dir, "Gimmu.dbf")) {
        const std::string uid = lower(field(row, "UNIT_ID"));
        auto              it = unit_by_id_.find(uid);
        if (it != unit_by_id_.end()) {
            units_[it->second].native_immunity_ids.push_back(
                std::to_string(safe_int(row, "IMMUNITY")));
        }
    }

    // ── Step 10: GimmuC — native immunity categories ──────────────────────────
    for (const auto& row : load_dbf(globals_dir, "GimmuC.dbf")) {
        const std::string uid = lower(field(row, "UNIT_ID"));
        auto              it = unit_by_id_.find(uid);
        if (it != unit_by_id_.end()) {
            units_[it->second].native_immunity_categories.push_back(
                std::to_string(safe_int(row, "IMMUNITY")));
        }
    }

    kLog->info("loaded units={} attacks={} upgrades={} races={}", units_.size(), attacks_.size(),
               dyn_upgrades_.size(), races_.size());
}

// ── lookup methods ────────────────────────────────────────────────────────────

const UnitDef* GameDataRegistry::find_unit(std::string_view id) const {
    const auto key = lower(std::string{id});
    auto       it = unit_by_id_.find(key);
    return (it != unit_by_id_.end()) ? &units_[it->second] : nullptr;
}

const RaceDef* GameDataRegistry::find_race(std::string_view race_id) const {
    const auto key = lower(std::string{race_id});
    auto       it = race_by_id_.find(key);
    return (it != race_by_id_.end()) ? &races_[it->second] : nullptr;
}

const RaceDef* GameDataRegistry::find_race_by_type(int race_type) const {
    auto it = race_by_type_.find(race_type);
    return (it != race_by_type_.end()) ? &races_[it->second] : nullptr;
}

std::string GameDataRegistry::resolve_visual_type(std::string_view     concrete_unit_id,
                                                  const VisualCheckFn& has_visual) const {
    std::string start = lower(std::string{concrete_unit_id});

    std::unordered_set<std::string> visited;
    std::string                     current = start;

    for (int depth = 0; depth < 16; ++depth) {
        if (!visited.insert(current).second)
            return start; // cycle → use concrete

        if (has_visual(current))
            return current;

        const auto* def = find_unit(current);
        if (def == nullptr || def->base_unit_id.empty())
            return start; // chain ends → use concrete

        const std::string base = lower(def->base_unit_id);
        if (base == current)
            return start;

        current = base;
    }

    return start; // guard: too deep → use concrete
}

} // namespace d2engine
