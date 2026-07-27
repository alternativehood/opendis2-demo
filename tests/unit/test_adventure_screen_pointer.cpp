#include <gtest/gtest.h>

#include "d2engine/app/adventure_screen.hpp"
#include "d2engine/app/adventure_pick_index.hpp"
#include "d2engine/app/app_runtime_context.hpp"
#include "d2engine/assets/asset_runtime.hpp"
#include "d2engine/assets/ff_asset_store.hpp"
#include "d2engine/assets/game_data_registry.hpp"
#include "d2engine/assets/portrait_manifest_index.hpp"
#include "d2engine/render/adventure_render_state.hpp"
#include "d2engine/render/render_asset_runtime.hpp"
#include "d2engine/render/render_tree.hpp"

#include <d2adventure_render/adventure_render_types.hpp>
#include <d2adventure_render/prepared_adventure_map.hpp>
#include <d2game/GameSession.hpp>
#include <d2runtime/AdventureWorldState.hpp>

#include <nlohmann/json.hpp>
#include <SDL3/SDL.h>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <stdexcept>
#include <vector>

#include "tests/test_process.hpp"

namespace d2engine {
namespace ar = adventure_render;
namespace {

std::filesystem::path make_temp_game_root(int sequence) {
    const auto temp_root = std::filesystem::temp_directory_path() /
                           ("opendis2_ptr_tst_" + std::to_string(test_support::process_id()) + "_" +
                            std::to_string(sequence));
    std::error_code ec;
    std::filesystem::remove_all(temp_root, ec);
    std::filesystem::create_directories(temp_root / "Imgs", ec);
    {
        std::ofstream dummy(temp_root / "Imgs" / "Dummy.ff", std::ios::binary);
        dummy << "not opened by this test";
    }
    return temp_root;
}

class AdventureScreenPointerTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            GTEST_SKIP() << "SDL_Init failed: " << SDL_GetError();
        }
        window_ = SDL_CreateWindow("ptr-test", 1, 1, SDL_WINDOW_HIDDEN);
        if (window_ == nullptr) {
            SDL_Quit();
            GTEST_SKIP() << "SDL_CreateWindow failed: " << SDL_GetError();
        }
        renderer_ = SDL_CreateRenderer(window_, nullptr);
        if (renderer_ == nullptr) {
            SDL_DestroyWindow(window_);
            SDL_Quit();
            GTEST_SKIP() << "SDL_CreateRenderer failed: " << SDL_GetError();
        }
    }

    void TearDown() override {
        if (temp_root_valid_) {
            std::error_code ec;
            std::filesystem::remove_all(temp_root_, ec);
        }
        if (renderer_ != nullptr)
            SDL_DestroyRenderer(renderer_);
        if (window_ != nullptr)
            SDL_DestroyWindow(window_);
        SDL_Quit();
    }

    SDL_Window*           window_ = nullptr;
    SDL_Renderer*         renderer_ = nullptr;
    std::filesystem::path temp_root_;
    bool                  temp_root_valid_ = false;

    static int next_seq() {
        static std::atomic<int> s{0};
        return s.fetch_add(1);
    }
};

static std::shared_ptr<const ar::InteractionMask> make_mask(int w, int h) {
    auto mask = std::make_shared<ar::InteractionMask>();
    mask->width = w;
    mask->height = h;
    const int stride = (w + 7) / 8;
    mask->bits.assign(static_cast<std::size_t>(stride * h), 0xFF);
    return mask;
}

