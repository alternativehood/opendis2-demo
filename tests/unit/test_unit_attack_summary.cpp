#include <gtest/gtest.h>

#include "d2engine/battle_view/unit_attack_summary.hpp"
#include "d2engine/battle_view/unit_id_helpers.hpp"
#include "d2engine/assets/attack_def.hpp"
#include "d2engine/assets/unit_def.hpp"

#include <memory>

namespace d2engine {
namespace {

struct AttackSummaryTest : ::testing::Test {
    // Helper: create a UnitDef with an optional AttackDef
    static std::unique_ptr<UnitDef> make_unit(const std::string& unit_id, const std::string& name,
                                              AttackDef* attack) {
        auto def = std::make_unique<UnitDef>();
        def->unit_id = unit_id;
        def->name = name;
        def->primary_attack = attack;
        return def;
    }

    static std::unique_ptr<AttackDef> make_attack(AttackReach reach, AttackClass cls,
                                                  int damage = 10) {
        auto att = std::make_unique<AttackDef>();
        att->attack_id = "AT001";
        att->reach = reach;
        att->attack_class = cls;
        att->damage = damage;
        att->source = AttackSource::Weapon;
        return att;
    }

    // Dummy registry — not used by extract() when UnitDef is non-null
    const GameDataRegistry registry_{std::filesystem::temp_directory_path() /
                                     "opendis2-missing-globals"};
};

TEST_F(AttackSummaryTest, UnitTypeFromResourceUnitId) {
    EXPECT_EQ(unit_type_from_resource_unit_id("g000uu8027"), "UU8027");
    EXPECT_TRUE(unit_type_from_resource_unit_id("g000").empty());
    EXPECT_TRUE(unit_type_from_resource_unit_id("").empty());
}

TEST_F(AttackSummaryTest, ResourceUnitIdFromUnitType) {
    EXPECT_EQ(resource_unit_id_from_unit_type("UU8027"), "g000uu8027");
    EXPECT_EQ(resource_unit_id_from_unit_type("UU0008"), "g000uu0008");
}

TEST_F(AttackSummaryTest, RoundTrip) {
    const std::string original = "g000uu8027";
    const auto        type = unit_type_from_resource_unit_id(original);
    const auto        round = resource_unit_id_from_unit_type(type);
    EXPECT_EQ(original, round);
}

TEST_F(AttackSummaryTest, UnknownReachGivesUnknownTargetShape) {
    auto       attack = make_attack(AttackReach::Unknown, AttackClass::Damage);
    auto       def = make_unit("g000uu0001", "Test", attack.get());
    const auto summary = UnitAttackSummaryExtractor::extract(def.get(), registry_);
    EXPECT_EQ(summary.attack_type, AttackType::Unknown);
    EXPECT_EQ(summary.target_shape, TargetShape::Unknown);
    bool found_warning = false;
    for (const auto& w : summary.warnings) {
        if (w.find("AttackReach is Unknown") != std::string::npos) {
            found_warning = true;
            break;
        }
    }
    EXPECT_TRUE(found_warning);
}

TEST_F(AttackSummaryTest, MissingPrimaryAttackGivesWarnings) {
    auto       def = make_unit("g000uu0002", "NoAttack", nullptr);
    const auto summary = UnitAttackSummaryExtractor::extract(def.get(), registry_);
    EXPECT_EQ(summary.attack_type, AttackType::Unknown);
    EXPECT_EQ(summary.target_shape, TargetShape::Unknown);
    EXPECT_EQ(summary.post_effect.presence, PostEffectPresence::Unknown);
    bool found = false;
    for (const auto& w : summary.warnings) {
        if (w.find("no primary attack") != std::string::npos) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(AttackSummaryTest, DamageClassGivesHitPostEffectNone) {
    auto       attack = make_attack(AttackReach::Adjacent, AttackClass::Damage);
    auto       def = make_unit("g000uu0003", "Warrior", attack.get());
    const auto summary = UnitAttackSummaryExtractor::extract(def.get(), registry_);
    EXPECT_EQ(summary.attack_type, AttackType::Melee);
    EXPECT_EQ(summary.target_shape, TargetShape::SingleTarget);
    EXPECT_EQ(summary.post_effect.presence, PostEffectPresence::None);
    ASSERT_TRUE(summary.damage.has_value());
    EXPECT_EQ(*summary.damage, 10);
}

TEST_F(AttackSummaryTest, HealClassDoesNotLookLikeDamage) {
    auto       attack = make_attack(AttackReach::Adjacent, AttackClass::Heal, 0);
    auto       def = make_unit("g000uu0004", "Healer", attack.get());
    const auto summary = UnitAttackSummaryExtractor::extract(def.get(), registry_);
    EXPECT_FALSE(summary.damage.has_value());
    EXPECT_EQ(summary.post_effect.presence, PostEffectPresence::None);
    bool found = false;
    for (const auto& w : summary.warnings) {
        if (w.find("Heal/support") != std::string::npos) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(AttackSummaryTest, AoeReachGivesAllEnemyUnits) {
    auto       attack = make_attack(AttackReach::All, AttackClass::Damage);
    auto       def = make_unit("g000uu0005", "AoE", attack.get());
    const auto summary = UnitAttackSummaryExtractor::extract(def.get(), registry_);
    EXPECT_EQ(summary.attack_type, AttackType::Aoe);
    EXPECT_EQ(summary.target_shape, TargetShape::AllEnemyUnits);
}

TEST_F(AttackSummaryTest, MeleeReachGivesSingleTarget) {
    auto       attack = make_attack(AttackReach::Adjacent, AttackClass::Damage);
    auto       def = make_unit("g000uu0006", "Swordsman", attack.get());
    const auto summary = UnitAttackSummaryExtractor::extract(def.get(), registry_);
    EXPECT_EQ(summary.attack_type, AttackType::Melee);
    EXPECT_EQ(summary.target_shape, TargetShape::SingleTarget);
}

TEST_F(AttackSummaryTest, RangedReachGivesSingleTarget) {
    auto       attack = make_attack(AttackReach::Any, AttackClass::Damage);
    auto       def = make_unit("g000uu0007", "Archer", attack.get());
    const auto summary = UnitAttackSummaryExtractor::extract(def.get(), registry_);
    EXPECT_EQ(summary.attack_type, AttackType::Ranged);
    EXPECT_EQ(summary.target_shape, TargetShape::SingleTarget);
}

TEST_F(AttackSummaryTest, AnyReachWithWeaponSourceGivesRanged) {
    auto attack = make_attack(AttackReach::Any, AttackClass::Damage);
    attack->source = AttackSource::Weapon;
    auto       def = make_unit("g000uu0007", "Archer", attack.get());
    const auto summary = UnitAttackSummaryExtractor::extract(def.get(), registry_);
    EXPECT_EQ(summary.attack_type, AttackType::Ranged);
    EXPECT_EQ(summary.target_shape, TargetShape::SingleTarget);
}

TEST_F(AttackSummaryTest, AnyReachWithFireSourceGivesRanged) {
    auto attack = make_attack(AttackReach::Any, AttackClass::Damage);
    attack->source = AttackSource::Fire;
    auto       def = make_unit("g000uu0007", "FireMage", attack.get());
    const auto summary = UnitAttackSummaryExtractor::extract(def.get(), registry_);
    EXPECT_EQ(summary.attack_type, AttackType::Ranged);
    EXPECT_EQ(summary.target_shape, TargetShape::SingleTarget);
}

TEST_F(AttackSummaryTest, AdjacentReachWithAnySourceGivesMelee) {
    auto attack = make_attack(AttackReach::Adjacent, AttackClass::Damage);
    attack->source = AttackSource::Fire;
    auto       def = make_unit("g000uu0007", "FireMelee", attack.get());
    const auto summary = UnitAttackSummaryExtractor::extract(def.get(), registry_);
    EXPECT_EQ(summary.attack_type, AttackType::Melee);
    EXPECT_EQ(summary.target_shape, TargetShape::SingleTarget);
}

TEST_F(AttackSummaryTest, PoisonGivesPresentPostEffect) {
    auto       attack = make_attack(AttackReach::Adjacent, AttackClass::Poison);
    auto       def = make_unit("g000uu0008", "Poisoner", attack.get());
    const auto summary = UnitAttackSummaryExtractor::extract(def.get(), registry_);
    EXPECT_EQ(summary.post_effect.presence, PostEffectPresence::Present);
    EXPECT_EQ(summary.post_effect.kind, "poison");
}

TEST_F(AttackSummaryTest, FormatterIncludesResourceUnitId) {
    auto       attack = make_attack(AttackReach::Adjacent, AttackClass::Damage);
    auto       def = make_unit("g000uu0008", "Warrior", attack.get());
    const auto summary = UnitAttackSummaryExtractor::extract(def.get(), registry_);
    const auto comment = format_unit_attack_summary_comment(summary, "hero", "A_FRONT_0");
    EXPECT_NE(comment.find("resource_unit_id: g000uu0008"), std::string::npos);
}

TEST_F(AttackSummaryTest, FormatterUsesCanonicalUnitType) {
    auto       attack = make_attack(AttackReach::Adjacent, AttackClass::Damage);
    auto       def = make_unit("g000uu0008", "Warrior", attack.get());
    const auto summary = UnitAttackSummaryExtractor::extract(def.get(), registry_);
    const auto comment = format_unit_attack_summary_comment(summary, "hero", "A_FRONT_0");
    EXPECT_NE(comment.find("unit_type: UU0008"), std::string::npos);
}

TEST_F(AttackSummaryTest, FormatterHealDamageIsUnknown) {
    auto       attack = make_attack(AttackReach::Adjacent, AttackClass::Heal);
    auto       def = make_unit("g000uu0009", "Healer", attack.get());
    const auto summary = UnitAttackSummaryExtractor::extract(def.get(), registry_);
    const auto comment = format_unit_attack_summary_comment(summary, "medic", "D_FRONT_0");
    EXPECT_NE(comment.find("damage: unknown"), std::string::npos);
}

TEST_F(AttackSummaryTest, FormatterPrintsPostEffectKindWhenPresent) {
    auto       attack = make_attack(AttackReach::Adjacent, AttackClass::Poison);
    auto       def = make_unit("g000uu0010", "Toxic", attack.get());
    const auto summary = UnitAttackSummaryExtractor::extract(def.get(), registry_);
    const auto comment = format_unit_attack_summary_comment(summary, "toxin", "A_FRONT_0");
    EXPECT_NE(comment.find("hit_post_effect: present"), std::string::npos);
    EXPECT_NE(comment.find("post_effect_kind: poison"), std::string::npos);
}

TEST_F(AttackSummaryTest, FormatterPrintsPostEffectKindParalyze) {
    auto       attack = make_attack(AttackReach::Adjacent, AttackClass::Paralyze);
    auto       def = make_unit("g000uu0011", "Stunner", attack.get());
    const auto summary = UnitAttackSummaryExtractor::extract(def.get(), registry_);
    const auto comment = format_unit_attack_summary_comment(summary, "stun", "A_FRONT_0");
    EXPECT_NE(comment.find("post_effect_kind: paralyze"), std::string::npos);
}

TEST_F(AttackSummaryTest, FormatterPrintsPostEffectKindDrain) {
    auto       attack = make_attack(AttackReach::Adjacent, AttackClass::Drain);
    auto       def = make_unit("g000uu0012", "Leech", attack.get());
    const auto summary = UnitAttackSummaryExtractor::extract(def.get(), registry_);
    const auto comment = format_unit_attack_summary_comment(summary, "leech", "A_FRONT_0");
    EXPECT_NE(comment.find("post_effect_kind: drain"), std::string::npos);
}

TEST_F(AttackSummaryTest, FormatterPrintsPostEffectRawId) {
    auto attack = make_attack(AttackReach::Adjacent, AttackClass::Poison);
    attack->attack_id = "AT9999";
    auto       def = make_unit("g000uu0013", "Custom", attack.get());
    const auto summary = UnitAttackSummaryExtractor::extract(def.get(), registry_);
    const auto comment = format_unit_attack_summary_comment(summary, "custom", "A_FRONT_0");
    EXPECT_NE(comment.find("post_effect_raw: AT9999"), std::string::npos);
}

TEST_F(AttackSummaryTest, DamageOnlyAttackDoesNotPrintPostEffectKind) {
    auto       attack = make_attack(AttackReach::Adjacent, AttackClass::Damage);
    auto       def = make_unit("g000uu0014", "PureDamage", attack.get());
    const auto summary = UnitAttackSummaryExtractor::extract(def.get(), registry_);
    const auto comment = format_unit_attack_summary_comment(summary, "dmg", "A_FRONT_0");
    EXPECT_NE(comment.find("hit_post_effect: none"), std::string::npos);
    EXPECT_EQ(comment.find("post_effect_kind:"), std::string::npos);
}

// ── format_unit_attack_summary_comment: size and footprint ───────────────────

TEST_F(AttackSummaryTest, SmallUnitCommentIncludesSizeSmall) {
    auto attack = make_attack(AttackReach::Adjacent, AttackClass::Damage);
    auto def = make_unit("g000uu0020", "Fighter", attack.get());
    def->size_small = true;
    const auto summary = UnitAttackSummaryExtractor::extract(def.get(), registry_);
    const auto comment = format_unit_attack_summary_comment(summary, "fighter", "A_FRONT_0");
    EXPECT_NE(comment.find("size: small"), std::string::npos);
}

TEST_F(AttackSummaryTest, SmallUnitCommentDoesNotIncludeFootprint) {
    auto attack = make_attack(AttackReach::Adjacent, AttackClass::Damage);
    auto def = make_unit("g000uu0021", "Fighter2", attack.get());
    def->size_small = true;
    const auto summary = UnitAttackSummaryExtractor::extract(def.get(), registry_);
    const auto comment = format_unit_attack_summary_comment(summary, "fighter2", "A_FRONT_1");
    EXPECT_EQ(comment.find("footprint:"), std::string::npos);
}

TEST_F(AttackSummaryTest, LargeUnitCommentIncludesSizeLarge) {
    auto attack = make_attack(AttackReach::Adjacent, AttackClass::Damage);
    auto def = make_unit("g000uu5021", "Dragon", attack.get());
    def->size_small = false;
    const auto summary = UnitAttackSummaryExtractor::extract(def.get(), registry_);
    const auto comment = format_unit_attack_summary_comment(summary, "dragon", "D_CENTER_1");
    EXPECT_NE(comment.find("size: large"), std::string::npos);
}

TEST_F(AttackSummaryTest, LargeUnitCommentIncludesFootprint) {
    auto attack = make_attack(AttackReach::Adjacent, AttackClass::Damage);
    auto def = make_unit("g000uu5021", "Dragon", attack.get());
    def->size_small = false;
    const auto summary = UnitAttackSummaryExtractor::extract(def.get(), registry_);
    const auto comment = format_unit_attack_summary_comment(summary, "dragon", "D_CENTER_1");
    EXPECT_NE(comment.find("footprint: D_FRONT_1 + D_BACK_1"), std::string::npos);
}

} // namespace
} // namespace d2engine
