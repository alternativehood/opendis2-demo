#include "cli/asset_reference_resolver.hpp"
#include "d2asset/asset_database.hpp"
#include "d2asset/asset_error.hpp"

#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "tests/test_process.hpp"

namespace fs = std::filesystem;

namespace {

// NOLINTBEGIN(bugprone-unchecked-optional-access)

class AssetLinkManifestTest : public ::testing::Test {
protected:
    void SetUp() override {
        static std::size_t sequence = 0;
        root_ = fs::temp_directory_path() /
                ("d2_asset_link_manifest_test_" + std::to_string(test_support::process_id()) + "_" +
                 std::to_string(++sequence));
        std::error_code error;
        fs::remove_all(root_, error);
        fs::create_directories(root_ / "images/Imgs/BatUnits.ff");
        fs::create_directories(root_ / "animations/Imgs/BatUnits.ff/G000UU0001IDLE");
        fs::create_directories(root_ / "data/Globals/Gunits.dbf");
        fs::create_directories(root_ / "data/Interf/Test.dlg");

        std::ofstream(root_ / "images/Imgs/BatUnits.ff/Portrait.json")
            << R"({"logical_name":"Portrait","output_size":{"w":8,"h":8}})" << '\n';
        std::ofstream(root_ / "animations/Imgs/BatUnits.ff/G000UU0001IDLE/frame_000.png")
            << "frame";
        std::ofstream(root_ / "animations/Imgs/BatUnits.ff/G000UU0001IDLE/anim.json")
            << R"({"name":"G000UU0001IDLE","frame_count":1,"frame_delay_ms":100,"frames":[{"index":0,"logical_name":"Missing","width":8,"height":8}]})"
            << '\n';
        std::ofstream(root_ / "data/Globals/Gunits.dbf/Gunits.json")
            << R"({"data_table_schema_version":1,"asset_id":"globals/gunits.dbf/gunits","logical_name":"Gunits","container_id":"globals/gunits.dbf","kind":"dbf","columns":[{"name":"UNIT_ID","source_type":"C","width":10,"decimal_count":0,"extensions":null}],"rows":[{"row_key":"00000000","values":[{"name":"UNIT_ID","value":"G000UN0001"}]}],"warnings":[],"extensions":null})"
            << '\n';
        std::ofstream(root_ / "data/Interf/Test.dlg/Test.json")
            << R"({"data_table_schema_version":1,"asset_id":"interf/test.dlg/test","logical_name":"Test","container_id":"interf/test.dlg","kind":"dlg","columns":[{"name":"IMAGES","source_type":null,"width":null,"decimal_count":null,"extensions":null},{"name":"TARGET_ID","source_type":null,"width":null,"decimal_count":null,"extensions":null},{"name":"SOUND","source_type":null,"width":null,"decimal_count":null,"extensions":null}],"rows":[{"row_key":"dialog","values":[{"name":"IMAGES","value":"Portrait"},{"name":"TARGET_ID","value":"imgs/batunits.ff/portrait"},{"name":"SOUND","value":"UNIT_HIT"}]}],"warnings":[],"extensions":null})"
            << '\n';

        const nlohmann::json manifest{
            {"asset_schema_version", 1},
            {"containers",
             nlohmann::json::array(
                 {{{"container_id", "imgs/batunits.ff"},
                   {"path", "Imgs/BatUnits.ff"},
                   {"content_kinds", nlohmann::json::array({"images", "animations"})}},
                  {{"container_id", "globals/gunits.dbf"},
                   {"path", "Globals/Gunits.dbf"},
                   {"content_kinds", nlohmann::json::array({"data"})}},
                  {{"container_id", "interf/test.dlg"},
                   {"path", "Interf/Test.dlg"},
                   {"content_kinds", nlohmann::json::array({"data"})}}})},
            {"assets", nlohmann::json::array(
                           {{{"asset_id", "globals/gunits.dbf/gunits"},
                             {"logical_name", "Gunits"},
                             {"type", "data_table"},
                             {"container_id", "globals/gunits.dbf"},
                             {"path", "data/Globals/Gunits.dbf/Gunits.json"}},
                            {{"asset_id", "imgs/batunits.ff/g000uu0001idle"},
                             {"logical_name", "G000UU0001IDLE"},
                             {"type", "animation"},
                             {"container_id", "imgs/batunits.ff"},
                             {"path", "animations/Imgs/BatUnits.ff/G000UU0001IDLE/anim.json"}},
                            {{"asset_id", "imgs/batunits.ff/portrait"},
                             {"logical_name", "Portrait"},
                             {"type", "image"},
                             {"container_id", "imgs/batunits.ff"},
                             {"path", "images/Imgs/BatUnits.ff/Portrait.json"}},
                            {{"asset_id", "interf/test.dlg/test"},
                             {"logical_name", "Test"},
                             {"type", "data_table"},
                             {"container_id", "interf/test.dlg"},
                             {"path", "data/Interf/Test.dlg/Test.json"}}})},
            {"warnings", nlohmann::json::array()}};
        write_json("game_manifest.json", manifest);
    }