static ar::PreparedAdventureMap make_pick_map(const std::string& stack_id, int cell_x, int cell_y) {
    ar::PreparedAdventureMap map;
    map.geometry = ar::AdventureMapGeometry::from_source(10, 10);

    const auto stable_id = ar::stable_render_id("Stack:" + stack_id);
    map.pick_entries.push_back(
        {.stable_id = stable_id, .kind = ar::PickEntryKind::Stack, .object_id = stack_id});

    const auto                           foot = map.geometry.cell_foot_anchor({cell_x, cell_y});
    ar::PreparedAdventureRenderPrimitive prim;
    prim.stable_id = stable_id;
    prim.level = ar::WorldRenderLevel::Actor;
    prim.depth_anchor = {cell_x, cell_y};
    prim.draw_origin = {foot.x - 40, foot.y - 80};
    prim.src_width = 80;
    prim.src_height = 90;
    prim.interaction_mask = make_mask(80, 90);
    map.world_graph.world.push_back(std::move(prim));
    return map;
}

static ar::PreparedAdventureMap make_contained_stack_shield_map(const std::string& stack_id,
                                                                const std::string& settlement_id,
                                                                int cell_x, int cell_y) {
    ar::PreparedAdventureMap map;
    map.geometry = ar::AdventureMapGeometry::from_source(10, 10);

    const auto stable_id = ar::stable_render_id("ContainedStackShield:" + stack_id);
    map.pick_entries.push_back(
        {.stable_id = stable_id, .kind = ar::PickEntryKind::Stack, .object_id = stack_id});

    const auto                           foot = map.geometry.cell_foot_anchor({cell_x, cell_y});
    ar::PreparedAdventureRenderPrimitive prim;
    prim.stable_id = stable_id;
    prim.debug_label = "ContainedStackShield:" + settlement_id + ":" + stack_id;
    prim.level = ar::WorldRenderLevel::Actor;
    prim.depth_anchor = {cell_x, cell_y};
    prim.draw_origin = {foot.x - 40, foot.y - 80};
    prim.src_width = 80;
    prim.src_height = 90;
    prim.interaction_mask = make_mask(80, 90);
    map.world_graph.world.push_back(std::move(prim));
    return map;
}

static d2runtime::AdventureWorldState make_world_with_single_stack() {
    d2runtime::AdventureWorldState world;
    world.scenario_id = "ptr-test";
    world.scenario_name = "ptr-test";
    world.map_width = 10;
    world.map_height = 10;

    d2runtime::AdventureStack stack;
    stack.id = "S143STPTR";
    stack.group_id = "S143GRPTR";
    stack.owner = "G000PL0001";
    stack.subrace = "";
    stack.position.x = 5;
    stack.position.y = 5;
    stack.move = 4;
    stack.morale = 50;
    stack.leader_id = "S143UNPTR";
    stack.leader_alive = 1;
    stack.group.members[0] = "S143UNPTR";
    stack.group.positions[0] = 0;
    world.stacks.push_back(std::move(stack));

    d2runtime::AdventureUnitInstance unit;
    unit.id = "S143UNPTR";
    unit.type_id = "G000UU0042";
    unit.serialized_level = 3;
    unit.current_hp = 42;
    unit.xp = 7;
    unit.creation = 1;
    unit.transformed = 0;
    world.units.push_back(std::move(unit));
    return world;
}

static AdventureRenderState make_test_render_state(SDL_Renderer* renderer) {
    constexpr int             w = 1416;
    constexpr int             h = 852;
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4,
                                     0);
    auto                      tex = create_sdl_texture(renderer, w, h, pixels.data(), w * 4);
    if (!tex) {
        throw std::runtime_error("failed to create test terrain texture");
    }

    AdventureRenderState state;
    state.set_terrain_texture(std::move(tex), w, h);
    state.set_camera(AdventureCamera::centered(w, h, 1416, 852));
    return state;
}

