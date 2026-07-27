#include <gtest/gtest.h>
#include <string>
#include <unordered_set>

#include "cli/manifest_builder.hpp"

// ── make_id ──────────────────────────────────────────────────────────────────

TEST(ManifestBuilderMakeId, LowercasesInput) {
    EXPECT_EQ(ManifestBuilder::make_id("Imgs/Battle.ff"), "imgs/battle.ff");
}

TEST(ManifestBuilderMakeId, BackslashBecomesForwardSlash) {
    EXPECT_EQ(ManifestBuilder::make_id("Sounds\\Capital.wdb"), "sounds/capital.wdb");
}

TEST(ManifestBuilderMakeId, MixedSeparatorsNormalized) {
    EXPECT_EQ(ManifestBuilder::make_id("Imgs\\Battle.ff/SUB"), "imgs/battle.ff/sub");
}

TEST(ManifestBuilderMakeId, AlreadyLowerUnchanged) {
    EXPECT_EQ(ManifestBuilder::make_id("imgs/battle.ff"), "imgs/battle.ff");
}

TEST(ManifestBuilderMakeId, ReplacesNonCanonicalCharacters) {
    EXPECT_EQ(ManifestBuilder::make_id("FIFROSTBITE COPY"), "fifrostbite_copy");
    EXPECT_EQ(ManifestBuilder::make_id("TORA'ACH"), "tora_ach");
    EXPECT_EQ(ManifestBuilder::make_id("0025_HIT_B(1)"), "0025_hit_b_1_");
}

// ── asset_id construction ─────────────────────────────────────────────────────

TEST(ManifestBuilderAssetId, AssetIdIsLowercase) {
    ManifestBuilder builder;
    builder.add_asset("image", "SELSMALLA", "Imgs/Battle.ff", "Imgs/Battle.ff/SELSMALLA.json");
    const nlohmann::json j = builder.to_json();
    const std::string    id = j["assets"][0]["asset_id"].get<std::string>();
    for (const char c : id) {
        EXPECT_FALSE(c >= 'A' && c <= 'Z') << "uppercase char in asset_id: " << id;
    }
}

TEST(ManifestBuilderAssetId, AssetIdEncodesBothParts) {
    ManifestBuilder builder;
    builder.add_asset("image", "SELSMALLA", "Imgs/Battle.ff", "Imgs/Battle.ff/SELSMALLA.json");
    const nlohmann::json j = builder.to_json();
    const std::string    id = j["assets"][0]["asset_id"].get<std::string>();
    EXPECT_EQ(id, "imgs/battle.ff/selsmalla");
}

// ── logical_name casing preserved ─────────────────────────────────────────────

TEST(ManifestBuilderLogicalName, OriginalCasingPreserved) {
    ManifestBuilder builder;
    builder.add_asset("animation", "SELSMALLA", "Imgs/Battle.ff",
                      "Imgs/Battle.ff/SELSMALLA/anim.json");
    const nlohmann::json j = builder.to_json();
    const std::string    name = j["assets"][0]["logical_name"].get<std::string>();
    EXPECT_EQ(name, "SELSMALLA");
}

TEST(ManifestBuilderLogicalName, AssetIdIsLowercaseWhileNameIsNot) {
    ManifestBuilder builder;
    builder.add_asset("image", "MixedCase", "Imgs/Battle.ff", "Imgs/Battle.ff/MixedCase.json");
    const nlohmann::json j = builder.to_json();
    EXPECT_EQ(j["assets"][0]["logical_name"].get<std::string>(), "MixedCase");
    EXPECT_EQ(j["assets"][0]["asset_id"].get<std::string>(), "imgs/battle.ff/mixedcase");
}

// ── uniqueness ────────────────────────────────────────────────────────────────

TEST(ManifestBuilderUniqueness, AllAssetIdsUnique) {
    ManifestBuilder builder;
    builder.add_asset("image", "IMG_A", "Imgs/Battle.ff", "Imgs/Battle.ff/IMG_A.json");
    builder.add_asset("image", "IMG_B", "Imgs/Battle.ff", "Imgs/Battle.ff/IMG_B.json");
    builder.add_asset("sound", "SND_A", "Sounds/Battle.wdb", "Sounds/Battle.wdb/SND_A.json");

    const nlohmann::json            j = builder.to_json();
    std::unordered_set<std::string> ids;
    for (const auto& a : j["assets"]) {
        const std::string id = a["asset_id"].get<std::string>();
        EXPECT_TRUE(ids.insert(id).second) << "duplicate asset_id: " << id;
    }
}

// ── schema version ────────────────────────────────────────────────────────────

TEST(ManifestBuilderSchema, ContainsAssetSchemaVersion1) {
    const ManifestBuilder builder;
    const nlohmann::json  j = builder.to_json();
    ASSERT_TRUE(j.contains("asset_schema_version"));
    EXPECT_EQ(j["asset_schema_version"].get<int>(), 1);
}

// ── container_id forward slashes ─────────────────────────────────────────────

TEST(ManifestBuilderContainerId, UsesForwardSlashes) {
    ManifestBuilder builder;
    builder.add_container("Sounds\\Capital.wdb", {"sounds"});
    const nlohmann::json j = builder.to_json();
    const std::string    cid = j["containers"][0]["container_id"].get<std::string>();
    EXPECT_EQ(cid.find('\\'), std::string::npos) << "backslash found in container_id: " << cid;
    EXPECT_NE(cid.find('/'), std::string::npos);
}