    void TearDown() override {
        std::error_code error;
        fs::remove_all(root_, error);
    }

    void write_json(const fs::path& relative, const nlohmann::json& value) {
        std::ofstream(root_ / relative) << value.dump(2) << '\n';
    }

    [[nodiscard]] static nlohmann::json valid_graph() {
        nlohmann::json graph = {
            {"asset_links_schema_version", 1},
            {"links",
             nlohmann::json::array(
                 {{{"source",
                    {{"kind", "data_row"},
                     {"table_asset_id", "globals/gunits.dbf/gunits"},
                     {"row_key", "00000000"}}},
                   {"target_asset_id", "imgs/batunits.ff/g000uu0001idle"},
                   {"link_kind", "idle_animation"},
                   {"resolution", "heuristic"},
                   {"confidence", 80},
                   {"reason_code", "unit_animation_prefix"},
                   {"evidence", nlohmann::json::array({{{"field", "UNIT_ID"},
                                                        {"source_value", "G000UN0001"},
                                                        {"target_value", "G000UU0001IDLE"}}})}},
                  {{"source", {{"kind", "asset"}, {"asset_id", "imgs/batunits.ff/portrait"}}},
                   {"target_asset_id", "imgs/batunits.ff/portrait"},
                   {"link_kind", "icon_image"},
                   {"resolution", "confirmed"},
                   {"confidence", 100},
                   {"reason_code", "explicit_asset_id"},
                   {"evidence",
                    nlohmann::json::array({{{"field", "asset_id"},
                                            {"source_value", "imgs/batunits.ff/portrait"},
                                            {"target_value", "imgs/batunits.ff/portrait"}}})}}})},
            {"unresolved",
             nlohmann::json::array(
                 {{{"source",
                    {{"kind", "data_row"},
                     {"table_asset_id", "globals/gunits.dbf/gunits"},
                     {"row_key", "00000000"}}},
                   {"link_kind", "sound_candidate"},
                   {"reason", "unsupported_mapping"},
                   {"reason_code", "symbolic_sound_mapping_unavailable"},
                   {"evidence",
                    nlohmann::json::array({{{"field", "SOUND"},
                                            {"source_value", "UNIT_HIT"},
                                            {"target_value", "canonical_sound_asset_id"}}})},
                   {"candidate_asset_ids", nlohmann::json::array()}}})},
            {"warnings", nlohmann::json::array()},
            {"extensions", nullptr},
        };
        const nlohmann::json first = graph["links"][0];
        graph["links"][0] = graph["links"][1];
        graph["links"][1] = first;
        return graph;
    }