TEST_F(AdventureScreenPointerTest, CursorKindDefaultWhenNoInteraction) {
    // Create minimal objects needed for AdventureScreen construction.
    temp_root_ = make_temp_game_root(next_seq());
    temp_root_valid_ = true;

    auto         store = std::make_unique<FfAssetStore>(temp_root_);
    AssetRuntime assets(
        std::move(store), [](const ImageAssetKey&) { return PreparedImageResult{}; }, 1);
    RenderAssetRuntime    render_assets(renderer_, assets);
    GameDataRegistry      game_data(std::filesystem::path("/nonexistent"));
    PortraitManifest      empty_manifest;
    PortraitManifestIndex portraits(empty_manifest);

    AppRuntimeContext runtime{.assets = assets,
                              .render_assets = render_assets,
                              .game_data = game_data,
                              .portraits = portraits};

    auto session = std::make_unique<d2game::GameSession>(make_world_with_single_stack());

    ar::PreparedAdventureMap prepared_map;
    prepared_map.geometry = ar::AdventureMapGeometry::from_source(10, 10);

    TreeLayout     tree;
    nlohmann::json j;
    j["/adventure"] = nlohmann::json::object();
    tree.load(j);

    AdventureWorldVisualResources visuals;
    AdventureScreen screen(runtime, std::move(session), std::move(prepared_map),
                           AdventureRenderState{}, 1416, 852, std::move(tree), "test://cfg",
                           nullptr, nullptr, nullptr, std::move(visuals));

    EXPECT_EQ(screen.cursor_kind(), CursorKind::Default);
}

TEST_F(AdventureScreenPointerTest, ToggleBannersFlipsDebugVisibilityState) {
    temp_root_ = make_temp_game_root(next_seq());
    temp_root_valid_ = true;

    auto         store = std::make_unique<FfAssetStore>(temp_root_);
    AssetRuntime assets(
        std::move(store), [](const ImageAssetKey&) { return PreparedImageResult{}; }, 1);
    RenderAssetRuntime    render_assets(renderer_, assets);
    GameDataRegistry      game_data(std::filesystem::path("/nonexistent"));
    PortraitManifest      empty_manifest;
    PortraitManifestIndex portraits(empty_manifest);

    AppRuntimeContext runtime{.assets = assets,
                              .render_assets = render_assets,
                              .game_data = game_data,
                              .portraits = portraits};

    auto session = std::make_unique<d2game::GameSession>(make_world_with_single_stack());

    ar::PreparedAdventureMap prepared_map;
    prepared_map.geometry = ar::AdventureMapGeometry::from_source(10, 10);

    TreeLayout     tree;
    nlohmann::json j;
    j["/adventure"] = nlohmann::json::object();
    tree.load(j);

    AdventureWorldVisualResources visuals;
    AdventureScreen screen(runtime, std::move(session), std::move(prepared_map),
                           AdventureRenderState{}, 1416, 852, std::move(tree), "test://cfg",
                           nullptr, nullptr, nullptr, std::move(visuals));

    EXPECT_TRUE(screen.debug_banners_enabled());
    screen.debug_toggle_banners();
    EXPECT_FALSE(screen.debug_banners_enabled());
    screen.debug_toggle_banners();
    EXPECT_TRUE(screen.debug_banners_enabled());
}

TEST_F(AdventureScreenPointerTest, SelectAtStoresPointerPosition) {
    temp_root_ = make_temp_game_root(next_seq());
    temp_root_valid_ = true;

    auto         store = std::make_unique<FfAssetStore>(temp_root_);
    AssetRuntime assets(
        std::move(store), [](const ImageAssetKey&) { return PreparedImageResult{}; }, 1);
    RenderAssetRuntime    render_assets(renderer_, assets);
    GameDataRegistry      game_data(std::filesystem::path("/nonexistent"));
    PortraitManifest      empty_manifest;
    PortraitManifestIndex portraits(empty_manifest);

    AppRuntimeContext runtime{.assets = assets,
                              .render_assets = render_assets,
                              .game_data = game_data,
                              .portraits = portraits};

    auto session = std::make_unique<d2game::GameSession>(make_world_with_single_stack());

    ar::PreparedAdventureMap prepared_map;
    prepared_map.geometry = ar::AdventureMapGeometry::from_source(10, 10);

    TreeLayout     tree;
    nlohmann::json j;
    j["/adventure"] = nlohmann::json::object();
    tree.load(j);

    AdventureWorldVisualResources visuals;
    AdventureScreen screen(runtime, std::move(session), std::move(prepared_map),
                           AdventureRenderState{}, 1416, 852, std::move(tree), "test://cfg",
                           nullptr, nullptr, nullptr, std::move(visuals));

    // Click at (100, 200) — should store pointer position even without PointerMoved
    InputEvent click = PointerPressed{PointerButton::Left, 100, 200};
    screen.handle_input(click);

    EXPECT_EQ(screen.cursor_kind(), CursorKind::Default);
}

