#include <d2runtime/AdventureWorldBuilder.hpp>
#include <d2runtime/AdventureWorldState.hpp>
#include <d2runtime/AdventureResourceNode.hpp>

#include <d2scenario/ScenarioTemplate.hpp>

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace d2runtime;
using namespace d2scenario;

// ── Helpers ──────────────────────────────────────────────────────────────

static SgCrystal make_crystal(const std::string& id, int pos_x, int pos_y, int resource,
                              const std::string& type, const std::string& owner, int ai_priority) {
    SgCrystal cr;
    cr.id = id;
    cr.pos_x = pos_x;
    cr.pos_y = pos_y;
    cr.resource = resource;
    cr.type = type;
    cr.owner = owner;
    cr.ai_priority = ai_priority;
    return cr;
}

static ScenarioTemplate scenario_with_crystals(std::vector<SgCrystal> crystals) {
    ScenarioTemplate tmpl;
    tmpl.info.id = "scn_res";
    tmpl.info.name = "Resource Node Test";
    tmpl.info.map_size = 36;
    // Need terrain for valid map dims
    tmpl.map.terrain.width = 36;
    tmpl.map.terrain.height = 36;
    tmpl.map.terrain.tiles.assign(36, std::vector<uint32_t>(36, 0));
    tmpl.crystals = std::move(crystals);
    return tmpl;
}

// ── All six RESOURCE values map correctly ────────────────────────────────

TEST(AdventureResourceNodeBuilder, AllResourceKindsMapExactly) {
    struct Trial {
        int                   raw;
        AdventureResourceKind expected_kind;
        const char*           expected_label;
    };
    const Trial trials[] = {
        {0, AdventureResourceKind::GoldMine, "GoldMine"},
        {1, AdventureResourceKind::RedMana, "RedMana"},
        {2, AdventureResourceKind::YellowMana, "YellowMana"},
        {3, AdventureResourceKind::OrangeMana, "OrangeMana"},
        {4, AdventureResourceKind::WhiteMana, "WhiteMana"},
        {5, AdventureResourceKind::BlueMana, "BlueMana"},
    };

    std::vector<SgCrystal> crystals;
    for (const auto& t : trials) {
        crystals.push_back(make_crystal(std::string("CR") + std::to_string(t.raw), t.raw,
                                        t.raw + 10, t.raw, "", "", 3));
    }

    auto tmpl = scenario_with_crystals(std::move(crystals));
    // Add plan entries matching each crystal's position
    SgMidgardPlan plan;
    plan.id = "plan_crystals";
    for (const auto& t : trials) {
        SgPlanEntry entry;
        entry.element = std::string("CR") + std::to_string(t.raw);
        entry.pos_x = t.raw;
        entry.pos_y = t.raw + 10;
        plan.entries.push_back(entry);
    }
    tmpl.plans.push_back(plan);

    AdventureWorldBuilder builder;
    auto                  result = builder.build(tmpl);

    EXPECT_EQ(result.world.resource_nodes.size(), 6u);
    EXPECT_EQ(result.diagnostics.size(), 0u) << "no diagnostics expected for valid data";

    for (std::size_t i = 0; i < result.world.resource_nodes.size(); ++i) {
        const auto& node = result.world.resource_nodes[i];
        EXPECT_EQ(node.resource_kind, trials[i].expected_kind)
            << "index=" << i << " raw=" << trials[i].raw;
        EXPECT_EQ(node.position.x, trials[i].raw);
        EXPECT_EQ(node.position.y, trials[i].raw + 10);
        EXPECT_EQ(node.raw_resource, trials[i].raw);
    }
}

// ── All raw fields survive ───────────────────────────────────────────────

TEST(AdventureResourceNodeBuilder, RawFieldsSurvive) {
    SgCrystal cr;
    cr.id = "S143CR0005";
    cr.pos_x = 19;
    cr.pos_y = 1;
    cr.resource = 0;
    cr.type = "G000MG0001";
    cr.owner = "G000PL0003";
    cr.ai_priority = 7;

    auto          tmpl = scenario_with_crystals({cr});
    SgMidgardPlan plan;
    plan.id = "plan_cr";
    plan.entries.push_back({"S143CR0005", 19, 1});
    tmpl.plans.push_back(plan);

    AdventureWorldBuilder builder;
    auto                  result = builder.build(tmpl);
    ASSERT_EQ(result.world.resource_nodes.size(), 1u);

    const auto& node = result.world.resource_nodes[0];
    EXPECT_EQ(node.id, "S143CR0005");
    EXPECT_EQ(node.position.x, 19);
    EXPECT_EQ(node.position.y, 1);
    EXPECT_EQ(node.raw_resource, 0);
    EXPECT_EQ(node.resource_kind, AdventureResourceKind::GoldMine);
    EXPECT_EQ(node.raw_type, "G000MG0001");
    EXPECT_EQ(node.owner, "G000PL0003");
    EXPECT_EQ(node.ai_priority, 7);
}

// ── Full footprint survives ──────────────────────────────────────────────

