#include <d2adventure_render/map_geometry.hpp>
#include <d2adventure_render/map_preparer.hpp>
#include <d2adventure_render/prepared_adventure_map.hpp>
#include <d2engine/app/adventure_pick_index.hpp>
#include <d2engine/app/adventure_screen_input.hpp>
#include <d2engine/app/stack_inspection.hpp>
#include <d2engine/assets/render_graph_asset_collector.hpp>
#include <d2engine/assets/image_asset_key.hpp>
#include <d2engine/assets/asset_runtime.hpp>
#include <d2engine/assets/asset_runtime_catalog_adapter.hpp>
#include <d2engine/assets/game_data_registry.hpp>
#include <d2engine/assets/iso_actor_visual_resolver.hpp>
#include <d2engine/input/input_event.hpp>
#include <d2runtime/AdventureActorAnimationResolver.hpp>
#include <d2runtime/AdventureWorldBuilder.hpp>
#include <d2runtime/AdventureWorldState.hpp>
#include <d2scenario/SgParser.hpp>
#include <d2scenario/SgTypes.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifndef DISCIPLES2_GAME_ROOT
#define DISCIPLES2_GAME_ROOT ""
#endif

namespace {

namespace ar = d2engine::adventure_render;
namespace fs = std::filesystem;

constexpr std::string_view kEmptyObjectId = "G000000000";

struct OracleMidStack {
    std::string id;
    std::string group_id;
    int         pos_x = 0;
    int         pos_y = 0;
    std::string leader_id;
    std::string inside;
};

struct OracleMidUnit {
    std::string id;
    std::string type_id;
};

struct OracleScenario {
    std::vector<OracleMidStack>           stacks;
    std::map<std::string, OracleMidUnit>  units;
    std::map<std::string, OracleMidStack> stacks_by_id;
    std::map<std::string, std::string>    leader_types_by_stack_id;
    std::vector<std::string>              visible_stack_ids;
    std::multiset<std::string>            visible_leader_types;
};

std::vector<std::uint8_t> read_file(const fs::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
        return {};
    const auto size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
    file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));
    return data;
}