TEST_F(AdventureScreenPointerTest, NullSessionConstructorThrows) {
    temp_root_ = make_temp_game_root(next_seq());
    temp_root_valid_ = true;

    auto         store = std::make_unique<FfAssetStore>(temp_root_);
    AssetRuntime assets(
        std::move(store), [](const ImageAssetKey&) { return PreparedImageResult{}; }, 1);
    RenderAssetRuntime    render_assets(renderer_, assets);
    GameDataRegistry      game_data(std::filesystem::path("/nonexistent"));
    PortraitManifest      empty_manifest;
    PortraitManifestIndex portraits(empty_manifest);

    AppRuntimeContext runtime{.assets = assets,
                              .render_assets = render_assets,
                              .game_data = game_data,
                              .portraits = portraits};

    ar::PreparedAdventureMap prepared_map;
    prepared_map.geometry = ar::AdventureMapGeometry::from_source(10, 10);

    TreeLayout     tree;
    nlohmann::json j;
    j["/adventure"] = nlohmann::json::object();
    tree.load(j);

    AdventureWorldVisualResources visuals;
    EXPECT_THROW(AdventureScreen(runtime, std::unique_ptr<d2game::GameSession>{},
                                 std::move(prepared_map), AdventureRenderState{}, 1416, 852,
                                 std::move(tree), "test://cfg", nullptr, nullptr, nullptr,
                                 std::move(visuals)),
                 std::invalid_argument);
}

TEST_F(AdventureScreenPointerTest, RightClickInspectUsesOwnedSessionAndReportsStackInfo) {
    temp_root_ = make_temp_game_root(next_seq());
    temp_root_valid_ = true;

    auto         store = std::make_unique<FfAssetStore>(temp_root_);
    AssetRuntime assets(
        std::move(store), [](const ImageAssetKey&) { return PreparedImageResult{}; }, 1);
    RenderAssetRuntime    render_assets(renderer_, assets);
    GameDataRegistry      game_data(std::filesystem::path("/nonexistent"));
    PortraitManifest      empty_manifest;
    PortraitManifestIndex portraits(empty_manifest);

    AppRuntimeContext runtime{.assets = assets,
                              .render_assets = render_assets,
                              .game_data = game_data,
                              .portraits = portraits};

    std::optional<d2engine::StackInspectionModel> received_model;
    int                                           callback_count = 0;

    std::unique_ptr<AdventureScreen> screen;
    {
        auto session = std::make_unique<d2game::GameSession>(make_world_with_single_stack());
        auto prepared_map = make_pick_map("S143STPTR", 5, 5);
        auto render_state = make_test_render_state(renderer_);

        TreeLayout     tree;
        nlohmann::json j;
        j["/adventure"] = nlohmann::json::object();
        tree.load(j);

        AdventureWorldVisualResources visuals;
        screen = std::make_unique<AdventureScreen>(
            runtime, std::move(session), std::move(prepared_map), std::move(render_state), 1416,
            852, std::move(tree), "test://cfg", nullptr, nullptr,
            [&](StackInspectionModel model) {
                ++callback_count;
                received_model = std::move(model);
            },
            std::move(visuals));

        ASSERT_EQ(session, nullptr);
    }

    ASSERT_NE(screen, nullptr);
    screen->on_enter();

    const auto foot = ar::AdventureMapGeometry::from_source(10, 10).cell_foot_anchor({5, 5});
    InputEvent event = PointerPressed{PointerButton::Right, foot.x, foot.y};
    EXPECT_TRUE(screen->handle_input(event));

    ASSERT_EQ(callback_count, 1);
    ASSERT_TRUE(received_model.has_value());
    EXPECT_EQ(received_model->id, "S143STPTR");
    EXPECT_EQ(received_model->position.x, 5);
    EXPECT_EQ(received_model->position.y, 5);
}

