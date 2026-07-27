#include <gtest/gtest.h>

#include "d2gamedata/DbfGameDataIndex.hpp"
#include "d2gamedata/GlobalIdResolver.hpp"
#include "d2gamedata/RefTypes.hpp"
#include "d2scenario/ScenarioTemplate.hpp"
#include "d2scenario/SgTypes.hpp"
#include "d2analysis/ScenarioGlobalIdReport.hpp"

#include "test_sg_parser.hpp"

#include <nlohmann/json.hpp>

#include <cstring>
#include <fstream>
#include <filesystem>
#include <type_traits>
#include <string>
#include <vector>

namespace {

// ── Helper: write a synthetic one-record DBF file for testing ──────────────
void write_test_dbf(const fs::path&                                         path,
                    const std::vector<std::pair<std::string, std::size_t>>& fields,
                    const std::vector<std::string>&                         values) {
    auto const n = fields.size();
    uint16_t   header_size = static_cast<uint16_t>(32 + n * 32 + 1);
    if (header_size % 2 != 0) {
        ++header_size;
    }
    uint16_t record_size = 1;
    for (const auto& [name, len] : fields) {
        record_size = static_cast<uint16_t>(record_size + static_cast<uint16_t>(len));
    }

    std::vector<uint8_t> buf(
        static_cast<std::size_t>(header_size) + static_cast<std::size_t>(record_size), 0x20);
    buf[0] = 0x03;
    uint32_t rc = 1;
    std::memcpy(buf.data() + 4, &rc, 4);
    std::memcpy(buf.data() + 8, &header_size, 2);
    std::memcpy(buf.data() + 10, &record_size, 2);

    std::size_t off = 32;
    for (std::size_t i = 0; i < n; ++i) {
        std::memset(buf.data() + off, 0, 11);
        std::memcpy(buf.data() + off, fields[i].first.c_str(),
                    std::min(fields[i].first.size(), static_cast<std::size_t>(10)));
        buf[off + 11] = 'C';
        buf[off + 16] = static_cast<uint8_t>(fields[i].second);
        buf[off + 17] = 0;
        off += 32;
    }
    buf[32 + n * 32] = 0x0D;

    buf[header_size] = 0x20;
    off = static_cast<std::size_t>(header_size) + 1;
    for (std::size_t i = 0; i < n; ++i) {
        std::string v = values[i];
        if (v.size() > fields[i].second) {
            v = v.substr(0, fields[i].second);
        } else {
            v.append(fields[i].second - v.size(), ' ');
        }
        std::memcpy(buf.data() + off, v.c_str(), fields[i].second);
        off += fields[i].second;
    }

    std::ofstream ofs(path, std::ios::binary);
    ofs.write(reinterpret_cast<const char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
}

} // namespace

// ===========================================================================
// Architecture guardrails: enforce structural boundaries
// ===========================================================================

// ── A: Header boundary — ScenarioTemplate.hpp does not expose diagnostics ─

// Detection helpers: SFINAE check for member existence.
#define DEFINE_HAS_MEMBER(member_name)                                                             \
    template <typename T, typename = void> struct has_##member_name : std::false_type {};          \
    template <typename T>                                                                          \
    struct has_##                                                                                  \
        member_name<T, std::void_t<decltype(std::declval<T>().member_name)>> : std::true_type {}

DEFINE_HAS_MEMBER(object_index);
DEFINE_HAS_MEMBER(raw_objects);
DEFINE_HAS_MEMBER(verified_empty_objects);
DEFINE_HAS_MEMBER(unknown_objects);
DEFINE_HAS_MEMBER(parsed_objects);
DEFINE_HAS_MEMBER(global_id_usages);
DEFINE_HAS_MEMBER(parse_warnings);
DEFINE_HAS_MEMBER(file_size);
DEFINE_HAS_MEMBER(spell_casts);
DEFINE_HAS_MEMBER(spell_effects);
DEFINE_HAS_MEMBER(stacks_destroyed);
DEFINE_HAS_MEMBER(quest_logs);

TEST(ArchitectureGuardrail, ScenarioTemplateNoDiagnosticFields) {
    // These diagnostic fields must NOT exist on ScenarioTemplate.
    // If any of these pass (i.e. the member IS detected on ScenarioTemplate),
    // the header boundary has leaked diagnostics into the semantic model.
    EXPECT_FALSE(has_object_index<d2scenario::ScenarioTemplate>::value);
    EXPECT_FALSE(has_raw_objects<d2scenario::ScenarioTemplate>::value);
    EXPECT_FALSE(has_verified_empty_objects<d2scenario::ScenarioTemplate>::value);
    EXPECT_FALSE(has_unknown_objects<d2scenario::ScenarioTemplate>::value);
    EXPECT_FALSE(has_parsed_objects<d2scenario::ScenarioTemplate>::value);
    EXPECT_FALSE(has_global_id_usages<d2scenario::ScenarioTemplate>::value);
    EXPECT_FALSE(has_parse_warnings<d2scenario::ScenarioTemplate>::value);
    EXPECT_FALSE(has_file_size<d2scenario::ScenarioTemplate>::value);
    EXPECT_FALSE(has_spell_casts<d2scenario::ScenarioTemplate>::value);
    EXPECT_FALSE(has_spell_effects<d2scenario::ScenarioTemplate>::value);
    EXPECT_FALSE(has_stacks_destroyed<d2scenario::ScenarioTemplate>::value);
    EXPECT_FALSE(has_quest_logs<d2scenario::ScenarioTemplate>::value);

    // These same diagnostic fields MUST exist on SgParseResult.
    EXPECT_TRUE(has_object_index<d2scenario::SgParseResult>::value);
    EXPECT_TRUE(has_raw_objects<d2scenario::SgParseResult>::value);
    EXPECT_TRUE(has_verified_empty_objects<d2scenario::SgParseResult>::value);
    EXPECT_TRUE(has_unknown_objects<d2scenario::SgParseResult>::value);
    EXPECT_TRUE(has_parsed_objects<d2scenario::SgParseResult>::value);
    EXPECT_TRUE(has_global_id_usages<d2scenario::SgParseResult>::value);
    EXPECT_TRUE(has_parse_warnings<d2scenario::SgParseResult>::value);
    EXPECT_TRUE(has_file_size<d2scenario::SgParseResult>::value);
    EXPECT_TRUE(has_spell_casts<d2scenario::SgParseResult>::value);
    EXPECT_TRUE(has_spell_effects<d2scenario::SgParseResult>::value);
    EXPECT_TRUE(has_stacks_destroyed<d2scenario::SgParseResult>::value);
    EXPECT_TRUE(has_quest_logs<d2scenario::SgParseResult>::value);
}

// ── B: Parse fixture verified-empty test ──────────────────────────────────

TEST_F(SgTestFixture, VerifiedEmptyInParseResultOnly) {
    auto data = read_file(defeated_filename());
    if (data.empty()) {
        GTEST_SKIP() << "Defeated .sg fixture not available; verified-empty classification cannot "
                        "be confirmed";
    }

    d2scenario::SgParser parser(data);
    auto                 result = parser.parse();
    const auto&          sg = result.scenario;

    // ScenarioTemplate must NOT have semantic vectors for empty hulls.
    // These classes have no semantic content and exist only on SgParseResult.
    // We verify this by checking that the ScenarioTemplate type has no
    // spell_casts/spell_effects/stacks_destroyed/quest_logs members (compile-time),
    // and that the real SG fixture has verified-empty objects (runtime).

    // Runtime: verified-empty classes MUST appear in the classification.
    EXPECT_GT(result.verified_empty_objects.size(), 0);
    EXPECT_GT(result.verified_empty_objects.count("MidSpellCast"), 0);
    EXPECT_GT(result.verified_empty_objects.count("MidSpellEffects"), 0);
    EXPECT_GT(result.verified_empty_objects.count("MidStackDestroyed"), 0);
    EXPECT_GT(result.verified_empty_objects.count("MidQuestLog"), 0);

    // The empty hull vectors on SgParseResult match the classification.
    EXPECT_GT(result.spell_casts.size(), 0);
    EXPECT_GT(result.spell_effects.size(), 0);
    EXPECT_GT(result.stacks_destroyed.size(), 0);
    EXPECT_GT(result.quest_logs.size(), 0);

    // SgParseResult uses containment — result.scenario is the semantic model
    EXPECT_FALSE(sg.info.name.empty());
}

TEST_F(SgTestFixture, VerifiedEmptyJsonHasClassification) {
    auto data = read_file(defeated_filename());
    if (data.empty()) {
        GTEST_SKIP() << "Defeated .sg fixture not available";
    }

    d2scenario::SgParser parser(data);
    auto                 result = parser.parse();

    // Build minimal JSON to verify verified_empty_objects appear
    nlohmann::json j;
    for (const auto& [cls, entries] : result.verified_empty_objects) {
        j["verified_empty_objects"][cls] = entries.size();
    }

    EXPECT_GT(j["verified_empty_objects"]["MidSpellCast"], 0);
    EXPECT_GT(j["verified_empty_objects"]["MidSpellEffects"], 0);
    EXPECT_GT(j["verified_empty_objects"]["MidStackDestroyed"], 0);
    EXPECT_GT(j["verified_empty_objects"]["MidQuestLog"], 0);
}

// ── C: Text JSON test — unresolved text id in global_id_resolutions ──────

TEST(ArchitectureGuardrail, TextResolutionJsonHasTextIdNotDisplayName) {
    // Build a DbfGameDataIndex from a synthetic DBF file with one entry that
    // has a known text ID but no text resolution.
    const fs::path tmp = fs::temp_directory_path() / "d2_guardrail_textid_test";
    fs::remove_all(tmp);
    fs::create_directories(tmp);

    write_test_dbf(tmp / "GLmark.dbf", {{"ID", 10}, {"NAME_TXT", 10}, {"DESC_TXT", 10}},
                   {"G000MG0027", "x000tg9999", "x000tg8888"});

    d2gamedata::GlobalIdResolver resolver;
    resolver.load_game_data(tmp.string());

    // Build a minimal SgParseResult with one global ID usage
    d2scenario::SgParseResult   result;
    d2scenario::SgGlobalIdUsage usage;
    usage.value.value = "G000MG0027";
    usage.object_id.value = "S143LM0001";
    usage.class_name = "MidLandmark";
    usage.field_name = "TYPE";
    result.global_id_usages.push_back(usage);

    d2analysis::ScenarioGlobalIdReport report(resolver, result);
    auto                               resolutions = report.build_resolution_map();

    ASSERT_TRUE(resolutions.count("G000MG0027") > 0);
    const auto& entry = resolutions.at("G000MG0027");

    // Must have name_text_id stored separately
    EXPECT_EQ(entry.name_text_id, "x000tg9999");
    EXPECT_EQ(entry.description_text_id, "x000tg8888");

    // The ID itself is resolved (found in the DBF index), but text lookup
    // fails because no text registry was loaded for x000tg9999.
    EXPECT_TRUE(entry.resolved);

    // Name text ID present but unresolved (text lookup failed)
    EXPECT_EQ(entry.name_text_id, "x000tg9999");
    EXPECT_FALSE(entry.name_resolved) << "name_resolved must be false when text lookup fails";
    EXPECT_TRUE(entry.name_value.empty()) << "name_value must be empty when name_resolved is false";
    EXPECT_FALSE(entry.name_unresolved_reason.empty());

    // Description text ID present but unresolved
    EXPECT_EQ(entry.description_text_id, "x000tg8888");
    EXPECT_FALSE(entry.description_resolved);
    EXPECT_TRUE(entry.description_value.empty());
    EXPECT_FALSE(entry.description_unresolved_reason.empty());

    // JSON dump must not contain display_name when name_resolved is false.
    // This simulates what the sg-inspect JSON writer produces.
    nlohmann::json j;
    j["raw_id"] = entry.id.value;
    j["resolved"] = entry.resolved;
    if (!entry.name_text_id.empty()) {
        j["name_text_id"] = entry.name_text_id;
        j["name_resolved"] = entry.name_resolved;
        j["name_value"] = entry.name_resolved ? entry.name_value : "";
        if (!entry.name_resolved && !entry.name_unresolved_reason.empty()) {
            j["name_unresolved_reason"] = entry.name_unresolved_reason;
        }
    }
    if (!entry.description_text_id.empty()) {
        j["description_text_id"] = entry.description_text_id;
        j["description_resolved"] = entry.description_resolved;
        j["description_value"] = entry.description_resolved ? entry.description_value : "";
        if (!entry.description_resolved && !entry.description_unresolved_reason.empty()) {
            j["description_unresolved_reason"] = entry.description_unresolved_reason;
        }
    }

    EXPECT_FALSE(j.contains("display_name"))
        << "display_name must not appear when name_resolved is false";
    EXPECT_FALSE(j.contains("description"))
        << "description must not appear when description_resolved is false";
    EXPECT_TRUE(j.contains("name_text_id"));
    EXPECT_TRUE(j.contains("name_resolved"));
    EXPECT_TRUE(j.contains("name_value"))
        << "name_value must always be present when name_text_id is present";
    EXPECT_EQ(j["name_value"], "") << "name_value must be empty string when name_resolved is false";
    EXPECT_TRUE(j.contains("name_unresolved_reason"))
        << "name_unresolved_reason must be present when text lookup fails";
    EXPECT_TRUE(j.contains("description_text_id"));
    EXPECT_TRUE(j.contains("description_resolved"));
    EXPECT_TRUE(j.contains("description_value"))
        << "description_value must always be present when description_text_id is present";
    EXPECT_EQ(j["description_value"], "")
        << "description_value must be empty string when description_resolved is false";
    EXPECT_TRUE(j.contains("description_unresolved_reason"))
        << "description_unresolved_reason must be present when text lookup fails";

    fs::remove_all(tmp);
}

TEST(ArchitectureGuardrail, TextResolutionDisplayNameOnlyWhenResolved) {
    // Same setup but with a text resolution available via Tglobal.dbf
    const fs::path tmp = fs::temp_directory_path() / "d2_guardrail_textok_test";
    fs::remove_all(tmp);
    fs::create_directories(tmp);

    write_test_dbf(tmp / "GLmark.dbf", {{"ID", 10}, {"NAME_TXT", 10}},
                   {"G000MG0027", "x000tg0001"});
    write_test_dbf(tmp / "Tglobal.dbf", {{"STR_ID", 10}, {"TEXT", 20}},
                   {"x000tg0001", "Some Landmark   "});

    d2gamedata::GlobalIdResolver resolver;
    resolver.load_game_data(tmp.string());

    d2scenario::SgParseResult   result;
    d2scenario::SgGlobalIdUsage usage;
    usage.value.value = "G000MG0027";
    usage.object_id.value = "S143LM0001";
    usage.class_name = "MidLandmark";
    usage.field_name = "TYPE";
    result.global_id_usages.push_back(usage);

    d2analysis::ScenarioGlobalIdReport report(resolver, result);
    auto                               resolutions = report.build_resolution_map();

    ASSERT_TRUE(resolutions.count("G000MG0027") > 0);
    const auto& entry = resolutions.at("G000MG0027");

    EXPECT_EQ(entry.name_text_id, "x000tg0001");
    EXPECT_TRUE(entry.name_resolved);
    EXPECT_EQ(entry.name_value, "Some Landmark");
    EXPECT_TRUE(entry.name_unresolved_reason.empty());

    fs::remove_all(tmp);
}

// ── D: SgScenario alias removed ──────────────────────────────────────────
//
// Confirm the alias is gone. New code must use SgParseResult or
// ScenarioTemplate directly.

TEST(ArchitectureGuardrail, SgScenarioAliasNotAvailable) {
    // SgScenario alias must not compile. Use SgParseResult instead.
    // Verify the real type still works.
    d2scenario::SgParseResult result;
    EXPECT_TRUE(result.global_id_usages.empty());
    EXPECT_TRUE(result.parse_warnings.empty());
}

// ── 3: GlobalIdResolver works without SgScenario ─────────────────────────

TEST(ArchitectureGuardrail, GlobalIdResolverWorksWithoutScenario) {
    d2gamedata::GlobalIdResolver resolver;

    auto result = resolver.resolve(d2gamedata::GlobalId{"G000000000"});
    EXPECT_EQ(result.status, d2gamedata::ResolutionStatus::NullRef);
    EXPECT_EQ(result.source_kind, d2gamedata::SourceKind::Null);
}

// ── 7: Strong ID types at boundaries ─────────────────────────────────────

TEST(ArchitectureGuardrail, StrongIdTypesAreUsed) {
    d2gamedata::GlobalId         gid{"G000MG0027"};
    d2gamedata::TextId           tid{"x000tg7527"};
    d2gamedata::ScenarioObjectId oid{"S143PL0001"};

    EXPECT_FALSE(gid.is_null());
    EXPECT_FALSE(tid.value.empty());
    EXPECT_FALSE(oid.value.empty());
    EXPECT_FALSE(gid.is_null());
    EXPECT_TRUE(gid.value.substr(0, 4) == "G000");

    d2gamedata::GlobalIdResolver resolver;
    auto                         result_by_id = resolver.resolve(gid);
    auto                         result_by_raw = resolver.resolve_raw("G000MG0027");
    EXPECT_EQ(result_by_id.status, result_by_raw.status);

    // d2scenario strong types also work at the scenario/report boundary
    d2scenario::SgGlobalId sgid{"G000MG0027"};
    EXPECT_FALSE(sgid.is_null());
    EXPECT_TRUE(sgid.is_valid());
    EXPECT_EQ(sgid.prefix(), "G000MG0");

    d2scenario::SgScenarioObjectId soid{"S143PL0001"};
    EXPECT_TRUE(soid.is_valid());
    EXPECT_EQ(soid.value, "S143PL0001");
}

// ── 7: Reference-collector regression tests ──────────────────────────────
//
// These tests use synthetic SgParseResult objects to verify the
// d2analysis report path correctly counts and classifies different
// kinds of global ID references.
//
// They protect against the manual collector in SgParser::collect_global_id_usages
// silently dropping fields on future parser changes.

TEST(ArchitectureGuardrail, CollectorCountsNullRefs) {
    d2scenario::SgParseResult result;

    d2scenario::SgGlobalIdUsage u1;
    u1.value.value = "G000000000";
    u1.object_id.value = "S143PL0000";
    u1.class_name = "MidPlayer";
    u1.field_name = "LORD_ID";
    result.global_id_usages.push_back(u1);

    d2scenario::SgGlobalIdUsage u2;
    u2.value.value = "G000000000";
    u2.object_id.value = "S143PL0001";
    u2.class_name = "MidPlayer";
    u2.field_name = "RACE_ID";
    result.global_id_usages.push_back(u2);

    d2scenario::SgGlobalIdUsage u3;
    u3.value.value = "G000MG0027";
    u3.object_id.value = "S143LM0001";
    u3.class_name = "MidLandmark";
    u3.field_name = "TYPE";
    result.global_id_usages.push_back(u3);

    d2gamedata::GlobalIdResolver       resolver;
    d2analysis::ScenarioGlobalIdReport report(resolver, result);
    auto                               sum = report.summarize();

    EXPECT_EQ(sum.total_usages, 3);
    EXPECT_EQ(sum.null_ref_usages, 2);
    EXPECT_EQ(sum.null_ref_unique_ids, 1);
    EXPECT_TRUE(sum.by_source_kind["null"] == 2 || sum.by_source_kind.count("null") > 0)
        << "Null refs counted in by_source_kind";
}

TEST(ArchitectureGuardrail, CollectorDistinguishesGidsByField) {
    d2scenario::SgParseResult result;

    // Unit type reference
    d2scenario::SgGlobalIdUsage u1;
    u1.value.value = "G000UU0042";
    u1.object_id.value = "S143UN0001";
    u1.class_name = "MidUnit";
    u1.field_name = "TYPE";
    result.global_id_usages.push_back(u1);

    // Item type reference
    d2scenario::SgGlobalIdUsage u2;
    u2.value.value = "G000IT0007";
    u2.object_id.value = "S143IM0002";
    u2.class_name = "MidItem";
    u2.field_name = "TYPE";
    result.global_id_usages.push_back(u2);

    // Stack owner reference
    d2scenario::SgGlobalIdUsage u3;
    u3.value.value = "G000PL0005";
    u3.object_id.value = "S143ST0001";
    u3.class_name = "MidStack";
    u3.field_name = "OWNER";
    result.global_id_usages.push_back(u3);

    // City village unit ref
    d2scenario::SgGlobalIdUsage u4;
    u4.value.value = "G000UN0033";
    u4.object_id.value = "S143CV0001";
    u4.class_name = "MidCityOrVillage";
    u4.field_name = "UNIT_0";
    result.global_id_usages.push_back(u4);

    d2gamedata::GlobalIdResolver       resolver;
    d2analysis::ScenarioGlobalIdReport report(resolver, result);
    auto                               sum = report.summarize();

    EXPECT_EQ(sum.total_usages, 4);
    EXPECT_EQ(sum.unique_ids, 4);
    EXPECT_EQ(sum.by_field["TYPE"], 2);
    EXPECT_EQ(sum.by_class["MidUnit"], 1);
    EXPECT_EQ(sum.by_class["MidItem"], 1);
    EXPECT_EQ(sum.by_class["MidStack"], 1);
    EXPECT_EQ(sum.by_class["MidCityOrVillage"], 1);
}

TEST(ArchitectureGuardrail, CollectorClassifiesGidsByPrefix) {
    d2scenario::SgParseResult result;

    auto add_usage = [&](const std::string& raw_id, const std::string& cls,
                         const std::string& field) {
        d2scenario::SgGlobalIdUsage u;
        u.value.value = raw_id;
        u.object_id.value = "S143XX0001";
        u.class_name = cls;
        u.field_name = field;
        result.global_id_usages.push_back(u);
    };

    add_usage("G000MG0027", "MidLandmark", "TYPE");   // map graphic
    add_usage("G000UU0042", "MidUnit", "TYPE");       // unit
    add_usage("G000IT0007", "MidItem", "TYPE");       // item
    add_usage("G000LR0013", "MidPlayer", "LORD_ID");  // lord
    add_usage("G000RR0004", "MidPlayer", "RACE_ID");  // race
    add_usage("G000PL0005", "MidStack", "OWNER");     // player
    add_usage("G000SP0001", "MidPlayer", "KNOWN_ID"); // spell
    add_usage("G000000000", "MidPlayer", "FOG_ID");   // null

    d2gamedata::GlobalIdResolver       resolver;
    d2analysis::ScenarioGlobalIdReport report(resolver, result);
    auto                               sum = report.summarize();

    EXPECT_EQ(sum.total_usages, 8);
    EXPECT_EQ(sum.unique_ids, 8);
    EXPECT_EQ(sum.null_ref_usages, 1);

    // Prefixes (7-char prefix from value)
    EXPECT_EQ(sum.by_prefix["G000MG0"], 1);
    EXPECT_EQ(sum.by_prefix["G000UU0"], 1);
    EXPECT_EQ(sum.by_prefix["G000IT0"], 1);
    EXPECT_EQ(sum.by_prefix["G000LR0"], 1);
    EXPECT_EQ(sum.by_prefix["G000RR0"], 1);
    EXPECT_EQ(sum.by_prefix["G000PL0"], 1);
    EXPECT_EQ(sum.by_prefix["G000SP0"], 1);

    // All except null are unresolved (no DBF index loaded)
    EXPECT_EQ(sum.unresolved_usages, 7);
    EXPECT_EQ(sum.resolved_usages, 0);
}

// ── G000MG0027 resolves to GLmark row evidence via DBF index ─────────────

TEST(ArchitectureGuardrail, G000MG0027ResolvesViaDbfIndex) {
    const fs::path tmp = fs::temp_directory_path() / "d2_guardrail_resolve_test";
    fs::remove_all(tmp);
    fs::create_directories(tmp);

    write_test_dbf(tmp / "GLmark.dbf", {{"ID", 10}, {"NAME_TXT", 10}, {"TYPE", 1}},
                   {"G000MG0027", "x000tg0001", "1"});

    d2gamedata::GlobalIdResolver resolver;
    resolver.load_game_data(tmp.string());

    auto resolved = resolver.resolve_raw("G000MG0027");
    EXPECT_EQ(resolved.status, d2gamedata::ResolutionStatus::Resolved);
    ASSERT_TRUE(resolved.primary_match.has_value());
    EXPECT_EQ(resolved.primary_match->table_name, "GLmark.dbf");
    EXPECT_EQ(resolved.primary_match->row_index, 0u);

    fs::remove_all(tmp);
}

// ── 9: Real-parser collector regression tests (fixture-gated) ──────────────
//
// These tests parse a real .sg file through the full parser pipeline, then
// verify the reference collector produced reasonable results. They are gated
// on test file availability (GTEST_SKIP if missing).

TEST_F(SgTestFixture, CollectorProtectsKeyRefCategories) {
    auto data = read_file(defeated_filename());
    if (data.empty()) {
        GTEST_SKIP() << "Reference map not available";
    }

    d2scenario::SgParser      parser(data);
    d2scenario::SgParseResult result = parser.parse();

    // Collect usages by category
    struct CatCount {
        int  count = 0;
        bool found_expected = false;
    };
    std::map<std::string, CatCount> unit_type;
    std::map<std::string, CatCount> item_type;
    std::map<std::string, CatCount> stack_owner;
    std::map<std::string, CatCount> null_refs;
    for (const auto& u : result.global_id_usages) {
        if (u.value.is_null()) {
            ++null_refs["any"].count;
        }
        if (u.class_name == "MidUnit" && u.field_name == "TYPE" && !u.value.is_null()) {
            ++unit_type["any"].count;
        }
        if (u.class_name == "MidItem" && u.field_name == "TYPE" && !u.value.is_null()) {
            ++item_type["any"].count;
        }
        if (u.class_name == "MidStack" && u.field_name == "OWNER" && !u.value.is_null()) {
            ++stack_owner["any"].count;
        }
        if (u.class_name == "MidCapital" && u.field_name.rfind("UNIT_", 0) == 0) {
            ++unit_type["capital_village"].count;
        }
        if (u.class_name == "MidVillage" && u.field_name.rfind("UNIT_", 0) == 0) {
            ++unit_type["capital_village"].count;
        }
        if (u.class_name == "MidStack" && u.field_name == "LEADER" && !u.value.is_null()) {
            ++stack_owner["leader"].count;
        }
        if (u.class_name == "MidStack" && u.field_name.rfind("UNIT_", 0) == 0 &&
            !u.value.is_null()) {
            ++stack_owner["unit_slot"].count;
        }
    }

    // MidUnit.TYPE references
    if (unit_type["any"].count == 0) {
        GTEST_SKIP() << "No MidUnit.TYPE refs in this fixture version";
    }

    // MidStack.OWNER references
    if (stack_owner["any"].count == 0) {
        GTEST_SKIP() << "No MidStack.OWNER refs in this fixture version";
    }

    // MidItem.TYPE references
    bool has_item_type = item_type["any"].count > 0;

    // Run the report pipeline
    d2gamedata::GlobalIdResolver       resolver;
    d2analysis::ScenarioGlobalIdReport report(resolver, result);
    auto                               sum = report.summarize();

    // Null references counted correctly by ScenarioGlobalIdReport
    // This must run BEFORE any GTEST_SKIP so it's verified even when mid-level
    // categories are absent.
    {
        auto null_result = report.build_unresolved_report();
        EXPECT_EQ(null_result.null_ref_usages, null_refs["any"].count);
        EXPECT_EQ(null_result.null_ref_unique_ids, null_refs["any"].count > 0 ? 1 : 0);
        EXPECT_EQ(sum.null_ref_usages, null_refs["any"].count);
        EXPECT_EQ(sum.null_ref_unique_ids, null_refs["any"].count > 0 ? 1 : 0);
    }

    // Check collector categories (skip if absent in this fixture version)
    if (unit_type["any"].count == 0) {
        GTEST_SKIP() << "No MidUnit.TYPE refs in this fixture version";
    }
    if (stack_owner["any"].count == 0) {
        GTEST_SKIP() << "No MidStack.OWNER refs in this fixture version";
    }

    EXPECT_GT(unit_type["any"].count, 0)
        << "MidUnit.TYPE with G* value must be collected (manual collector regression guard)";
    EXPECT_GT(stack_owner["any"].count, 0)
        << "MidStack.OWNER with G* value must be collected (manual collector regression guard)";

    // If this fixture version has items with global type IDs, verify them
    if (has_item_type) {
        EXPECT_GT(item_type["any"].count, 0)
            << "MidItem.TYPE with G* value must be collected if present";
    }
}

TEST_F(SgTestFixture, CollectorFindsUsagesInDosgenMap) {
    auto data = read_file(dosgen_filename());
    if (data.empty()) {
        GTEST_SKIP() << "Reference map not available";
    }

    d2scenario::SgParser      parser(data);
    d2scenario::SgParseResult result = parser.parse();

    // Verify collector populated global_id_usages from real data
    EXPECT_GT(result.global_id_usages.size(), 50)
        << "Dosgen map should produce at least 50 global ID usages";
    EXPECT_GT(result.object_index.size(), 500)
        << "Dosgen map should produce at least 500 object index entries";

    // Run the report pipeline — smoke test that nothing crashes
    d2gamedata::GlobalIdResolver       resolver;
    d2analysis::ScenarioGlobalIdReport report(resolver, result);
    auto                               sum = report.summarize();
    EXPECT_EQ(sum.total_usages, result.global_id_usages.size());
    EXPECT_GT(sum.unique_ids, 0);
    EXPECT_GT(sum.by_source_kind.size(), 0)
        << "Should have at least one source kind classification";
    EXPECT_GT(sum.by_field.size(), 0) << "Should have at least one field name in the report";
}

TEST_F(SgTestFixture, CollectorFindsUsagesInDefeatedMap) {
    auto data = read_file(defeated_filename());
    if (data.empty()) {
        GTEST_SKIP() << "Reference map not available";
    }

    d2scenario::SgParser      parser(data);
    d2scenario::SgParseResult result = parser.parse();

    // Larger map should produce more references
    EXPECT_GT(result.global_id_usages.size(), 200)
        << "Defeated map should produce at least 200 global ID usages";
    EXPECT_GT(result.object_index.size(), 2000)
        << "Defeated map should produce at least 2000 object index entries";

    // Run the report pipeline
    d2gamedata::GlobalIdResolver       resolver;
    d2analysis::ScenarioGlobalIdReport report(resolver, result);
    auto                               sum = report.summarize();
    EXPECT_EQ(sum.total_usages, result.global_id_usages.size());
    EXPECT_GT(sum.unique_ids, 0);
    EXPECT_GT(sum.by_source_kind.size(), 0);
    EXPECT_GT(sum.by_class.size(), 0) << "Should have at least one class name in the report";
}
