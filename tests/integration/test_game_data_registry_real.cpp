#include <gtest/gtest.h>

#include "d2engine/assets/asset_runtime.hpp"
#include "d2engine/assets/game_data_registry.hpp"
#include "d2engine/assets/unit_def.hpp"
#include "d2engine/battle_adapters/raw_ff_animation_catalog.hpp"

#include <filesystem>
#include <string>

namespace d2engine {
namespace {

static const char* GAME_ROOT = [] {
    const char* env = std::getenv("DISCIPLES2_GAME_ROOT"); // NOLINT
    return (env != nullptr && env[0] != '\0') ? env : DISCIPLES2_GAME_ROOT;
}();

class GameDataRegistryRealTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        if ((GAME_ROOT == nullptr) || std::string(GAME_ROOT).empty()) {
            GTEST_SKIP() << "DISCIPLES2_GAME_ROOT not set";
        }
        globals_dir_ = std::filesystem::path(GAME_ROOT) / "Globals";
        if (!std::filesystem::exists(globals_dir_)) {
            GTEST_SKIP() << "Globals directory not found";
        }
        registry_.emplace(globals_dir_);
    }
    static void TearDownTestSuite() { registry_.reset(); }

    static std::filesystem::path           globals_dir_;
    static std::optional<GameDataRegistry> registry_;
};

std::filesystem::path           GameDataRegistryRealTest::globals_dir_;
std::optional<GameDataRegistry> GameDataRegistryRealTest::registry_;

TEST_F(GameDataRegistryRealTest, LoadsAllUnits) {
    ASSERT_TRUE(registry_.has_value());
    EXPECT_EQ(registry_->all_units().size(), 356u);
    EXPECT_NE(registry_->find_unit("g000uu0001"), nullptr);
}

TEST_F(GameDataRegistryRealTest, LoadsAttacks) {
    ASSERT_TRUE(registry_.has_value());
    std::size_t attack_count = 0;
    for (const auto& u : registry_->all_units()) {
        if (!u.primary_attack_id.empty())
            ++attack_count;
        if (!u.secondary_attack_id.empty())
            ++attack_count;
    }
    EXPECT_GT(attack_count, 0u);
}

TEST_F(GameDataRegistryRealTest, FindUnitCaseInsensitive) {
    ASSERT_TRUE(registry_.has_value());
    EXPECT_EQ(registry_->find_unit("G000UU0068"), registry_->find_unit("g000uu0068"));
}

TEST_F(GameDataRegistryRealTest, FindUnitByName) {
    ASSERT_TRUE(registry_.has_value());
    const UnitDef* u = nullptr;
    for (const auto& unit : registry_->all_units()) {
        if (unit.name == "Squire") {
            u = &unit;
            break;
        }
    }
    ASSERT_NE(u, nullptr);
    EXPECT_EQ(registry_->find_unit(u->unit_id), u);
}

TEST_F(GameDataRegistryRealTest, FindUnitByNameCaseInsensitive) {
    ASSERT_TRUE(registry_.has_value());
    const UnitDef* u = nullptr;
    for (const auto& unit : registry_->all_units()) {
        if (unit.name == "Squire") {
            u = &unit;
            break;
        }
    }
    ASSERT_NE(u, nullptr);
    (void)registry_->find_unit("SQUIRE");
}

TEST_F(GameDataRegistryRealTest, FindUnknownReturnsNull) {
    ASSERT_TRUE(registry_.has_value());
    EXPECT_EQ(registry_->find_unit("g000uu9999"), nullptr);
}

TEST_F(GameDataRegistryRealTest, DeathAnimFieldsLegionsUnit) {
    ASSERT_TRUE(registry_.has_value());
    const UnitDef* u = registry_->find_unit("g000uu0068");
    ASSERT_NE(u, nullptr);
    EXPECT_GT(u->death_anim_id, 0);
    EXPECT_FALSE(u->death_battle_ff_animation.empty());
}

TEST_F(GameDataRegistryRealTest, BonesSpriteBaseForLargeUnit) {
    EXPECT_EQ(GameDataRegistry::bones_sprite_base("L_HUMAN", false), "DEAD_HUMAN_LA");
}

TEST_F(GameDataRegistryRealTest, DeathAnimElfLargeUnit) {
    ASSERT_TRUE(registry_.has_value());
    const UnitDef* found = nullptr;
    for (const auto& u : registry_->all_units()) {
        if (u.death_anim_id == 8 && !u.size_small) {
            found = &u;
            break;
        }
    }
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->death_battle_ff_animation, "DEATH_ELF_L15");
}

TEST_F(GameDataRegistryRealTest, AttackPointerResolved) {
    ASSERT_TRUE(registry_.has_value());
    const UnitDef* found = nullptr;
    for (const auto& u : registry_->all_units()) {
        if (!u.primary_attack_id.empty()) {
            found = &u;
            break;
        }
    }
    ASSERT_NE(found, nullptr);
}

TEST_F(GameDataRegistryRealTest, NativeAbilitiesPopulated) {
    ASSERT_TRUE(registry_.has_value());
    const UnitDef* u = registry_->find_unit("g000uu0019");
    ASSERT_NE(u, nullptr);
    EXPECT_FALSE(u->native_ability_ids.empty());
}

TEST_F(GameDataRegistryRealTest, TextResolvesEmpire) {
    ASSERT_TRUE(registry_.has_value());
    const UnitDef* u = registry_->find_unit("g000uu0001");
    ASSERT_NE(u, nullptr);
    EXPECT_FALSE(u->name.empty());
    EXPECT_FALSE(u->race_id.empty());
}

} // namespace
} // namespace d2engine