    fs::path root_;
};

TEST_F(AssetLinkManifestTest, MissingOptionalGraphProducesEmptyIndexes) {
    const auto database = d2asset::AssetDatabase::open(root_);
    EXPECT_TRUE(database.asset_links().links.empty());
    const auto links = database.links_from_data_row("globals/gunits.dbf/gunits", "00000000");
    ASSERT_TRUE(links.value.has_value());
    EXPECT_TRUE(links.value->empty());
}

TEST_F(AssetLinkManifestTest, LoadsAndQueriesCanonicalGraph) {
    write_json("asset_links.json", valid_graph());
    const auto database = d2asset::AssetDatabase::open(root_);

    const auto outgoing = database.links_from_data_row("globals/gunits.dbf/gunits", "00000000");
    ASSERT_TRUE(outgoing.value.has_value());
    ASSERT_EQ(outgoing.value->size(), 1U);
    EXPECT_EQ(outgoing.value->front().kind, d2asset::AssetLinkKind::IdleAnimation);

    const auto incoming = database.links_to_asset("imgs/batunits.ff/g000uu0001idle");
    ASSERT_TRUE(incoming.value.has_value());
    EXPECT_EQ(incoming.value->size(), 1U);

    const auto asset_links =
        database.links_from_asset("imgs/batunits.ff/portrait", d2asset::AssetLinkKind::IconImage,
                                  d2asset::AssetLinkResolution::Confirmed);
    ASSERT_TRUE(asset_links.value.has_value());
    ASSERT_EQ(asset_links.value->size(), 1U);
    EXPECT_EQ(asset_links.value->front().target_asset_id, "imgs/batunits.ff/portrait");

    const auto asset_unresolved = database.unresolved_from_asset("imgs/batunits.ff/portrait");
    ASSERT_TRUE(asset_unresolved.value.has_value());
    EXPECT_TRUE(asset_unresolved.value->empty());
    EXPECT_FALSE(database.unresolved_from_asset("missing").value.has_value());

    const auto filtered = database.links_from_data_row("globals/gunits.dbf/gunits", "00000000",
                                                       d2asset::AssetLinkKind::AttackAnimation);
    ASSERT_TRUE(filtered.value.has_value());
    EXPECT_TRUE(filtered.value->empty());

    const auto unresolved =
        database.unresolved_from_data_row("globals/gunits.dbf/gunits", "00000000");
    ASSERT_TRUE(unresolved.value.has_value());
    EXPECT_EQ(unresolved.value->size(), 1U);
}

TEST_F(AssetLinkManifestTest, RejectsUnsupportedMalformedAndDuplicateGraphs) {
    auto graph = valid_graph();
    graph["asset_links_schema_version"] = 2;
    write_json("asset_links.json", graph);
    EXPECT_THROW(
        {
            try {
                (void)d2asset::AssetDatabase::open(root_);
            } catch (const d2asset::AssetError& error) {
                EXPECT_EQ(error.code(), d2asset::AssetErrorCode::UnsupportedAssetLinksSchema);
                throw;
            }
        },
        d2asset::AssetError);

    graph = valid_graph();
    graph["links"].push_back(graph["links"][0]);
    write_json("asset_links.json", graph);
    EXPECT_THROW(
        {
            try {
                (void)d2asset::AssetDatabase::open(root_);
            } catch (const d2asset::AssetError& error) {
                EXPECT_EQ(error.code(), d2asset::AssetErrorCode::DuplicateAssetLink);
                throw;
            }
        },
        d2asset::AssetError);

    graph = valid_graph();
    graph["links"][0]["confidence"] = 101;
    write_json("asset_links.json", graph);
    EXPECT_THROW((void)d2asset::AssetDatabase::open(root_), d2asset::AssetError);

    std::vector<nlohmann::json> malformed_graphs;
    graph = valid_graph();
    graph["links"][1]["source"]["row_key"] = "missing";
    malformed_graphs.push_back(graph);
    graph = valid_graph();
    graph["links"][1]["target_asset_id"] = "missing";
    malformed_graphs.push_back(graph);
    graph = valid_graph();
    graph["links"][1]["target_asset_id"] = "imgs/batunits.ff/portrait";
    malformed_graphs.push_back(graph);
    graph = valid_graph();
    graph["links"][1]["link_kind"] = "invented";
    malformed_graphs.push_back(graph);
    graph = valid_graph();
    graph["links"][1]["evidence"] = nlohmann::json::array();
    malformed_graphs.push_back(graph);
    graph = valid_graph();
    graph["unresolved"][0]["link_kind"] = "idle_animation";
    malformed_graphs.push_back(graph);
    for (const auto& malformed : malformed_graphs) {
        write_json("asset_links.json", malformed);
        EXPECT_THROW((void)d2asset::AssetDatabase::open(root_), d2asset::AssetError);
    }
}

TEST_F(AssetLinkManifestTest, ResolverWritesByteStableUnitAnimationLink) {
    AssetReferenceResolver::write(root_);
    std::ifstream     first_input(root_ / "asset_links.json");
    const std::string first((std::istreambuf_iterator<char>(first_input)),
                            std::istreambuf_iterator<char>());
    fs::remove(root_ / "asset_links.json");
    nlohmann::json manifest;
    {
        std::ifstream input(root_ / "game_manifest.json");
        input >> manifest;
    }
    std::ranges::reverse(manifest["assets"]);
    write_json("game_manifest.json", manifest);
    AssetReferenceResolver::write(root_);
    std::ifstream     second_input(root_ / "asset_links.json");
    const std::string second((std::istreambuf_iterator<char>(second_input)),
                             std::istreambuf_iterator<char>());
    EXPECT_EQ(first, second);

    const auto database = d2asset::AssetDatabase::open(root_);
    const auto links = database.links_from_data_row("globals/gunits.dbf/gunits", "00000000");
    ASSERT_TRUE(links.value.has_value());
    ASSERT_EQ(links.value->size(), 1U);
    EXPECT_EQ(links.value->front().target_asset_id, "imgs/batunits.ff/g000uu0001idle");
    EXPECT_EQ(links.value->front().resolution, d2asset::AssetLinkResolution::Heuristic);
}

TEST_F(AssetLinkManifestTest, ResolverHandlesDlgImagesExplicitIdsAndSoundTriggers) {
    AssetReferenceResolver::write(root_);
    const auto database = d2asset::AssetDatabase::open(root_);
    const auto links = database.links_from_data_row("interf/test.dlg/test", "dialog");
    ASSERT_TRUE(links.value.has_value());
    ASSERT_EQ(links.value->size(), 2U);
    EXPECT_TRUE(std::ranges::any_of(*links.value, [](const auto& link) {
        return link.kind == d2asset::AssetLinkKind::DialogImage &&
               link.resolution == d2asset::AssetLinkResolution::Confirmed;
    }));
    EXPECT_TRUE(std::ranges::any_of(*links.value, [](const auto& link) {
        return link.kind == d2asset::AssetLinkKind::Unknown &&
               link.reason_code == "explicit_asset_id";
    }));

    const auto unresolved = database.unresolved_from_data_row("interf/test.dlg/test", "dialog");
    ASSERT_TRUE(unresolved.value.has_value());
    ASSERT_EQ(unresolved.value->size(), 1U);
    EXPECT_EQ(unresolved.value->front().reason, d2asset::UnresolvedAssetReason::UnsupportedMapping);
}

TEST_F(AssetLinkManifestTest, ResolverPreservesCrossContainerImageAmbiguity) {
    fs::create_directories(root_ / "images/Other/Test.ff");
    std::ofstream(root_ / "images/Other/Test.ff/Portrait.json")
        << R"({"logical_name":"Portrait","output_size":{"w":8,"h":8}})" << '\n';
    nlohmann::json manifest;
    {
        std::ifstream input(root_ / "game_manifest.json");
        input >> manifest;
    }
    manifest["containers"].push_back({{"container_id", "other/test.ff"},
                                      {"path", "Other/Test.ff"},
                                      {"content_kinds", nlohmann::json::array({"images"})}});
    manifest["assets"].push_back({{"asset_id", "other/test.ff/portrait"},
                                  {"logical_name", "Portrait"},
                                  {"type", "image"},
                                  {"container_id", "other/test.ff"},
                                  {"path", "images/Other/Test.ff/Portrait.json"}});
    write_json("game_manifest.json", manifest);

    AssetReferenceResolver::write(root_);
    const auto database = d2asset::AssetDatabase::open(root_);
    const auto unresolved = database.unresolved_from_data_row("interf/test.dlg/test", "dialog");
    ASSERT_TRUE(unresolved.value.has_value());
    const auto ambiguous =
        std::ranges::find(*unresolved.value, d2asset::UnresolvedAssetReason::Ambiguous,
                          &d2asset::UnresolvedAssetReference::reason);
    ASSERT_NE(ambiguous, unresolved.value->end());
    EXPECT_EQ(ambiguous->candidate_asset_ids,
              (std::vector<std::string>{"imgs/batunits.ff/portrait", "other/test.ff/portrait"}));
}

TEST_F(AssetLinkManifestTest, ResolverReportsMissingAndWrongTypeDlgImages) {
    nlohmann::json table;
    {
        std::ifstream input(root_ / "data/Interf/Test.dlg/Test.json");
        input >> table;
    }
    table["rows"][0]["values"][0]["value"] = "MissingPortrait";
    write_json("data/Interf/Test.dlg/Test.json", table);
    AssetReferenceResolver::write(root_);
    auto database = d2asset::AssetDatabase::open(root_);
    auto unresolved = database.unresolved_from_data_row("interf/test.dlg/test", "dialog");
    ASSERT_TRUE(unresolved.value.has_value());
    EXPECT_TRUE(std::ranges::any_of(*unresolved.value, [](const auto& reference) {
        return reference.kind == d2asset::AssetLinkKind::DialogImage &&
               reference.reason == d2asset::UnresolvedAssetReason::NoCandidate;
    }));

    table["rows"][0]["values"][0]["value"] = "imgs/batunits.ff/g000uu0001idle";
    write_json("data/Interf/Test.dlg/Test.json", table);
    fs::remove(root_ / "asset_links.json");
    AssetReferenceResolver::write(root_);
    database = d2asset::AssetDatabase::open(root_);
    unresolved = database.unresolved_from_data_row("interf/test.dlg/test", "dialog");
    ASSERT_TRUE(unresolved.value.has_value());
    EXPECT_TRUE(std::ranges::any_of(*unresolved.value, [](const auto& reference) {
        return reference.kind == d2asset::AssetLinkKind::DialogImage &&
               reference.reason == d2asset::UnresolvedAssetReason::WrongType;
    }));
}

TEST(AssetLinkEnums, StableLowercaseNames) {
    EXPECT_STREQ(d2asset::to_string(d2asset::AssetLinkEndpointKind::DataRow), "data_row");
    EXPECT_STREQ(d2asset::to_string(d2asset::AssetLinkKind::DeathAnimation), "death_animation");
    EXPECT_STREQ(d2asset::to_string(d2asset::AssetLinkResolution::Confirmed), "confirmed");
    EXPECT_STREQ(d2asset::to_string(d2asset::UnresolvedAssetReason::WrongType), "wrong_type");
}

// NOLINTEND(bugprone-unchecked-optional-access)

} // namespace
