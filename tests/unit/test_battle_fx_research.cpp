#include "cli/commands_research.hpp"

#include <gtest/gtest.h>

using json = nlohmann::json;

TEST(BattleFxResearch, LinksUnitToAttackRecord) {
    const std::map<std::string, std::map<std::string, std::string>> units{
        {"g000uu5352",
         {{"UNIT_ID", "G000UU5352"},
          {"BASE_UNIT", "g000uu0152"},
          {"ATTACK_ID", "G000AT0001"},
          {"ATTACK2_ID", "G000AT0002"},
          {"DEATH_ANIM", "7"}}}};
    const std::map<std::string, std::map<std::string, std::string>> attacks{
        {"g000at0001",
         {{"ATT_ID", "G000AT0001"},
          {"SOURCE", "4"},
          {"CLASS", "1"},
          {"REACH", "2"},
          {"ALT_ATTACK", ""},
          {"QTY_DAM", "50"},
          {"QTY_HEAL", "0"},
          {"POWER", "0"},
          {"INFINITE", "F"},
          {"CRIT_HIT", "T"}}},
        {"g000at0002", {{"ATT_ID", "G000AT0002"}, {"SOURCE", "1"}, {"CLASS", "2"}}}};

    const auto linkage = d2cli_research::build_gameplay_linkage("UU5352", units, attacks);

    EXPECT_EQ(linkage["gunit_id"], "g000uu5352");
    EXPECT_EQ(linkage["base_unit_id"], "g000uu0152");
    EXPECT_EQ(linkage["attack_id"], "g000at0001");
    EXPECT_EQ(linkage["secondary_attack_id"], "g000at0002");
    EXPECT_EQ(linkage["death_animation"], "7");
    EXPECT_EQ(linkage["primary_attack"]["SOURCE"], "4");
    EXPECT_TRUE(linkage["diagnostics"].empty());
}

TEST(BattleFxResearch, ReportsMissingAttackReference) {
    const std::map<std::string, std::map<std::string, std::string>> units{
        {"g000uu5352",
         {{"UNIT_ID", "G000UU5352"}, {"ATTACK_ID", "G000AT404"}, {"DEATH_ANIM", "7"}}}};
    const std::map<std::string, std::map<std::string, std::string>> attacks;

    const auto linkage = d2cli_research::build_gameplay_linkage("UU5352", units, attacks);

    ASSERT_EQ(linkage["diagnostics"].size(), 1);
    EXPECT_EQ(linkage["diagnostics"][0]["message"], "missing Gattacks record");
    EXPECT_EQ(linkage["diagnostics"][0]["attack_id"], "g000at404");
}

TEST(BattleFxResearch, ClassifiesFullCanvasAdditiveMorphology) {
    const json frames =
        json::array({{{"width", 800},
                      {"height", 600},
                      {"research_metadata",
                       {{"transparency_mode", "AdditiveBlend"},
                        {"parts", json::array({{{"dest", {{"x", 10}, {"y", 20}}},
                                                {"source_rect", {{"w", 30}, {"h", 40}}}}})}}}}});

    const auto summary = d2cli_research::summarize_role_morphology(frames, 2);

    EXPECT_EQ(summary["output_size_range"]["max_w"], 800);
    EXPECT_EQ(summary["piece_destination_bounds"]["max_x"], 40);
    EXPECT_NE(std::ranges::find(summary["labels"], "full_canvas_overlay_candidate"),
              summary["labels"].end());
    EXPECT_NE(std::ranges::find(summary["labels"], "additive_effect_candidate"),
              summary["labels"].end());
    EXPECT_NE(std::ranges::find(summary["labels"], "multi_variant_role"), summary["labels"].end());
}

TEST(BattleFxResearch, LeavesMorphologyUnknownWithoutFrameEvidence) {
    const auto summary = d2cli_research::summarize_role_morphology(json::array(), 1);

    EXPECT_NE(std::ranges::find(summary["labels"], "unknown"), summary["labels"].end());
    EXPECT_NE(std::ranges::find(summary["labels"], "no_effect_role"), summary["labels"].end());
}
