#include <gtest/gtest.h>

#include "d2engine/assets/portrait_manifest.hpp"

#include <map>
#include <string>
#include <vector>

namespace d2engine {

// ── normalize_unit_id_to_resource ────────────────────────────────────

TEST(NormalizeUnitIdToResource, G000UN_ConvertsToG000UU) {
    EXPECT_EQ(normalize_unit_id_to_resource("G000UN0001"), "G000UU0001");
}

TEST(NormalizeUnitIdToResource, G000UU_StaysG000UU) {
    EXPECT_EQ(normalize_unit_id_to_resource("G000UU0001"), "G000UU0001");
}

TEST(NormalizeUnitIdToResource, LowercaseInputHandled) {
    EXPECT_EQ(normalize_unit_id_to_resource("g000un0001"), "G000UU0001");
}

TEST(NormalizeUnitIdToResource, MixedCaseInput) {
    EXPECT_EQ(normalize_unit_id_to_resource("G000uN0001"), "G000UU0001");
}

TEST(NormalizeUnitIdToResource, ShortInputUnchanged) {
    EXPECT_EQ(normalize_unit_id_to_resource("AB"), "AB");
}

// ── build_portrait_manifest_from_names ───────────────────────────────

TEST(BuildPortraitManifestFromNames, EmptyInputs) {
    const auto m = build_portrait_manifest_from_names({}, {});
    EXPECT_EQ(m.schema_version, 1);
    EXPECT_EQ(m.container, "Imgs/Faces.ff");
    EXPECT_TRUE(m.units.empty());
    EXPECT_TRUE(m.warnings.empty());
}

TEST(BuildPortraitManifestFromNames, PairsFaceAndFacebByResourceId) {
    const std::vector<std::string> names = {
        "G000UU0001FACE.PNG",
        "G000UU0001FACEB.PNG",
    };
    const std::vector<std::map<std::string, std::string>> gunits = {
        {{"UNIT_ID", "G000UN0001"}},
    };

    const auto m = build_portrait_manifest_from_names(names, gunits);
    ASSERT_EQ(m.units.size(), 1u);
    EXPECT_EQ(m.units[0].unit_id, "g000uu0001");
    EXPECT_EQ(m.units[0].resource_unit_id, "G000UU0001");
    EXPECT_EQ(m.units[0].face_record_name, "G000UU0001FACE.PNG");
    EXPECT_EQ(m.units[0].faceb_record_name, "G000UU0001FACEB.PNG");
    EXPECT_TRUE(m.warnings.empty()) << "warnings: " << testing::PrintToString(m.warnings);
}

TEST(BuildPortraitManifestFromNames, MissingFaceProducesWarning) {
    const std::vector<std::string> names = {
        "G000UU0001FACEB.PNG",
    };
    const std::vector<std::map<std::string, std::string>> gunits = {
        {{"UNIT_ID", "G000UN0001"}},
    };

    const auto m = build_portrait_manifest_from_names(names, gunits);
    ASSERT_EQ(m.units.size(), 1u);
    EXPECT_EQ(m.units[0].face_record_name, "");
    EXPECT_EQ(m.units[0].faceb_record_name, "G000UU0001FACEB.PNG");
    ASSERT_FALSE(m.warnings.empty());
    bool found = false;
    for (const auto& w : m.warnings) {
        if (w.find("Missing FACE ") != std::string::npos)
            found = true;
    }
    EXPECT_TRUE(found);
}

TEST(BuildPortraitManifestFromNames, MissingFacebProducesWarning) {
    const std::vector<std::string> names = {
        "G000UU0001FACE.PNG",
    };
    const std::vector<std::map<std::string, std::string>> gunits = {
        {{"UNIT_ID", "G000UN0001"}},
    };

    const auto m = build_portrait_manifest_from_names(names, gunits);
    ASSERT_EQ(m.units.size(), 1u);
    EXPECT_EQ(m.units[0].face_record_name, "G000UU0001FACE.PNG");
    EXPECT_EQ(m.units[0].faceb_record_name, "");
    ASSERT_FALSE(m.warnings.empty());
    bool found = false;
    for (const auto& w : m.warnings) {
        if (w.find("Missing FACEB") != std::string::npos)
            found = true;
    }
    EXPECT_TRUE(found);
}

TEST(BuildPortraitManifestFromNames, IgnoresNonPortraitRecords) {
    const std::vector<std::string> names = {
        "G000UU0001FACE.PNG",       "SOME_OTHER_RECORD",
        "G000UV0001FACE.PNG", // wrong prefix
        "G000UU000XFACE.PNG", // non-digit character
        "G000UU0001FACEB.PNG",
        "G000UU0001FACE.PNG.EXTRA", // doesn't end with .PNG exactly
        "G000UU12345FACE.PNG",      // 5 digits instead of 4
    };
    const std::vector<std::map<std::string, std::string>> gunits = {
        {{"UNIT_ID", "G000UN0001"}},
    };

    const auto m = build_portrait_manifest_from_names(names, gunits);
    ASSERT_EQ(m.units.size(), 1u);
    EXPECT_EQ(m.units[0].face_record_name, "G000UU0001FACE.PNG");
    EXPECT_EQ(m.units[0].faceb_record_name, "G000UU0001FACEB.PNG");
}

TEST(BuildPortraitManifestFromNames, SortedByUnitId) {
    const std::vector<std::map<std::string, std::string>> gunits = {
        {{"UNIT_ID", "G000UN0003"}},
        {{"UNIT_ID", "G000UN0001"}},
        {{"UNIT_ID", "G000UN0002"}},
    };
    const std::vector<std::string> names = {
        "G000UU0002FACE.PNG",
        "G000UU0003FACE.PNG",
        "G000UU0001FACE.PNG",
    };

    const auto m = build_portrait_manifest_from_names(names, gunits);
    ASSERT_EQ(m.units.size(), 3u);
    EXPECT_EQ(m.units[0].unit_id, "g000uu0001");
    EXPECT_EQ(m.units[1].unit_id, "g000uu0002");
    EXPECT_EQ(m.units[2].unit_id, "g000uu0003");
}

TEST(BuildPortraitManifestFromNames, WarnsUnlinkedFacesRecords) {
    const std::vector<std::string> names = {
        "G000UU0001FACE.PNG",
        "G000UU9999FACE.PNG",
    };
    const std::vector<std::map<std::string, std::string>> gunits = {
        {{"UNIT_ID", "G000UN0001"}},
    };

    const auto m = build_portrait_manifest_from_names(names, gunits);
    ASSERT_EQ(m.units.size(), 1u);
    bool found = false;
    for (const auto& w : m.warnings) {
        if (w.find("unlinked") != std::string::npos && w.find("G000UU9999") != std::string::npos) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST(BuildPortraitManifestFromNames, WarningsAreDeduplicated) {
    const std::vector<std::map<std::string, std::string>> gunits = {
        {{"UNIT_ID", "G000UN0001"}},
        {{"UNIT_ID", "G000UN0001"}},
    };

    const auto m = build_portrait_manifest_from_names({}, gunits);
    ASSERT_EQ(m.units.size(), 2u);

    int missing_face_count = 0;
    int missing_faceb_count = 0;
    for (const auto& w : m.warnings) {
        if (w.find("Missing FACE ") != std::string::npos)
            ++missing_face_count;
        if (w.find("Missing FACEB") != std::string::npos)
            ++missing_faceb_count;
    }
    EXPECT_EQ(missing_face_count, 1);
    EXPECT_EQ(missing_faceb_count, 1);
    EXPECT_EQ(m.warnings.size(), 2u) << "warnings: " << testing::PrintToString(m.warnings);
}

TEST(BuildPortraitManifestFromNames, GunitsMissingUnitIdGetsWarning) {
    const std::vector<std::map<std::string, std::string>> gunits = {
        {{"OTHER_FIELD", "value"}},
    };

    const auto m = build_portrait_manifest_from_names({}, gunits);
    EXPECT_TRUE(m.units.empty());
    ASSERT_FALSE(m.warnings.empty());
    EXPECT_TRUE(m.warnings[0].find("missing UNIT_ID") != std::string::npos);
}

} // namespace d2engine