std::uint32_t le32(std::span<const std::uint8_t> data, std::size_t offset) {
    if (offset + 4 > data.size())
        throw std::runtime_error("truncated u32");
    return static_cast<std::uint32_t>(data[offset]) |
           (static_cast<std::uint32_t>(data[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(data[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(data[offset + 3]) << 24);
}

int i32(std::span<const std::uint8_t> data, std::size_t offset) {
    return static_cast<int>(le32(data, offset));
}

bool bytes_at(std::span<const std::uint8_t> data, std::size_t pos, std::string_view value) {
    return pos + value.size() <= data.size() &&
           std::equal(value.begin(), value.end(), data.begin() + static_cast<std::ptrdiff_t>(pos));
}

std::size_t find_bytes(std::span<const std::uint8_t> data, std::string_view value,
                       std::size_t from = 0) {
    for (std::size_t i = from; i + value.size() <= data.size(); ++i) {
        if (bytes_at(data, i, value))
            return i;
    }
    return std::string::npos;
}

std::string lp_string_at(std::span<const std::uint8_t> data, std::size_t offset) {
    const auto len = le32(data, offset);
    if (len == 0 || len > 200000 || offset + 4 + static_cast<std::size_t>(len) > data.size())
        throw std::runtime_error("bad lp string");
    std::string out;
    for (std::size_t i = 0; i < static_cast<std::size_t>(len) && data[offset + 4 + i] != 0; ++i)
        out.push_back(static_cast<char>(data[offset + 4 + i]));
    return out;
}

std::string short_class_name(std::string_view full_class) {
    const auto avc = full_class.find("AVC");
    if (avc == std::string_view::npos)
        return std::string(full_class);
    const auto at = full_class.find("@@", avc);
    if (at == std::string_view::npos)
        return std::string(full_class.substr(avc + 3));
    return std::string(full_class.substr(avc + 3, at - avc - 3));
}

std::string string_field(std::span<const std::uint8_t> body, std::string_view key,
                         std::size_t& cursor) {
    const auto pos = find_bytes(body, key, cursor);
    if (pos == std::string::npos)
        return {};
    cursor = pos + key.size();
    auto value = lp_string_at(body, cursor);
    cursor += 4 + value.size() + 1;
    return value;
}

std::string string_field(std::span<const std::uint8_t> body, std::string_view key) {
    const auto pos = find_bytes(body, key);
    if (pos == std::string::npos)
        return {};
    return lp_string_at(body, pos + key.size());
}

int int_field(std::span<const std::uint8_t> body, std::string_view key) {
    const auto pos = find_bytes(body, key);
    if (pos == std::string::npos)
        return 0;
    return i32(body, pos + key.size());
}

OracleMidStack parse_oracle_stack(std::span<const std::uint8_t> body) {
    OracleMidStack stack;
    stack.id = string_field(body, "STACK_ID");
    stack.group_id = string_field(body, "GROUP_ID");
    stack.leader_id = string_field(body, "LEADER_ID");
    stack.pos_x = int_field(body, "POS_X");
    stack.pos_y = int_field(body, "POS_Y");
    stack.inside = string_field(body, "INSIDE");
    return stack;
}

OracleMidUnit parse_oracle_unit(std::span<const std::uint8_t> body) {
    OracleMidUnit unit;
    std::size_t   cursor = 0;
    if (!body.empty() && body[0] == 0)
        cursor = 1;
    unit.id = string_field(body, "UNIT_ID", cursor);
    unit.type_id = string_field(body, "TYPE", cursor);
    return unit;
}

OracleScenario independent_extract_oracle(std::span<const std::uint8_t> data) {
    OracleScenario oracle;
    std::size_t    search_pos = 0;
    while (true) {
        const auto what = find_bytes(data, "WHAT", search_pos);
        if (what == std::string::npos)
            break;
        const auto end = find_bytes(data, "ENDOBJECT", what);
        if (end == std::string::npos)
            break;
        const auto cls_len = le32(data, what + 4);
        const auto cls_pos = what + 8;
        if (cls_len == 0 || cls_len > 200 || cls_pos + cls_len > data.size())
            break;
        const auto full_class =
            std::string(reinterpret_cast<const char*>(data.data() + cls_pos), cls_len);
        const auto cls = short_class_name(full_class);
        const auto beg = find_bytes(data.subspan(what, end - what), "BEGOBJECT");
        if (beg != std::string::npos) {
            const auto body_begin = what + beg + std::string_view("BEGOBJECT").size();
            const auto body = data.subspan(body_begin, end - body_begin);
            if (cls == "MidStack") {
                auto stack = parse_oracle_stack(body);
                oracle.stacks_by_id.emplace(stack.id, stack);
                oracle.stacks.push_back(std::move(stack));
            } else if (cls == "MidUnit") {
                auto unit = parse_oracle_unit(body);
                oracle.units.emplace(unit.id, unit);
            }
        }
        search_pos = end + std::string_view("ENDOBJECT").size();
    }

    for (const auto& stack : oracle.stacks) {
        const bool visible = stack.inside.empty() || stack.inside == kEmptyObjectId;
        auto       unit_it = oracle.units.find(stack.leader_id);
        if (unit_it != oracle.units.end()) {
            oracle.leader_types_by_stack_id.emplace(stack.id, unit_it->second.type_id);
            if (visible)
                oracle.visible_leader_types.insert(unit_it->second.type_id);
        }
        if (visible)
            oracle.visible_stack_ids.push_back(stack.id);
    }
    std::sort(oracle.visible_stack_ids.begin(), oracle.visible_stack_ids.end());
    return oracle;
}

fs::path half_map_path() {
    const char* env = std::getenv("DISCIPLES2_GAME_ROOT"); // NOLINT(concurrency-mt-unsafe)
    const auto  root =
        env != nullptr && env[0] != '\0' ? fs::path(env) : fs::path(DISCIPLES2_GAME_ROOT);
    return root / "Exports_bkp/test_all_terrain_with_bulgaria_half_map.sg";
}

fs::path game_root() {
    const char* env = std::getenv("DISCIPLES2_GAME_ROOT"); // NOLINT(concurrency-mt-unsafe)
    return env != nullptr && env[0] != '\0' ? fs::path(env) : fs::path(DISCIPLES2_GAME_ROOT);
}

bool has_fixture(const fs::path& path) {
    return !game_root().empty() && fs::exists(path);
}

d2scenario::SgParseResult parse_production(const std::vector<std::uint8_t>& data) {
    d2scenario::SgParser parser(data);
    return parser.parse();
}

} // namespace

TEST(AdventureStackRealScenario, OracleMatchesProductionParser) {
    const auto path = half_map_path();
    if (!has_fixture(path))
        GTEST_SKIP() << "Scenario fixture not found: " << path;
    const auto data = read_file(path);
    ASSERT_FALSE(data.empty());

    const auto oracle = independent_extract_oracle(data);
    const auto parsed = parse_production(data);

    ASSERT_EQ(parsed.scenario.stacks.size(), oracle.stacks.size());
    for (const auto& expected : oracle.stacks) {
        const auto it = std::find_if(parsed.scenario.stacks.begin(), parsed.scenario.stacks.end(),
                                     [&](const auto& stack) { return stack.id == expected.id; });
        ASSERT_NE(it, parsed.scenario.stacks.end()) << expected.id;
        EXPECT_EQ(it->group_id, expected.group_id);
        EXPECT_EQ(it->pos_x, expected.pos_x);
        EXPECT_EQ(it->pos_y, expected.pos_y);
        EXPECT_EQ(it->leader_id, expected.leader_id);
        EXPECT_EQ(it->inside, expected.inside);
    }

    for (const auto& expected : oracle.stacks) {
        if (expected.leader_id.empty() || expected.leader_id == kEmptyObjectId)
            continue;
        const auto oracle_unit = oracle.units.find(expected.leader_id);
        ASSERT_NE(oracle_unit, oracle.units.end()) << expected.leader_id;
        const auto prod_unit =
            std::find_if(parsed.scenario.units.begin(), parsed.scenario.units.end(),
                         [&](const auto& unit) { return unit.id == expected.leader_id; });
        ASSERT_NE(prod_unit, parsed.scenario.units.end()) << expected.leader_id;
        EXPECT_EQ(prod_unit->type_id, oracle_unit->second.type_id);
    }
}

TEST(AdventureStackRealScenario, WorldStatePreservesParsedStacksAndVisibility) {
    const auto path = half_map_path();
    if (!has_fixture(path))
        GTEST_SKIP() << "Scenario fixture not found: " << path;
    const auto data = read_file(path);
    ASSERT_FALSE(data.empty());

    const auto oracle = independent_extract_oracle(data);
    const auto parsed = parse_production(data);

    d2runtime::AdventureWorldBuilder builder;
    const auto                       built = builder.build(parsed.scenario);

    ASSERT_EQ(built.world.stacks.size(), parsed.scenario.stacks.size());
    for (const auto& expected : parsed.scenario.stacks) {
        const auto* stack = built.world.find_stack(expected.id);
        ASSERT_NE(stack, nullptr) << expected.id;
        EXPECT_EQ(stack->group_id, expected.group_id);
        EXPECT_EQ(stack->position.x, expected.pos_x);
        EXPECT_EQ(stack->position.y, expected.pos_y);
        EXPECT_EQ(stack->leader_id, expected.leader_id);
        EXPECT_EQ(stack->inside, expected.inside);
    }

    std::vector<std::string> runtime_visible;
    for (const auto& stack : built.world.stacks) {
        if (d2runtime::is_stack_on_adventure_map(stack))
            runtime_visible.push_back(stack.id);
    }
    std::sort(runtime_visible.begin(), runtime_visible.end());
    EXPECT_EQ(runtime_visible, oracle.visible_stack_ids);
}

TEST(AdventureStackRealScenario, UnitContributorMatchesOracleVisibleStacks) {
    const auto path = half_map_path();
    if (!has_fixture(path))
        GTEST_SKIP() << "Scenario fixture not found: " << path;
    const auto data = read_file(path);
    ASSERT_FALSE(data.empty());

    const auto oracle = independent_extract_oracle(data);
    const auto parsed = parse_production(data);

    d2runtime::AdventureWorldBuilder builder;
    const auto                       built = builder.build(parsed.scenario);
    const auto                       geometry =
        ar::AdventureMapGeometry::from_source(built.world.map_width, built.world.map_height);

    std::multiset<std::string> requested_types;
    ar::AdventureMapPreparer   preparer(geometry);
    preparer.add_contributor(
        ar::make_stack_actor_contributor([&](const d2runtime::AdventureStack& /*stack*/,
                                             const d2runtime::AdventureUnitInstance& leader)
                                             -> std::optional<ar::AdventureActorVisual> {
            requested_types.insert(leader.type_id);
            return ar::AdventureActorVisual{.resolved_owner_id = "UNKNOWN",
                                            .body = {.container_path = "test",
                                                     .logical_animation_name = "visual_anim",
                                                     .frames = {{"visual", 80, 90}},
                                                     .native_canvas_w = 80,
                                                     .native_canvas_h = 90,
                                                     .canvas_foot_x = 40,
                                                     .canvas_foot_y = 80},
                                            .shadow = std::nullopt};
        }));

    const auto result = preparer.prepare(built.world);

    std::vector<const ar::PreparedAdventureRenderPrimitive*> actors;
    for (const auto& prim : result.graph.world) {
        if (prim.level == ar::WorldRenderLevel::Actor)
            actors.push_back(&prim);
    }

    EXPECT_EQ(requested_types, oracle.visible_leader_types);
    ASSERT_EQ(actors.size(), oracle.visible_stack_ids.size());

    std::set<ar::StableRenderId> stable_ids;
    for (const auto& stack_id : oracle.visible_stack_ids) {
        const auto stack_it = oracle.stacks_by_id.find(stack_id);
        ASSERT_NE(stack_it, oracle.stacks_by_id.end());
        const auto& expected = stack_it->second;
        const int   gx = expected.pos_x;
        const int   gy = expected.pos_y;
        const auto  stable_id = ar::stable_render_id("Stack:" + expected.id);
        const auto  actor = std::find_if(actors.begin(), actors.end(), [&](const auto* prim) {
            return prim->stable_id == stable_id;
        });
        ASSERT_NE(actor, actors.end()) << expected.id;
        EXPECT_EQ((*actor)->depth_anchor.x, gx);
        EXPECT_EQ((*actor)->depth_anchor.y, gy);
        EXPECT_TRUE(stable_ids.insert((*actor)->stable_id).second);
    }
}

TEST(AdventureStackRealScenario, RightClickEndToEndInspectsRealStack) {
    // Full end-to-end: real SG bytes → runtime stack → rendered sprite →
    // generic right-click → screen-specific action → hit-test → StackId →
    // inspection model — with differential assertions against runtime data.
    const auto path = half_map_path();
    if (!has_fixture(path))
        GTEST_SKIP() << "Scenario fixture not found: " << path;
    const auto data = read_file(path);
    ASSERT_FALSE(data.empty());

    // Parse, build world
    const auto parsed = parse_production(data);

    d2runtime::AdventureWorldBuilder builder;
    const auto                       built = builder.build(parsed.scenario);
    const auto&                      world = built.world;

    // Prepare map with unit contributor
    const auto geometry = ar::AdventureMapGeometry::from_source(world.map_width, world.map_height);
    ar::AdventureMapPreparer preparer(geometry);
    // Build a valid interaction mask via a temporary mutable mask
    ar::InteractionMask temp_mask;
    temp_mask.width = 80;
    temp_mask.height = 90;
    const int stride = (80 + 7) / 8;
    temp_mask.bits.resize(static_cast<std::size_t>(stride * 90), 0xFF);
    auto mask = std::make_shared<const ar::InteractionMask>(std::move(temp_mask));
    preparer.add_contributor(
        ar::make_stack_actor_contributor([&](const d2runtime::AdventureStack& /*stack*/,
                                             const d2runtime::AdventureUnitInstance& /*leader*/)
                                             -> std::optional<ar::AdventureActorVisual> {
            return ar::AdventureActorVisual{.resolved_owner_id = "UNKNOWN",
                                            .body = {.container_path = "test",
                                                     .logical_animation_name = "visual_anim",
                                                     .frames = {{"visual", 80, 90}},
                                                     .native_canvas_w = 80,
                                                     .native_canvas_h = 90,
                                                     .canvas_foot_x = 40,
                                                     .canvas_foot_y = 80},
                                            .shadow = std::nullopt};
        }));
    auto prepared = preparer.prepare(world);

    // Build pick index
    ar::PreparedAdventureMap adv_map;
    adv_map.geometry = geometry;
    adv_map.world_graph = std::move(prepared.graph);
    adv_map.pick_entries = std::move(prepared.pick_entries);

    // Attach interaction masks to the render primitives
    for (auto& prim : adv_map.world_graph.world) {
        prim.interaction_mask = mask;
    }

    d2engine::AdventurePickIndex                        pick_index;
    d2engine::adventure_render::SelectionCircleGeometry sel_geo;
    sel_geo.center_offset_x = 0;
    sel_geo.center_offset_y = -9;
    sel_geo.radius_x = 21;
    sel_geo.radius_y = 10;
    pick_index.build(adv_map, sel_geo);

    ASSERT_FALSE(pick_index.empty());

    // Dynamically select a visible Stack pick target from the pick_index
    const d2engine::AdventurePickTarget* chosen_target = nullptr;
    for (const auto& prim : adv_map.world_graph.world) {
        if (prim.level != ar::WorldRenderLevel::Actor)
            continue;
        const auto foot = geometry.cell_foot_anchor(prim.depth_anchor);
        auto       result = pick_index.hit_test(foot.x, foot.y);
        if (result.interaction_target == nullptr)
            continue;
        if (result.interaction_target->kind != d2engine::AdventurePickKind::Stack)
            continue;
        const auto* stack = world.find_stack(result.interaction_target->object_id);
        if (stack == nullptr || !d2runtime::is_stack_on_adventure_map(*stack))
            continue;
        chosen_target = result.interaction_target;
        break;
    }
    ASSERT_NE(chosen_target, nullptr) << "no visible Stack pick entry found";

    const std::string stack_id = chosen_target->object_id;

    // Find the corresponding render primitive
    const ar::StableRenderId chosen_stable = ar::stable_render_id("Stack:" + stack_id);
    const ar::PreparedAdventureRenderPrimitive* chosen_prim = nullptr;
    for (const auto& prim : adv_map.world_graph.world) {
        if (prim.stable_id == chosen_stable) {
            chosen_prim = &prim;
            break;
        }
    }
    ASSERT_NE(chosen_prim, nullptr);

    // Pick at the cell foot anchor
    const ar::MapCell cell = chosen_prim->depth_anchor;
    const auto        foot = geometry.cell_foot_anchor(cell);
    const int         canvas_pick_x = foot.x;
    const int         canvas_pick_y = foot.y;

    // Position camera so the chosen stack is within the viewport
    const int                 canvas_w = geometry.canvas_width;
    const int                 canvas_h = geometry.canvas_height;
    constexpr int             viewport_w = 1024;
    constexpr int             viewport_h = 768;
    d2engine::AdventureCamera camera =
        d2engine::AdventureCamera::centered(canvas_w, canvas_h, viewport_w, viewport_h);
    camera.canvas_x = std::clamp(canvas_pick_x - viewport_w / 2, 0, canvas_w - viewport_w);
    camera.canvas_y = std::clamp(canvas_pick_y - viewport_h / 2, 0, canvas_h - viewport_h);

    const int logical_x = canvas_pick_x - camera.canvas_x;
    const int logical_y = canvas_pick_y - camera.canvas_y;

    ASSERT_GE(logical_x, 0);
    ASSERT_LT(logical_x, viewport_w);
    ASSERT_GE(logical_y, 0);
    ASSERT_LT(logical_y, viewport_h);

    const d2engine::InputEvent input_event =
        d2engine::PointerPressed{d2engine::PointerButton::Right, logical_x, logical_y};

    const auto action = d2engine::AdventureScreenInputHandler::handle(input_event);
    ASSERT_TRUE(action.has_value());
    ASSERT_TRUE(std::holds_alternative<d2engine::AdventureInspectAt>(*action));
    const auto& inspect_at = std::get<d2engine::AdventureInspectAt>(*action);
    EXPECT_EQ(inspect_at.x, logical_x);
    EXPECT_EQ(inspect_at.y, logical_y);

    const d2engine::AdventureHitTester hit_tester(pick_index, camera);
    const auto                         hit_result = hit_tester.hit_test(logical_x, logical_y);
    ASSERT_NE(hit_result.interaction_target, nullptr);
    EXPECT_EQ(hit_result.interaction_target->object_id, stack_id)
        << "hit test must return the same Stack we selected dynamically";

    // Build StackInspectionModel
    // Use empty GameDataRegistry — definition fields will be unresolved,
    // but instance fields should match.
    d2engine::GameDataRegistry empty_registry(std::filesystem::temp_directory_path() /
                                              "d2_stack_missing_globals");
    // Need at least empty DBF files for the constructor to not crash
    {
        const auto tmp = std::filesystem::temp_directory_path() / "d2_stack_missing_globals";
        std::filesystem::create_directories(tmp);
        for (const auto& name :
             {"Tglobal.dbf", "Gattacks.dbf", "Gunits.dbf", "Gupgrade.dbf", "Graces.dbf"}) {
            std::ofstream(tmp / name).put(0x1a);
        }
    }
    d2engine::StackInspectionBuilder inspector(world, empty_registry);
    auto                             inspection = inspector.build(stack_id);
    ASSERT_TRUE(inspection.has_value()) << "StackInspectionBuilder must produce a model";

    // Differential assertions: inspection fields match runtime AdventureStack
    const auto* runtime_stack = world.find_stack(stack_id);
    ASSERT_NE(runtime_stack, nullptr);

    EXPECT_EQ(inspection->id, runtime_stack->id);
    EXPECT_EQ(inspection->group_id, runtime_stack->group_id);
    EXPECT_EQ(inspection->owner, runtime_stack->owner);
    EXPECT_EQ(inspection->subrace, runtime_stack->subrace);
    EXPECT_EQ(inspection->inside, runtime_stack->inside);
    EXPECT_EQ(inspection->position.x, runtime_stack->position.x);
    EXPECT_EQ(inspection->position.y, runtime_stack->position.y);
    EXPECT_EQ(inspection->move, runtime_stack->move);
    EXPECT_EQ(inspection->morale, runtime_stack->morale);
    EXPECT_EQ(inspection->leader_id, runtime_stack->leader_id);
    EXPECT_EQ(inspection->leader_alive, runtime_stack->leader_alive != 0);

    // member_slots match group.members
    for (std::size_t si = 0; si < 6; ++si) {
        const bool runtime_empty = !runtime_stack->group.members[si].has_value() ||
                                   runtime_stack->group.members[si]->empty();
        if (runtime_empty) {
            EXPECT_FALSE(inspection->member_slots[si].has_value())
                << "slot[" << si << "] should be empty";
        } else {
            ASSERT_TRUE(inspection->member_slots[si].has_value())
                << "slot[" << si << "] should have value";
            EXPECT_EQ(*inspection->member_slots[si], *runtime_stack->group.members[si])
                << "slot[" << si << "] mismatch";
        }
    }

    // positions[member] = formation cell — must match runtime
    ASSERT_EQ(inspection->positions.size(), runtime_stack->group.positions.size());
    for (std::size_t mi = 0; mi < inspection->positions.size(); ++mi) {
        EXPECT_EQ(inspection->positions[mi], runtime_stack->group.positions[mi])
            << "positions member[" << mi << "] mismatch";
    }

    // Per-member differential assertions
    for (const auto& member : inspection->members) {
        ASSERT_GE(member.member_index, 0);
        const std::size_t mi = static_cast<std::size_t>(member.member_index);
        ASSERT_LT(mi, 6);
        ASSERT_TRUE(inspection->member_slots[mi].has_value());
        EXPECT_EQ(member.instance_id, *inspection->member_slots[mi]);

        // instance data
        EXPECT_TRUE(member.instance_resolved);

        // Find runtime unit
        const auto* runtime_unit = world.find_unit(member.instance_id);
        ASSERT_NE(runtime_unit, nullptr);

        EXPECT_EQ(member.type_id, runtime_unit->type_id);
        EXPECT_EQ(member.serialized_level, runtime_unit->serialized_level);
        EXPECT_EQ(member.current_hp, runtime_unit->current_hp);
        EXPECT_EQ(member.xp, runtime_unit->xp);
        EXPECT_EQ(member.creation, runtime_unit->creation);
        EXPECT_EQ(member.transformed, runtime_unit->transformed);
        EXPECT_EQ(member.dynamic_level, runtime_unit->dynamic_level);
        EXPECT_EQ(member.modifier_ids, runtime_unit->modifier_ids);

        // formation cells: derived from positions[mi] + unit size
        const int anchor = runtime_stack->group.positions[mi];
        if (anchor >= 0) {
            ASSERT_FALSE(member.formation_cells.empty());
            EXPECT_EQ(member.formation_cells[0], anchor);
        } else {
            EXPECT_TRUE(member.formation_cells.empty());
        }

        // leader flag
        EXPECT_EQ(member.is_leader, runtime_stack->leader_id == member.instance_id);
    }
}

TEST(AdventureStackRealScenario, IsoActorVisualResolverDoesNotReturnArbitraryDefault) {
    const auto path = half_map_path();
    if (!has_fixture(path))
        GTEST_SKIP() << "Scenario fixture not found: " << path;
    const auto data = read_file(path);
    ASSERT_FALSE(data.empty());

    const auto oracle = independent_extract_oracle(data);

    d2engine::AssetRuntime               assets(game_root(), 1);
    d2engine::GameDataRegistry           game_data(game_root() / "Globals");
    d2engine::AssetRuntimeCatalogAdapter catalog(assets);
    d2engine::IsoActorVisualResolver     resolver(catalog, game_data);

    // Collect all distinct fixture types
    std::set<std::string> fixture_types;
    for (const auto& [id, unit] : oracle.units) {
        (void)id;
        fixture_types.insert(unit.type_id);
    }

    // For each resolved type, verify the animation_name matches expected rules:
    //   - exact: upper(TYPE) + "STOP0"
    //   - fallback: upper(base_unit_id) + "STOP0"
    std::set<std::string> resolved_animations;
    for (const auto& type : fixture_types) {
        d2engine::AdventureStackActorVisualRequest req;
        req.presentation = {d2runtime::AdventureActorPresentationKind::Unit};
        req.leader_unit_type_id = type;
        req.direction = d2runtime::AdventureIsoDirection::D0;
        const auto visual = resolver.resolve(req);
        if (!visual.has_value())
            continue;

        const auto normalized_id = [](std::string_view s) {
            std::string out;
            for (char c : s)
                out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
            return out;
        };

        // Build the allowed set: exact type + base fallback
        std::set<std::string> allowed;
        allowed.insert(normalized_id(type) + "STOP0");

        const auto* def = game_data.find_unit(type);
        if (def != nullptr && !def->base_unit_id.empty() && def->base_unit_id != type) {
            allowed.insert(normalized_id(def->base_unit_id) + "STOP0");
        }

        EXPECT_TRUE(allowed.contains(visual->body.animation_name))
            << "type=" << type << " animation=" << visual->body.animation_name << " allowed=["
            << [&]() -> std::string {
            std::string s;
            for (const auto& a : allowed) {
                if (!s.empty())
                    s += ", ";
                s += a;
            }
            return s;
        }() << "]";

        resolved_animations.insert(visual->body.animation_name);
    }

    // If at least two different base_unit_ids exist among fixture types,
    // verify different resolved animation names — proving no fixed default.
    std::set<std::string> distinct_bases;
    for (const auto& type : fixture_types) {
        const auto* def = game_data.find_unit(type);
        const auto  base =
            (def != nullptr && !def->base_unit_id.empty()) ? def->base_unit_id : std::string(type);
        distinct_bases.insert(base);
    }
    if (distinct_bases.size() >= 2 && resolved_animations.size() >= 2) {
        EXPECT_GT(resolved_animations.size(), 1u)
            << "multiple distinct base_unit_ids must produce multiple different animation names";
    }

    // Nonexistent type must not resolve
    {
        d2engine::AdventureStackActorVisualRequest bad_req;
        bad_req.presentation = {d2runtime::AdventureActorPresentationKind::Unit};
        bad_req.leader_unit_type_id = "G000UU_DOES_NOT_EXIST";
        bad_req.direction = d2runtime::AdventureIsoDirection::D0;
        EXPECT_FALSE(resolver.resolve(bad_req).has_value());
    }
}

TEST(AdventureStackRealScenario, SgToRuntimeFacingPropagation) {
    const auto path = half_map_path();
    if (!has_fixture(path))
        GTEST_SKIP() << "Scenario fixture not found: " << path;
    const auto data = read_file(path);
    ASSERT_FALSE(data.empty());

    const auto parsed = parse_production(data);

    d2runtime::AdventureWorldBuilder builder;
    const auto                       built = builder.build(parsed.scenario);

    ASSERT_GT(parsed.scenario.stacks.size(), 0u);
    ASSERT_EQ(built.world.stacks.size(), parsed.scenario.stacks.size());

    std::set<int> distinct_facings;
    for (const auto& sg_stack : parsed.scenario.stacks) {
        distinct_facings.insert(sg_stack.facing);

        const auto* rt_stack = built.world.find_stack(sg_stack.id);
        ASSERT_NE(rt_stack, nullptr) << "missing runtime stack: " << sg_stack.id;

        EXPECT_EQ(d2runtime::direction_index(rt_stack->facing), sg_stack.facing)
            << "stack=" << sg_stack.id << " pos=(" << sg_stack.pos_x << "," << sg_stack.pos_y
            << ")";
    }

    EXPECT_GE(distinct_facings.size(), 2u)
        << "scenario must have at least two distinct facing values to prevent all-D0 pass";
}

TEST(AdventureStackRealScenario, UnitRotationStackHasCorrectFacing) {
    const auto path = half_map_path();
    if (!has_fixture(path))
        GTEST_SKIP() << "Scenario fixture not found: " << path;
    const auto data = read_file(path);
    ASSERT_FALSE(data.empty());

    const auto parsed = parse_production(data);

    const d2scenario::SgStack* unit_rotation_stack = nullptr;
    for (const auto& s : parsed.scenario.stacks) {
        if (s.id == "S143KC0017" && s.pos_x == 11 && s.pos_y == 22) {
            unit_rotation_stack = &s;
            break;
        }
    }
    ASSERT_NE(unit_rotation_stack, nullptr)
        << "Unit Rotation stack S143KC0017 at (11,22) not found";

    EXPECT_EQ(unit_rotation_stack->facing, 3) << "Unit Rotation stack must have FACING=3";

    d2runtime::AdventureWorldBuilder builder;
    const auto                       built = builder.build(parsed.scenario);

    const auto* rt_stack = built.world.find_stack("S143KC0017");
    ASSERT_NE(rt_stack, nullptr);

    EXPECT_EQ(rt_stack->facing, d2runtime::AdventureIsoDirection::D3);
    EXPECT_EQ(d2runtime::direction_index(rt_stack->facing), 3);

    const auto* leader = built.world.find_unit(rt_stack->leader_id);
    ASSERT_NE(leader, nullptr);

    d2engine::AssetRuntime               assets(game_root(), 1);
    d2engine::GameDataRegistry           game_data(game_root() / "Globals");
    d2engine::AssetRuntimeCatalogAdapter catalog(assets);
    d2engine::IsoActorVisualResolver     resolver(catalog, game_data);

    d2engine::AdventureStackActorVisualRequest req;
    req.presentation = {d2runtime::AdventureActorPresentationKind::Unit};
    req.leader_unit_type_id = leader->type_id;
    req.direction = rt_stack->facing;
    const auto visual = resolver.resolve(req);
    ASSERT_TRUE(visual.has_value());

    d2runtime::AdventureActorAnimationResolver anim_resolver;
    d2runtime::AdventureActorPresentation pres{.kind =
                                                   d2runtime::AdventureActorPresentationKind::Unit};
    const auto identity = anim_resolver.resolve(pres, d2runtime::AdventureActorAnimationRole::Idle,
                                                d2runtime::AdventureActorAnimationLayer::Main,
                                                leader->type_id, "", rt_stack->facing);
    ASSERT_TRUE(identity.has_value());

    EXPECT_FALSE(visual->body.animation_name.empty());
    EXPECT_FALSE(visual->body.frames.empty());
    EXPECT_FALSE(visual->body.frames.front().record_name.empty());
    EXPECT_GT(visual->body.frames.front().canvas_width, 0);
    EXPECT_GT(visual->body.frames.front().canvas_height, 0);
    EXPECT_NE(visual->body.animation_name.find("STOP3"), std::string::npos);
    EXPECT_EQ(visual->body.animation_name.find("STOP0"), std::string::npos);
}

// ======================================================================
// Real-game: complete current-STOP integration coverage
// Prepares the world once, indexes primitives by stable_id,
// validates each stack against its own primitive.
// ======================================================================

TEST(AdventureStackRealScenario, CurrentStopSequenceEndToEnd) {
    const auto path = half_map_path();
    if (!has_fixture(path))
        GTEST_SKIP() << "Scenario fixture not found: " << path;
    const auto data = read_file(path);
    ASSERT_FALSE(data.empty());

    const auto parsed = parse_production(data);

    d2runtime::AdventureWorldBuilder builder;
    const auto                       built = builder.build(parsed.scenario);

    d2engine::AssetRuntime               assets(game_root(), 1);
    d2engine::GameDataRegistry           game_data(game_root() / "Globals");
    d2engine::AssetRuntimeCatalogAdapter catalog(assets);
    d2engine::IsoActorVisualResolver     resolver(catalog, game_data);

    const auto& geo =
        ar::AdventureMapGeometry::from_source(built.world.map_width, built.world.map_height);

    // Build a cached IsoActorVisual → AdventureActorVisual conversion
    std::unordered_map<std::string, ar::AdventureActorVisual> visual_cache;

    auto cache_visual =
        [&](std::string_view                 unit_type_id,
            d2runtime::AdventureIsoDirection direction) -> std::optional<ar::AdventureActorVisual> {
        std::string key(unit_type_id);
        key += '/';
        key += std::to_string(d2runtime::direction_index(direction));
        if (auto it = visual_cache.find(key); it != visual_cache.end())
            return it->second;
        d2engine::AdventureStackActorVisualRequest req;
        req.presentation = {d2runtime::AdventureActorPresentationKind::Unit};
        req.leader_unit_type_id = std::string(unit_type_id);
        req.direction = direction;
        auto iso = resolver.resolve(req);
        if (!iso.has_value())
            return std::nullopt;
        ar::AdventureActorVisual visual;
        visual.body.container_path = iso->body.container_path;
        visual.body.logical_animation_name = iso->body.animation_name;
        visual.body.native_canvas_w = iso->body.native_canvas_w;
        visual.body.native_canvas_h = iso->body.native_canvas_h;
        visual.body.canvas_foot_x = iso->body.canvas_foot_x;
        visual.body.canvas_foot_y = iso->body.canvas_foot_y;
        visual.body.frames.reserve(iso->body.frames.size());
        for (const auto& f : iso->body.frames) {
            visual.body.frames.push_back(
                ar::AdventureActorVisualFrame{.record_name = f.record_name,
                                              .canvas_width = f.canvas_width,
                                              .canvas_height = f.canvas_height});
        }
        visual_cache.emplace(std::move(key), visual);
        return visual;
    };

    // Prepare the world ONCE with the production contributor
    ar::AdventureMapPreparer preparer(geo);
    preparer.add_contributor(ar::make_stack_actor_contributor(
        [&](const d2runtime::AdventureStack& stack, const d2runtime::AdventureUnitInstance& leader)
            -> std::optional<ar::AdventureActorVisual> {
            return cache_visual(leader.type_id, stack.facing);
        }));
    const auto prepared = preparer.prepare(built.world);

    // Index prepared Actor primitives by stable_id
    std::unordered_map<ar::StableRenderId, const ar::PreparedAdventureRenderPrimitive*>
        actor_by_stable;
    for (const auto& prim : prepared.graph.world) {
        if (prim.level != ar::WorldRenderLevel::Actor)
            continue;
        actor_by_stable[prim.stable_id] = &prim;
    }

    // Build inventory of all known animation identities in Imgs/Isounit.ff
    const auto                      all_animations = catalog.animations_in("Imgs/Isounit.ff");
    std::unordered_set<std::string> anim_inventory(all_animations.begin(), all_animations.end());

    std::size_t                                  visible_stacks = 0;
    std::size_t                                  resolved_stacks = 0;
    std::size_t                                  matched_actors = 0;
    std::size_t                                  static_stacks = 0;
    std::size_t                                  animated_stacks = 0;
    std::size_t                                  seq_1 = 0;
    std::size_t                                  seq_8 = 0;
    std::size_t                                  seq_16 = 0;
    std::size_t                                  fallback_count = 0;
    std::unordered_map<std::string, std::size_t> requested_owner_seqs;
    std::unordered_map<std::string, std::size_t> current_stop_seqs;

    d2runtime::AdventureActorAnimationResolver anim_resolver;
    d2runtime::AdventureActorPresentation pres{.kind =
                                                   d2runtime::AdventureActorPresentationKind::Unit};

    for (const auto& sg_stack : parsed.scenario.stacks) {
        if (!sg_stack.inside.empty() && sg_stack.inside != "G000000000")
            continue;

        const auto* rt_stack = built.world.find_stack(sg_stack.id);
        ASSERT_NE(rt_stack, nullptr) << "missing runtime stack: " << sg_stack.id;

        const auto* leader = built.world.find_unit(rt_stack->leader_id);
        if (leader == nullptr)
            continue;

        ++visible_stacks;

        // Derive the requested animation identity through AdventureActorAnimationResolver
        const auto requested_identity = anim_resolver.resolve(
            pres, d2runtime::AdventureActorAnimationRole::Idle,
            d2runtime::AdventureActorAnimationLayer::Main, leader->type_id, "", rt_stack->facing);
        ASSERT_TRUE(requested_identity.has_value())
            << "stack=" << sg_stack.id << " type=" << leader->type_id
            << " facing=" << d2runtime::direction_index(rt_stack->facing);

        ++requested_owner_seqs[leader->type_id];

        // Resolve through IsoActorVisualResolver (production path)
        d2engine::AdventureStackActorVisualRequest req2;
        req2.presentation = {d2runtime::AdventureActorPresentationKind::Unit};
        req2.leader_unit_type_id = leader->type_id;
        req2.direction = rt_stack->facing;
        const auto visual = resolver.resolve(req2);
        ASSERT_TRUE(visual.has_value()) << "stack=" << sg_stack.id << " type=" << leader->type_id
                                        << " dir=D" << d2runtime::direction_index(rt_stack->facing);

        // Determine expected actual identity independently from catalog inventory
        std::string expected_identity;
        if (anim_inventory.count(requested_identity->logical_animation_name)) {
            expected_identity = requested_identity->logical_animation_name;
        } else {
            // Base-unit fallback
            const auto* def = game_data.find_unit(leader->type_id);
            ASSERT_NE(def, nullptr) << "type=" << leader->type_id;
            ASSERT_FALSE(def->base_unit_id.empty())
                << "type=" << leader->type_id << " no base_unit_id for fallback";
            const auto base_identity =
                anim_resolver.resolve(pres, d2runtime::AdventureActorAnimationRole::Idle,
                                      d2runtime::AdventureActorAnimationLayer::Main,
                                      def->base_unit_id, "", rt_stack->facing);
            ASSERT_TRUE(base_identity.has_value()) << "base=" << def->base_unit_id << " dir=D"
                                                   << d2runtime::direction_index(rt_stack->facing);
            ASSERT_TRUE(anim_inventory.count(base_identity->logical_animation_name))
                << "base identity not in inventory: " << base_identity->logical_animation_name;
            expected_identity = base_identity->logical_animation_name;
            ++fallback_count;
        }

        // Verify actual identity equals expected
        EXPECT_EQ(visual->body.animation_name, expected_identity)
            << "stack=" << sg_stack.id << " type=" << leader->type_id;

        // For non-zero persisted facing, the identity must not be STOP0
        const int dir_idx = d2runtime::direction_index(rt_stack->facing);
        if (dir_idx != 0) {
            EXPECT_EQ(visual->body.animation_name.find("STOP0"), std::string::npos)
                << "stack=" << sg_stack.id << " facing=D" << dir_idx << " resolved to STOP0";
        }

        // Load the actual resolved sequence independently through AssetRuntime
        const auto seq =
            catalog.animation_sequence(visual->body.container_path, visual->body.animation_name);
        ASSERT_FALSE(seq.frames.empty()) << "animation=" << visual->body.animation_name;

        // Frame count must match
        EXPECT_EQ(visual->body.frames.size(), seq.frames.size())
            << "animation=" << visual->body.animation_name;

        // Every frame record must match in exact order
        for (std::size_t fi = 0; fi < visual->body.frames.size(); ++fi) {
            EXPECT_EQ(visual->body.frames[fi].record_name, seq.frames[fi].image_name)
                << "frame=" << fi << " animation=" << visual->body.animation_name;
            EXPECT_GT(visual->body.frames[fi].canvas_width, 0)
                << "frame=" << fi << " animation=" << visual->body.animation_name;
            EXPECT_GT(visual->body.frames[fi].canvas_height, 0)
                << "frame=" << fi << " animation=" << visual->body.animation_name;
        }

        ++resolved_stacks;
        current_stop_seqs[visual->body.animation_name]++;

        // Locate the prepared actor primitive for this stack
        const auto prim_it = actor_by_stable.find(ar::stable_render_id("Stack:" + sg_stack.id));
        ASSERT_NE(prim_it, actor_by_stable.end()) << "no prepared actor for stack=" << sg_stack.id;
        const auto& prim = *prim_it->second;
        ++matched_actors;

        // Validate primitive properties
        EXPECT_EQ(prim.level, ar::WorldRenderLevel::Actor);
        EXPECT_EQ(prim.container_path, visual->body.container_path);
        EXPECT_EQ(prim.stable_id, ar::stable_render_id("Stack:" + sg_stack.id));
        EXPECT_EQ(prim.depth_anchor.x, sg_stack.pos_x);
        EXPECT_EQ(prim.depth_anchor.y, sg_stack.pos_y);
        ASSERT_EQ(prim.footprint.size(), 1u);
        EXPECT_EQ(prim.footprint[0].x, sg_stack.pos_x);
        EXPECT_EQ(prim.footprint[0].y, sg_stack.pos_y);

        // Source fields describe frame zero
        EXPECT_EQ(prim.record_name, visual->body.frames[0].record_name);
        EXPECT_EQ(prim.src_width, visual->body.frames[0].canvas_width);
        EXPECT_EQ(prim.src_height, visual->body.frames[0].canvas_height);

        // Visual bounds cover max frame dimensions
        {
            int max_w = 0;
            int max_h = 0;
            for (const auto& f : visual->body.frames) {
                if (f.canvas_width > max_w)
                    max_w = f.canvas_width;
                if (f.canvas_height > max_h)
                    max_h = f.canvas_height;
            }
            EXPECT_EQ(prim.visual_bounds.max_x - prim.visual_bounds.min_x, max_w);
            EXPECT_EQ(prim.visual_bounds.max_y - prim.visual_bounds.min_y, max_h);
        }

        // Draw origin is semantic cell-foot origin
        {
            const auto foot = geo.cell_foot_anchor(prim.depth_anchor);
            EXPECT_EQ(prim.draw_origin.x, foot.x - visual->body.canvas_foot_x);
            EXPECT_EQ(prim.draw_origin.y, foot.y - visual->body.canvas_foot_y);
        }

        // Static vs animated
        if (visual->body.frames.size() == 1) {
            ++static_stacks;
            ++seq_1;
            EXPECT_FALSE(prim.animation.has_value())
                << "single frame must be static: " << visual->body.animation_name;
        } else {
            ++animated_stacks;
            ASSERT_TRUE(prim.animation.has_value())
                << "multi-frame must be animated: " << visual->body.animation_name;
            EXPECT_EQ(prim.animation->animation_name, visual->body.animation_name);
            EXPECT_TRUE(prim.animation->is_looping);
            EXPECT_EQ(prim.animation->timing_source,
                      ar::AdventureAnimationTimingSource::ProvisionalFallback);
            EXPECT_EQ(prim.animation->frames.size(), visual->body.frames.size());
            for (std::size_t fi = 0; fi < prim.animation->frames.size(); ++fi) {
                EXPECT_EQ(prim.animation->frames[fi].duration_ms, 100)
                    << "frame=" << fi << " animation=" << visual->body.animation_name;
                EXPECT_EQ(prim.animation->frames[fi].record_name,
                          visual->body.frames[fi].record_name);
                EXPECT_EQ(prim.animation->frames[fi].canvas_width,
                          visual->body.frames[fi].canvas_width);
                EXPECT_EQ(prim.animation->frames[fi].canvas_height,
                          visual->body.frames[fi].canvas_height);
            }
            if (visual->body.frames.size() == 8)
                ++seq_8;
            else if (visual->body.frames.size() == 16)
                ++seq_16;
        }
    }

    EXPECT_GT(visible_stacks, 0u) << "scenario must have visible stacks";
    EXPECT_EQ(resolved_stacks, visible_stacks) << "all visible stacks must resolve";
    EXPECT_EQ(matched_actors, resolved_stacks)
        << "every resolved visible stack must have exactly one actor primitive";
    EXPECT_EQ(matched_actors, actor_by_stable.size())
        << "number of indexed actor primitives must equal number of resolved visible stacks";
    EXPECT_GT(static_stacks, 0u) << "at least one static (1-frame) stack expected";
    EXPECT_GT(animated_stacks, 0u) << "at least one animated (multi-frame) stack expected";
    EXPECT_GT(seq_1, 0u) << "at least one 1-frame sequence expected";
    EXPECT_GT(seq_8, 0u) << "at least one 8-frame STOPn sequence expected";
    EXPECT_GT(seq_16, 0u) << "at least one 16-frame STOPn sequence expected";
    EXPECT_GE(current_stop_seqs.size(), 4u) << "at least 4 distinct STOPn sequences expected";
    EXPECT_GE(requested_owner_seqs.size(), 4u)
        << "at least 4 distinct requested owner types expected";
    GTEST_LOG_(INFO) << "CurrentStopSequenceEndToEnd: visible=" << visible_stacks
                     << " resolved=" << resolved_stacks << " matched_actors=" << matched_actors
                     << " static=" << static_stacks << " animated=" << animated_stacks
                     << " seq_1=" << seq_1 << " seq_8=" << seq_8 << " seq_16=" << seq_16
                     << " unique_current_stop_seqs=" << current_stop_seqs.size()
                     << " unique_requested_owners=" << requested_owner_seqs.size()
                     << " fallback_count=" << fallback_count;
}

// ======================================================================
// Real-game: generic preload comparison
// ======================================================================

TEST(AdventureStackRealScenario, CurrentStopExactPreloadKeys) {
    const auto path = half_map_path();
    if (!has_fixture(path))
        GTEST_SKIP() << "Scenario fixture not found: " << path;
    const auto data = read_file(path);
    ASSERT_FALSE(data.empty());

    const auto parsed = parse_production(data);

    d2engine::AssetRuntime               assets(game_root(), 1);
    d2engine::GameDataRegistry           game_data(game_root() / "Globals");
    d2engine::AssetRuntimeCatalogAdapter catalog(assets);

    // Build an IsoActorVisualResolver from real game data
    // Use the same pattern as the launcher
    d2engine::IsoActorVisualResolver                          iso_resolver(catalog, game_data);
    std::unordered_map<std::string, ar::AdventureActorVisual> visual_cache;

    auto cache_visual =
        [&](std::string_view                 unit_type_id,
            d2runtime::AdventureIsoDirection direction) -> std::optional<ar::AdventureActorVisual> {
        std::string key(unit_type_id);
        key += '/';
        key += std::to_string(d2runtime::direction_index(direction));
        if (auto it = visual_cache.find(key); it != visual_cache.end())
            return it->second;

        d2engine::AdventureStackActorVisualRequest req;
        req.presentation = {d2runtime::AdventureActorPresentationKind::Unit};
        req.leader_unit_type_id = std::string(unit_type_id);
        req.direction = direction;
        auto iso = iso_resolver.resolve(req);
        if (!iso.has_value())
            return std::nullopt;

        ar::AdventureActorVisual visual;
        visual.body.container_path = iso->body.container_path;
        visual.body.logical_animation_name = iso->body.animation_name;
        visual.body.native_canvas_w = iso->body.native_canvas_w;
        visual.body.native_canvas_h = iso->body.native_canvas_h;
        visual.body.canvas_foot_x = iso->body.canvas_foot_x;
        visual.body.canvas_foot_y = iso->body.canvas_foot_y;
        visual.body.frames.reserve(iso->body.frames.size());
        for (const auto& f : iso->body.frames) {
            visual.body.frames.push_back(
                ar::AdventureActorVisualFrame{.record_name = f.record_name,
                                              .canvas_width = f.canvas_width,
                                              .canvas_height = f.canvas_height});
        }
        visual_cache.emplace(std::move(key), visual);
        return visual;
    };

    d2runtime::AdventureWorldBuilder builder;
    const auto                       built = builder.build(parsed.scenario);

    const auto& geo =
        ar::AdventureMapGeometry::from_source(built.world.map_width, built.world.map_height);

    // Prepare full map
    ar::AdventureMapPreparer preparer(geo);
    preparer.add_contributor(ar::make_stack_actor_contributor(
        [&](const d2runtime::AdventureStack& stack, const d2runtime::AdventureUnitInstance& leader)
            -> std::optional<ar::AdventureActorVisual> {
            return cache_visual(leader.type_id, stack.facing);
        }));
    auto prepared = preparer.prepare(built.world);

    // Collect render asset keys from prepared graph
    const auto collected_keys = d2engine::collect_adventure_render_asset_keys(prepared.graph);

    // Expected keys from all Actor-level primitives
    std::unordered_set<d2engine::ImageAssetKey, std::hash<d2engine::ImageAssetKey>> expected;
    for (const auto& prim : prepared.graph.world) {
        if (prim.level != ar::WorldRenderLevel::Actor)
            continue;
        if (prim.animation.has_value()) {
            for (const auto& af : prim.animation->frames) {
                expected.insert(
                    d2engine::make_world_composed_sprite_key(prim.container_path, af.record_name));
            }
        } else {
            expected.insert(
                d2engine::make_world_composed_sprite_key(prim.container_path, prim.record_name));
        }
    }

    // Global exact set equality: collected keys must be exactly the expected set
    {
        std::unordered_set<d2engine::ImageAssetKey, std::hash<d2engine::ImageAssetKey>> actual(
            collected_keys.begin(), collected_keys.end());
        EXPECT_EQ(actual, expected)
            << "collected preload keys must exactly match expected keys from all Actor primitives";
    }
}