TEST(AdventureResourceNodeBuilder, FootprintSurvives) {
    SgCrystal cr;
    cr.id = "S143CR0001";
    cr.pos_x = 19;
    cr.pos_y = 4;
    cr.resource = 1;

    auto          tmpl = scenario_with_crystals({cr});
    SgMidgardPlan plan;
    plan.id = "plan_cr";
    plan.entries.push_back({"S143CR0001", 19, 4});
    tmpl.plans.push_back(plan);

    AdventureWorldBuilder builder;
    auto                  result = builder.build(tmpl);
    ASSERT_EQ(result.world.resource_nodes.size(), 1u);

    const auto& node = result.world.resource_nodes[0];
    ASSERT_EQ(node.footprint.size(), 1u);
    EXPECT_EQ(node.footprint[0].x, 19);
    EXPECT_EQ(node.footprint[0].y, 4);
}

// ── RESOURCE=-1 is fatal ─────────────────────────────────────────────────

TEST(AdventureResourceNodeBuilder, ResourceNegativeOneIsFatal) {
    auto cr = make_crystal("S143CR_BAD", 10, 10, -1, "", "", 0);
    auto tmpl = scenario_with_crystals({cr});

    AdventureWorldBuilder builder;
    auto                  result = builder.build(tmpl);

    bool found = false;
    for (const auto& d : result.diagnostics) {
        if (d.kind == BuildDiagnosticKind::UnsupportedResourceNodeValue) {
            found = true;
            EXPECT_TRUE(d.message.find("-1") != std::string::npos);
            EXPECT_TRUE(d.message.find("S143CR_BAD") != std::string::npos);
            EXPECT_TRUE(d.message.find("expected domain 0..5") != std::string::npos);
            break;
        }
    }
    EXPECT_TRUE(found) << "Expected UnsupportedResourceNodeValue diagnostic for RESOURCE=-1";
    EXPECT_TRUE(result.diagnostics.size() > 0);
    EXPECT_TRUE(is_build_error(BuildDiagnosticKind::UnsupportedResourceNodeValue));
}

// ── RESOURCE=6 is fatal ─────────────────────────────────────────────────

TEST(AdventureResourceNodeBuilder, ResourceSixIsFatal) {
    auto cr = make_crystal("S143CR_BAD6", 10, 10, 6, "", "", 0);
    auto tmpl = scenario_with_crystals({cr});

    AdventureWorldBuilder builder;
    auto                  result = builder.build(tmpl);

    bool found = false;
    for (const auto& d : result.diagnostics) {
        if (d.kind == BuildDiagnosticKind::UnsupportedResourceNodeValue) {
            found = true;
            EXPECT_TRUE(d.message.find("6") != std::string::npos);
            break;
        }
    }
    EXPECT_TRUE(found) << "Expected UnsupportedResourceNodeValue diagnostic for RESOURCE=6";
}

// ── Missing plan entry is fatal ──────────────────────────────────────────

TEST(AdventureResourceNodeBuilder, MissingPlanFootprintIsFatal) {
    auto cr = make_crystal("S143CR_NOPLAN", 15, 15, 2, "", "", 0);
    auto tmpl = scenario_with_crystals({cr});

    AdventureWorldBuilder builder;
    auto                  result = builder.build(tmpl);

    bool found = false;
    for (const auto& d : result.diagnostics) {
        if (d.kind == BuildDiagnosticKind::ResourceNodePlanFootprintMismatch) {
            found = true;
            EXPECT_TRUE(d.message.find("missing plan footprint") != std::string::npos);
            EXPECT_TRUE(d.message.find("S143CR_NOPLAN") != std::string::npos);
            break;
        }
    }
    EXPECT_TRUE(found) << "Expected ResourceNodePlanFootprintMismatch diagnostic for missing plan";
    EXPECT_TRUE(is_build_error(BuildDiagnosticKind::ResourceNodePlanFootprintMismatch));
}

// ── Inconsistent plan position is fatal ──────────────────────────────────

TEST(AdventureResourceNodeBuilder, InconsistentPlanPositionIsFatal) {
    auto          cr = make_crystal("S143CR_BADPOS", 5, 5, 3, "", "", 0);
    auto          tmpl = scenario_with_crystals({cr});
    SgMidgardPlan plan;
    plan.id = "plan_bad";
    plan.entries.push_back({"S143CR_BADPOS", 10, 10}); // different from crystal pos
    tmpl.plans.push_back(plan);

    AdventureWorldBuilder builder;
    auto                  result = builder.build(tmpl);

    bool found = false;
    for (const auto& d : result.diagnostics) {
        if (d.kind == BuildDiagnosticKind::ResourceNodePlanFootprintMismatch) {
            found = true;
            EXPECT_TRUE(d.message.find("inconsistent") != std::string::npos);
            break;
        }
    }
    EXPECT_TRUE(found) << "Expected ResourceNodePlanFootprintMismatch for inconsistent position";
}

// ── No resource node is represented as ManaCrystal ───────────────────────

TEST(AdventureResourceNodeBuilder, NoResourceNodeIsManaCrystal) {
    auto          cr = make_crystal("S143CR_NOGC", 7, 8, 4, "", "", 0);
    auto          tmpl = scenario_with_crystals({cr});
    SgMidgardPlan plan;
    plan.id = "plan_cr";
    plan.entries.push_back({"S143CR_NOGC", 7, 8});
    tmpl.plans.push_back(plan);

    AdventureWorldBuilder builder;
    auto                  result = builder.build(tmpl);

    // The compatibility map_objects entry must use ResourceNode kind
    for (const auto& mo : result.world.map_objects) {
        if (mo.id == "S143CR_NOGC") {
            EXPECT_EQ(mo.kind, AdventureMapObjectKind::ResourceNode);
        }
    }
}