TEST_F(AdventureScreenPointerTest,
       RightClickInspectContainedStackShieldUsesOwnedSessionAndReportsStackInfo) {
    temp_root_ = make_temp_game_root(next_seq());
    temp_root_valid_ = true;

    auto         store = std::make_unique<FfAssetStore>(temp_root_);
    AssetRuntime assets(
        std::move(store), [](const ImageAssetKey&) { return PreparedImageResult{}; }, 1);
    RenderAssetRuntime    render_assets(renderer_, assets);
    GameDataRegistry      game_data(std::filesystem::path("/nonexistent"));
    PortraitManifest      empty_manifest;
    PortraitManifestIndex portraits(empty_manifest);

    AppRuntimeContext runtime{.assets = assets,
                              .render_assets = render_assets,
                              .game_data = game_data,
                              .portraits = portraits};

    std::optional<d2engine::StackInspectionModel> received_model;
    int                                           callback_count = 0;

    std::unique_ptr<AdventureScreen> screen;
    {
        auto session = std::make_unique<d2game::GameSession>([] {
            d2runtime::AdventureWorldState world;
            world.map_width = 10;
            world.map_height = 10;
            world.terrain.width = 10;
            world.terrain.height = 10;
            world.terrain.tiles.assign(100, {});

            d2runtime::AdventureSubraceRef sr;
            sr.id = "SUB1";
            sr.race_id = "g000rr0000";
            world.subraces.push_back(sr);

            d2runtime::AdventureCity city;
            city.id = "CITY1";
            city.stack_id = "STACK1";
            city.footprint = {{5, 5}, {6, 5}, {5, 6}, {6, 6}};
            world.cities.push_back(city);

            d2runtime::AdventureStack stack;
            stack.id = "STACK1";
            stack.leader_id = "S143UNPTR";
            stack.inside = "CITY1";
            stack.subrace = "SUB1";
            world.stacks.push_back(stack);

            d2runtime::AdventureUnitInstance unit;
            unit.id = "S143UNPTR";
            unit.type_id = "G000UU0042";
            unit.current_hp = 42;
            world.units.push_back(unit);

            return world;
        }());
        auto prepared_map = make_contained_stack_shield_map("STACK1", "CITY1", 6, 6);
        auto render_state = make_test_render_state(renderer_);

        TreeLayout     tree;
        nlohmann::json j;
        j["/adventure"] = nlohmann::json::object();
        tree.load(j);

        AdventureWorldVisualResources visuals;
        screen = std::make_unique<AdventureScreen>(
            runtime, std::move(session), std::move(prepared_map), std::move(render_state), 1416,
            852, std::move(tree), "test://cfg", nullptr, nullptr,
            [&](StackInspectionModel model) {
                ++callback_count;
                received_model = std::move(model);
            },
            std::move(visuals));
    }

    ASSERT_NE(screen, nullptr);
    screen->on_enter();

    const auto foot = ar::AdventureMapGeometry::from_source(10, 10).cell_foot_anchor({6, 6});
    InputEvent event = PointerPressed{PointerButton::Right, foot.x, foot.y};
    EXPECT_TRUE(screen->handle_input(event));

    ASSERT_EQ(callback_count, 1);
    ASSERT_TRUE(received_model.has_value());
    EXPECT_EQ(received_model->id, "STACK1");
}

} // namespace
} // namespace d2engine
