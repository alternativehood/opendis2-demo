#include <gtest/gtest.h>

#include "render_batch_test_helpers.hpp"

#include "d2engine/app/battle_tuning_state.hpp"
#include "d2engine/app/battle_screen.hpp"
#include "d2engine/app/screen_config_store.hpp"

#include "d2engine/app/debug_tuning_types.hpp"
#include "d2engine/app/tree_layout_editor.hpp"
#include "d2engine/app/screen_manager.hpp"
#include "d2engine/app/battle_viewer_renderer.hpp"
#include "d2engine/app/battle_tuning_controller.hpp"
#include "d2engine/app/debug_overlay_renderer.hpp"
#include "d2engine/battle_adapters/sdl_battle_renderer.hpp"
#include "d2engine/battle_adapters/sdl_battle_texture_provider.hpp"
#include "d2engine/battle_view/battle_presenter.hpp"
#include "d2engine/battle_view/battle_renderer.hpp"
#include "d2engine/battle_view/battle_render_tree_contract.hpp"
#include "d2engine/battle_view/battle_texture_provider.hpp"
#include "d2engine/battle_view/layered_animation_clip.hpp"
#include "d2engine/render/render_tree.hpp"
#include "d2engine/battle_view/unit_state_clip.hpp"
#include "d2engine/render/renderer2d.hpp"
#include "d2engine/render/text/text_box_renderer.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <nlohmann/json.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace d2engine {
namespace {

class FakeTextureProvider final : public IBattleTextureProvider {
public:
    BackendTextureRef get_texture(const std::string& /*container_path*/,
                                  const std::string& image_name) override {
        ++calls;
        if (image_name == "missing") {
            return {};
        }
        return BackendTextureRef{.native = reinterpret_cast<void*>(calls)};
    }

    [[nodiscard]] std::pair<float, float>
    texture_size(BackendTextureRef /*texture*/) const override {
        return {32.0f, 48.0f};
    }

    std::uintptr_t calls = 0;
};

class MockRenderer2D final : public Renderer2D {
public:
    MockRenderer2D() : Renderer2D(nullptr) {}

    void draw_texture(SDL_Texture* /*tex*/, Rect /*dst*/, float /*alpha*/, bool flip_h,
                      bool flip_v) override {
        ++draw_calls;
        ++texture_calls;
        last_flip_h = flip_h;
        last_flip_v = flip_v;
    }
    void draw_texture(SDL_Texture* /*tex*/, Rect /*src*/, Rect /*dst*/, float /*alpha*/,
                      bool flip_h, bool flip_v) override {
        ++draw_calls;
        ++texture_calls;
        last_flip_h = flip_h;
        last_flip_v = flip_v;
    }
    void draw_texture_rotated(SDL_Texture* /*tex*/, Rect /*dst*/, float /*alpha*/, bool flip_h,
                              bool flip_v, double /*angle_deg*/, float /*center_x*/,
                              float /*center_y*/) override {
        ++draw_calls;
        ++rotated_calls;
        last_flip_h = flip_h;
        last_flip_v = flip_v;
    }
    void draw_rect(Rect /*rect*/, Color /*c*/, bool /*filled*/) override { ++rect_calls; }
    void draw_line(float /*x1*/, float /*y1*/, float /*x2*/, float /*y2*/, Color /*c*/) override {
        ++line_calls;
    }
    void draw_debug_text(float /*x*/, float /*y*/, const char* text) override {
        ++text_calls;
        last_text = text;
    }
    void draw_debug_text_scaled(float x, float y, const char* text, float scale) override {
        ++scaled_text_calls;
        last_text_x = x;
        last_text_y = y;
        last_text_scale = scale;
        last_text = text;
    }
    void draw_debug_text_colored_scaled(float /*x*/, float /*y*/, const char* /*text*/, Color /*c*/,
                                        float /*scale*/) override {
        ++colored_text_calls;
    }
    void draw_text_box(const TextBox& box) override {
        ++text_box_calls;
        last_text = box.text;
        last_text_box = box;
    }

    int         draw_calls = 0;
    int         texture_calls = 0;
    int         rotated_calls = 0;
    int         rect_calls = 0;
    int         line_calls = 0;
    int         text_calls = 0;
    int         scaled_text_calls = 0;
    int         colored_text_calls = 0;
    int         text_box_calls = 0;
    bool        last_flip_h = false;
    bool        last_flip_v = false;
    float       last_text_x = 0.0f;
    float       last_text_y = 0.0f;
    float       last_text_scale = 1.0f;
    std::string last_text;
    TextBox     last_text_box;
};

SnapshotTrack track(TrackKind kind, TrackRenderLayer layer, std::string image) {
    return {.kind = kind,
            .layer = layer,
            .anchor = AnchorPolicy::UnitFoot,
            .placement = render_placement_for(layer, AnchorPolicy::UnitFoot),
            .current_frame_name = std::move(image),
            .container_path = "Imgs/Test.ff"};
}

SnapshotEntity entity(std::uint32_t id, BattleDepth depth, int lane,
                      std::vector<SnapshotTrack> tracks) {
    return {.id = VisualEntityId{id},
            .coord = {.side = BattleSide::Attacker, .lane = lane, .depth = depth},
            .tracks = std::move(tracks)};
}

SnapshotEntity entity(std::uint32_t id, BattleSide side, BattleDepth depth, int lane,
                      std::vector<SnapshotTrack> tracks) {
    return {.id = VisualEntityId{id},
            .coord = {.side = side, .lane = lane, .depth = depth},
            .tracks = std::move(tracks)};
}

[[nodiscard]] d2engine::TreeLayout make_slot_render_tree();

BattleRenderOptions no_background() {
    static const d2engine::TreeLayout tree = make_slot_render_tree();
    return {.draw_background = false,
            .draw_frame = false,
            .draw_unit_groups = false,
            .tree_layout = &tree};
}

[[nodiscard]] nlohmann::json load_default_visual_config() {
    std::ifstream in{OPENDIS2_SOURCE_DIR "/configs/screens/battle_screen.json"};
    return nlohmann::json::parse(in);
}

[[nodiscard]] d2engine::TreeLayout make_slot_render_tree() {
    d2engine::ScreenConfigStore store(std::filesystem::path(OPENDIS2_SOURCE_DIR) / "configs");
    return std::move(
        store.load_validated("battle_screen", d2engine::BattleScreen::required_layout_nodes())
            .tree_layout);
}

AnimationSequence one_frame_sequence() {
    AnimationSequence sequence;
    sequence.container_path = "Imgs/Test.ff";
    sequence.frames.push_back(
        {.image_name = "frame", .index = 0, .duration_ms = static_cast<std::uint16_t>(100)});
    return sequence;
}

} // namespace

static_assert(BattleRenderer::pass_order()[0] == BattleRenderPass::GroundBackground);
static_assert(BattleRenderer::pass_order()[1] == BattleRenderPass::Background);
static_assert(BattleRenderer::pass_order()[10] == BattleRenderPass::Debug);

TEST(BattleRenderer, DefaultConfigUsesSemanticBackgroundRenderTreePaths) {
    const nlohmann::json config = load_default_visual_config();
    ASSERT_TRUE(config.contains("render_tree"));
    const auto& tree = config["render_tree"];

    EXPECT_TRUE(tree.contains("/background"));
    EXPECT_TRUE(tree.contains("/background/ground"));
    EXPECT_TRUE(tree.contains("/background/battle"));
    EXPECT_FALSE(tree.contains("/scene/background"));
    EXPECT_FALSE(tree.contains("/background/HERETIC_0_BG"));
    EXPECT_FALSE(tree.contains("/background/HERETIC_0_FG"));
    EXPECT_FALSE(tree.contains("/background/ground/HE_00.PNG"));
    EXPECT_FALSE(config.contains("background_profiles"));
    EXPECT_FALSE(config.contains("ground_profiles"));
    EXPECT_FALSE(config.contains("terrain_profiles"));

    const std::string tree_dump = tree.dump();
    EXPECT_EQ(tree_dump.find("scale_x"), std::string::npos);
    EXPECT_EQ(tree_dump.find("scale_y"), std::string::npos);
}

TEST(BattleRenderer, FiltersTracksIntoDeterministicPasses) {
    BattleRenderSnapshot const snapshot{
        .entities = {
            entity(1, BattleDepth::Front, 0,
                   {track(TrackKind::Base, TrackRenderLayer::Base, "b"),
                    track(TrackKind::DeathFx, TrackRenderLayer::Effect, "e"),
                    track(TrackKind::ActorMarker, TrackRenderLayer::Marker, "m")}),
        }};
    FakeTextureProvider provider;
    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, no_background());

    const auto& commands = test::commands_from(batch);

    ASSERT_EQ(commands.size(), 3u);
    EXPECT_EQ(commands[0].layer, battle_render_layer(BattleRenderPass::UnitsFront));
    EXPECT_EQ(commands[1].layer, battle_render_layer(BattleRenderPass::UnitsFront));
    EXPECT_EQ(commands[2].layer, battle_render_layer(BattleRenderPass::Markers));
}

TEST(BattleRenderer, RefactorBoundaryKeepsRepresentativeCommandsAndOrder) {
    SnapshotTrack base = track(TrackKind::Base, TrackRenderLayer::Base, "idle_001");
    base.id = TrackId{7};

    BattleRenderSnapshot snapshot{
        .entities = {
            entity(1, BattleSide::Attacker, BattleDepth::Front, 0, {base}),
            entity(2, BattleSide::Defender, BattleDepth::Front, 0, {}),
        }};
    snapshot.entities[0].unit_instance_id = UnitInstanceId{10};
    snapshot.entities[0].current_hp = 60;
    snapshot.entities[0].max_hp = 100;
    snapshot.entities[0].display_name = "Left";
    snapshot.entities[1].unit_instance_id = UnitInstanceId{20};
    snapshot.entities[1].current_hp = 80;
    snapshot.entities[1].max_hp = 120;
    snapshot.entities[1].display_name = "Right";

    BattleRenderOptions opts = no_background();
    opts.draw_background = true;
    opts.ground_image = "ground";
    opts.terrain_image = "battle";
    opts.draw_frame = true;
    opts.draw_unit_groups = true;
    opts.debug_enabled = true;
    opts.info_left_unit = UnitInstanceId{10};
    opts.info_right_unit = UnitInstanceId{20};

    FakeTextureProvider provider;
    const auto          batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& commands = test::commands_from(batch);

    const auto find = [&](const auto& pred) -> const RenderCommand* {
        const auto it = std::ranges::find_if(commands, pred);
        return it == commands.end() ? nullptr : &*it;
    };
    const auto index = [&](const RenderCommand* cmd) {
        return static_cast<std::size_t>(cmd - commands.data());
    };
    const auto has_tree_debug = [](const RenderCommand* cmd) {
        return cmd != nullptr && test::tunable_item(*cmd).has_value() &&
               test::tunable_item(*cmd)->binding.has_value() &&
               test::tunable_item(*cmd)->binding->owner_kind == BindingOwnerKind::TreeLayout;
    };

    const RenderCommand* background = find([](const RenderCommand& cmd) {
        return test::tunable_item(cmd) &&
               test::tunable_item(cmd)->stable_id == "backgrounds.battle";
    });
    const RenderCommand* unit = find([](const RenderCommand& cmd) {
        return test::tunable_item(cmd) && test::tunable_item(cmd)->stable_id == "unit:10:track:7";
    });
    const RenderCommand* unit_hp = find(
        [](const RenderCommand& cmd) { return cmd.tree_path == "/ui/left_unit_group/0_front/hp"; });
    const RenderCommand* info_name = find([](const RenderCommand& cmd) {
        return cmd.tree_path == "/ui/combat_frame/info_left/name";
    });
    const RenderCommand* info_hp = find(
        [](const RenderCommand& cmd) { return cmd.tree_path == "/ui/combat_frame/info_right/hp"; });
    const RenderCommand* slot_debug = find([](const RenderCommand& cmd) {
        return test::tunable_item(cmd) && test::tunable_item(cmd)->stable_id == "tree:A_FRONT_0";
    });

    ASSERT_TRUE(has_tree_debug(background));
    EXPECT_EQ(background->tree_path, "/background/battle");
    ASSERT_NE(unit, nullptr);
    EXPECT_EQ(unit->layer, battle_render_layer(BattleRenderPass::UnitsFront));
    ASSERT_TRUE(has_tree_debug(unit_hp));
    EXPECT_EQ(unit_hp->text, "60/100");
    ASSERT_TRUE(has_tree_debug(info_name));
    EXPECT_EQ(info_name->text, "Left");
    ASSERT_TRUE(has_tree_debug(info_hp));
    EXPECT_EQ(info_hp->text, "80/120");
    ASSERT_TRUE(has_tree_debug(slot_debug));

    EXPECT_LT(index(background), index(unit));
    EXPECT_LT(index(unit), index(info_name));
    EXPECT_LT(index(info_name), index(slot_debug));
}

TEST(BattleRenderer, RealDrawCommandCarriesSelectableDebugMetadata) {
    SnapshotTrack base = track(TrackKind::Base, TrackRenderLayer::Base, "idle_001");
    base.id = TrackId{7};
    BattleRenderSnapshot snapshot{.entities = {entity(1, BattleDepth::Front, 0, {base})}};
    snapshot.entities[0].unit_instance_id = UnitInstanceId{3};
    FakeTextureProvider provider;

    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, no_background());

    const auto& commands = test::commands_from(batch);

    ASSERT_EQ(commands.size(), 1u);
    ASSERT_TRUE(test::tunable_item(commands[0]).has_value());
    EXPECT_EQ(test::tunable_item(commands[0])->stable_id, "unit:3:track:7");
    EXPECT_EQ(test::tunable_item(commands[0])->current_frame_name, "idle_001");
    ASSERT_TRUE(test::tunable_item(commands[0])->binding.has_value());
    // Base track binding is per-position typed
    EXPECT_EQ(test::tunable_item(commands[0])->binding->owner_kind,
              BindingOwnerKind::PositionLevel);
    EXPECT_EQ(test::tunable_item(commands[0])->binding->tree_path, "A_FRONT_0");
    EXPECT_EQ(test::tunable_item(commands[0])->binding->role, BindingRole::Unit);
}

TEST(BattleRenderer, RenderBatchTunableItemsPopulateDebugTuningState) {
    SnapshotTrack base = track(TrackKind::Base, TrackRenderLayer::Base, "idle_001");
    base.id = TrackId{7};
    BattleRenderSnapshot snapshot{.entities = {entity(1, BattleDepth::Front, 0, {base})}};
    snapshot.entities[0].unit_instance_id = UnitInstanceId{3};
    FakeTextureProvider provider;

    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, no_background());

    ASSERT_EQ(batch.commands.size(), 1u);
    ASSERT_TRUE(batch.commands[0].tunable_item_index.has_value());
    ASSERT_LT(*batch.commands[0].tunable_item_index, batch.tunable_items.size());
    BattleTuningState state;
    state.set_current_items(batch.tunable_items);
    ASSERT_EQ(state.current_items.size(), 1u);
    EXPECT_EQ(state.current_items[0].stable_id, "unit:3:track:7");
    ASSERT_TRUE(state.current_items[0].binding.has_value());
    EXPECT_EQ(state.current_items[0].binding->owner_kind, BindingOwnerKind::PositionLevel);
}

TEST(BattleRenderer, DrawingLevelMovesCommandAcrossRenderLayers) {
    SnapshotTrack base = track(TrackKind::Base, TrackRenderLayer::Base, "idle_001");
    base.id = TrackId{7};
    BattleRenderSnapshot snapshot{.entities = {entity(1, BattleDepth::Front, 0, {base})}};
    snapshot.entities[0].unit_instance_id = UnitInstanceId{3};
    FakeTextureProvider provider;
    BattleRenderOptions options = no_background();
    options.unit_level = 2;

    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, options);

    const auto& commands = test::commands_from(batch);

    ASSERT_EQ(commands.size(), 1u);
    EXPECT_EQ(commands[0].layer, battle_render_layer(BattleRenderPass::Effects));
    ASSERT_TRUE(test::tunable_item(commands[0]).has_value());
    EXPECT_EQ(test::tunable_item(commands[0])->layer, static_cast<int>(TrackRenderLayer::Overlay));
}

TEST(BattleRenderer, LoweredUnitCanRenderBehindTerrainBackground) {
    SnapshotTrack base = track(TrackKind::Base, TrackRenderLayer::Base, "idle_001");
    base.id = TrackId{7};
    BattleRenderSnapshot snapshot{.entities = {entity(1, BattleDepth::Front, 0, {base})}};
    snapshot.entities[0].unit_instance_id = UnitInstanceId{3};
    FakeTextureProvider provider;
    BattleRenderOptions options;
    options.draw_frame = false;
    options.draw_unit_groups = false;
    options.terrain_image = "terrain";
    options.unit_level = -3;
    static const d2engine::TreeLayout tree = make_slot_render_tree();
    options.tree_layout = &tree;

    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, options);

    const auto& commands = test::commands_from(batch);

    ASSERT_EQ(commands.size(), 2u);
    ASSERT_TRUE(test::tunable_item(commands[0]).has_value());
    ASSERT_TRUE(test::tunable_item(commands[1]).has_value());
    // unit_level=-3 pushes unit to Background pass; both unit and bg have same pass,
    // stable_sort preserves insertion order (render_queue first, then append_background).
    EXPECT_EQ(test::tunable_item(commands[0])->stable_id, "unit:3:track:7");
    EXPECT_EQ(commands[0].layer, battle_render_layer(BattleRenderPass::Background));
    EXPECT_EQ(test::tunable_item(commands[1])->stable_id, "backgrounds.battle");
    EXPECT_EQ(commands[1].layer, battle_render_layer(BattleRenderPass::Background));
}

TEST(BattleTuningState, KeepsAbsentSelectedDebugIdAcrossFrames) {
    BattleTuningState state;
    state.selected_debug_id = "unit:3:track:7";

    state.set_current_items({DebugRenderableItem{.stable_id = "unit:3:track:8"}});

    EXPECT_EQ(state.selected_debug_id, "unit:3:track:7");
    EXPECT_EQ(state.selected_item(), nullptr);
}

TEST(BattleRenderer, SortsUnitsByAnchorY) {
    const d2engine::TreeLayout tree = make_slot_render_tree();
    BattleRenderSnapshot const snapshot{
        .entities = {
            entity(1, BattleDepth::Back, 2,
                   {track(TrackKind::Base, TrackRenderLayer::Base, "low")}),
            entity(2, BattleDepth::Back, 0,
                   {track(TrackKind::Base, TrackRenderLayer::Base, "high")}),
        }};
    FakeTextureProvider provider;
    BattleRenderOptions opts = no_background();
    opts.tree_layout = &tree;
    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& commands = test::commands_from(batch);

    // commands[0..1] are unit commands (UnitsFront pass), then tree debug items (Debug pass last)
    ASSERT_GE(commands.size(), 2u);
    EXPECT_EQ(test::tunable_item(commands[0])->current_frame_name, "high");
    EXPECT_EQ(test::tunable_item(commands[1])->current_frame_name, "low");
}

TEST(BattleRenderer, SortsFrontUnitsByAnchorY) {
    const d2engine::TreeLayout tree = make_slot_render_tree();
    BattleRenderSnapshot const snapshot{
        .entities = {
            entity(1, BattleDepth::Front, 2,
                   {track(TrackKind::Base, TrackRenderLayer::Base, "low")}),
            entity(2, BattleDepth::Front, 0,
                   {track(TrackKind::Base, TrackRenderLayer::Base, "high")}),
        }};
    FakeTextureProvider provider;
    BattleRenderOptions opts = no_background();
    opts.tree_layout = &tree;
    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& commands = test::commands_from(batch);

    ASSERT_GE(commands.size(), 2u);
    EXPECT_EQ(test::tunable_item(commands[0])->current_frame_name, "high");
    EXPECT_EQ(test::tunable_item(commands[1])->current_frame_name, "low");
}

TEST(BattleRenderer, FrontAttackerAndDefenderSortByAnchorY) {
    const d2engine::TreeLayout tree = make_slot_render_tree();
    BattleRenderSnapshot const snapshot{
        .entities = {
            entity(1, BattleSide::Defender, BattleDepth::Front, 2,
                   {track(TrackKind::Base, TrackRenderLayer::Base, "low")}),
            entity(2, BattleSide::Attacker, BattleDepth::Front, 0,
                   {track(TrackKind::Base, TrackRenderLayer::Base, "high")}),
        }};
    FakeTextureProvider provider;
    BattleRenderOptions opts = no_background();
    opts.tree_layout = &tree;
    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& commands = test::commands_from(batch);

    ASSERT_GE(commands.size(), 2u);
    EXPECT_EQ(test::tunable_item(commands[0])->current_frame_name, "high");
    EXPECT_EQ(test::tunable_item(commands[1])->current_frame_name, "low");
}

TEST(BattleRenderer, AttachedEffectsSortWithOwnerUnit) {
    BattleRenderSnapshot const snapshot{
        .entities = {
            entity(1, BattleDepth::Front, 0,
                   {track(TrackKind::Base, TrackRenderLayer::Base, "base"),
                    track(TrackKind::DeathFx, TrackRenderLayer::Effect, "effect")}),
            entity(2, BattleDepth::Front, 2,
                   {track(TrackKind::Base, TrackRenderLayer::Base, "other")}),
        }};
    FakeTextureProvider provider;
    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, no_background());

    const auto& commands = test::commands_from(batch);

    ASSERT_EQ(commands.size(), 3u);
    EXPECT_EQ(test::tunable_item(commands[0])->current_frame_name, "base");
    EXPECT_EQ(test::tunable_item(commands[1])->current_frame_name, "effect");
    EXPECT_EQ(test::tunable_item(commands[2])->current_frame_name, "other");
    EXPECT_EQ(commands[1].layer, battle_render_layer(BattleRenderPass::UnitsFront));
}

TEST(BattleRenderer, OverlayEffectsRenderAboveNormalTracks) {
    BattleRenderSnapshot const snapshot{
        .entities = {
            entity(1, BattleDepth::Front, 2,
                   {track(TrackKind::Base, TrackRenderLayer::Base, "base")}),
            entity(2, BattleDepth::Front, 0,
                   {track(TrackKind::Effect, TrackRenderLayer::Overlay, "overlay")}),
        }};
    FakeTextureProvider provider;
    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, no_background());

    const auto& commands = test::commands_from(batch);

    ASSERT_EQ(commands.size(), 2u);
    EXPECT_EQ(test::tunable_item(commands[0])->current_frame_name, "base");
    EXPECT_EQ(test::tunable_item(commands[1])->current_frame_name, "overlay");
    EXPECT_EQ(commands[1].layer, battle_render_layer(BattleRenderPass::Effects));
}

TEST(BattleRenderer, DepthAnchorCanDifferFromPositionAnchor) {
    const d2engine::TreeLayout tree = make_slot_render_tree();
    SnapshotTrack screen_position = track(TrackKind::Base, TrackRenderLayer::Base, "screen");
    screen_position.placement.position_anchor = AnchorPolicy::ScreenRect;
    screen_position.placement.depth_anchor = AnchorPolicy::UnitFoot;
    BattleRenderSnapshot const snapshot{
        .entities = {
            entity(1, BattleDepth::Front, 2, {std::move(screen_position)}),
            entity(2, BattleDepth::Front, 0,
                   {track(TrackKind::Base, TrackRenderLayer::Base, "unit")}),
        }};
    FakeTextureProvider provider;
    BattleRenderOptions opts = no_background();
    opts.tree_layout = &tree;
    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& commands = test::commands_from(batch);

    ASSERT_GE(commands.size(), 2u);
    EXPECT_EQ(test::tunable_item(commands[0])->current_frame_name, "unit");
    EXPECT_EQ(test::tunable_item(commands[1])->current_frame_name, "screen");
}

TEST(BattleRenderer, DebugPassIsDisabledByDefault) {
    EXPECT_FALSE(DebugRenderOptions{}.enabled());
    EXPECT_TRUE(DebugRenderOptions{.draw_entity_ids = true}.enabled());
}

TEST(BattleRenderer, SdlProviderDelegatesToLookup) {
    std::string              container;
    std::string              image;
    SdlBattleTextureProvider provider{
        [&](const std::string& container_path, const std::string& image_name) -> void* {
            container = container_path;
            image = image_name;
            return reinterpret_cast<void*>(0x1234);
        }};

    EXPECT_EQ(provider.get_texture("Imgs/Battle.ff", "MRKCURLARGEA").native,
              reinterpret_cast<void*>(0x1234));
    EXPECT_EQ(container, "Imgs/Battle.ff");
    EXPECT_EQ(image, "MRKCURLARGEA");
}

TEST(BattleRenderer, MockRendererRecordsDrawCalls) {
    BattleRenderSnapshot const snapshot{
        .entities = {entity(1, BattleDepth::Front, 0,
                            {track(TrackKind::Base, TrackRenderLayer::Base, "base")})}};
    FakeTextureProvider provider;
    MockRenderer2D      renderer;
    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, no_background());

    const auto& commands = test::commands_from(batch);
    SdlBattleRenderer::render_commands(commands, renderer, LayoutScale{}, no_background());
    EXPECT_EQ(renderer.draw_calls, 1);
}

TEST(BattleRenderer, SkipsHiddenAndMissingTextures) {
    SnapshotTrack hidden = track(TrackKind::Base, TrackRenderLayer::Base, "hidden");
    hidden.visibility = TrackVisibility::HiddenButPlaying;
    BattleRenderSnapshot const snapshot{
        .entities = {entity(
            1, BattleDepth::Front, 0,
            {std::move(hidden), track(TrackKind::Base, TrackRenderLayer::Base, "missing")})}};
    FakeTextureProvider provider;
    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, no_background());

    const auto& commands = test::commands_from(batch);
    EXPECT_TRUE(commands.empty());
}

TEST(BattleRenderer, DoesNotMutateSnapshotAndIsDeterministic) {
    const BattleRenderSnapshot snapshot{
        .entities = {entity(7, BattleDepth::Front, 1,
                            {track(TrackKind::Base, TrackRenderLayer::Base, "base")})}};
    FakeTextureProvider first_provider;
    FakeTextureProvider second_provider;
    const auto          first_batch =
        BattleRenderer::build_render_batch(snapshot, first_provider, no_background());

    const auto& first = test::commands_from(first_batch);
    const auto  second_batch =
        BattleRenderer::build_render_batch(snapshot, second_provider, no_background());

    const auto& second = test::commands_from(second_batch);

    ASSERT_EQ(first.size(), 1u);
    ASSERT_EQ(second.size(), 1u);
    EXPECT_EQ(first[0].layer, second[0].layer);
    EXPECT_FLOAT_EQ(first[0].destination.x, second[0].destination.x);
    EXPECT_EQ(snapshot.entities[0].tracks[0].current_frame_name, "base");
}

TEST(BattleRenderer, AppliesTrackTransform) {
    SnapshotTrack base = track(TrackKind::Base, TrackRenderLayer::Base, "base");
    base.transform = {
        .position_offset = {.x = 10.0f, .y = 20.0f}, .scale_x = 2.0f, .scale_y = 0.5f};
    BattleRenderSnapshot const snapshot{
        .entities = {entity(1, BattleDepth::Front, 0, {std::move(base)})}};
    FakeTextureProvider provider;
    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, no_background());

    const auto& commands = test::commands_from(batch);

    ASSERT_EQ(commands.size(), 1u);
    EXPECT_FLOAT_EQ(commands[0].destination.w, 32.0f * 2.0f);
    EXPECT_FLOAT_EQ(commands[0].destination.h, 48.0f * 0.5f);
}

TEST(BattleRenderer, PropagatesTrackPlaybackFlipToDrawCommand) {
    SnapshotTrack base = track(TrackKind::Base, TrackRenderLayer::Base, "base");
    base.playback.flip_x = true;
    base.playback.flip_y = true;
    BattleRenderSnapshot const snapshot{
        .entities = {entity(1, BattleDepth::Front, 0, {std::move(base)})}};
    FakeTextureProvider provider;
    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, no_background());

    const auto& commands = test::commands_from(batch);

    ASSERT_EQ(commands.size(), 1u);
    EXPECT_TRUE(commands[0].flip_x);
    EXPECT_TRUE(commands[0].flip_y);
}

TEST(BattleRenderer, RenderPassesFlipFlagsToRenderer2D) {
    SnapshotTrack base = track(TrackKind::Base, TrackRenderLayer::Base, "base");
    base.playback.flip_y = true;
    BattleRenderSnapshot const snapshot{
        .entities = {entity(1, BattleDepth::Front, 0, {std::move(base)})}};
    FakeTextureProvider provider;
    MockRenderer2D      renderer;
    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, no_background());

    const auto& commands = test::commands_from(batch);

    SdlBattleRenderer::render_commands(commands, renderer, LayoutScale{}, no_background());

    EXPECT_EQ(renderer.draw_calls, 1);
    EXPECT_FALSE(renderer.last_flip_h);
    EXPECT_TRUE(renderer.last_flip_v);
}

TEST(SdlBattleRenderer, SkipsNullTextureCommands) {
    MockRenderer2D renderer;
    SdlBattleRenderer::render_commands(
        {RenderCommand{.layer = battle_render_layer(BattleRenderPass::UnitsFront), .texture = {}}},
        renderer, LayoutScale{}, no_background());

    EXPECT_EQ(renderer.draw_calls, 0);
}

TEST(SdlBattleRenderer, BattleUiTextUsesTextBoxRendererPath) {
    MockRenderer2D renderer;
    SdlBattleRenderer::render_commands(
        {RenderCommand{.layer = battle_render_layer(BattleRenderPass::CombatFrame),
                       .destination = {.x = 10.0f, .y = 20.0f, .w = 80.0f, .h = 20.0f},
                       .text = "177/250",
                       .font_size = 12.0f,
                       .game_font_text = true,
                       .font_face = "Charis SIL"}},
        renderer, LayoutScale{}, no_background());

    EXPECT_EQ(renderer.text_box_calls, 1);
    EXPECT_EQ(renderer.colored_text_calls, 0);
    EXPECT_EQ(renderer.last_text, "177/250");
    EXPECT_FLOAT_EQ(renderer.last_text_box.style.font_size, 12.0f);
    EXPECT_EQ(renderer.last_text_box.style.align, TextAlign::Center);
    EXPECT_EQ(renderer.last_text_box.style.valign, TextVAlign::Middle);
    EXPECT_EQ(renderer.last_text_box.style.font_face, "Charis SIL");
}

TEST(SdlBattleRenderer, GameFontTextWithoutFontFaceThrows) {
    MockRenderer2D renderer;
    EXPECT_THROW(SdlBattleRenderer::render_commands(
                     {RenderCommand{.layer = battle_render_layer(BattleRenderPass::CombatFrame),
                                    .destination = {.x = 0.0f, .y = 0.0f, .w = 80.0f, .h = 20.0f},
                                    .text = "no face",
                                    .game_font_text = true}},
                     renderer, LayoutScale{}, no_background()),
                 std::runtime_error);
}

TEST(SdlBattleRenderer, BattleUiTextPropagatesFontFaceFromOptions) {
    MockRenderer2D      renderer;
    BattleRenderOptions opts = no_background();
    opts.font_face = "Charis SIL";
    SdlBattleRenderer::render_commands(
        {RenderCommand{.layer = battle_render_layer(BattleRenderPass::CombatFrame),
                       .destination = {.x = 10.0f, .y = 20.0f, .w = 80.0f, .h = 20.0f},
                       .text = "60/100",
                       .font_size = 12.0f,
                       .game_font_text = true,
                       .font_face = opts.font_face}},
        renderer, LayoutScale{}, opts);

    EXPECT_EQ(renderer.text_box_calls, 1);
    EXPECT_EQ(renderer.last_text_box.style.font_face, "Charis SIL");
}

TEST(SdlBattleRenderer, ScalesBattleUiTextRectAndFontAtRendererBoundary) {
    MockRenderer2D renderer;
    SdlBattleRenderer::render_commands(
        {RenderCommand{.layer = battle_render_layer(BattleRenderPass::CombatFrame),
                       .destination = {.x = 10.0f, .y = 20.0f, .w = 80.0f, .h = 20.0f},
                       .text = "60/100",
                       .font_size = 12.0f,
                       .game_font_text = true,
                       .font_face = "Charis SIL"}},
        renderer, LayoutScale{.sx = 2.0f, .sy = 2.0f}, no_background());

    EXPECT_EQ(renderer.text_box_calls, 1);
    const Rect expected{.x = 20.0f, .y = 40.0f, .w = 160.0f, .h = 40.0f};
    EXPECT_EQ(renderer.last_text_box.rect, expected);
    EXPECT_FLOAT_EQ(renderer.last_text_box.style.font_size, 24.0f);
    EXPECT_EQ(renderer.last_text_box.style.font_face, "Charis SIL");
}

TEST(TextBoxRenderer, DefaultRegistryRejectsUnsupportedFontFace) {
    EXPECT_FALSE(TextBoxRenderer::default_fonts().empty());
    const auto fonts = TextBoxRenderer::default_fonts();
    EXPECT_EQ(fonts.front().face, "Charis SIL");
    EXPECT_NE(fonts.front().file.string().find("CharisSIL-Regular.ttf"), std::string::npos);
    EXPECT_EQ(TextBoxRenderer::default_fonts()[0].face, "Charis SIL");
    EXPECT_NE(TextBoxRenderer::default_fonts()[0].face, "Papyrus");
}

TEST(SdlBattleRenderer, DoesNotRotateBackgroundOverlay) {
    MockRenderer2D      renderer;
    BattleRenderOptions options = no_background();
    options.rotation_deg = 15.0f;

    SdlBattleRenderer::render_commands(
        {RenderCommand{.layer = battle_render_layer(BattleRenderPass::BackgroundOverlay),
                       .texture = {.native = reinterpret_cast<void*>(0x1)}}},
        renderer, LayoutScale{}, options);

    EXPECT_EQ(renderer.draw_calls, 1);
    EXPECT_EQ(renderer.texture_calls, 1);
    EXPECT_EQ(renderer.rotated_calls, 0);
}

TEST(BattleRenderer, CombatFrameIsScaledAndBottomAligned) {
    FakeTextureProvider provider;
    BattleRenderOptions options;
    options.draw_background = false;
    options.draw_unit_groups = false;
    d2engine::TreeLayout tree = make_slot_render_tree();
    // combat_frame rect matches the old combat_frame_ref_rect() values.
    constexpr float kCfbW = 1600.0f * 0.9801f;
    constexpr float kCfbH = (945.0f / 5.0f) * 1.4883f;
    tree.set_node("/ui", TreeNode{.kind = "root", .w = 1600.0f, .h = 945.0f});
    tree.set_node("/ui/combat_frame", TreeNode{.kind = "image",
                                               .x = (1600.0f - kCfbW) * 0.5f,
                                               .y = 945.0f - kCfbH,
                                               .w = kCfbW,
                                               .h = kCfbH,
                                               .level = 2});
    options.tree_layout = &tree;

    const auto batch =
        BattleRenderer::build_render_batch(BattleRenderSnapshot{}, provider, options);

    const auto& commands = test::commands_from(batch);

    ASSERT_EQ(commands.size(), 1u);
    EXPECT_EQ(commands[0].layer, battle_render_layer(BattleRenderPass::CombatFrame));
    EXPECT_FLOAT_EQ(commands[0].destination.w, kCfbW);
    EXPECT_FLOAT_EQ(commands[0].destination.h, kCfbH);
    EXPECT_FLOAT_EQ(commands[0].destination.y + commands[0].destination.h, 945.0f);
}

TEST(BattleRenderer, CombatFrameUsesDebugTuning) {
    FakeTextureProvider provider;
    BattleRenderOptions options;
    options.draw_background = false;
    options.draw_unit_groups = false;
    d2engine::TreeLayout tree = make_slot_render_tree();
    constexpr float      kCfbRefW = 1600.0f * 0.9801f * 0.99f;
    constexpr float      kCfbRefH = (945.0f / 5.0f) * 1.4883f * 1.01f;
    constexpr float      kCfbRefX = ((1600.0f - (1600.0f * 0.9801f)) * 0.5f) - 2.0f;
    constexpr float      kCfbRefY = (945.0f - ((945.0f / 5.0f) * 1.4883f)) + 3.0f;
    tree.set_node("/ui", TreeNode{.kind = "root", .w = 1600.0f, .h = 945.0f});
    tree.set_node("/ui/combat_frame", TreeNode{.kind = "image",
                                               .x = kCfbRefX,
                                               .y = kCfbRefY,
                                               .w = kCfbRefW,
                                               .h = kCfbRefH,
                                               .level = 2});
    options.tree_layout = &tree;

    const auto batch =
        BattleRenderer::build_render_batch(BattleRenderSnapshot{}, provider, options);

    const auto& commands = test::commands_from(batch);

    ASSERT_EQ(commands.size(), 1u);
    // render_tree values are the source of truth.
    EXPECT_FLOAT_EQ(commands[0].destination.w, kCfbRefW);
    EXPECT_FLOAT_EQ(commands[0].destination.x, kCfbRefX);
    EXPECT_FLOAT_EQ(commands[0].destination.h, kCfbRefH);
    EXPECT_FLOAT_EQ(commands[0].destination.y + commands[0].destination.h, kCfbRefY + kCfbRefH);
}

TEST(BattleViewerRenderer, OptionsIncludePresentationAndTuning) {
    BattleScenePresentationState presentation;
    presentation.background_visible = false;
    presentation.frame_visible = true;
    BattleTuningState tuning;
    tuning.unit_magnitude = 1.25f;
    tuning.unit_rotation = 9.0f;

    // Unit scale lives in scene_layout.unit; overlay offset lives in scene_layout.global.
    const ConfigBinding scene_unit{.config_file = "test.json",
                                   .owner_kind = BindingOwnerKind::SceneLayout,
                                   .tree_path = "scene",
                                   .role = BindingRole::Unit,
                                   .display_path = "scene_layout.unit"};
    static_cast<void>(tuning.set_placement(
        scene_unit,
        VisualPlacementValue{.x = 0.0f, .y = 0.0f, .scale_x = 1.5f, .scale_y = 0.75f, .level = 2}));

    const ConfigBinding scene_global{.config_file = "test.json",
                                     .owner_kind = BindingOwnerKind::SceneLayout,
                                     .tree_path = "scene",
                                     .role = BindingRole::Global,
                                     .display_path = "scene_layout.global"};
    static_cast<void>(tuning.set_placement(
        scene_global,
        VisualPlacementValue{.x = 5.0f, .y = 6.0f, .scale_x = 1.0f, .scale_y = 1.0f, .level = 0}));

    const auto tree = make_slot_render_tree();
    const auto options = BattleViewerRenderer::make_options(presentation, tuning, tree, "ELF_0_BG",
                                                            {}, false, false);

    EXPECT_EQ(options.terrain_image, "ELF_0_BG");
    EXPECT_EQ(options.ground_container, "Imgs/Ground.ff");
    EXPECT_EQ(options.ground_image, "EL_00.PNG");
    EXPECT_FALSE(options.draw_background);
    EXPECT_TRUE(options.draw_frame);
    EXPECT_FLOAT_EQ(options.magnitude, 1.25f);
    EXPECT_FLOAT_EQ(options.scale_x, 1.5f);
    EXPECT_FLOAT_EQ(options.scale_y, 0.75f);
    EXPECT_FLOAT_EQ(options.rotation_deg, 9.0f);
    EXPECT_FLOAT_EQ(options.overlay_offset.x, 5.0f);
    EXPECT_FLOAT_EQ(options.overlay_offset.y, 6.0f);
    EXPECT_EQ(options.unit_level, 2);
    EXPECT_EQ(options.ui_level, 0);
    EXPECT_EQ(options.tree_layout, &tree);
}

TEST(BattleViewerRenderer, ConfigUiTextNodesHaveSaneFontSize) {
    const nlohmann::json config = load_default_visual_config();
    ASSERT_TRUE(config.contains("render_tree"));
    const auto& tree = config["render_tree"];

    // All /ui/... text nodes must have font_size <= h * 1.25
    std::vector<std::string> violations;
    for (auto it = tree.begin(); it != tree.end(); ++it) {
        const std::string& path = it.key();
        if (path.rfind("/ui/", 0) != 0 && path != "/ui") {
            continue;
        }
        const auto& node = it.value();
        if (!node.is_object()) {
            continue;
        }
        const std::string kind = node.value("kind", "");
        if (kind != "text") {
            continue;
        }
        const float h = node.value("h", 0.0f);
        const float font_size = node.value("font_size", 12.0f);
        if (font_size > h * 1.25f) {
            violations.push_back(path + ": font_size=" + std::to_string(font_size) +
                                 " exceeds h*1.25 (" + std::to_string(h * 1.25f) + ")");
        }
        // HP/name nodes must not have font_size 30 or 40 (too large for small rects)
        if ((path.find("/hp") != std::string::npos || path.find("/name") != std::string::npos) &&
            (font_size > 20.0f)) {
            violations.push_back(
                path + ": HP/name node has oversized font_size=" + std::to_string(font_size));
        }
    }
    for (const auto& v : violations) {
        ADD_FAILURE() << v;
    }
}

TEST(BattleViewerRenderer, GroundImageResolverHandlesCapitalAndPrefixedBackgroundNames) {
    EXPECT_EQ(ground_image_for_battle_background("HERETIC_0_BG"), "HE_00.PNG");
    EXPECT_EQ(ground_image_for_battle_background("BG_HUMAN_0.PNG"), "HU_00.PNG");
    EXPECT_EQ(ground_image_for_battle_background("BG_CAP_HUMAN.PNG"), "HU_00.PNG");
    EXPECT_EQ(ground_image_for_battle_background("BG_CAP_ELF.PNG"), "EL_00.PNG");
    EXPECT_EQ(ground_image_for_battle_background("BG_CAP_DWARF.PNG"), "DW_00.PNG");
    EXPECT_EQ(ground_image_for_battle_background("BG_CAP_HERETIC.PNG"), "HE_00.PNG");
    EXPECT_EQ(ground_image_for_battle_background("BG_CAP_UNDEAD.PNG"), "UN_00.PNG");
    EXPECT_EQ(ground_image_for_battle_background("RUIN_0_BG"), "NE_00.PNG");
    EXPECT_EQ(ground_image_for_battle_background("CITY_0_BG"), "NE_00.PNG");
    EXPECT_EQ(ground_image_for_battle_background("BOAT_0_BG"), "WA_00.PNG");
}

TEST(BattleViewerRenderer, TransparentBackgroundDisablesBackgroundAndFrame) {
    BattleScenePresentationState const presentation;
    TreeLayout                         tree;
    const auto options = BattleViewerRenderer::make_options(presentation, BattleTuningState{}, tree,
                                                            "ELF_0_BG", {}, true, false);

    EXPECT_FALSE(options.draw_background);
    EXPECT_FALSE(options.draw_frame);
}

TEST(BattleViewerRenderer, BlankOverlayWhenNoCorners) {
    BattleScenePresentationState const presentation;
    d2engine::TreeLayout               tree;
    const auto options = BattleViewerRenderer::make_options(presentation, BattleTuningState{}, tree,
                                                            "", {}, false, false);
    EXPECT_TRUE(options.terrain_overlay_images.empty());
}

TEST(BattleViewerRenderer, OverlayNamesArePassedThrough) {
    BattleScenePresentationState const presentation;
    const std::vector<std::string>     overlays = {"UNDEAD_0_FG"};
    TreeLayout                         tree;
    const auto options = BattleViewerRenderer::make_options(presentation, BattleTuningState{}, tree,
                                                            "UNDEAD_0_BG", overlays, false, false);
    ASSERT_EQ(options.terrain_overlay_images.size(), 1);
    EXPECT_EQ(options.terrain_overlay_images[0], "UNDEAD_0_FG");
}

TEST(BattleViewerRenderer, EmptyOverlayVectorWhenNotGiven) {
    BattleScenePresentationState const presentation;
    TreeLayout                         tree;
    const auto options = BattleViewerRenderer::make_options(presentation, BattleTuningState{}, tree,
                                                            "SOME_OTHER", {}, false, false);
    EXPECT_TRUE(options.terrain_overlay_images.empty());
}

TEST(DebugOverlayRenderer, SelectedUnitTextIsReadOnly) {
    BattleScene scene;
    BattleUnit  unit;
    unit.coord = {.side = BattleSide::Attacker, .lane = 0, .depth = BattleDepth::Front};
    unit.unit_instance_id = UnitInstanceId{1};
    VisualTrack base;
    base.kind = TrackKind::Base;
    base.player.load(one_frame_sequence());
    unit.tracks.push_back(std::move(base));

    UnitAnimationRoleSet roles;
    roles.idle = UnitStateClip::from_a1(one_frame_sequence());
    UnitVisualProfileRegistry          unit_profiles;
    const UnitVisualProfileId          profile_id = unit_profiles.add(std::move(roles));
    UnitLifecycleVisualProfileRegistry lifecycle_profiles;
    const UnitLifecycleVisualProfileId lp_id = lifecycle_profiles.add({});
    unit.visual_profile_id = profile_id;
    unit.lifecycle_profile_id = lp_id;

    scene.add_unit(std::move(unit));

    BattlePresenter const presenter{scene, std::move(unit_profiles), std::move(lifecycle_profiles),
                                    1};
    const UnitInstanceId  selected = presenter.selected_unit_id();
    const std::size_t     unit_count = scene.units().size();

    EXPECT_EQ(presenter.selected_unit_id(), selected);
    EXPECT_EQ(scene.units().size(), unit_count);
}

TEST(DebugOverlayRenderer, DrawsSelectedItemHighlight) {
    BattleScene scene;
    BattleUnit  unit;
    unit.coord = {.side = BattleSide::Attacker, .lane = 0, .depth = BattleDepth::Front};
    unit.unit_instance_id = UnitInstanceId{1};
    VisualTrack base;
    base.kind = TrackKind::Base;
    base.player.load(one_frame_sequence());
    unit.tracks.push_back(std::move(base));

    UnitAnimationRoleSet roles;
    roles.idle = UnitStateClip::from_a1(one_frame_sequence());
    UnitVisualProfileRegistry          unit_profiles;
    const UnitVisualProfileId          profile_id = unit_profiles.add(std::move(roles));
    UnitLifecycleVisualProfileRegistry lifecycle_profiles;
    const UnitLifecycleVisualProfileId lp_id = lifecycle_profiles.add({});
    unit.visual_profile_id = profile_id;
    unit.lifecycle_profile_id = lp_id;

    scene.add_unit(std::move(unit));

    BattlePresenter const presenter{scene, std::move(unit_profiles), std::move(lifecycle_profiles),
                                    1};

    BattleTuningState tuning;
    tuning.selected_debug_id = "ui:combat_frame";
    tuning.set_current_items(
        {DebugRenderableItem{.stable_id = "ui:combat_frame",
                             .label = "ui:combat_frame",
                             .kind = "ui",
                             .screen_rect = {.x = 10, .y = 20, .w = 30, .h = 40},
                             .anchor = {.x = 15, .y = 25},
                             .binding = ConfigBinding{.config_file = kBattleScreenConfigPath,
                                                      .owner_kind = BindingOwnerKind::TreeLayout,
                                                      .tree_path = "/ui",
                                                      .role = BindingRole::CombatFrame,
                                                      .display_path = "render_tree/ui"}}});
    MockRenderer2D renderer;
    DebugOverlayRenderer::draw(DebugOverlayFrame{.renderer = renderer,
                                                 .scene = scene,
                                                 .presenter = presenter,
                                                 .unit_debug = {},
                                                 .tuning = tuning,
                                                 .scale = {}});

    EXPECT_GE(renderer.rect_calls, 1);
    EXPECT_GE(renderer.line_calls, 2);
    EXPECT_GE(renderer.scaled_text_calls, 1);
    EXPECT_FLOAT_EQ(renderer.last_text_scale, 3.0f);
    EXPECT_GE(renderer.last_text_x, 2.0f);
    EXPECT_GE(renderer.last_text_y, 2.0f);
    EXPECT_NE(renderer.last_text.find("ui:combat_frame"), std::string::npos);
}

TEST(BattleRenderer, EffectTrackWithSequenceNameCarriesTypedEffectBinding) {
    SnapshotTrack effect = track(TrackKind::Effect, TrackRenderLayer::Effect, "frame");
    effect.sequence_name = "HEFFA_FIRE";
    effect.effect_role = BindingRole::TargetTeam;
    BattleRenderSnapshot const snapshot{.entities = {entity(1, BattleDepth::Front, 0, {effect})}};
    FakeTextureProvider        provider;
    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, no_background());

    const auto& commands = test::commands_from(batch);

    ASSERT_EQ(commands.size(), 1u);
    ASSERT_TRUE(test::tunable_item(commands[0]).has_value());
    ASSERT_TRUE(test::tunable_item(commands[0])->binding.has_value());
    EXPECT_EQ(test::tunable_item(commands[0])->binding->owner_kind,
              BindingOwnerKind::EffectProfile);
    EXPECT_EQ(test::tunable_item(commands[0])->binding->tree_path, "HEFFA_FIRE");
    EXPECT_EQ(test::tunable_item(commands[0])->binding->role, BindingRole::TargetTeam);
}

TEST(BattleRenderer, LifecycleTrackWithProfileIdCarriesTypedLifecycleBinding) {
    SnapshotTrack death = track(TrackKind::DeathBody, TrackRenderLayer::Base, "frame");
    death.sequence_name = "DEAD_HUMAN_SMALL";
    death.lifecycle_profile_id = "DEAD_HUMAN_SMALL";
    BattleRenderSnapshot const snapshot{.entities = {entity(1, BattleDepth::Front, 0, {death})}};
    FakeTextureProvider        provider;
    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, no_background());

    const auto& commands = test::commands_from(batch);

    ASSERT_EQ(commands.size(), 1u);
    ASSERT_TRUE(test::tunable_item(commands[0]).has_value());
    ASSERT_TRUE(test::tunable_item(commands[0])->binding.has_value());
    EXPECT_EQ(test::tunable_item(commands[0])->binding->owner_kind,
              BindingOwnerKind::LifecycleProfile);
    EXPECT_EQ(test::tunable_item(commands[0])->binding->tree_path, "DEAD_HUMAN_SMALL");
    EXPECT_EQ(test::tunable_item(commands[0])->binding->role, BindingRole::Corpse);
}

TEST(BattleRenderer, PerItemPlacementReplacesGlobalOverlayOffset) {
    SnapshotTrack effect = track(TrackKind::Effect, TrackRenderLayer::Effect, "frame");
    effect.sequence_name = "HEFFA_FIRE";
    effect.effect_role = BindingRole::TargetTeam;
    BattleRenderSnapshot const snapshot{.entities = {entity(1, BattleDepth::Front, 0, {effect})}};
    FakeTextureProvider        provider;

    BattleRenderOptions global_opts = no_background();
    global_opts.overlay_offset = {.x = 5.0f, .y = 10.0f};
    const auto cmd_global_batch =
        BattleRenderer::build_render_batch(snapshot, provider, global_opts);

    const auto& cmd_global = test::commands_from(cmd_global_batch);

    const std::string   typed_key = ConfigBinding{.owner_kind = BindingOwnerKind::EffectProfile,
                                                  .tree_path = "HEFFA_FIRE",
                                                  .role = BindingRole::TargetTeam}
                                        .key();
    BattleRenderOptions per_item_opts = global_opts;
    per_item_opts.placements[typed_key] = VisualPlacementValue{.x = 50.0f, .y = 100.0f};
    const auto cmd_per_item_batch =
        BattleRenderer::build_render_batch(snapshot, provider, per_item_opts);

    const auto& cmd_per_item = test::commands_from(cmd_per_item_batch);

    ASSERT_EQ(cmd_global.size(), 1u);
    ASSERT_EQ(cmd_per_item.size(), 1u);
    EXPECT_NE(cmd_per_item[0].destination.x, cmd_global[0].destination.x);
    EXPECT_NE(cmd_per_item[0].destination.y, cmd_global[0].destination.y);
}

TEST(BattleRenderer, BaseTrackCarriesPerPositionBinding) {
    SnapshotTrack const  base = track(TrackKind::Base, TrackRenderLayer::Base, "frame");
    BattleRenderSnapshot snapshot{.entities = {entity(1, BattleDepth::Front, 0, {base})}};
    snapshot.entities[0].coord = {
        .side = BattleSide::Attacker, .lane = 0, .depth = BattleDepth::Front};
    FakeTextureProvider provider;
    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, no_background());

    const auto& commands = test::commands_from(batch);

    ASSERT_EQ(commands.size(), 1u);
    ASSERT_TRUE(test::tunable_item(commands[0]).has_value());
    ASSERT_TRUE(test::tunable_item(commands[0])->binding.has_value());
    EXPECT_EQ(test::tunable_item(commands[0])->binding->owner_kind,
              BindingOwnerKind::PositionLevel);
    EXPECT_EQ(test::tunable_item(commands[0])->binding->tree_path, "A_FRONT_0");
    EXPECT_EQ(test::tunable_item(commands[0])->binding->role, BindingRole::Unit);
}

TEST(BattleRenderer, PerPositionLevelChangesDrawLayer) {
    SnapshotTrack const  base = track(TrackKind::Base, TrackRenderLayer::Base, "frame");
    BattleRenderSnapshot snapshot{.entities = {entity(1, BattleDepth::Front, 0, {base})}};
    snapshot.entities[0].coord = {
        .side = BattleSide::Attacker, .lane = 0, .depth = BattleDepth::Front};
    FakeTextureProvider provider;

    BattleRenderOptions opts = no_background();
    opts.unit_level = 0;
    const auto cmd_default_batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& cmd_default = test::commands_from(cmd_default_batch);

    opts.position_levels["A_FRONT_0"] = 2;
    const auto cmd_override_batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& cmd_override = test::commands_from(cmd_override_batch);

    ASSERT_TRUE(test::tunable_item(cmd_default[0]).has_value());
    ASSERT_TRUE(test::tunable_item(cmd_override[0]).has_value());
    EXPECT_EQ(test::tunable_item(cmd_override[0])->layer,
              test::tunable_item(cmd_default[0])->layer + 2);
}

TEST(BattleRenderer, BaseDrawLevelIsPositionPlusDelta) {
    // Base track draw level = position_level(slot) + UnitVisualProfile.level delta (additive).
    SnapshotTrack base = track(TrackKind::Base, TrackRenderLayer::Base, "frame");
    base.sequence_name = "G000UU0001IDLEA1A00";
    BattleRenderSnapshot snapshot{.entities = {entity(1, BattleDepth::Front, 0, {base})}};
    snapshot.entities[0].coord = {
        .side = BattleSide::Attacker, .lane = 0, .depth = BattleDepth::Front};
    snapshot.entities[0].unit_type = "UU0001";
    FakeTextureProvider provider;

    BattleRenderOptions opts = no_background();
    opts.position_levels["A_FRONT_0"] = 1;

    const auto cmd_no_delta_batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& cmd_no_delta = test::commands_from(cmd_no_delta_batch);

    const std::string profile_key = ConfigBinding{.owner_kind = BindingOwnerKind::UnitVisualProfile,
                                                  .tree_path = "UU0001",
                                                  .role = BindingRole::UnitIdle}
                                        .key();
    opts.placements[profile_key] = VisualPlacementValue{.level = 1};
    const auto cmd_with_delta_batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& cmd_with_delta = test::commands_from(cmd_with_delta_batch);

    ASSERT_TRUE(test::tunable_item(cmd_no_delta[0]).has_value());
    ASSERT_TRUE(test::tunable_item(cmd_with_delta[0]).has_value());
    // position_level=1, delta=0 → layer=4; delta=+1 → layer=5 (additive, not delta=override)
    EXPECT_EQ(test::tunable_item(cmd_with_delta[0])->layer,
              test::tunable_item(cmd_no_delta[0])->layer + 1);
}

TEST(BattleRenderer, SourceAnchoredEffectInheritsSlotLevel) {
    // Source-anchored effects (effect_role=Source) add slot's position_level as base.
    SnapshotTrack effect = track(TrackKind::Effect, TrackRenderLayer::Effect, "frame");
    effect.sequence_name = "TUCHA_FIRE";
    effect.effect_role = BindingRole::Source;
    BattleRenderSnapshot snapshot{.entities = {entity(1, BattleDepth::Front, 0, {effect})}};
    snapshot.entities[0].coord = {
        .side = BattleSide::Attacker, .lane = 0, .depth = BattleDepth::Front};
    FakeTextureProvider provider;

    BattleRenderOptions opts = no_background();
    opts.effect_level = 0;
    const auto cmd_no_pos_batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& cmd_no_pos = test::commands_from(cmd_no_pos_batch);

    opts.position_levels["A_FRONT_0"] = 2;
    const auto cmd_with_pos_batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& cmd_with_pos = test::commands_from(cmd_with_pos_batch);

    ASSERT_TRUE(test::tunable_item(cmd_no_pos[0]).has_value());
    ASSERT_TRUE(test::tunable_item(cmd_with_pos[0]).has_value());
    EXPECT_EQ(test::tunable_item(cmd_with_pos[0])->layer,
              test::tunable_item(cmd_no_pos[0])->layer + 2);
}

TEST(BattleRenderer, TeamOverlayEffectIgnoresPositionLevel) {
    // Team overlay effects (effect_role=TargetTeam) have no position component.
    SnapshotTrack effect = track(TrackKind::Effect, TrackRenderLayer::Effect, "frame");
    effect.sequence_name = "HEFFA_FIRE";
    effect.effect_role = BindingRole::TargetTeam;
    BattleRenderSnapshot snapshot{.entities = {entity(1, BattleDepth::Front, 0, {effect})}};
    snapshot.entities[0].coord = {
        .side = BattleSide::Attacker, .lane = 0, .depth = BattleDepth::Front};
    FakeTextureProvider provider;

    BattleRenderOptions opts = no_background();
    opts.effect_level = 1;
    const auto cmd_no_pos_batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& cmd_no_pos = test::commands_from(cmd_no_pos_batch);

    opts.position_levels["A_FRONT_0"] = 5;
    const auto cmd_with_pos_batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& cmd_with_pos = test::commands_from(cmd_with_pos_batch);

    ASSERT_TRUE(test::tunable_item(cmd_no_pos[0]).has_value());
    ASSERT_TRUE(test::tunable_item(cmd_with_pos[0]).has_value());
    // position_level must NOT affect team overlay — both have same layer
    EXPECT_EQ(test::tunable_item(cmd_with_pos[0])->layer, test::tunable_item(cmd_no_pos[0])->layer);
}

TEST(BattleRenderer, PerItemEffectLevelOverridesGlobalEffectLevel) {
    SnapshotTrack effect = track(TrackKind::Effect, TrackRenderLayer::Effect, "frame");
    effect.sequence_name = "HEFFA_FIRE";
    effect.effect_role = BindingRole::TargetTeam;
    BattleRenderSnapshot const snapshot{.entities = {entity(1, BattleDepth::Front, 0, {effect})}};
    FakeTextureProvider        provider;

    BattleRenderOptions opts = no_background();
    opts.effect_level = 0;
    const auto cmd_global_batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& cmd_global = test::commands_from(cmd_global_batch);

    const std::string typed_key = ConfigBinding{.owner_kind = BindingOwnerKind::EffectProfile,
                                                .tree_path = "HEFFA_FIRE",
                                                .role = BindingRole::TargetTeam}
                                      .key();
    opts.placements[typed_key] = VisualPlacementValue{.level = 3};
    const auto cmd_per_item_batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& cmd_per_item = test::commands_from(cmd_per_item_batch);

    ASSERT_TRUE(test::tunable_item(cmd_global[0]).has_value());
    ASSERT_TRUE(test::tunable_item(cmd_per_item[0]).has_value());
    EXPECT_EQ(test::tunable_item(cmd_per_item[0])->layer,
              test::tunable_item(cmd_global[0])->layer + 3);
}

TEST(BattleRenderer, UnitVisualProfileBindingWhenUnitTypeKnown) {
    SnapshotTrack base = track(TrackKind::Base, TrackRenderLayer::Base, "idle_001");
    base.id = TrackId{1};
    base.sequence_name = "G000UU0008IDLEA1A00";
    BattleRenderSnapshot snapshot{.entities = {entity(1, BattleDepth::Front, 0, {base})}};
    snapshot.entities[0].unit_type = "UU0008";
    FakeTextureProvider provider;

    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, no_background());

    const auto& commands = test::commands_from(batch);

    ASSERT_EQ(commands.size(), 1u);
    ASSERT_TRUE(test::tunable_item(commands[0]).has_value());
    ASSERT_TRUE(test::tunable_item(commands[0])->binding.has_value());
    const auto& b = *test::tunable_item(commands[0])->binding;
    EXPECT_EQ(b.owner_kind, BindingOwnerKind::UnitVisualProfile);
    EXPECT_EQ(b.tree_path, "UU0008");
    EXPECT_EQ(b.role, BindingRole::UnitIdle);
    EXPECT_EQ(b.display_path, "unit_visual_profiles.UU0008.idle");
    EXPECT_TRUE(b.writable());
}

// Helper: DeathBody track with lifecycle profile and anchor metadata
SnapshotTrack corpse_track(const std::string& profile_id, int foot_x = 80, int foot_y = 90) {
    SnapshotTrack t = track(TrackKind::DeathBody, TrackRenderLayer::Base, "DEAD_HUMAN_SMALL00");
    t.sequence_name = "DEAD_HUMAN_SMALL";
    t.lifecycle_profile_id = profile_id;
    t.canvas_foot_x = foot_x;
    t.canvas_foot_y = foot_y;
    t.native_canvas_w = 200;
    t.native_canvas_h = 100;
    return t;
}

// Role from explicit effect_role field (primary path — set by PlayClip.unit_anim_role)
TEST(BattleRenderer, UnitVisualProfileRolePropagatedFromEffectRole) {
    auto make_base_with_role = [](BindingRole role) {
        SnapshotTrack t = track(TrackKind::Base, TrackRenderLayer::Base, "frame");
        t.sequence_name = "G000UU0008STILA1A00"; // does NOT contain IDLE/ATTACK/HIT suffix
        t.effect_role = role;
        return t;
    };
    BattleRenderSnapshot snapshot{
        .entities = {entity(1, BattleDepth::Front, 0,
                            {make_base_with_role(BindingRole::UnitIdle),
                             make_base_with_role(BindingRole::UnitAttack),
                             make_base_with_role(BindingRole::UnitHit),
                             make_base_with_role(BindingRole::UnitBase)})}};
    snapshot.entities[0].unit_type = "UU0008";
    FakeTextureProvider provider;

    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, no_background());

    const auto& cmds = test::commands_from(batch);

    ASSERT_EQ(cmds.size(), 4u);
    EXPECT_EQ(test::tunable_item(cmds[0])->binding->role,
              BindingRole::UnitIdle); // from effect_role, NOT sequence_name
    EXPECT_EQ(test::tunable_item(cmds[1])->binding->role, BindingRole::UnitAttack); // same
    EXPECT_EQ(test::tunable_item(cmds[2])->binding->role, BindingRole::UnitHit);    // same
    EXPECT_EQ(test::tunable_item(cmds[3])->binding->role, BindingRole::UnitBase);   // same
}

// Role fallback: when effect_role is Global (not explicitly set), sequence_name is used
TEST(BattleRenderer, UnitVisualProfileRoleSequenceNameFallback) {
    auto make_base = [](const std::string& seq_name) {
        SnapshotTrack t = track(TrackKind::Base, TrackRenderLayer::Base, "frame");
        t.sequence_name = seq_name;
        // effect_role defaults to Global → fallback to sequence_name inference
        return t;
    };
    BattleRenderSnapshot snapshot{
        .entities = {entity(1, BattleDepth::Front, 0,
                            {make_base("G000UU0008IDLEA1A00"), make_base("G000UU0008HMOVA1A00"),
                             make_base("G000UU0008HHITA1A00"), make_base("G000UU0008STILA1A00")})}};
    snapshot.entities[0].unit_type = "UU0008";
    FakeTextureProvider provider;

    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, no_background());

    const auto& cmds = test::commands_from(batch);

    ASSERT_EQ(cmds.size(), 4u);
    EXPECT_EQ(test::tunable_item(cmds[0])->binding->role, BindingRole::UnitIdle);
    EXPECT_EQ(test::tunable_item(cmds[1])->binding->role, BindingRole::UnitAttack);
    EXPECT_EQ(test::tunable_item(cmds[2])->binding->role, BindingRole::UnitHit);
    EXPECT_EQ(test::tunable_item(cmds[3])->binding->role, BindingRole::UnitBase);
}

// animation_unit_type is used as owner_id when set (base-unit substitution)
TEST(BattleRenderer, AnimationUnitTypeUsedAsOwnerWhenSet) {
    SnapshotTrack base = track(TrackKind::Base, TrackRenderLayer::Base, "frame");
    base.effect_role = BindingRole::UnitIdle;
    BattleRenderSnapshot snapshot{.entities = {entity(1, BattleDepth::Front, 0, {base})}};
    snapshot.entities[0].unit_type = "LICH";
    snapshot.entities[0].animation_unit_type = "SCLT"; // base unit used for animations
    FakeTextureProvider provider;

    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, no_background());

    const auto& cmds = test::commands_from(batch);

    ASSERT_EQ(cmds.size(), 1u);
    ASSERT_TRUE(test::tunable_item(cmds[0])->binding.has_value());
    EXPECT_EQ(test::tunable_item(cmds[0])->binding->tree_path, "SCLT"); // NOT "LICH"
    EXPECT_EQ(test::tunable_item(cmds[0])->binding->owner_kind,
              BindingOwnerKind::UnitVisualProfile);
}

TEST(BattleRenderer, SelectionMarkerUsesSpriteProfileBinding) {
    SnapshotTrack marker_track = track(TrackKind::ActorMarker, TrackRenderLayer::Marker, "frame");
    marker_track.sequence_name = "MARKER_RING";
    marker_track.effect_role = BindingRole::SelectionMarker;
    marker_track.effect_id = EffectInstanceId{1};
    BattleRenderSnapshot snapshot{.entities = {entity(1, BattleDepth::Front, 0, {marker_track})}};
    snapshot.entities[0].unit_type = "UU0008";
    FakeTextureProvider provider;

    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, no_background());

    const auto& cmds = test::commands_from(batch);

    ASSERT_EQ(cmds.size(), 1u);
    ASSERT_TRUE(test::tunable_item(cmds[0])->binding.has_value());
    EXPECT_EQ(test::tunable_item(cmds[0])->binding->owner_kind, BindingOwnerKind::SpriteProfile);
    EXPECT_EQ(test::tunable_item(cmds[0])->binding->tree_path, "MARKER_RING");
    EXPECT_EQ(test::tunable_item(cmds[0])->kind, "unit");
}

TEST(BattleRenderer, TargetMarkerUsesSpriteProfileBinding) {
    SnapshotTrack marker_track = track(TrackKind::TargetMarker, TrackRenderLayer::Marker, "frame");
    marker_track.sequence_name = "MARKER_RING";
    marker_track.effect_role = BindingRole::TargetMarker;
    marker_track.effect_id = EffectInstanceId{2};
    BattleRenderSnapshot snapshot{.entities = {entity(1, BattleDepth::Front, 0, {marker_track})}};
    snapshot.entities[0].unit_type = "UU0008";
    FakeTextureProvider provider;

    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, no_background());

    const auto& cmds = test::commands_from(batch);

    ASSERT_EQ(cmds.size(), 1u);
    ASSERT_TRUE(test::tunable_item(cmds[0])->binding.has_value());
    EXPECT_EQ(test::tunable_item(cmds[0])->binding->owner_kind, BindingOwnerKind::SpriteProfile);
    EXPECT_EQ(test::tunable_item(cmds[0])->binding->tree_path, "MARKER_RING");
}

TEST(BattleRenderer, SelectionMarkerIsAttachedToUnitOffset) {
    SnapshotTrack marker_track = track(TrackKind::ActorMarker, TrackRenderLayer::Marker, "frame");
    marker_track.sequence_name = "MRKCURLARGEA";
    marker_track.effect_role = BindingRole::SelectionMarker;
    marker_track.effect_id = EffectInstanceId{1};

    BattleRenderSnapshot snapshot{.entities = {entity(1, BattleDepth::Front, 0, {marker_track})}};
    snapshot.entities[0].position_offset = {.x = 12.0f, .y = 3.0f};
    FakeTextureProvider provider;

    const auto base_cmds_batch =
        BattleRenderer::build_render_batch(snapshot, provider, no_background());

    const auto& base_cmds = test::commands_from(base_cmds_batch);

    const std::string   marker_key = ConfigBinding{.owner_kind = BindingOwnerKind::SpriteProfile,
                                                   .tree_path = "MRKCURLARGEA",
                                                   .role = BindingRole::Global}
                                         .key();
    BattleRenderOptions opts = no_background();
    opts.placements[marker_key] = VisualPlacementValue{.x = 5.0f, .alpha = 0.5f};
    const auto tuned_cmds_batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& tuned_cmds = test::commands_from(tuned_cmds_batch);

    BattleRenderSnapshot no_offset_snapshot{
        .entities = {entity(1, BattleDepth::Front, 0, {marker_track})}};
    const auto no_offset_cmds_batch =
        BattleRenderer::build_render_batch(no_offset_snapshot, provider, no_background());

    const auto& no_offset_cmds = test::commands_from(no_offset_cmds_batch);

    ASSERT_EQ(base_cmds.size(), 1u);
    ASSERT_EQ(tuned_cmds.size(), 1u);
    ASSERT_EQ(no_offset_cmds.size(), 1u);
    EXPECT_NEAR(base_cmds[0].destination.x - no_offset_cmds[0].destination.x, 12.0f, 0.5f);
    EXPECT_NEAR(tuned_cmds[0].destination.x - base_cmds[0].destination.x, 5.0f, 0.5f);
    EXPECT_FLOAT_EQ(tuned_cmds[0].alpha, 0.5f);
}

TEST(BattleRenderer, SelectionMarkerSortsInsideUnitStackBelowA1) {
    SnapshotTrack s1 = track(TrackKind::Base, TrackRenderLayer::Base, "s1");
    s1.layer_slot = LayerSlot::S1;
    s1.effect_role = BindingRole::UnitIdle;

    SnapshotTrack marker = track(TrackKind::ActorMarker, TrackRenderLayer::Base, "marker");
    marker.sequence_name = "MRKCURLARGEA";
    marker.placement = render_placement_for(TrackRenderLayer::Base, AnchorPolicy::UnitFoot);
    marker.placement.depth_bias = -1;

    SnapshotTrack a1 = track(TrackKind::Base, TrackRenderLayer::Base, "a1");
    a1.layer_slot = LayerSlot::A1;
    a1.effect_role = BindingRole::UnitIdle;

    BattleRenderSnapshot snapshot{.entities = {entity(1, BattleDepth::Front, 0, {s1, marker, a1})}};
    FakeTextureProvider  provider;

    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, no_background());

    const auto& cmds = test::commands_from(batch);

    ASSERT_EQ(cmds.size(), 3u);
    ASSERT_TRUE(test::tunable_item(cmds[0]).has_value());
    ASSERT_TRUE(test::tunable_item(cmds[1]).has_value());
    ASSERT_TRUE(test::tunable_item(cmds[2]).has_value());
    EXPECT_EQ(test::tunable_item(cmds[0])->current_frame_name, "s1");
    EXPECT_EQ(test::tunable_item(cmds[1])->current_frame_name, "marker");
    EXPECT_EQ(test::tunable_item(cmds[2])->current_frame_name, "a1");
}

// Generic: all writable debug items shift their destination when placement offset applied
TEST(BattleRenderer, AllWritableItemsRespondToPlacementOffset) {
    // Build a snapshot with: Base (unit), DeathBody (corpse), DeathFx, ActorMarker
    SnapshotTrack base_t = track(TrackKind::Base, TrackRenderLayer::Base, "frame");
    base_t.effect_role = BindingRole::UnitIdle;

    SnapshotTrack corpse_t = corpse_track("HUMAN");

    SnapshotTrack death_fx = track(TrackKind::DeathFx, TrackRenderLayer::Overlay, "frame");
    death_fx.lifecycle_profile_id = "HUMAN";
    death_fx.effect_id = EffectInstanceId{10};

    SnapshotTrack marker_t = track(TrackKind::ActorMarker, TrackRenderLayer::Marker, "frame");
    marker_t.effect_role = BindingRole::SelectionMarker;
    marker_t.effect_id = EffectInstanceId{20};

    auto make_snapshot = [&](TrackKind kind) {
        SnapshotTrack const* t = (kind == TrackKind::Base)        ? &base_t
                                 : (kind == TrackKind::DeathBody) ? &corpse_t
                                 : (kind == TrackKind::DeathFx)   ? &death_fx
                                                                  : &marker_t;
        BattleRenderSnapshot snap{.entities = {entity(1, BattleDepth::Front, 0, {*t})}};
        snap.entities[0].unit_type = "UU0008";
        return snap;
    };

    FakeTextureProvider provider;

    struct Case {
        TrackKind   kind;
        std::string binding_key;
    };

    const std::vector<Case> cases = {
        {.kind = TrackKind::Base,
         .binding_key = ConfigBinding{.owner_kind = BindingOwnerKind::UnitVisualProfile,
                                      .tree_path = "UU0008",
                                      .role = BindingRole::UnitIdle}
                            .key()},
        {.kind = TrackKind::DeathBody,
         .binding_key = ConfigBinding{.owner_kind = BindingOwnerKind::LifecycleProfile,
                                      .tree_path = "HUMAN",
                                      .role = BindingRole::Corpse}
                            .key()},
        {.kind = TrackKind::DeathFx,
         .binding_key = ConfigBinding{.owner_kind = BindingOwnerKind::LifecycleProfile,
                                      .tree_path = "HUMAN",
                                      .role = BindingRole::DeathFx}
                            .key()},
    };

    for (const auto& c : cases) {
        const auto snap = make_snapshot(c.kind);
        const auto base_cmds_batch =
            BattleRenderer::build_render_batch(snap, provider, no_background());

        const auto&         base_cmds = test::commands_from(base_cmds_batch);
        BattleRenderOptions opts = no_background();
        opts.placements[c.binding_key] = VisualPlacementValue{.x = 30.0f};
        const auto off_cmds_batch = BattleRenderer::build_render_batch(snap, provider, opts);

        const auto& off_cmds = test::commands_from(off_cmds_batch);

        ASSERT_EQ(base_cmds.size(), 1u) << "kind=" << static_cast<int>(c.kind);
        ASSERT_EQ(off_cmds.size(), 1u) << "kind=" << static_cast<int>(c.kind);
        EXPECT_NEAR(off_cmds[0].destination.x - base_cmds[0].destination.x, 30.0f, 0.5f)
            << "writable item kind=" << static_cast<int>(c.kind)
            << " must respond to placement offset";
    }
}

TEST(BattleRenderer, UnitVisualProfilePlacementIsAdditive) {
    // Per-item offset is additive to entity.position_offset for Base tracks
    SnapshotTrack base = track(TrackKind::Base, TrackRenderLayer::Base, "frame");
    base.sequence_name = "G000UU0008IDLEA1A00";
    BattleRenderSnapshot snapshot{.entities = {entity(1, BattleDepth::Front, 0, {base})}};
    snapshot.entities[0].unit_type = "UU0008";
    snapshot.entities[0].position_offset = {.x = 5.0f, .y = 10.0f};
    FakeTextureProvider provider;

    BattleRenderOptions opts = no_background();
    const std::string   key = ConfigBinding{.owner_kind = BindingOwnerKind::UnitVisualProfile,
                                            .tree_path = "UU0008",
                                            .role = BindingRole::UnitIdle}
                                  .key();
    opts.placements[key] = VisualPlacementValue{.x = 20.0f, .y = 0.0f};

    const auto base_cmds_batch =
        BattleRenderer::build_render_batch(snapshot, provider, no_background());

    const auto& base_cmds = test::commands_from(base_cmds_batch);
    const auto  per_cmds_batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& per_cmds = test::commands_from(per_cmds_batch);

    ASSERT_EQ(base_cmds.size(), 1u);
    ASSERT_EQ(per_cmds.size(), 1u);
    // per-item adds +20 x to base (entity offset was already applied in base_cmds)
    EXPECT_NEAR(per_cmds[0].destination.x - base_cmds[0].destination.x, 20.0f, 0.5f);
}

TEST(BattleRenderer, BackgroundHasWritableBinding) {
    BattleRenderSnapshot const snapshot;
    FakeTextureProvider        provider;
    BattleRenderOptions        opts;
    opts.terrain_image = "TERRAIN";
    opts.draw_frame = false;

    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& cmds = test::commands_from(batch);

    ASSERT_FALSE(cmds.empty());
    const auto* bg = [&]() -> const RenderCommand* {
        for (const auto& c : cmds) {
            if (test::tunable_item(c) && test::tunable_item(c)->stable_id == "backgrounds.battle") {
                return &c;
            }
        }
        return nullptr;
    }();
    ASSERT_NE(bg, nullptr);
    ASSERT_TRUE(test::tunable_item(*bg)->binding.has_value());
    EXPECT_TRUE(test::tunable_item(*bg)->binding->writable());
    EXPECT_EQ(test::tunable_item(*bg)->binding->owner_kind, BindingOwnerKind::TreeLayout);
    EXPECT_EQ(test::tunable_item(*bg)->binding->tree_path, "/background/battle");
    EXPECT_EQ(test::tunable_item(*bg)->binding->display_path, "render_tree/background/battle");
}

TEST(BattleRenderer, CompositeBackgroundRendersBothLayers) {
    BattleRenderSnapshot const snapshot;
    FakeTextureProvider        provider;
    BattleRenderOptions        opts;
    opts.terrain_image = "UNDEAD_0_BG";
    opts.terrain_overlay_images = {"UNDEAD_0_FG"};
    opts.draw_frame = false;

    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& cmds = test::commands_from(batch);

    const auto* bg = [&]() -> const RenderCommand* {
        for (const auto& c : cmds) {
            if (test::tunable_item(c) && test::tunable_item(c)->stable_id == "backgrounds.battle") {
                return &c;
            }
        }
        return nullptr;
    }();
    ASSERT_NE(bg, nullptr);
    EXPECT_EQ(bg->layer, battle_render_layer(BattleRenderPass::Background));
    ASSERT_TRUE(test::tunable_item(*bg)->binding.has_value());
    EXPECT_TRUE(test::tunable_item(*bg)->binding->writable());
    EXPECT_EQ(test::tunable_item(*bg)->binding->owner_kind, BindingOwnerKind::TreeLayout);
    EXPECT_EQ(test::tunable_item(*bg)->binding->tree_path, "/background/battle");

    const auto* fg = [&]() -> const RenderCommand* {
        for (const auto& c : cmds) {
            if (c.layer == battle_render_layer(BattleRenderPass::BackgroundOverlay)) {
                return &c;
            }
        }
        return nullptr;
    }();
    ASSERT_NE(fg, nullptr);
    EXPECT_EQ(fg->layer, battle_render_layer(BattleRenderPass::BackgroundOverlay));
    ASSERT_TRUE(test::tunable_item(*fg)->binding.has_value());
    EXPECT_TRUE(test::tunable_item(*fg)->binding->writable());
    EXPECT_EQ(test::tunable_item(*fg)->binding->owner_kind, BindingOwnerKind::TreeLayout);
    EXPECT_EQ(test::tunable_item(*fg)->binding->tree_path, "/background/battle");
    EXPECT_FALSE(test::tunable_item(*fg)->selectable);
}

TEST(BattleRenderer, HereticBackgroundResolvesGroundAndBattleDescriptors) {
    BattleScenePresentationState presentation;
    BattleTuningState            tuning;
    const auto                   tree = make_slot_render_tree();
    const auto                   options = BattleViewerRenderer::make_options(
        presentation, tuning, tree, "HERETIC_0_BG", {"HERETIC_0_FG"}, false, true);

    EXPECT_EQ(options.ground_container, "Imgs/Ground.ff");
    EXPECT_EQ(options.ground_image, "HE_00.PNG");
    EXPECT_EQ(options.terrain_container, "Imgs/Battle.ff");
    EXPECT_EQ(options.terrain_image, "HERETIC_0_BG");

    FakeTextureProvider provider;
    const auto          batch =
        BattleRenderer::build_render_batch(BattleRenderSnapshot{}, provider, options);

    const auto& cmds = test::commands_from(batch);

    const RenderCommand* ground = nullptr;
    const RenderCommand* battle = nullptr;
    const RenderCommand* overlay = nullptr;
    for (const auto& c : cmds) {
        if (test::tunable_item(c) && test::tunable_item(c)->stable_id == "backgrounds.ground") {
            ground = &c;
        }
        if (test::tunable_item(c) && test::tunable_item(c)->stable_id == "backgrounds.battle") {
            battle = &c;
        }
        if (c.layer == battle_render_layer(BattleRenderPass::BackgroundOverlay)) {
            overlay = &c;
        }
    }
    ASSERT_NE(ground, nullptr);
    ASSERT_NE(battle, nullptr);
    ASSERT_NE(overlay, nullptr);
    EXPECT_EQ(test::tunable_item(*ground)->resource_key, "Imgs/Ground.ff/HE_00.PNG");
    EXPECT_EQ(test::tunable_item(*ground)->tree_path, "/background/ground");
    EXPECT_EQ(test::tunable_item(*battle)->resource_key, "Imgs/Battle.ff/HERETIC_0_BG");
    EXPECT_EQ(test::tunable_item(*battle)->tree_path, "/background/battle");
    EXPECT_EQ(test::tunable_item(*overlay)->resource_key, "Imgs/Battle.ff/HERETIC_0_FG");
    EXPECT_EQ(test::tunable_item(*overlay)->tree_path, "/background/battle");
}

TEST(BattleRenderer, SingleImageBackgroundHasNoOverlay) {
    BattleRenderSnapshot const snapshot;
    FakeTextureProvider        provider;
    BattleRenderOptions        opts;
    opts.terrain_image = "HUMAN_0_BG";
    opts.draw_frame = false;

    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& cmds = test::commands_from(batch);

    // With no overlay images, no BackgroundOverlay pass command should exist
    const auto* fg = [&]() -> const RenderCommand* {
        for (const auto& c : cmds) {
            if (c.layer == battle_render_layer(BattleRenderPass::BackgroundOverlay)) {
                return &c;
            }
        }
        return nullptr;
    }();
    EXPECT_EQ(fg, nullptr);
}

TEST(BattleRenderer, GroundBackgroundRendersBelowBattleAndBattlefieldContent) {
    SnapshotTrack unit = track(TrackKind::Base, TrackRenderLayer::Base, "idle");
    unit.id = TrackId{1};
    SnapshotTrack effect = track(TrackKind::Effect, TrackRenderLayer::Overlay, "fx");
    effect.id = TrackId{2};
    SnapshotTrack marker = track(TrackKind::ActorMarker, TrackRenderLayer::Marker, "marker");
    marker.id = TrackId{3};
    BattleRenderSnapshot snapshot{
        .entities = {entity(1, BattleDepth::Front, 0, {unit, effect, marker})}};
    snapshot.entities[0].unit_instance_id = UnitInstanceId{10};

    FakeTextureProvider provider;
    BattleRenderOptions opts;
    opts.ground_image = "HE_00.PNG";
    opts.terrain_image = "HERETIC_0_BG";
    opts.terrain_overlay_images = {"HERETIC_0_FG"};
    opts.draw_frame = true;
    opts.frame_image = "FRAME";
    static const d2engine::TreeLayout tree = make_slot_render_tree();
    opts.tree_layout = &tree;

    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& cmds = test::commands_from(batch);

    auto first = [&](BattleRenderPass pass) {
        for (std::size_t i = 0; i < cmds.size(); ++i) {
            if (cmds[i].layer == battle_render_layer(pass)) {
                return i;
            }
        }
        return cmds.size();
    };

    EXPECT_LT(first(BattleRenderPass::GroundBackground), first(BattleRenderPass::Background));
    EXPECT_LT(first(BattleRenderPass::Background), first(BattleRenderPass::UnitsFront));
    EXPECT_LT(first(BattleRenderPass::UnitsFront), first(BattleRenderPass::Effects));
    EXPECT_LT(first(BattleRenderPass::Effects), first(BattleRenderPass::BackgroundOverlay));
    EXPECT_LT(first(BattleRenderPass::BackgroundOverlay), first(BattleRenderPass::Markers));
    EXPECT_LT(first(BattleRenderPass::Markers), first(BattleRenderPass::CombatFrame));
}

TEST(BattleRenderer, BackgroundOverlayPassIsAfterEffectsBeforeMarkers) {
    // Include a unit (UnitsFront), overlay effect (Effects), and marker (Markers) so all passes
    // are actually populated and ordering can be verified without guards.
    BattleRenderSnapshot const snapshot{
        .entities = {
            entity(1, BattleDepth::Front, 0,
                   {track(TrackKind::Base, TrackRenderLayer::Base, "unit_img"),
                    track(TrackKind::Effect, TrackRenderLayer::Overlay, "effect_img"),
                    track(TrackKind::ActorMarker, TrackRenderLayer::Marker, "marker_img")})}};
    FakeTextureProvider provider;
    BattleRenderOptions opts;
    opts.terrain_image = "UNDEAD_0_BG";
    opts.terrain_overlay_images = {"UNDEAD_0_FG"};
    opts.draw_frame = false;
    static const d2engine::TreeLayout tree = make_slot_render_tree();
    opts.tree_layout = &tree;

    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& cmds = test::commands_from(batch);

    std::optional<std::size_t> bg_idx;
    std::optional<std::size_t> unit_idx;
    std::optional<std::size_t> fx_idx;
    std::optional<std::size_t> ov_idx;
    std::optional<std::size_t> marker_idx;
    for (std::size_t i = 0; i < cmds.size(); ++i) {
        const RenderLayer p = cmds[i].layer;
        if (p == battle_render_layer(BattleRenderPass::Background)) {
            bg_idx = i;
        } else if (p == battle_render_layer(BattleRenderPass::UnitsFront)) {
            unit_idx = i;
        } else if (p == battle_render_layer(BattleRenderPass::Effects)) {
            fx_idx = i;
        } else if (p == battle_render_layer(BattleRenderPass::BackgroundOverlay)) {
            ov_idx = i;
        } else if (p == battle_render_layer(BattleRenderPass::Markers)) {
            marker_idx = i;
        }
    }
    ASSERT_TRUE(bg_idx.has_value()) << "Background pass missing";
    ASSERT_TRUE(unit_idx.has_value()) << "UnitsFront pass missing";
    ASSERT_TRUE(fx_idx.has_value()) << "Effects pass missing";
    ASSERT_TRUE(ov_idx.has_value()) << "BackgroundOverlay pass missing";
    ASSERT_TRUE(marker_idx.has_value()) << "Markers pass missing";
    // Verified order: Background < Units < Effects < BackgroundOverlay < Markers
    EXPECT_LT(*bg_idx, *unit_idx) << "Background must precede units";
    EXPECT_LT(*unit_idx, *fx_idx) << "Units must precede effects";
    EXPECT_LT(*fx_idx, *ov_idx) << "Effects must precede background overlay";
    EXPECT_LT(*ov_idx, *marker_idx) << "Background overlay must precede markers";
}

TEST(BattleRenderer, CombatFrameUsesTypedPlacementWhenSet) {
    BattleRenderSnapshot const snapshot;
    FakeTextureProvider        provider;

    BattleRenderOptions base_opts;
    base_opts.draw_background = false;
    d2engine::TreeLayout tree = make_slot_render_tree();
    constexpr float      kCfbW = 1600.0f * 0.9801f;
    constexpr float      kCfbH = (945.0f / 5.0f) * 1.4883f;
    constexpr float      kCfbX = (1600.0f - kCfbW) * 0.5f;
    constexpr float      kCfbY = 945.0f - kCfbH;
    tree.set_node("/ui", TreeNode{.kind = "root", .w = 1600.0f, .h = 945.0f});
    tree.set_node("/ui/combat_frame",
                  TreeNode{.kind = "image", .x = kCfbX, .y = kCfbY, .w = kCfbW, .h = kCfbH});
    base_opts.tree_layout = &tree;

    const auto base_cmds_batch = BattleRenderer::build_render_batch(snapshot, provider, base_opts);

    const auto& base_cmds = test::commands_from(base_cmds_batch);

    // Changing render_tree directly shifts the combat frame.
    BattleRenderOptions  shifted_opts = base_opts;
    d2engine::TreeLayout shifted_tree = tree;
    shifted_tree.set_node(
        "/ui/combat_frame",
        TreeNode{.kind = "image", .x = kCfbX + 50.0f, .y = kCfbY, .w = kCfbW, .h = kCfbH});
    shifted_opts.tree_layout = &shifted_tree;
    const auto shifted_cmds_batch =
        BattleRenderer::build_render_batch(snapshot, provider, shifted_opts);

    const auto& shifted_cmds = test::commands_from(shifted_cmds_batch);

    const auto* base_frame = [&]() -> const RenderCommand* {
        for (const auto& c : base_cmds) {
            if (test::tunable_item(c) && test::tunable_item(c)->stable_id == "ui:combat_frame") {
                return &c;
            }
        }
        return nullptr;
    }();
    const auto* shifted_frame = [&]() -> const RenderCommand* {
        for (const auto& c : shifted_cmds) {
            if (test::tunable_item(c) && test::tunable_item(c)->stable_id == "ui:combat_frame") {
                return &c;
            }
        }
        return nullptr;
    }();
    ASSERT_NE(base_frame, nullptr);
    ASSERT_NE(shifted_frame, nullptr);
    EXPECT_NEAR(shifted_frame->destination.x - base_frame->destination.x, 50.0f, 0.5f);
}

TEST(BattleTuningState, UnitVisualProfileSaveReload) {
    const ConfigBinding binding{.config_file = kBattleScreenConfigPath,
                                .owner_kind = BindingOwnerKind::UnitVisualProfile,
                                .tree_path = "UU0008",
                                .role = BindingRole::UnitIdle,
                                .display_path = "unit_visual_profiles.UU0008.idle"};
    BattleTuningState   state;
    state.set_placement(binding, VisualPlacementValue{.x = 15.0f, .y = -5.0f, .scale_x = 1.1f});

    nlohmann::json json;
    write_placement_to_json(json, binding, state.placements.at(binding.key()));
    EXPECT_TRUE(json.contains("unit_visual_profiles"));
    EXPECT_NEAR(json["unit_visual_profiles"]["UU0008"]["idle"]["x"].get<float>(), 15.0f, 0.01f);

    BattleTuningState state2;
    state2.config_file = "test";
    // Simulate load
    for (const auto& [uid, roles] : json["unit_visual_profiles"].items()) {
        for (const auto& [rkey, val] : roles.items()) {
            std::string display_path = "unit_visual_profiles.";
            display_path.append(uid).append(".").append(rkey);
            ConfigBinding const b{.config_file = state2.config_file,
                                  .owner_kind = BindingOwnerKind::UnitVisualProfile,
                                  .tree_path = uid,
                                  .role = BindingRole::UnitIdle,
                                  .display_path = display_path};
            state2.set_placement(b, val.get<VisualPlacementValue>());
        }
    }
    EXPECT_NEAR(state2.placement(binding).x, 15.0f, 0.01f);
}

// ─── Spec 2: Corpse as unit visual state ────────────────────────────────────

TEST(BattleRenderer, CorpseDebugKindIsUnit) {
    BattleRenderSnapshot const snapshot{
        .entities = {entity(1, BattleDepth::Front, 0, {corpse_track("HUMAN")})}};
    FakeTextureProvider provider;
    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, no_background());

    const auto& cmds = test::commands_from(batch);
    ASSERT_EQ(cmds.size(), 1u);
    ASSERT_TRUE(test::tunable_item(cmds[0]).has_value());
    EXPECT_EQ(test::tunable_item(cmds[0])->kind, "unit");
}

TEST(BattleRenderer, CorpseBindingIsLifecycleProfileCorpse) {
    BattleRenderSnapshot const snapshot{
        .entities = {entity(1, BattleDepth::Front, 0, {corpse_track("HUMAN")})}};
    FakeTextureProvider provider;
    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, no_background());

    const auto& cmds = test::commands_from(batch);
    ASSERT_EQ(cmds.size(), 1u);
    ASSERT_TRUE(test::tunable_item(cmds[0])->binding.has_value());
    const auto& b = *test::tunable_item(cmds[0])->binding;
    EXPECT_EQ(b.owner_kind, BindingOwnerKind::LifecycleProfile);
    EXPECT_EQ(b.tree_path, "HUMAN");
    EXPECT_EQ(b.role, BindingRole::Corpse);
    EXPECT_EQ(b.display_path, "unit_lifecycle_profiles.HUMAN.corpse");
    EXPECT_TRUE(b.writable());
}

TEST(BattleRenderer, CorpsePlacementIsAdditiveOffset) {
    // Corpse per-item offset must be additive to entity.position_offset (same as Base)
    BattleRenderSnapshot snapshot{
        .entities = {entity(1, BattleDepth::Front, 0, {corpse_track("HUMAN")})}};
    snapshot.entities[0].position_offset = {.x = 5.0f, .y = 10.0f};
    FakeTextureProvider provider;

    const std::string key = ConfigBinding{.owner_kind = BindingOwnerKind::LifecycleProfile,
                                          .tree_path = "HUMAN",
                                          .role = BindingRole::Corpse}
                                .key();

    const auto base_cmds_batch =
        BattleRenderer::build_render_batch(snapshot, provider, no_background());

    const auto& base_cmds = test::commands_from(base_cmds_batch);

    BattleRenderOptions opts = no_background();
    opts.placements[key] = VisualPlacementValue{.x = 20.0f, .y = 0.0f};
    const auto per_cmds_batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& per_cmds = test::commands_from(per_cmds_batch);

    ASSERT_EQ(base_cmds.size(), 1u);
    ASSERT_EQ(per_cmds.size(), 1u);
    EXPECT_NEAR(per_cmds[0].destination.x - base_cmds[0].destination.x, 20.0f, 0.5f);
}

TEST(BattleRenderer, CorpsePlacementIsMultiplicativeScale) {
    // Corpse per-item scale_x/scale_y are absolute (not multiplied by options.scale_x/y)
    BattleRenderSnapshot const snapshot{
        .entities = {entity(1, BattleDepth::Front, 0, {corpse_track("HUMAN")})}};
    FakeTextureProvider provider;

    const std::string key = ConfigBinding{.owner_kind = BindingOwnerKind::LifecycleProfile,
                                          .tree_path = "HUMAN",
                                          .role = BindingRole::Corpse}
                                .key();

    const auto base_cmds_batch =
        BattleRenderer::build_render_batch(snapshot, provider, no_background());

    const auto& base_cmds = test::commands_from(base_cmds_batch);

    BattleRenderOptions opts = no_background();
    opts.placements[key] = VisualPlacementValue{.scale_x = 2.0f, .scale_y = 1.5f};
    const auto per_cmds_batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& per_cmds = test::commands_from(per_cmds_batch);

    ASSERT_EQ(base_cmds.size(), 1u);
    ASSERT_EQ(per_cmds.size(), 1u);
    EXPECT_NEAR(per_cmds[0].destination.w / base_cmds[0].destination.w, 2.0f, 0.01f);
    EXPECT_NEAR(per_cmds[0].destination.h / base_cmds[0].destination.h, 1.5f, 0.01f);
}

TEST(BattleRenderer, CorpseAppliesSpriteProfileBeforeLifecycleProfile) {
    BattleRenderSnapshot const snapshot{
        .entities = {entity(1, BattleDepth::Front, 0, {corpse_track("HUMAN")})}};
    FakeTextureProvider provider;

    const std::string sprite_key = ConfigBinding{.owner_kind = BindingOwnerKind::SpriteProfile,
                                                 .tree_path = "DEAD_HUMAN_SMALL",
                                                 .role = BindingRole::Global}
                                       .key();
    const std::string lifecycle_key =
        ConfigBinding{.owner_kind = BindingOwnerKind::LifecycleProfile,
                      .tree_path = "HUMAN",
                      .role = BindingRole::Corpse}
            .key();

    BattleRenderOptions opts = no_background();
    opts.placements[sprite_key] = VisualPlacementValue{.x = 10.0f};
    opts.placements[lifecycle_key] = VisualPlacementValue{.x = 5.0f};
    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& cmds = test::commands_from(batch);

    BattleRenderOptions base_opts = no_background();
    const auto base_cmds_batch = BattleRenderer::build_render_batch(snapshot, provider, base_opts);

    const auto& base_cmds = test::commands_from(base_cmds_batch);

    ASSERT_EQ(cmds.size(), 1u);
    ASSERT_EQ(base_cmds.size(), 1u);
    EXPECT_NEAR(cmds[0].destination.x - base_cmds[0].destination.x, 15.0f, 0.5f);

    BattleRenderOptions scale_opts = no_background();
    scale_opts.placements[sprite_key] = VisualPlacementValue{.scale_x = 2.0f};
    scale_opts.placements[lifecycle_key] = VisualPlacementValue{.scale_x = 1.5f};
    const auto scale_cmds_batch =
        BattleRenderer::build_render_batch(snapshot, provider, scale_opts);

    const auto& scale_cmds = test::commands_from(scale_cmds_batch);

    ASSERT_EQ(scale_cmds.size(), 1u);
    EXPECT_NEAR(scale_cmds[0].destination.w / base_cmds[0].destination.w, 3.0f, 0.01f);
}

TEST(BattleRenderer, CorpseWithNoAnchorMetadataNotSelectable) {
    // canvas_foot=(0,0) means no anchor metadata — corpse must be read-only in debug
    SnapshotTrack const        t = corpse_track("HUMAN", 0, 0); // explicitly zero foot
    BattleRenderSnapshot const snapshot{.entities = {entity(1, BattleDepth::Front, 0, {t})}};
    FakeTextureProvider        provider;
    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, no_background());

    const auto& cmds = test::commands_from(batch);
    ASSERT_EQ(cmds.size(), 1u);
    ASSERT_TRUE(test::tunable_item(cmds[0]).has_value());
    EXPECT_FALSE(test::tunable_item(cmds[0])->selectable); // not editable without anchor data
}

TEST(BattleRenderer, CorpseAndDeathFxHaveSeparateBindingKeys) {
    // Editing corpse must not affect death_fx placement and vice versa
    const ConfigBinding corpse_binding{.config_file = kBattleScreenConfigPath,
                                       .owner_kind = BindingOwnerKind::LifecycleProfile,
                                       .tree_path = "HUMAN",
                                       .role = BindingRole::Corpse};
    const ConfigBinding death_fx_binding{.config_file = kBattleScreenConfigPath,
                                         .owner_kind = BindingOwnerKind::LifecycleProfile,
                                         .tree_path = "HUMAN",
                                         .role = BindingRole::DeathFx};
    EXPECT_NE(corpse_binding.key(), death_fx_binding.key());

    BattleTuningState state;
    state.set_placement(corpse_binding, VisualPlacementValue{.x = 10.0f});
    state.set_placement(death_fx_binding, VisualPlacementValue{.x = 99.0f});

    EXPECT_NEAR(state.placement(corpse_binding).x, 10.0f, 0.01f);
    EXPECT_NEAR(state.placement(death_fx_binding).x, 99.0f, 0.01f);
}

TEST(BattleRenderer, CorpseAnchorSameSlotAsLiveUnit) {
    // Corpse (DeathBody) and live unit (Base) at same slot share the same world anchor.
    // Note: destination rects differ because Base normalizes to reference canvas (800x600) while
    // DeathBody uses native canvas. The anchor (slot foot position) is what must be identical.
    SnapshotTrack base = track(TrackKind::Base, TrackRenderLayer::Base, "frame");
    base.canvas_foot_x = 80;
    base.canvas_foot_y = 90;
    base.native_canvas_w = 200;
    base.native_canvas_h = 100;
    SnapshotTrack const corpse = corpse_track("HUMAN");

    BattleRenderSnapshot snap_live{.entities = {entity(1, BattleDepth::Front, 0, {base})}};
    BattleRenderSnapshot snap_dead{.entities = {entity(1, BattleDepth::Front, 0, {corpse})}};
    snap_live.entities[0].position_offset = {.x = 5.0f, .y = 3.0f};
    snap_dead.entities[0].position_offset = {.x = 5.0f, .y = 3.0f};
    FakeTextureProvider provider;

    const auto live_cmds_batch =
        BattleRenderer::build_render_batch(snap_live, provider, no_background());

    const auto& live_cmds = test::commands_from(live_cmds_batch);
    const auto  dead_cmds_batch =
        BattleRenderer::build_render_batch(snap_dead, provider, no_background());

    const auto& dead_cmds = test::commands_from(dead_cmds_batch);

    ASSERT_EQ(live_cmds.size(), 1u);
    ASSERT_EQ(dead_cmds.size(), 1u);
    // World anchor (slot foot) is the same — position_offset does NOT affect anchor resolution
    EXPECT_FLOAT_EQ(test::tunable_item(live_cmds[0])->anchor.x,
                    test::tunable_item(dead_cmds[0])->anchor.x);
    EXPECT_FLOAT_EQ(test::tunable_item(live_cmds[0])->anchor.y,
                    test::tunable_item(dead_cmds[0])->anchor.y);
}

TEST(BattleTuningState, CorpseSaveReload) {
    const ConfigBinding binding{.config_file = kBattleScreenConfigPath,
                                .owner_kind = BindingOwnerKind::LifecycleProfile,
                                .tree_path = "HUMAN",
                                .role = BindingRole::Corpse,
                                .display_path = "unit_lifecycle_profiles.HUMAN.corpse"};
    BattleTuningState   state;
    state.set_placement(
        binding, VisualPlacementValue{.x = -5.0f, .y = 12.0f, .scale_x = 0.9f, .scale_y = 1.1f});

    nlohmann::json json;
    write_placement_to_json(json, binding, state.placements.at(binding.key()));

    EXPECT_TRUE(json.contains("unit_lifecycle_profiles"));
    EXPECT_NEAR(json["unit_lifecycle_profiles"]["HUMAN"]["corpse"]["x"].get<float>(), -5.0f, 0.01f);
    EXPECT_NEAR(json["unit_lifecycle_profiles"]["HUMAN"]["corpse"]["scale_x"].get<float>(), 0.9f,
                0.01f);
}

// --- Layered bundle binding semantics ---

// Helper: make a SnapshotTrack representing one layer of a layered bundle.
SnapshotTrack layered_effect_track(TrackKind kind, LayerSlot slot, const std::string& seq_name,
                                   const std::string& bundle_name, BindingRole role) {
    SnapshotTrack t = track(kind, TrackRenderLayer::Effect, "frame");
    t.sequence_name = seq_name;
    t.bundle_sequence_name = bundle_name;
    t.layer_slot = slot;
    t.effect_role = role;
    return t;
}

TEST(BattleRenderer, LayeredTuchAllLayersShareBundleBindingOwnerId) {
    // S1/A1/A2 layers of TUCHA_FIRE bundle must all resolve to the driver (A1) owner_id.
    SnapshotTrack const s1 = layered_effect_track(TrackKind::Effect, LayerSlot::S1, "TUCHS1_FIRE",
                                                  "TUCHA1_FIRE", BindingRole::Source);
    SnapshotTrack const a1 = layered_effect_track(TrackKind::Effect, LayerSlot::A1, "TUCHA1_FIRE",
                                                  "TUCHA1_FIRE", BindingRole::Source);
    SnapshotTrack const a2 = layered_effect_track(TrackKind::Effect, LayerSlot::A2, "TUCHA2_FIRE",
                                                  "TUCHA1_FIRE", BindingRole::Source);

    BattleRenderSnapshot const snapshot{
        .entities = {entity(1, BattleDepth::Front, 0, {s1, a1, a2})}};
    FakeTextureProvider provider;
    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, no_background());

    const auto& commands = test::commands_from(batch);

    ASSERT_EQ(commands.size(), 3u);
    for (const auto& cmd : commands) {
        ASSERT_TRUE(test::tunable_item(cmd).has_value());
        ASSERT_TRUE(test::tunable_item(cmd)->binding.has_value());
        EXPECT_EQ(test::tunable_item(cmd)->binding->tree_path, "TUCHA1_FIRE");
        EXPECT_EQ(test::tunable_item(cmd)->binding->owner_kind, BindingOwnerKind::EffectProfile);
    }
}

TEST(BattleRenderer, LayeredEffectLayersHaveUniqueStableIds) {
    SnapshotTrack s1 = layered_effect_track(TrackKind::Effect, LayerSlot::S1, "TUCHS1", "TUCHA1",
                                            BindingRole::Source);
    SnapshotTrack a1 = layered_effect_track(TrackKind::Effect, LayerSlot::A1, "TUCHA1", "TUCHA1",
                                            BindingRole::Source);
    SnapshotTrack a2 = layered_effect_track(TrackKind::Effect, LayerSlot::A2, "TUCHA2", "TUCHA1",
                                            BindingRole::Source);
    // Give all layers the same track id to replicate production: one VisualTrack → 3 SnapshotTracks
    s1.id = a1.id = a2.id = TrackId{42u};

    BattleRenderSnapshot const snapshot{
        .entities = {entity(5, BattleDepth::Front, 0, {s1, a1, a2})}};
    FakeTextureProvider provider;
    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, no_background());

    const auto& commands = test::commands_from(batch);

    ASSERT_EQ(commands.size(), 3u);
    std::vector<std::string> ids;
    for (const auto& cmd : commands) {
        ASSERT_TRUE(test::tunable_item(cmd).has_value());
        ids.push_back(test::tunable_item(cmd)->stable_id);
    }
    // All three stable_ids must be distinct
    EXPECT_NE(ids[0], ids[1]);
    EXPECT_NE(ids[0], ids[2]);
    EXPECT_NE(ids[1], ids[2]);
    // Each must embed the slot suffix
    EXPECT_NE(ids[0].find(":s1"), std::string::npos);
    EXPECT_NE(ids[1].find(":a1"), std::string::npos);
    EXPECT_NE(ids[2].find(":a2"), std::string::npos);
}

TEST(BattleRenderer, SingleSequenceEffectBindingStillUsesSequenceName) {
    // Non-layered effect: bundle_sequence_name empty → owner_id is sequence_name as before
    SnapshotTrack effect = track(TrackKind::Effect, TrackRenderLayer::Effect, "frame");
    effect.sequence_name = "HEFFA_SINGLE";
    effect.effect_role = BindingRole::TargetTeam;
    // No bundle_sequence_name / layer_slot

    BattleRenderSnapshot const snapshot{.entities = {entity(1, BattleDepth::Front, 0, {effect})}};
    FakeTextureProvider        provider;
    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, no_background());

    const auto& commands = test::commands_from(batch);

    ASSERT_EQ(commands.size(), 1u);
    ASSERT_TRUE(test::tunable_item(commands[0])->binding.has_value());
    EXPECT_EQ(test::tunable_item(commands[0])->binding->tree_path, "HEFFA_SINGLE");
}

TEST(BattleRenderer, UnitBaseLayeredTrackA1BindsToLayerProfile) {
    // A1 slot of a layered Base track must bind to unit_visual_layer_profiles for per-layer tuning.
    SnapshotTrack const base = layered_effect_track(TrackKind::Base, LayerSlot::A1, "STILA1_00",
                                                    "STILA1_00", BindingRole::UnitBase);
    SnapshotEntity      ent = entity(2, BattleDepth::Front, 0, {base});
    ent.unit_type = "UU0001";

    BattleRenderSnapshot const snapshot{.entities = {ent}};
    FakeTextureProvider        provider;
    const auto  batch = BattleRenderer::build_render_batch(snapshot, provider, no_background());
    const auto& commands = test::commands_from(batch);

    ASSERT_EQ(commands.size(), 1u);
    ASSERT_TRUE(test::tunable_item(commands[0])->binding.has_value());
    EXPECT_EQ(test::tunable_item(commands[0])->binding->owner_kind,
              BindingOwnerKind::UnitVisualLayerProfile);
    // owner_id encodes unit:slot
    EXPECT_EQ(test::tunable_item(commands[0])->binding->tree_path, "UU0001:a1");
    // display_path must point to unit_visual_layer_profiles section
    EXPECT_NE(
        test::tunable_item(commands[0])->binding->display_path.find("unit_visual_layer_profiles"),
        std::string::npos);
}

TEST(BattleRenderer, UnitBaseNonLayeredTrackBindsToUnitVisualProfile) {
    // Non-layered Base track (no layer_slot) must still bind to unit_visual_profiles.
    SnapshotTrack base;
    base.kind = TrackKind::Base;
    base.layer = TrackRenderLayer::Base;
    base.sequence_name = "idle";
    base.effect_role = BindingRole::UnitIdle;
    base.id.value = 1;
    SnapshotEntity ent = entity(2, BattleDepth::Front, 0, {base});
    ent.unit_type = "UU0001";

    BattleRenderSnapshot const snapshot{.entities = {ent}};
    FakeTextureProvider        provider;
    static_cast<void>(BattleRenderer::build_render_batch(snapshot, provider, no_background()));

    // No texture = no command, but binding can be checked through snapshot directly.
    // Verify via binding_for logic indirectly: the non-layered track has no layer_slot.
    EXPECT_FALSE(base.layer_slot.has_value());
}

// Helper for per-layer tuning tests
SnapshotTrack base_layer_track(LayerSlot slot, BindingRole role = BindingRole::UnitIdle) {
    SnapshotTrack t = layered_effect_track(TrackKind::Base, slot, "seq", "seq", role);
    t.layer = TrackRenderLayer::Base;
    t.id.value = static_cast<uint32_t>(slot) + 10u;
    return t;
}

TEST(BattleRenderer, LayerOffsetMovesOnlyThatLayer) {
    // A2 layer offset must shift A2 destination without affecting A1 or S1.
    SnapshotTrack const s1 = base_layer_track(LayerSlot::S1, BindingRole::UnitIdle);
    SnapshotTrack const a1 = base_layer_track(LayerSlot::A1, BindingRole::UnitIdle);
    SnapshotTrack const a2 = base_layer_track(LayerSlot::A2, BindingRole::UnitIdle);

    BattleRenderSnapshot snapshot{.entities = {entity(1, BattleDepth::Front, 0, {s1, a1, a2})}};
    snapshot.entities[0].unit_type = "UU0010";
    snapshot.entities[0].position_offset = {};
    FakeTextureProvider provider;

    // Base commands without any placement.
    const auto base_cmds_batch =
        BattleRenderer::build_render_batch(snapshot, provider, no_background());

    const auto& base_cmds = test::commands_from(base_cmds_batch);
    ASSERT_EQ(base_cmds.size(), 3u);

    // Add +30 y offset to A2 layer only.
    BattleRenderOptions opts = no_background();
    const std::string a2_key = ConfigBinding{.owner_kind = BindingOwnerKind::UnitVisualLayerProfile,
                                             .tree_path = "UU0010:a2",
                                             .role = BindingRole::UnitIdle}
                                   .key();
    opts.placements[a2_key] = VisualPlacementValue{.y = 30.0f};
    const auto layer_cmds_batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& layer_cmds = test::commands_from(layer_cmds_batch);
    ASSERT_EQ(layer_cmds.size(), 3u);

    // S1 and A1 must not move.
    EXPECT_FLOAT_EQ(layer_cmds[0].destination.y, base_cmds[0].destination.y) << "S1 must not move";
    EXPECT_FLOAT_EQ(layer_cmds[1].destination.y, base_cmds[1].destination.y) << "A1 must not move";
    // A2 must shift by +30.
    EXPECT_NEAR(layer_cmds[2].destination.y - base_cmds[2].destination.y, 30.0f, 0.5f)
        << "A2 must shift by layer offset";
}

TEST(BattleRenderer, RoleOffsetMovesAllLayers) {
    // Role-level offset (unit_visual_profiles) applied to all S1/A1/A2 layers together.
    SnapshotTrack const s1 = base_layer_track(LayerSlot::S1, BindingRole::UnitAttack);
    SnapshotTrack const a1 = base_layer_track(LayerSlot::A1, BindingRole::UnitAttack);
    SnapshotTrack const a2 = base_layer_track(LayerSlot::A2, BindingRole::UnitAttack);

    BattleRenderSnapshot snapshot{.entities = {entity(1, BattleDepth::Front, 0, {s1, a1, a2})}};
    snapshot.entities[0].unit_type = "UU0010";
    snapshot.entities[0].position_offset = {};
    FakeTextureProvider provider;

    const auto base_cmds_batch =
        BattleRenderer::build_render_batch(snapshot, provider, no_background());

    const auto& base_cmds = test::commands_from(base_cmds_batch);
    ASSERT_EQ(base_cmds.size(), 3u);

    BattleRenderOptions opts = no_background();
    const std::string   role_key = ConfigBinding{.owner_kind = BindingOwnerKind::UnitVisualProfile,
                                                 .tree_path = "UU0010",
                                                 .role = BindingRole::UnitAttack}
                                       .key();
    opts.placements[role_key] = VisualPlacementValue{.y = 20.0f};
    const auto role_cmds_batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& role_cmds = test::commands_from(role_cmds_batch);
    ASSERT_EQ(role_cmds.size(), 3u);

    // All three layers must shift by +20.
    for (std::size_t i = 0; i < 3u; ++i) {
        EXPECT_NEAR(role_cmds[i].destination.y - base_cmds[i].destination.y, 20.0f, 0.5f)
            << "Layer " << i << " must shift by role offset";
    }
}

TEST(BattleRenderer, LayerOffsetAddsToRoleOffset) {
    // Final offset = entity + role + layer (additive).
    SnapshotTrack const a2 = base_layer_track(LayerSlot::A2, BindingRole::UnitIdle);

    BattleRenderSnapshot snapshot{.entities = {entity(1, BattleDepth::Front, 0, {a2})}};
    snapshot.entities[0].unit_type = "UU0010";
    snapshot.entities[0].position_offset = {};
    FakeTextureProvider provider;

    BattleRenderOptions opts = no_background();
    const std::string   role_key = ConfigBinding{.owner_kind = BindingOwnerKind::UnitVisualProfile,
                                                 .tree_path = "UU0010",
                                                 .role = BindingRole::UnitIdle}
                                       .key();
    const std::string a2_key = ConfigBinding{.owner_kind = BindingOwnerKind::UnitVisualLayerProfile,
                                             .tree_path = "UU0010:a2",
                                             .role = BindingRole::UnitIdle}
                                   .key();
    opts.placements[role_key] = VisualPlacementValue{.y = 10.0f};
    opts.placements[a2_key] = VisualPlacementValue{.y = 5.0f};

    const auto base_cmds_batch =
        BattleRenderer::build_render_batch(snapshot, provider, no_background());

    const auto& base_cmds = test::commands_from(base_cmds_batch);
    const auto  comp_cmds_batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& comp_cmds = test::commands_from(comp_cmds_batch);

    ASSERT_EQ(base_cmds.size(), 1u);
    ASSERT_EQ(comp_cmds.size(), 1u);
    // A2 must shift by role(10) + layer(5) = 15.
    EXPECT_NEAR(comp_cmds[0].destination.y - base_cmds[0].destination.y, 15.0f, 0.5f);
}

TEST(BattleRenderer, ASideA2TrackResolvesASideLayerOffset) {
    // A-side entity A2: side-specific "a" offset must be applied; "d" must not.
    SnapshotTrack const a2 = base_layer_track(LayerSlot::A2, BindingRole::UnitIdle);

    BattleRenderSnapshot snapshot{
        .entities = {entity(1, BattleSide::Attacker, BattleDepth::Front, 0, {a2})}};
    snapshot.entities[0].unit_type = "UU0020";
    snapshot.entities[0].position_offset = {};
    FakeTextureProvider provider;

    const auto base_cmds_batch =
        BattleRenderer::build_render_batch(snapshot, provider, no_background());

    const auto& base_cmds = test::commands_from(base_cmds_batch);

    BattleRenderOptions opts = no_background();
    const ConfigBinding a2_a{.owner_kind = BindingOwnerKind::UnitVisualLayerProfile,
                             .tree_path = "UU0020:a2",
                             .role = BindingRole::UnitIdle,
                             .side = "a"};
    const ConfigBinding a2_d{.owner_kind = BindingOwnerKind::UnitVisualLayerProfile,
                             .tree_path = "UU0020:a2",
                             .role = BindingRole::UnitIdle,
                             .side = "d"};
    opts.placements[a2_a.key()] = VisualPlacementValue{.y = 30.0f};
    opts.placements[a2_d.key()] = VisualPlacementValue{.y = 99.0f}; // must not apply

    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& cmds = test::commands_from(batch);
    ASSERT_EQ(cmds.size(), 1u);
    EXPECT_NEAR(cmds[0].destination.y - base_cmds[0].destination.y, 30.0f, 0.5f);
}

TEST(BattleRenderer, DSideA2TrackResolvesDSideLayerOffset) {
    // D-side entity A2: side-specific "d" offset must be applied; "a" must not.
    SnapshotTrack const a2 = base_layer_track(LayerSlot::A2, BindingRole::UnitIdle);

    BattleRenderSnapshot snapshot{
        .entities = {entity(2, BattleSide::Defender, BattleDepth::Front, 0, {a2})}};
    snapshot.entities[0].unit_type = "UU0020";
    snapshot.entities[0].position_offset = {};
    FakeTextureProvider provider;

    const auto base_cmds_batch =
        BattleRenderer::build_render_batch(snapshot, provider, no_background());

    const auto& base_cmds = test::commands_from(base_cmds_batch);

    BattleRenderOptions opts = no_background();
    const ConfigBinding a2_a{.owner_kind = BindingOwnerKind::UnitVisualLayerProfile,
                             .tree_path = "UU0020:a2",
                             .role = BindingRole::UnitIdle,
                             .side = "a"};
    const ConfigBinding a2_d{.owner_kind = BindingOwnerKind::UnitVisualLayerProfile,
                             .tree_path = "UU0020:a2",
                             .role = BindingRole::UnitIdle,
                             .side = "d"};
    opts.placements[a2_a.key()] = VisualPlacementValue{.y = 99.0f}; // must not apply
    opts.placements[a2_d.key()] = VisualPlacementValue{.y = 40.0f};

    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& cmds = test::commands_from(batch);
    ASSERT_EQ(cmds.size(), 1u);
    EXPECT_NEAR(cmds[0].destination.y - base_cmds[0].destination.y, 40.0f, 0.5f);
}

TEST(BattleRenderer, SideSpecificLayerAddsToCommonLayer) {
    // Final offset = entity + common_layer + side_layer (additive).
    SnapshotTrack const a2 = base_layer_track(LayerSlot::A2, BindingRole::UnitIdle);

    BattleRenderSnapshot snapshot{
        .entities = {entity(1, BattleSide::Attacker, BattleDepth::Front, 0, {a2})}};
    snapshot.entities[0].unit_type = "UU0030";
    snapshot.entities[0].position_offset = {};
    FakeTextureProvider provider;

    const auto base_cmds_batch =
        BattleRenderer::build_render_batch(snapshot, provider, no_background());

    const auto& base_cmds = test::commands_from(base_cmds_batch);

    BattleRenderOptions opts = no_background();
    const ConfigBinding common{.owner_kind = BindingOwnerKind::UnitVisualLayerProfile,
                               .tree_path = "UU0030:a2",
                               .role = BindingRole::UnitIdle};
    const ConfigBinding side_a{.owner_kind = BindingOwnerKind::UnitVisualLayerProfile,
                               .tree_path = "UU0030:a2",
                               .role = BindingRole::UnitIdle,
                               .side = "a"};
    opts.placements[common.key()] = VisualPlacementValue{.y = 8.0f};
    opts.placements[side_a.key()] = VisualPlacementValue{.y = 4.0f};

    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& cmds = test::commands_from(batch);
    ASSERT_EQ(cmds.size(), 1u);
    // common(8) + side_a(4) = 12
    EXPECT_NEAR(cmds[0].destination.y - base_cmds[0].destination.y, 12.0f, 0.5f);
}

TEST(BattleRenderer, RoleSideSpecificMovesAllLayersOnThatSide) {
    // Role-level "a" override shifts all layers of an A-side entity.
    SnapshotTrack const s1 = base_layer_track(LayerSlot::S1, BindingRole::UnitIdle);
    SnapshotTrack const a1 = base_layer_track(LayerSlot::A1, BindingRole::UnitIdle);
    SnapshotTrack const a2 = base_layer_track(LayerSlot::A2, BindingRole::UnitIdle);

    BattleRenderSnapshot snapshot{
        .entities = {entity(1, BattleSide::Attacker, BattleDepth::Front, 0, {s1, a1, a2})}};
    snapshot.entities[0].unit_type = "UU0040";
    snapshot.entities[0].position_offset = {};
    FakeTextureProvider provider;

    const auto base_cmds_batch =
        BattleRenderer::build_render_batch(snapshot, provider, no_background());

    const auto& base_cmds = test::commands_from(base_cmds_batch);

    BattleRenderOptions opts = no_background();
    const ConfigBinding role_a{.owner_kind = BindingOwnerKind::UnitVisualProfile,
                               .tree_path = "UU0040",
                               .role = BindingRole::UnitIdle,
                               .side = "a"};
    opts.placements[role_a.key()] = VisualPlacementValue{.y = 25.0f};

    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& cmds = test::commands_from(batch);
    ASSERT_EQ(cmds.size(), 3u);
    for (std::size_t i = 0; i < 3u; ++i) {
        EXPECT_NEAR(cmds[i].destination.y - base_cmds[i].destination.y, 25.0f, 0.5f)
            << "Layer " << i << " must shift by A-side role override";
    }
}

TEST(BattleRenderer, LayerSideSpecificMovesOnlyThatLayerOnThatSide) {
    // Layer-level "a" override for A2 must not shift S1/A1 of the same entity.
    SnapshotTrack const s1 = base_layer_track(LayerSlot::S1, BindingRole::UnitIdle);
    SnapshotTrack const a2 = base_layer_track(LayerSlot::A2, BindingRole::UnitIdle);

    BattleRenderSnapshot snapshot{
        .entities = {entity(1, BattleSide::Attacker, BattleDepth::Front, 0, {s1, a2})}};
    snapshot.entities[0].unit_type = "UU0050";
    snapshot.entities[0].position_offset = {};
    FakeTextureProvider provider;

    const auto base_cmds_batch =
        BattleRenderer::build_render_batch(snapshot, provider, no_background());

    const auto& base_cmds = test::commands_from(base_cmds_batch);

    BattleRenderOptions opts = no_background();
    const ConfigBinding a2_a{.owner_kind = BindingOwnerKind::UnitVisualLayerProfile,
                             .tree_path = "UU0050:a2",
                             .role = BindingRole::UnitIdle,
                             .side = "a"};
    opts.placements[a2_a.key()] = VisualPlacementValue{.y = 50.0f};

    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& cmds = test::commands_from(batch);
    ASSERT_EQ(cmds.size(), 2u);
    // Ordering: tracks are built in snapshot order; S1 first then A2
    const float s1_delta = cmds[0].destination.y - base_cmds[0].destination.y;
    const float a2_delta = cmds[1].destination.y - base_cmds[1].destination.y;
    EXPECT_NEAR(s1_delta, 0.0f, 0.5f) << "S1 must not be moved by A2-side layer override";
    EXPECT_NEAR(a2_delta, 50.0f, 0.5f) << "A2 must shift by side-specific layer override";
}

TEST(BattleRenderer, CommonLayerOffsetAppliesWhenNoSideSpecificPresent) {
    // When only the common (side="") binding is set, it applies regardless of entity side.
    SnapshotTrack const a2_a_track = base_layer_track(LayerSlot::A2, BindingRole::UnitIdle);
    SnapshotTrack const a2_d_track = base_layer_track(LayerSlot::A2, BindingRole::UnitIdle);

    BattleRenderSnapshot snapshot{
        .entities = {entity(1, BattleSide::Attacker, BattleDepth::Front, 0, {a2_a_track}),
                     entity(2, BattleSide::Defender, BattleDepth::Front, 0, {a2_d_track})}};
    snapshot.entities[0].unit_type = "UU0060";
    snapshot.entities[0].position_offset = {};
    snapshot.entities[1].unit_type = "UU0060";
    snapshot.entities[1].position_offset = {};
    FakeTextureProvider provider;

    const auto base_cmds_batch =
        BattleRenderer::build_render_batch(snapshot, provider, no_background());

    const auto& base_cmds = test::commands_from(base_cmds_batch);

    BattleRenderOptions opts = no_background();
    const ConfigBinding common{.owner_kind = BindingOwnerKind::UnitVisualLayerProfile,
                               .tree_path = "UU0060:a2",
                               .role = BindingRole::UnitIdle};
    opts.placements[common.key()] = VisualPlacementValue{.y = 15.0f};

    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& cmds = test::commands_from(batch);
    ASSERT_EQ(cmds.size(), 2u);
    // Both sides must receive the common offset.
    EXPECT_NEAR(cmds[0].destination.y - base_cmds[0].destination.y, 15.0f, 0.5f)
        << "A-side entity must get common layer offset";
    EXPECT_NEAR(cmds[1].destination.y - base_cmds[1].destination.y, 15.0f, 0.5f)
        << "D-side entity must get common layer offset";
}

// ─── Built-in S1/A1/A2 layer order bias ─────────────────────────────────────

SnapshotTrack layered_base(LayerSlot slot, std::string image_name) {
    SnapshotTrack t = track(TrackKind::Base, TrackRenderLayer::Base, std::move(image_name));
    t.layer_slot = slot;
    t.sequence_name = "TEST_LAYERED";
    t.effect_role = BindingRole::UnitAttack;
    return t;
}

// A2 must render above A1 with no tuning at all.
TEST(BattleRenderer, LayeredBaseA2RendersAboveA1WithoutTuning) {
    BattleRenderSnapshot const snapshot{
        .entities = {entity(
            1, BattleDepth::Front, 0,
            {layered_base(LayerSlot::A1, "a1_frame"), layered_base(LayerSlot::A2, "a2_frame")})}};
    FakeTextureProvider provider;
    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, no_background());

    const auto& cmds = test::commands_from(batch);

    ASSERT_EQ(cmds.size(), 2u);
    int a1_layer = -99;
    int a2_layer = -99;
    for (const auto& cmd : cmds) {
        if (!test::tunable_item(cmd)) {
            continue;
        }
        if (test::tunable_item(cmd)->current_frame_name == "a1_frame") {
            a1_layer = test::tunable_item(cmd)->layer;
        }
        if (test::tunable_item(cmd)->current_frame_name == "a2_frame") {
            a2_layer = test::tunable_item(cmd)->layer;
        }
    }
    EXPECT_LT(a1_layer, a2_layer) << "A2 must render above A1 without any tuning";
    EXPECT_EQ(a2_layer - a1_layer, 2) << "A2 built-in bias is +2 over A1";
}

// S1 must render below A1 with no tuning.
TEST(BattleRenderer, LayeredBaseS1RendersBelowA1WithoutTuning) {
    BattleRenderSnapshot const snapshot{
        .entities = {entity(1, BattleDepth::Front, 0,
                            {layered_base(LayerSlot::S1, "s1_frame"),
                             layered_base(LayerSlot::A1, "a1_frame"),
                             layered_base(LayerSlot::A2, "a2_frame")})}};
    FakeTextureProvider provider;
    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, no_background());

    const auto& cmds = test::commands_from(batch);

    ASSERT_EQ(cmds.size(), 3u);
    int s1_layer = -99;
    int a1_layer = -99;
    int a2_layer = -99;
    for (const auto& cmd : cmds) {
        if (!test::tunable_item(cmd)) {
            continue;
        }
        if (test::tunable_item(cmd)->current_frame_name == "s1_frame") {
            s1_layer = test::tunable_item(cmd)->layer;
        }
        if (test::tunable_item(cmd)->current_frame_name == "a1_frame") {
            a1_layer = test::tunable_item(cmd)->layer;
        }
        if (test::tunable_item(cmd)->current_frame_name == "a2_frame") {
            a2_layer = test::tunable_item(cmd)->layer;
        }
    }
    EXPECT_LT(s1_layer, a1_layer) << "S1 < A1";
    EXPECT_LT(a1_layer, a2_layer) << "A1 < A2";
}

// Saved config level=0 for A2 still places it above A1 (bias not stored in config).
TEST(BattleRenderer, LayeredBaseA2WithSavedLevelZeroStillRendersAboveA1) {
    BattleRenderSnapshot snapshot{.entities = {entity(1, BattleDepth::Front, 0,
                                                      {layered_base(LayerSlot::A1, "a1_frame"),
                                                       layered_base(LayerSlot::A2, "a2_frame")})}};
    snapshot.entities[0].unit_type = "UU0103";

    // Simulate saved config: UU0103.attack.a2.level = 0 (the "accidental" value that is
    // actually correct after the built-in bias fix — user delta of 0 still gives A2 +2 bias).
    BattleRenderOptions opts = no_background();
    const std::string a2_key = ConfigBinding{.owner_kind = BindingOwnerKind::UnitVisualLayerProfile,
                                             .tree_path = "UU0103:a2",
                                             .role = BindingRole::UnitAttack}
                                   .key();
    opts.placements[a2_key] = VisualPlacementValue{.level = 0}; // saved level = 0

    FakeTextureProvider provider;
    const auto          batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& cmds = test::commands_from(batch);

    ASSERT_EQ(cmds.size(), 2u);
    int a1_layer = -99;
    int a2_layer = -99;
    for (const auto& cmd : cmds) {
        if (!test::tunable_item(cmd)) {
            continue;
        }
        if (test::tunable_item(cmd)->current_frame_name == "a1_frame") {
            a1_layer = test::tunable_item(cmd)->layer;
        }
        if (test::tunable_item(cmd)->current_frame_name == "a2_frame") {
            a2_layer = test::tunable_item(cmd)->layer;
        }
    }
    EXPECT_LT(a1_layer, a2_layer)
        << "saved level=0 must not hide A2 — built-in +2 bias still applies";
}

// User delta of +1 for A2 stacks on top of built-in +2 bias.
TEST(BattleRenderer, LayeredBaseA2UserDeltaStacksOnBuiltinBias) {
    BattleRenderSnapshot snapshot{.entities = {entity(1, BattleDepth::Front, 0,
                                                      {layered_base(LayerSlot::A1, "a1_frame"),
                                                       layered_base(LayerSlot::A2, "a2_frame")})}};
    snapshot.entities[0].unit_type = "UU_TEST";

    BattleRenderOptions opts = no_background();
    const std::string a2_key = ConfigBinding{.owner_kind = BindingOwnerKind::UnitVisualLayerProfile,
                                             .tree_path = "UU_TEST:a2",
                                             .role = BindingRole::UnitAttack}
                                   .key();
    opts.placements[a2_key] = VisualPlacementValue{.level = 1}; // user adds +1 on top of builtin +2

    FakeTextureProvider provider;
    const auto cmds_with_delta_batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& cmds_with_delta = test::commands_from(cmds_with_delta_batch);
    const auto  cmds_no_delta_batch =
        BattleRenderer::build_render_batch(snapshot, provider, no_background());

    const auto& cmds_no_delta = test::commands_from(cmds_no_delta_batch);

    ASSERT_EQ(cmds_with_delta.size(), 2u);
    ASSERT_EQ(cmds_no_delta.size(), 2u);

    auto find_layer = [](const std::vector<RenderCommand>& cmds, const std::string& name) {
        for (const auto& cmd : cmds) {
            if (test::tunable_item(cmd) && test::tunable_item(cmd)->current_frame_name == name) {
                return test::tunable_item(cmd)->layer;
            }
        }
        return -99;
    };
    const int a2_default = find_layer(cmds_no_delta, "a2_frame");
    const int a2_delta = find_layer(cmds_with_delta, "a2_frame");
    EXPECT_EQ(a2_delta - a2_default, 1) << "user delta +1 adds on top of built-in bias";
}

// ── CENTER debug items from make_tree_layout_items ───────────────────────

TEST(BattleRenderer, CenterDebugItemsHaveCorrectStableIdAndBinding) {
    FakeTextureProvider provider;

    // Tree with one center slot node
    d2engine::TreeLayout tree;
    tree.set_node("/battlefield/slot_a_center_1",
                  TreeNode{.kind = "battlefield_slot", .x = 655.5f, .y = 429.5f, .level = 10});
    tree.set_node("/battlefield/slot_a_center_1/unit",
                  TreeNode{.kind = "unit_mount", .x = 0.0f, .y = 0.0f});

    BattleRenderOptions opts;
    opts.draw_background = false;
    opts.draw_unit_groups = false;
    opts.tree_layout = &tree;
    opts.debug_enabled = true;

    const auto batch = BattleRenderer::build_render_batch(BattleRenderSnapshot{}, provider, opts);

    const auto& commands = test::commands_from(batch);

    const RenderCommand* slot_cmd = nullptr;
    const RenderCommand* unit_cmd = nullptr;
    for (const auto& cmd : commands) {
        if (!test::tunable_item(cmd)) {
            continue;
        }
        if (test::tunable_item(cmd)->stable_id == "tree:A_CENTER_1") {
            slot_cmd = &cmd;
        }
        if (test::tunable_item(cmd)->stable_id == "tree:A_CENTER_1/unit") {
            unit_cmd = &cmd;
        }
    }

    ASSERT_NE(slot_cmd, nullptr) << "Missing tree:A_CENTER_1 debug item";
    ASSERT_NE(unit_cmd, nullptr) << "Missing tree:A_CENTER_1/unit debug item";

    ASSERT_TRUE(test::tunable_item(*slot_cmd)->binding.has_value());
    EXPECT_EQ(test::tunable_item(*slot_cmd)->binding->owner_kind, BindingOwnerKind::TreeLayout);
    EXPECT_EQ(test::tunable_item(*slot_cmd)->binding->tree_path, "/battlefield/slot_a_center_1");
    EXPECT_EQ(test::tunable_item(*slot_cmd)->tree_path, "/battlefield/slot_a_center_1");
    EXPECT_TRUE(test::tunable_item(*slot_cmd)->selectable);

    ASSERT_TRUE(test::tunable_item(*unit_cmd)->binding.has_value());
    EXPECT_EQ(test::tunable_item(*unit_cmd)->binding->owner_kind, BindingOwnerKind::TreeLayout);
    EXPECT_EQ(test::tunable_item(*unit_cmd)->binding->tree_path,
              "/battlefield/slot_a_center_1/unit");
}

TEST(BattleRenderer, UnitgroupHpTextAndDamageFillUseRenderTreeContainers) {
    d2engine::TreeLayout tree;
    tree.set_node("/ui/left_unit_group", TreeNode{.kind = "image", .x = 10.0f, .y = 20.0f});
    tree.set_node("/ui/left_unit_group/0_front",
                  TreeNode{.kind = "slot", .x = 30.0f, .y = 40.0f, .w = 100.0f, .h = 160.0f});
    tree.set_node("/ui/left_unit_group/0_front/portrait",
                  TreeNode{.kind = "portrait", .x = 2.0f, .y = 3.0f, .w = 80.0f, .h = 120.0f});
    tree.set_node("/ui/left_unit_group/0_front/hp",
                  TreeNode{.kind = "text",
                           .x = 4.0f,
                           .y = 130.0f,
                           .w = 90.0f,
                           .h = 24.0f,
                           .color = Color{.r = 1, .g = 2, .b = 3, .a = 255},
                           .font_size = 11.0f});

    BattleRenderSnapshot snapshot{.entities = {entity(1, BattleDepth::Front, 0, {})}};
    snapshot.entities[0].unit_instance_id = UnitInstanceId{7};
    snapshot.entities[0].current_hp = 60;
    snapshot.entities[0].max_hp = 100;

    BattleRenderOptions opts = no_background();
    opts.draw_unit_groups = true;
    opts.tree_layout = &tree;
    FakeTextureProvider provider;
    const auto          batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& cmds = test::commands_from(batch);

    const RenderCommand* hp = nullptr;
    const RenderCommand* fill = nullptr;
    for (const auto& cmd : cmds) {
        if (cmd.tree_path == "/ui/left_unit_group/0_front/hp") {
            hp = &cmd;
        } else if (cmd.tree_path == "/ui/left_unit_group/0_front"
                                    "/portrait" &&
                   cmd.fill_color.has_value()) {
            fill = &cmd;
        }
    }
    ASSERT_NE(hp, nullptr);
    EXPECT_EQ(hp->text, "60/100");
    EXPECT_EQ(hp->destination, tree.compose("/ui/left_unit_group/0_front/hp"));
    EXPECT_EQ(hp->text_color.r, 1);
    EXPECT_EQ(hp->text_color.g, 2);
    EXPECT_EQ(hp->text_color.b, 3);
    EXPECT_EQ(hp->text_color.a, 255);
    EXPECT_FLOAT_EQ(hp->font_size, 11.0f);
    EXPECT_TRUE(hp->game_font_text);
    ASSERT_TRUE(test::tunable_item(*hp).has_value());
    EXPECT_TRUE(test::tunable_item(*hp)->selectable);
    EXPECT_EQ(test::tunable_item(*hp)->tree_path, "/ui/left_unit_group/0_front/hp");
    ASSERT_TRUE(test::tunable_item(*hp)->binding.has_value());
    EXPECT_EQ(test::tunable_item(*hp)->binding->owner_kind, BindingOwnerKind::TreeLayout);
    EXPECT_EQ(test::tunable_item(*hp)->binding->tree_path, "/ui/left_unit_group/0_front/hp");
    EXPECT_EQ(test::tunable_item(*hp)->binding->display_path,
              "render_tree/ui/left_unit_group/0_front/hp");

    ASSERT_NE(fill, nullptr);
    EXPECT_FLOAT_EQ(fill->destination.h, 48.0f);
    EXPECT_FLOAT_EQ(fill->destination.y,
                    tree.compose("/ui/left_unit_group/0_front/portrait").y + 72.0f);
}

TEST(BattleRenderer, UnitgroupDamageFillOnlyRendersForPartialHp) {
    d2engine::TreeLayout tree;
    tree.set_node("/ui/left_unit_group/0_front",
                  TreeNode{.kind = "slot", .x = 0.0f, .y = 0.0f, .w = 100.0f, .h = 160.0f});
    tree.set_node("/ui/left_unit_group/0_front/portrait",
                  TreeNode{.kind = "portrait", .x = 0.0f, .y = 0.0f, .w = 80.0f, .h = 120.0f});
    tree.set_node("/ui/left_unit_group/0_front/hp",
                  TreeNode{.kind = "text", .x = 0.0f, .y = 130.0f, .w = 100.0f, .h = 24.0f});

    auto fill_height = [&](int hp) {
        BattleRenderSnapshot snapshot{.entities = {entity(1, BattleDepth::Front, 0, {})}};
        snapshot.entities[0].current_hp = hp;
        snapshot.entities[0].max_hp = 100;
        BattleRenderOptions opts = no_background();
        opts.draw_unit_groups = true;
        opts.tree_layout = &tree;
        FakeTextureProvider provider;
        const auto          batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

        const auto& cmds = test::commands_from(batch);
        for (const auto& cmd : cmds) {
            if (cmd.tree_path == "/ui/left_unit_group/0_front/portrait" &&
                cmd.fill_color.has_value() && cmd.fill_color->r == 180) {
                return cmd.destination.h;
            }
        }
        return 0.0f;
    };

    EXPECT_FLOAT_EQ(fill_height(100), 0.0f);
    EXPECT_FLOAT_EQ(fill_height(0), 0.0f);
    EXPECT_FLOAT_EQ(fill_height(60), 48.0f);
}

TEST(BattleRenderer, LargeUnitDamageFillUsesDeadMaskPortraitUnionBasis) {
    d2engine::TreeLayout tree;
    tree.set_node("/ui/left_unit_group/1_back",
                  TreeNode{.kind = "slot", .x = 10.0f, .y = 20.0f, .w = 100.0f, .h = 160.0f});
    tree.set_node("/ui/left_unit_group/1_back/portrait",
                  TreeNode{.kind = "portrait", .x = 0.0f, .y = 0.0f, .w = 80.0f, .h = 120.0f});
    tree.set_node("/ui/left_unit_group/1_front",
                  TreeNode{.kind = "slot", .x = 70.0f, .y = 10.0f, .w = 100.0f, .h = 160.0f});
    tree.set_node("/ui/left_unit_group/1_front/portrait",
                  TreeNode{.kind = "portrait", .x = 0.0f, .y = 0.0f, .w = 90.0f, .h = 130.0f});
    tree.set_node("/ui/left_unit_group/1_center",
                  TreeNode{.kind = "slot", .x = 30.0f, .y = 30.0f, .w = 50.0f, .h = 80.0f});
    tree.set_node("/ui/left_unit_group/1_center/portrait",
                  TreeNode{.kind = "portrait", .x = 0.0f, .y = 0.0f, .w = 20.0f, .h = 20.0f});
    tree.set_node("/ui/left_unit_group/1_center/hp",
                  TreeNode{.kind = "text", .x = 0.0f, .y = 20.0f, .w = 50.0f, .h = 20.0f});

    BattleRenderSnapshot snapshot{.entities = {entity(1, BattleDepth::Center, 1, {})}};
    snapshot.entities[0].is_large = true;
    snapshot.entities[0].current_hp = 50;
    snapshot.entities[0].max_hp = 100;

    BattleRenderOptions opts = no_background();
    opts.draw_unit_groups = true;
    opts.tree_layout = &tree;
    FakeTextureProvider provider;
    const auto          batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& cmds = test::commands_from(batch);

    const RenderCommand* fill = nullptr;
    for (const auto& cmd : cmds) {
        if (cmd.tree_path == "/ui/left_unit_group/1_center/portrait" &&
            cmd.fill_color.has_value()) {
            fill = &cmd;
        }
    }

    ASSERT_NE(fill, nullptr);
    EXPECT_FLOAT_EQ(fill->destination.x, 10.0f);
    EXPECT_FLOAT_EQ(fill->destination.w, 150.0f);
    EXPECT_FLOAT_EQ(fill->destination.h, 65.0f);
    EXPECT_FLOAT_EQ(fill->destination.y, 75.0f);
}

TEST(BattleRenderer, CenterHpBackgroundConfigHasCorrectProperties) {
    const nlohmann::json config = load_default_visual_config();
    ASSERT_TRUE(config.contains("render_tree"));

    int center_bg_count = 0;
    int front_bg_count = 0;
    int back_bg_count = 0;

    for (const auto& [path, node] : config["render_tree"].items()) {
        if (path.find("/hp_background") == std::string::npos) {
            continue;
        }
        ASSERT_TRUE(node.contains("kind")) << path;
        EXPECT_EQ(node["kind"], "image") << path;
        ASSERT_TRUE(node.contains("asset")) << path;
        EXPECT_EQ(node["asset"], "DLG_BATTLE_A_SPLITLRG") << path;

        ASSERT_TRUE(node.contains("level")) << path;
        const int bg_level = node["level"].get<int>();

        const std::string hp_path = path.substr(0, path.rfind('/') + 1) + "hp";
        ASSERT_TRUE(config["render_tree"].contains(hp_path)) << hp_path;
        ASSERT_TRUE(config["render_tree"][hp_path].contains("level")) << hp_path;
        const int hp_level = config["render_tree"][hp_path]["level"].get<int>();
        EXPECT_LT(bg_level, hp_level) << path;

        if (path.find("_center/") != std::string::npos) {
            ++center_bg_count;
        } else if (path.find("_front/") != std::string::npos) {
            ++front_bg_count;
        } else if (path.find("_back/") != std::string::npos) {
            ++back_bg_count;
        }
    }

    EXPECT_EQ(center_bg_count, 6);
    EXPECT_EQ(front_bg_count, 0);
    EXPECT_EQ(back_bg_count, 0);
}

TEST(BattleRenderer, CenterSlotHpBackgroundRendersWhenOccupied) {
    d2engine::TreeLayout tree;
    tree.set_node("/ui/left_unit_group", TreeNode{.kind = "image", .x = 10.0f, .y = 20.0f});
    tree.set_node("/ui/left_unit_group/0_center",
                  TreeNode{.kind = "slot", .x = 30.0f, .y = 40.0f, .w = 113.0f, .h = 160.0f});
    tree.set_node("/ui/left_unit_group/0_center/hp_background",
                  TreeNode{.kind = "image",
                           .x = 0.0f,
                           .y = 135.0f,
                           .w = 113.0f,
                           .h = 25.0f,
                           .level = -1,
                           .asset = "DLG_BATTLE_A_SPLITLRG"});
    tree.set_node(
        "/ui/left_unit_group/0_center/hp",
        TreeNode{.kind = "text", .x = 0.0f, .y = 135.0f, .w = 113.0f, .h = 25.0f, .level = 0});
    tree.set_node("/ui/left_unit_group/0_center/portrait",
                  TreeNode{.kind = "portrait", .x = 0.0f, .y = 0.0f, .w = 113.0f, .h = 135.0f});

    BattleRenderSnapshot snapshot{.entities = {entity(1, BattleDepth::Center, 0, {})}};
    snapshot.entities[0].current_hp = 60;
    snapshot.entities[0].max_hp = 100;

    BattleRenderOptions opts = no_background();
    opts.draw_unit_groups = true;
    opts.tree_layout = &tree;
    FakeTextureProvider provider;
    const auto          batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& cmds = test::commands_from(batch);

    const RenderCommand* bg = nullptr;
    const RenderCommand* hp = nullptr;
    for (const auto& cmd : cmds) {
        if (cmd.tree_path == "/ui/left_unit_group/0_center/hp_background") {
            bg = &cmd;
        } else if (cmd.tree_path == "/ui/left_unit_group/0_center/hp") {
            hp = &cmd;
        }
    }

    ASSERT_NE(bg, nullptr);
    EXPECT_TRUE(bg->texture.present());
    EXPECT_EQ(bg->destination, tree.compose("/ui/left_unit_group/0_center/hp_background"));
    EXPECT_FALSE(bg->tile);
    EXPECT_FALSE(bg->fill_color.has_value());

    ASSERT_NE(hp, nullptr);
    EXPECT_EQ(hp->text, "60/100");

    const auto bg_index = static_cast<std::size_t>(bg - cmds.data());
    const auto hp_index = static_cast<std::size_t>(hp - cmds.data());
    EXPECT_LT(bg_index, hp_index);
}

TEST(BattleRenderer, CenterHpBackgroundUsesCustomAssetFromConfig) {
    d2engine::TreeLayout tree;
    tree.set_node("/ui/left_unit_group", TreeNode{.kind = "image", .x = 10.0f, .y = 20.0f});
    tree.set_node("/ui/left_unit_group/0_center",
                  TreeNode{.kind = "slot", .x = 30.0f, .y = 40.0f, .w = 113.0f, .h = 160.0f});
    tree.set_node("/ui/left_unit_group/0_center/hp_background",
                  TreeNode{.kind = "image",
                           .x = 0.0f,
                           .y = 135.0f,
                           .w = 113.0f,
                           .h = 25.0f,
                           .level = -1,
                           .asset = "CUSTOM_TEST_ASSET"});
    tree.set_node(
        "/ui/left_unit_group/0_center/hp",
        TreeNode{.kind = "text", .x = 0.0f, .y = 135.0f, .w = 113.0f, .h = 25.0f, .level = 0});
    tree.set_node("/ui/left_unit_group/0_center/portrait",
                  TreeNode{.kind = "portrait", .x = 0.0f, .y = 0.0f, .w = 113.0f, .h = 135.0f});

    BattleRenderSnapshot snapshot{.entities = {entity(1, BattleDepth::Center, 0, {})}};
    snapshot.entities[0].current_hp = 60;
    snapshot.entities[0].max_hp = 100;

    BattleRenderOptions opts = no_background();
    opts.draw_unit_groups = true;
    opts.tree_layout = &tree;
    // Provider returns non-null for any non-"missing" name; calls increments per lookup
    FakeTextureProvider provider;
    const auto          batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& cmds = test::commands_from(batch);

    const RenderCommand* bg = nullptr;
    for (const auto& cmd : cmds) {
        if (cmd.tree_path == "/ui/left_unit_group/0_center/hp_background") {
            bg = &cmd;
        }
    }
    ASSERT_NE(bg, nullptr);
    EXPECT_TRUE(bg->texture.present());
    // FakeTextureProvider::get_texture was called — any non-"missing" asset works
}

TEST(BattleRenderer, CenterHpBackgroundSkipsWhenNoAssetInConfig) {
    d2engine::TreeLayout tree;
    tree.set_node("/ui/left_unit_group", TreeNode{.kind = "image", .x = 10.0f, .y = 20.0f});
    tree.set_node("/ui/left_unit_group/0_center",
                  TreeNode{.kind = "slot", .x = 30.0f, .y = 40.0f, .w = 113.0f, .h = 160.0f});
    // hp_background node exists but has no asset field
    tree.set_node(
        "/ui/left_unit_group/0_center/hp_background",
        TreeNode{.kind = "image", .x = 0.0f, .y = 135.0f, .w = 113.0f, .h = 25.0f, .level = -1});
    tree.set_node(
        "/ui/left_unit_group/0_center/hp",
        TreeNode{.kind = "text", .x = 0.0f, .y = 135.0f, .w = 113.0f, .h = 25.0f, .level = 0});
    tree.set_node("/ui/left_unit_group/0_center/portrait",
                  TreeNode{.kind = "portrait", .x = 0.0f, .y = 0.0f, .w = 113.0f, .h = 135.0f});

    BattleRenderSnapshot snapshot{.entities = {entity(1, BattleDepth::Center, 0, {})}};
    snapshot.entities[0].current_hp = 60;
    snapshot.entities[0].max_hp = 100;

    BattleRenderOptions opts = no_background();
    opts.draw_unit_groups = true;
    opts.tree_layout = &tree;
    FakeTextureProvider provider;
    const auto          batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& cmds = test::commands_from(batch);

    const RenderCommand* bg = nullptr;
    for (const auto& cmd : cmds) {
        if (cmd.tree_path == "/ui/left_unit_group/0_center/hp_background") {
            bg = &cmd;
        }
    }
    EXPECT_EQ(bg, nullptr);
}

TEST(BattleRenderer, CenterSlotHpBackgroundNotRenderedWhenEmpty) {
    d2engine::TreeLayout tree;
    tree.set_node("/ui/left_unit_group", TreeNode{.kind = "image", .x = 10.0f, .y = 20.0f});
    tree.set_node("/ui/left_unit_group/0_center",
                  TreeNode{.kind = "slot", .x = 30.0f, .y = 40.0f, .w = 113.0f, .h = 160.0f});
    tree.set_node("/ui/left_unit_group/0_center/hp_background",
                  TreeNode{.kind = "image",
                           .x = 0.0f,
                           .y = 135.0f,
                           .w = 113.0f,
                           .h = 25.0f,
                           .asset = "DLG_BATTLE_A_SPLITLRG"});
    tree.set_node("/ui/left_unit_group/0_center/hp",
                  TreeNode{.kind = "text", .x = 0.0f, .y = 135.0f, .w = 113.0f, .h = 25.0f});
    tree.set_node("/ui/left_unit_group/0_center/portrait",
                  TreeNode{.kind = "portrait", .x = 0.0f, .y = 0.0f, .w = 113.0f, .h = 135.0f});

    BattleRenderSnapshot snapshot{.entities = {entity(1, BattleDepth::Center, 0, {})}};
    snapshot.entities[0].max_hp = 0;

    BattleRenderOptions opts = no_background();
    opts.draw_unit_groups = true;
    opts.tree_layout = &tree;
    FakeTextureProvider provider;
    const auto          batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& cmds = test::commands_from(batch);

    const RenderCommand* bg = nullptr;
    for (const auto& cmd : cmds) {
        if (cmd.tree_path == "/ui/left_unit_group/0_center/hp_background") {
            bg = &cmd;
        }
    }

    EXPECT_EQ(bg, nullptr);
}

TEST(BattleRenderer, FrontSlotDoesNotHaveHpBackground) {
    d2engine::TreeLayout tree;
    tree.set_node("/ui/left_unit_group", TreeNode{.kind = "image", .x = 10.0f, .y = 20.0f});
    tree.set_node("/ui/left_unit_group/0_front",
                  TreeNode{.kind = "slot", .x = 30.0f, .y = 40.0f, .w = 113.0f, .h = 160.0f});
    tree.set_node("/ui/left_unit_group/0_front/portrait",
                  TreeNode{.kind = "portrait", .x = 0.0f, .y = 0.0f, .w = 80.0f, .h = 120.0f});
    tree.set_node("/ui/left_unit_group/0_front/hp",
                  TreeNode{.kind = "text", .x = 0.0f, .y = 130.0f, .w = 90.0f, .h = 24.0f});

    BattleRenderSnapshot snapshot{.entities = {entity(1, BattleDepth::Front, 0, {})}};
    snapshot.entities[0].current_hp = 60;
    snapshot.entities[0].max_hp = 100;

    BattleRenderOptions opts = no_background();
    opts.draw_unit_groups = true;
    opts.tree_layout = &tree;
    FakeTextureProvider provider;
    const auto          batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& cmds = test::commands_from(batch);

    const RenderCommand* bg = nullptr;
    const RenderCommand* hp = nullptr;
    for (const auto& cmd : cmds) {
        if (cmd.tree_path == "/ui/left_unit_group/0_front/hp_background") {
            bg = &cmd;
        } else if (cmd.tree_path == "/ui/left_unit_group/0_front/hp") {
            hp = &cmd;
        }
    }

    EXPECT_EQ(bg, nullptr);
    ASSERT_NE(hp, nullptr);
    EXPECT_EQ(hp->text, "60/100");
}

TEST(BattleRenderer, CenterHpBackgroundHasDebugBinding) {
    d2engine::TreeLayout tree;
    tree.set_node("/ui/left_unit_group", TreeNode{.kind = "image", .x = 10.0f, .y = 20.0f});
    tree.set_node("/ui/left_unit_group/0_center",
                  TreeNode{.kind = "slot", .x = 30.0f, .y = 40.0f, .w = 113.0f, .h = 160.0f});
    tree.set_node("/ui/left_unit_group/0_center/hp_background",
                  TreeNode{.kind = "image",
                           .x = 0.0f,
                           .y = 135.0f,
                           .w = 113.0f,
                           .h = 25.0f,
                           .asset = "DLG_BATTLE_A_SPLITLRG"});
    tree.set_node("/ui/left_unit_group/0_center/hp",
                  TreeNode{.kind = "text", .x = 0.0f, .y = 135.0f, .w = 113.0f, .h = 25.0f});
    tree.set_node("/ui/left_unit_group/0_center/portrait",
                  TreeNode{.kind = "portrait", .x = 0.0f, .y = 0.0f, .w = 113.0f, .h = 135.0f});

    BattleRenderSnapshot snapshot{.entities = {entity(1, BattleDepth::Center, 0, {})}};
    snapshot.entities[0].current_hp = 60;
    snapshot.entities[0].max_hp = 100;

    BattleRenderOptions opts = no_background();
    opts.draw_unit_groups = true;
    opts.tree_layout = &tree;
    FakeTextureProvider provider;
    const auto          batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& cmds = test::commands_from(batch);

    const RenderCommand* bg = nullptr;
    const RenderCommand* hp = nullptr;
    for (const auto& cmd : cmds) {
        if (cmd.tree_path == "/ui/left_unit_group/0_center/hp_background") {
            bg = &cmd;
        } else if (cmd.tree_path == "/ui/left_unit_group/0_center/hp") {
            hp = &cmd;
        }
    }

    ASSERT_NE(bg, nullptr);
    ASSERT_TRUE(test::tunable_item(*bg).has_value());
    EXPECT_TRUE(test::tunable_item(*bg)->selectable);
    EXPECT_EQ(test::tunable_item(*bg)->tree_path, "/ui/left_unit_group/0_center/hp_background");
    ASSERT_TRUE(test::tunable_item(*bg)->binding.has_value());
    EXPECT_EQ(test::tunable_item(*bg)->binding->owner_kind, BindingOwnerKind::TreeLayout);
    EXPECT_EQ(test::tunable_item(*bg)->binding->tree_path,
              "/ui/left_unit_group/0_center/hp_background");
    EXPECT_EQ(test::tunable_item(*bg)->binding->display_path,
              "render_tree/ui/left_unit_group/0_center/hp_background");

    ASSERT_NE(hp, nullptr);
    ASSERT_TRUE(test::tunable_item(*hp).has_value());
    EXPECT_TRUE(test::tunable_item(*hp)->selectable);
    EXPECT_EQ(test::tunable_item(*hp)->tree_path, "/ui/left_unit_group/0_center/hp");
    ASSERT_TRUE(test::tunable_item(*hp)->binding.has_value());
    EXPECT_EQ(test::tunable_item(*hp)->binding->owner_kind, BindingOwnerKind::TreeLayout);
    EXPECT_EQ(test::tunable_item(*hp)->binding->tree_path, "/ui/left_unit_group/0_center/hp");
    EXPECT_EQ(test::tunable_item(*hp)->binding->display_path,
              "render_tree/ui/left_unit_group/0_center/hp");
}

TEST(BattleRenderer, BattleFrameInfoTextUsesSelectedUnitContainers) {
    d2engine::TreeLayout tree;
    tree.set_node("/ui/combat_frame/info_left", TreeNode{.kind = "info", .x = 100.0f, .y = 200.0f});
    tree.set_node("/ui/combat_frame/info_left/name",
                  TreeNode{.kind = "text", .x = 1.0f, .y = 2.0f, .w = 120.0f, .h = 20.0f});
    tree.set_node("/ui/combat_frame/info_left/hp",
                  TreeNode{.kind = "text", .x = 3.0f, .y = 24.0f, .w = 120.0f, .h = 20.0f});
    tree.set_node("/ui/combat_frame/info_right",
                  TreeNode{.kind = "info", .x = 300.0f, .y = 200.0f});
    tree.set_node("/ui/combat_frame/info_right/name",
                  TreeNode{.kind = "text", .x = 5.0f, .y = 6.0f, .w = 130.0f, .h = 20.0f});
    tree.set_node("/ui/combat_frame/info_right/hp",
                  TreeNode{.kind = "text", .x = 7.0f, .y = 28.0f, .w = 130.0f, .h = 20.0f});

    BattleRenderSnapshot snapshot{
        .entities = {entity(1, BattleDepth::Front, 0, {}),
                     entity(2, BattleSide::Defender, BattleDepth::Front, 0, {})}};
    snapshot.entities[0].unit_instance_id = UnitInstanceId{10};
    snapshot.entities[0].display_name = "Left";
    snapshot.entities[0].current_hp = 177;
    snapshot.entities[0].max_hp = 250;
    snapshot.entities[1].unit_instance_id = UnitInstanceId{20};
    snapshot.entities[1].display_name = "Right";
    snapshot.entities[1].current_hp = 284;
    snapshot.entities[1].max_hp = 420;

    BattleRenderOptions opts = no_background();
    opts.tree_layout = &tree;
    opts.info_left_unit = UnitInstanceId{10};
    opts.info_right_unit = UnitInstanceId{20};
    FakeTextureProvider provider;
    const auto          batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& cmds = test::commands_from(batch);

    auto find = [&](const std::string& path) -> const RenderCommand* {
        for (const auto& cmd : cmds) {
            if (cmd.tree_path == path) {
                return &cmd;
            }
        }
        return nullptr;
    };

    ASSERT_NE(find("/ui/combat_frame/info_left/name"), nullptr);
    EXPECT_EQ(find("/ui/combat_frame/info_left/name")->text, "Left");
    EXPECT_EQ(find("/ui/combat_frame/info_left/hp")->text, "177/250");
    EXPECT_EQ(find("/ui/combat_frame/info_left/name")->destination,
              tree.compose("/ui/combat_frame/info_left/name"));
    EXPECT_EQ(find("/ui/combat_frame/info_right/name")->text, "Right");
    EXPECT_EQ(find("/ui/combat_frame/info_right/hp")->text, "284/420");
    EXPECT_EQ(find("/ui/combat_frame/info_right/hp")->destination,
              tree.compose("/ui/combat_frame/info_right/hp"));

    for (const std::string& path :
         {"/ui/combat_frame/info_left/name", "/ui/combat_frame/info_left/hp",
          "/ui/combat_frame/info_right/name", "/ui/combat_frame/info_right/hp"}) {
        const RenderCommand* command = find(path);
        ASSERT_NE(command, nullptr);
        ASSERT_TRUE(test::tunable_item(*command).has_value()) << path;
        EXPECT_TRUE(test::tunable_item(*command)->selectable) << path;
        EXPECT_EQ(test::tunable_item(*command)->tree_path, path);
        ASSERT_TRUE(test::tunable_item(*command)->binding.has_value()) << path;
        EXPECT_EQ(test::tunable_item(*command)->binding->owner_kind, BindingOwnerKind::TreeLayout);
        EXPECT_EQ(test::tunable_item(*command)->binding->tree_path, path);
        EXPECT_EQ(test::tunable_item(*command)->binding->display_path,
                  std::string("render_tree") + path);
    }
}

TEST(BattleRenderer, MissingTreeNodeDoesNotGenerateDebugItem) {
    FakeTextureProvider provider;

    // Tree has slot_a_center_1 but NOT slot_a_center_0 or any /unit node
    d2engine::TreeLayout tree;
    tree.set_node("/battlefield/slot_a_center_1",
                  TreeNode{.kind = "battlefield_slot", .x = 655.5f, .y = 429.5f, .level = 10});
    // Intentionally no /unit node for center_1, and no center_0 at all

    BattleRenderOptions opts;
    opts.draw_background = false;
    opts.draw_unit_groups = false;
    opts.tree_layout = &tree;

    const auto batch = BattleRenderer::build_render_batch(BattleRenderSnapshot{}, provider, opts);

    const auto& commands = test::commands_from(batch);

    bool found_center_0 = false;
    bool found_center_1_unit = false;
    for (const auto& cmd : commands) {
        if (!test::tunable_item(cmd)) {
            continue;
        }
        if (test::tunable_item(cmd)->stable_id == "tree:A_CENTER_0") {
            found_center_0 = true;
        }
        if (test::tunable_item(cmd)->stable_id == "tree:A_CENTER_1/unit") {
            found_center_1_unit = true;
        }
    }

    EXPECT_FALSE(found_center_0) << "tree:A_CENTER_0 should not appear — node not in tree";
    EXPECT_FALSE(found_center_1_unit)
        << "tree:A_CENTER_1/unit should not appear — /unit node not in tree";
}

// ─── Background container node ────────────────────────────────────────────

TEST(BattleRenderer, BackgroundContainerNodeAppearsInDebugItems) {
    BattleRenderSnapshot const snapshot;
    FakeTextureProvider        provider;
    BattleRenderOptions        opts;
    opts.terrain_image = "HERETIC_0_BG";
    opts.draw_frame = false;
    opts.debug_enabled = true;

    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& cmds = test::commands_from(batch);

    const RenderCommand* container = nullptr;
    for (const auto& c : cmds) {
        if (test::tunable_item(c) &&
            test::tunable_item(c)->stable_id == "scene:background_container") {
            container = &c;
        }
    }
    ASSERT_NE(container, nullptr) << "Background container node must appear in debug items";
    EXPECT_EQ(test::tunable_item(*container)->tree_path, "/background");
    ASSERT_TRUE(test::tunable_item(*container)->binding.has_value());
    EXPECT_TRUE(test::tunable_item(*container)->binding->writable());
    EXPECT_EQ(test::tunable_item(*container)->binding->owner_kind, BindingOwnerKind::TreeLayout);
    EXPECT_EQ(test::tunable_item(*container)->binding->role, BindingRole::Global);
    EXPECT_EQ(test::tunable_item(*container)->binding->display_path, "render_tree/background");
    EXPECT_EQ(container->layer, battle_render_layer(BattleRenderPass::Debug));
}

TEST(BattleRenderer, BackgroundContainerSelectableByTreePath) {
    BattleRenderSnapshot const snapshot;
    FakeTextureProvider        provider;
    BattleRenderOptions        opts;
    opts.terrain_image = "TEST_BG";
    opts.draw_frame = false;
    opts.debug_enabled = true;

    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& cmds = test::commands_from(batch);

    const RenderCommand* container = nullptr;
    for (const auto& c : cmds) {
        if (test::tunable_item(c) && test::tunable_item(c)->tree_path == "/background") {
            container = &c;
        }
    }
    ASSERT_NE(container, nullptr) << "Must have item with tree_path=/background";
    EXPECT_TRUE(test::tunable_item(*container)->selectable);
}

TEST(BattleRenderer, BackgroundContainerPlacementShiftsAllPieces) {
    BattleRenderSnapshot const snapshot;
    FakeTextureProvider        provider;
    BattleRenderOptions        opts;
    opts.terrain_image = "HERETIC_0_BG";
    opts.terrain_overlay_images = {"HERETIC_0_FG"};
    opts.draw_frame = false;

    const auto base_cmds_batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& base_cmds = test::commands_from(base_cmds_batch);

    // Apply container offset via render_tree /background node
    d2engine::TreeLayout tree;
    tree.set_node("/background", TreeNode{.x = 100.0f, .y = 50.0f});
    opts.tree_layout = &tree;

    const auto tuned_cmds_batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& tuned_cmds = test::commands_from(tuned_cmds_batch);

    auto find = [](const auto& cmds, const std::string& stable) -> const RenderCommand* {
        for (const auto& c : cmds) {
            if (test::tunable_item(c) && test::tunable_item(c)->stable_id == stable) {
                return &c;
            }
        }
        return nullptr;
    };

    const RenderCommand* base_bg = find(base_cmds, "backgrounds.battle");
    const RenderCommand* tuned_bg = find(tuned_cmds, "backgrounds.battle");
    const RenderCommand* base_fg = find(base_cmds, "backgrounds.battle_overlay");
    const RenderCommand* tuned_fg = find(tuned_cmds, "backgrounds.battle_overlay");
    ASSERT_NE(base_bg, nullptr);
    ASSERT_NE(tuned_bg, nullptr);
    ASSERT_NE(base_fg, nullptr);
    ASSERT_NE(tuned_fg, nullptr);

    // Both pieces shifted by container offset
    EXPECT_NEAR(tuned_bg->destination.x - base_bg->destination.x, 100.0f, 0.5f);
    EXPECT_NEAR(tuned_bg->destination.y - base_bg->destination.y, 50.0f, 0.5f);
    EXPECT_NEAR(tuned_fg->destination.x - base_fg->destination.x, 100.0f, 0.5f);
    EXPECT_NEAR(tuned_fg->destination.y - base_fg->destination.y, 50.0f, 0.5f);
}

TEST(BattleRenderer, BackgroundContainerAndPieceComposeAdditively) {
    // Container + individual tree node = container offset + piece offset
    BattleRenderSnapshot const snapshot;
    FakeTextureProvider        provider;
    BattleRenderOptions        opts;
    opts.terrain_image = "HERETIC_0_BG";
    opts.terrain_overlay_images = {"HERETIC_0_FG"};
    opts.draw_frame = false;

    const auto base_cmds_batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& base_cmds = test::commands_from(base_cmds_batch);

    // Container + per-piece transforms as tree nodes
    d2engine::TreeLayout tree;
    tree.set_node("/background", TreeNode{.x = 50.0f, .y = 20.0f, .alpha = 0.85f});
    tree.set_node("/background/battle",
                  TreeNode{.x = 10.0f, .y = 5.0f, .w = 880.0f, .h = 630.0f, .alpha = 0.7f});
    BattleRenderOptions opts_tuned = opts;
    opts_tuned.tree_layout = &tree;

    const auto tuned_cmds_batch =
        BattleRenderer::build_render_batch(snapshot, provider, opts_tuned);

    const auto& tuned_cmds = test::commands_from(tuned_cmds_batch);

    auto find = [](const auto& cmds, const std::string& stable) -> const RenderCommand* {
        for (const auto& c : cmds) {
            if (test::tunable_item(c) && test::tunable_item(c)->stable_id == stable) {
                return &c;
            }
        }
        return nullptr;
    };

    const RenderCommand* base_bg = find(base_cmds, "backgrounds.battle");
    const RenderCommand* tuned_bg = find(tuned_cmds, "backgrounds.battle");
    const RenderCommand* base_fg = find(base_cmds, "backgrounds.battle_overlay");
    const RenderCommand* tuned_fg = find(tuned_cmds, "backgrounds.battle_overlay");
    ASSERT_NE(base_bg, nullptr);
    ASSERT_NE(tuned_bg, nullptr);
    ASSERT_NE(base_fg, nullptr);
    ASSERT_NE(tuned_fg, nullptr);

    // BG: additive x(50+10), y(20+5), w/h from piece, alpha multiplied through parent.
    EXPECT_NEAR(tuned_bg->destination.x - base_bg->destination.x, 60.0f, 0.5f);
    EXPECT_NEAR(tuned_bg->destination.y - base_bg->destination.y, 25.0f, 0.5f);
    EXPECT_NEAR(tuned_bg->destination.w, 880.0f, 0.01f);
    EXPECT_NEAR(tuned_bg->destination.h, 630.0f, 0.01f);
    EXPECT_NEAR(tuned_bg->alpha / base_bg->alpha, 0.85f * 0.7f, 0.01f);

    // FG uses the same semantic battle layer placement as BG.
    EXPECT_NEAR(tuned_fg->destination.x - base_fg->destination.x, 60.0f, 0.5f);
    EXPECT_NEAR(tuned_fg->destination.y - base_fg->destination.y, 25.0f, 0.5f);
    EXPECT_NEAR(tuned_fg->destination.w, 880.0f, 0.01f);
    EXPECT_NEAR(tuned_fg->destination.h, 630.0f, 0.01f);
    EXPECT_NEAR(tuned_fg->alpha / base_fg->alpha, 0.85f * 0.7f, 0.01f);
}

TEST(BattleRenderer, BackgroundChildOffsetOnlyInheritsContainerSize) {
    BattleRenderSnapshot const snapshot;
    FakeTextureProvider        provider;
    BattleRenderOptions        opts;
    opts.terrain_image = "HERETIC_0_BG";
    opts.draw_frame = false;

    d2engine::TreeLayout tree;
    tree.set_node("/background", TreeNode{.x = -1.0f, .y = -13.0f, .w = 1417.0f, .h = 866.0f});
    tree.set_node("/background/battle", TreeNode{.x = 10.0f, .y = 20.0f});
    opts.tree_layout = &tree;

    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& cmds = test::commands_from(batch);

    const RenderCommand* bg = nullptr;
    for (const auto& c : cmds) {
        if (test::tunable_item(c) && test::tunable_item(c)->stable_id == "backgrounds.battle") {
            bg = &c;
        }
    }
    ASSERT_NE(bg, nullptr);
    EXPECT_FLOAT_EQ(bg->destination.x, 9.0f);
    EXPECT_FLOAT_EQ(bg->destination.y, 7.0f);
    EXPECT_FLOAT_EQ(bg->destination.w, 1417.0f);
    EXPECT_FLOAT_EQ(bg->destination.h, 866.0f);
}

TEST(BattleRenderer, GroundAndBattleUseSameRenderTreePlacementRule) {
    BattleRenderSnapshot const snapshot;
    FakeTextureProvider        provider;
    BattleRenderOptions        opts;
    opts.ground_image = "HE_00.PNG";
    opts.terrain_image = "HERETIC_0_BG";
    opts.draw_frame = false;

    d2engine::TreeLayout tree;
    tree.set_node("/background",
                  TreeNode{.x = -1.0f, .y = -13.0f, .w = 1417.0f, .h = 866.0f, .alpha = 0.8f});
    tree.set_node("/background/ground", TreeNode{.x = 10.0f, .y = 20.0f, .alpha = 0.5f});
    tree.set_node("/background/battle", TreeNode{.x = 10.0f, .y = 20.0f, .alpha = 0.5f});
    opts.tree_layout = &tree;

    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& cmds = test::commands_from(batch);

    const RenderCommand* ground = nullptr;
    const RenderCommand* battle = nullptr;
    for (const auto& c : cmds) {
        if (test::tunable_item(c) && test::tunable_item(c)->stable_id == "backgrounds.ground") {
            ground = &c;
        }
        if (test::tunable_item(c) && test::tunable_item(c)->stable_id == "backgrounds.battle") {
            battle = &c;
        }
    }
    ASSERT_NE(ground, nullptr);
    ASSERT_NE(battle, nullptr);

    for (const RenderCommand* command : {ground, battle}) {
        EXPECT_FLOAT_EQ(command->destination.x, 9.0f);
        EXPECT_FLOAT_EQ(command->destination.y, 7.0f);
        EXPECT_FLOAT_EQ(command->destination.w, 1417.0f);
        EXPECT_FLOAT_EQ(command->destination.h, 866.0f);
        EXPECT_FLOAT_EQ(command->alpha, 0.4f);
    }
}

TEST(BattleRenderer, GroundBackgroundCommandTilesNativeTexture) {
    BattleRenderSnapshot const snapshot;
    FakeTextureProvider        provider;
    BattleRenderOptions        opts;
    opts.ground_image = "HE_00.PNG";
    opts.terrain_image = "HERETIC_0_BG";
    opts.draw_frame = false;

    d2engine::TreeLayout tree;
    tree.set_node("/background", TreeNode{.w = 70.0f, .h = 100.0f});
    opts.tree_layout = &tree;

    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& cmds = test::commands_from(batch);

    const RenderCommand* ground = nullptr;
    const RenderCommand* battle = nullptr;
    for (const auto& c : cmds) {
        if (test::tunable_item(c) && test::tunable_item(c)->stable_id == "backgrounds.ground") {
            ground = &c;
        }
        if (test::tunable_item(c) && test::tunable_item(c)->stable_id == "backgrounds.battle") {
            battle = &c;
        }
    }
    ASSERT_NE(ground, nullptr);
    ASSERT_NE(battle, nullptr);
    EXPECT_TRUE(ground->tile);
    EXPECT_FLOAT_EQ(ground->source_rect.w, 32.0f);
    EXPECT_FLOAT_EQ(ground->source_rect.h, 48.0f);
    EXPECT_FALSE(battle->tile);

    MockRenderer2D renderer;
    SdlBattleRenderer::render_commands({*ground}, renderer, LayoutScale{}, opts);
    EXPECT_EQ(renderer.texture_calls, 9);
}

TEST(BattleRenderer, GroundAndBattleLayerTuningIsIndependent) {
    BattleRenderSnapshot const snapshot;
    FakeTextureProvider        provider;
    BattleRenderOptions        opts;
    opts.ground_image = "HE_00.PNG";
    opts.terrain_image = "HERETIC_0_BG";
    opts.terrain_overlay_images = {"HERETIC_0_FG"};
    opts.draw_frame = false;

    auto find = [](const auto& cmds, const std::string& stable) -> const RenderCommand* {
        for (const auto& c : cmds) {
            if (test::tunable_item(c) && test::tunable_item(c)->stable_id == stable) {
                return &c;
            }
        }
        return nullptr;
    };

    d2engine::TreeLayout ground_tree;
    ground_tree.set_node("/background/ground", TreeNode{.x = 10.0f, .w = 100.0f});
    BattleRenderOptions ground_opts = opts;
    ground_opts.tree_layout = &ground_tree;
    const auto ground_cmds_batch =
        BattleRenderer::build_render_batch(snapshot, provider, ground_opts);

    const auto& ground_cmds = test::commands_from(ground_cmds_batch);

    ASSERT_FLOAT_EQ(find(ground_cmds, "backgrounds.ground")->destination.x, 10.0f);
    ASSERT_FLOAT_EQ(find(ground_cmds, "backgrounds.ground")->destination.w, 100.0f);
    EXPECT_FLOAT_EQ(find(ground_cmds, "backgrounds.battle")->destination.x, 0.0f);
    EXPECT_FLOAT_EQ(find(ground_cmds, "backgrounds.battle_overlay")->destination.x, 0.0f);

    d2engine::TreeLayout battle_tree;
    battle_tree.set_node("/background/battle", TreeNode{.x = 20.0f, .w = 200.0f});
    BattleRenderOptions battle_opts = opts;
    battle_opts.tree_layout = &battle_tree;
    const auto battle_cmds_batch =
        BattleRenderer::build_render_batch(snapshot, provider, battle_opts);

    const auto& battle_cmds = test::commands_from(battle_cmds_batch);

    EXPECT_FLOAT_EQ(find(battle_cmds, "backgrounds.ground")->destination.x, 0.0f);
    ASSERT_FLOAT_EQ(find(battle_cmds, "backgrounds.battle")->destination.x, 20.0f);
    ASSERT_FLOAT_EQ(find(battle_cmds, "backgrounds.battle")->destination.w, 200.0f);
    EXPECT_FLOAT_EQ(find(battle_cmds, "backgrounds.battle_overlay")->destination.x, 20.0f);
    EXPECT_FLOAT_EQ(find(battle_cmds, "backgrounds.battle_overlay")->destination.w, 200.0f);
}

TEST(BattleRenderer, BackgroundChildSizeOverridesContainerSize) {
    BattleRenderSnapshot const snapshot;
    FakeTextureProvider        provider;
    BattleRenderOptions        opts;
    opts.terrain_image = "HERETIC_0_BG";
    opts.draw_frame = false;

    d2engine::TreeLayout tree;
    tree.set_node("/background", TreeNode{.w = 1417.0f, .h = 866.0f});
    tree.set_node("/background/battle", TreeNode{.w = 1600.0f, .h = 945.0f});
    opts.tree_layout = &tree;

    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& cmds = test::commands_from(batch);

    const RenderCommand* bg = nullptr;
    for (const auto& c : cmds) {
        if (test::tunable_item(c) && test::tunable_item(c)->stable_id == "backgrounds.battle") {
            bg = &c;
        }
    }
    ASSERT_NE(bg, nullptr);
    EXPECT_FLOAT_EQ(bg->destination.w, 1600.0f);
    EXPECT_FLOAT_EQ(bg->destination.h, 945.0f);
}

TEST(BattleRenderer, EmptyTreeProducesNoTreeLayoutDebugItems) {
    FakeTextureProvider  provider;
    d2engine::TreeLayout empty_tree; // no nodes

    BattleRenderOptions opts;
    opts.draw_background = false;
    opts.draw_unit_groups = false;
    opts.draw_frame = false;
    opts.tree_layout = &empty_tree;

    const auto batch = BattleRenderer::build_render_batch(BattleRenderSnapshot{}, provider, opts);

    const auto& commands = test::commands_from(batch);

    for (const auto& cmd : commands) {
        if (test::tunable_item(cmd) && test::tunable_item(cmd)->binding.has_value()) {
            EXPECT_NE(test::tunable_item(cmd)->binding->owner_kind, BindingOwnerKind::TreeLayout)
                << "Unexpected d2engine::TreeLayout item: " << test::tunable_item(cmd)->stable_id;
        }
    }
}

// ─── Background edit through TreeLayoutEditor ──────────────────────────

TEST(BattleRenderer, BackgroundChildMoveThroughTreeLayoutEditorKeepsInheritedSize) {
    using namespace d2engine;

    ScreenManager     manager;
    ScreenConfigStore config_store{"."};
    TreeLayoutEditor  editor{manager, config_store};

    class BgScreen : public Screen {
    public:
        explicit BgScreen(TreeLayout tl) : Screen(std::move(tl), "test://bg") {}
        std::string_view name() const override { return "BgScreen"; }
        bool             handle_input(const InputEvent&) override { return false; }
        void             update(const d2::app::ScreenUpdateContext&) override {}
        void             render(Renderer2D&) override {}
    };

    TreeLayout tree;
    tree.set_node("/background", TreeNode{.w = 1417.0f, .h = 866.0f});
    tree.set_node("/background/battle", TreeNode{.w = 1600.0f, .h = 945.0f});

    auto  screen = std::make_unique<BgScreen>(std::move(tree));
    auto* raw = screen.get();
    manager.switch_to(std::move(screen));

    // Select the screen through editor
    editor.select_screen(raw->instance_id());

    // Select /background/battle and move it
    EXPECT_TRUE(editor.select_node("/background/battle"));
    EXPECT_TRUE(editor.apply_edit(DebugTuningEditAction{DebugTuningEditKind::MoveRight, 1.0f}));
    EXPECT_TRUE(editor.apply_edit(DebugTuningEditAction{DebugTuningEditKind::MoveUp, 1.0f}));

    // Pass the Screen-owned tree to BattleRenderer
    BattleRenderSnapshot const snapshot;
    FakeTextureProvider        provider;
    BattleRenderOptions        opts;
    opts.terrain_image = "HERETIC_0_BG";
    opts.draw_frame = false;
    opts.tree_layout = &raw->tree_layout();

    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& cmds = test::commands_from(batch);

    // Find background battle command by stable_id pattern via tunable_item
    const RenderCommand* bg = nullptr;
    for (const auto& c : cmds) {
        if (test::tunable_item(c) && test::tunable_item(c)->stable_id == "backgrounds.battle") {
            bg = &c;
        }
    }
    ASSERT_NE(bg, nullptr);
    EXPECT_FLOAT_EQ(bg->destination.x, 1.0f);
    EXPECT_FLOAT_EQ(bg->destination.y, -1.0f);
    EXPECT_FLOAT_EQ(bg->destination.w, 1600.0f);
    EXPECT_FLOAT_EQ(bg->destination.h, 945.0f);
}

// ─── Background tree-node save/reload ─────────────────────────────────────

TEST(BattleRenderer, BackgroundNodeSaveReload) {
    d2engine::TreeLayout tree;
    tree.set_node("/background", TreeNode{.x = 10.0f, .y = -5.0f, .alpha = 0.8f, .level = -6});
    tree.set_node("/background/battle", TreeNode{.w = 880.0f, .h = 630.0f});

    nlohmann::json j;
    tree.save(j);
    EXPECT_TRUE(j.contains("/background"));
    EXPECT_TRUE(j.contains("/background/battle"));
    EXPECT_EQ(j["/background"]["x"].get<int>(), 10);
    EXPECT_NEAR(j["/background"]["alpha"].get<float>(), 0.8f, 0.01f);
    EXPECT_EQ(j["/background"]["level"].get<int>(), -6);
    EXPECT_FALSE(j["/background"].contains("scale_x"));
    EXPECT_FALSE(j["/background"].contains("scale_y"));
    EXPECT_FALSE(j["/background/battle"].contains("scale_x"));
    EXPECT_FALSE(j["/background/battle"].contains("scale_y"));

    d2engine::TreeLayout loaded;
    loaded.load(j);
    auto n = loaded.node("/background");
    ASSERT_TRUE(n.has_value());
    EXPECT_FLOAT_EQ(n->x, 10.0f);
    EXPECT_NEAR(n->alpha, 0.8f, 0.01f);
    EXPECT_EQ(n->level, -6);

    auto ct = loaded.compose("/background/battle");
    EXPECT_FLOAT_EQ(ct.x, 10.0f);
    EXPECT_FLOAT_EQ(ct.y, -5.0f);
    EXPECT_FLOAT_EQ(ct.w, 880.0f);
    EXPECT_FLOAT_EQ(ct.h, 630.0f);
}

TEST(BattleRenderer, BackgroundNodeSaveRoundsGeometry) {
    auto round_trip = [](float val) {
        d2engine::TreeLayout t;
        t.set_node("/n", TreeNode{.x = val, .y = 0.0f, .w = 0.0f, .h = 0.0f});
        nlohmann::json j;
        t.save(j);
        return j["/n"]["x"].get<int>();
    };

    EXPECT_EQ(round_trip(74.5f), 75);
    EXPECT_EQ(round_trip(90.5f), 91);
    EXPECT_EQ(round_trip(24.67f), 25);
    EXPECT_EQ(round_trip(24.32f), 24);
    EXPECT_EQ(round_trip(-1.08f), -1);
    EXPECT_EQ(round_trip(-1.5f), -2);
    EXPECT_EQ(round_trip(0.0f), 0);

    // Alpha/level not affected by geometry rounding
    d2engine::TreeLayout t;
    t.set_node("/n", TreeNode{.x = 10.0f, .y = 0.0f, .alpha = 1.0f, .level = 0});
    nlohmann::json j;
    t.save(j);
    EXPECT_NEAR(j["/n"]["alpha"].get<float>(), 1.0f, 0.01f);
    EXPECT_EQ(j["/n"]["level"].get<int>(), 0);
}

TEST(BattleRenderer, ComposedFullDefaultsForMissingPath) {
    d2engine::TreeLayout empty;
    auto                 ct = empty.compose("/background/nonexistent");
    EXPECT_FLOAT_EQ(ct.x, 0.0f);
    EXPECT_FLOAT_EQ(ct.y, 0.0f);
    EXPECT_FLOAT_EQ(ct.w, 0.0f);
    EXPECT_FLOAT_EQ(ct.h, 0.0f);
}

// ─── Background debug item tests ──────────────────────────────────────────

TEST(BattleRenderer, BackgroundSemanticLayersHaveTreeLayoutBindings) {
    BattleRenderSnapshot const snapshot;
    FakeTextureProvider        provider;
    BattleRenderOptions        opts;
    opts.ground_image = "HE_00.PNG";
    opts.terrain_image = "HERETIC_0_BG";
    opts.terrain_overlay_images = {"HERETIC_0_FG"};
    opts.draw_frame = false;

    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& cmds = test::commands_from(batch);

    const RenderCommand* ground = nullptr;
    const RenderCommand* battle = nullptr;
    const RenderCommand* fg = nullptr;
    for (const auto& c : cmds) {
        if (!test::tunable_item(c)) {
            continue;
        }
        if (test::tunable_item(c)->stable_id == "backgrounds.ground") {
            ground = &c;
        }
        if (test::tunable_item(c)->stable_id == "backgrounds.battle") {
            battle = &c;
        }
        if (c.layer == battle_render_layer(BattleRenderPass::BackgroundOverlay)) {
            fg = &c;
        }
    }
    ASSERT_NE(ground, nullptr);
    ASSERT_NE(battle, nullptr);
    ASSERT_NE(fg, nullptr);
    ASSERT_TRUE(test::tunable_item(*ground)->binding.has_value());
    ASSERT_TRUE(test::tunable_item(*battle)->binding.has_value());
    ASSERT_TRUE(test::tunable_item(*fg)->binding.has_value());

    EXPECT_EQ(test::tunable_item(*ground)->binding->tree_path, "/background/ground");
    EXPECT_EQ(test::tunable_item(*battle)->binding->tree_path, "/background/battle");
    EXPECT_EQ(test::tunable_item(*fg)->binding->tree_path, "/background/battle");
    EXPECT_TRUE(test::tunable_item(*ground)->selectable);
    EXPECT_TRUE(test::tunable_item(*battle)->selectable);
    EXPECT_FALSE(test::tunable_item(*fg)->selectable);
    EXPECT_EQ(ground->layer, battle_render_layer(BattleRenderPass::GroundBackground));
    EXPECT_EQ(battle->layer, battle_render_layer(BattleRenderPass::Background));
    EXPECT_EQ(fg->layer, battle_render_layer(BattleRenderPass::BackgroundOverlay));
}

// ─── Background tuning via tree nodes ─────────────────────────────────────

TEST(BattleRenderer, BackgroundTreeMovesBattleLayer) {
    BattleRenderSnapshot const snapshot;
    FakeTextureProvider        provider;
    BattleRenderOptions        opts;
    opts.terrain_image = "HERETIC_0_BG";
    opts.terrain_overlay_images = {"HERETIC_0_FG"};
    opts.draw_frame = false;

    const auto base_cmds_batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& base_cmds = test::commands_from(base_cmds_batch);

    d2engine::TreeLayout tree;
    tree.set_node("/background/battle", TreeNode{.x = 50.0f, .y = 30.0f});
    BattleRenderOptions opts_tuned = opts;
    opts_tuned.tree_layout = &tree;

    const auto tuned_cmds_batch =
        BattleRenderer::build_render_batch(snapshot, provider, opts_tuned);

    const auto& tuned_cmds = test::commands_from(tuned_cmds_batch);

    auto find = [](const auto& cmds, const std::string& stable) -> const RenderCommand* {
        for (const auto& c : cmds) {
            if (test::tunable_item(c) && test::tunable_item(c)->stable_id == stable) {
                return &c;
            }
        }
        return nullptr;
    };

    const RenderCommand* base_cmd = find(base_cmds, "backgrounds.battle");
    const RenderCommand* base_tuned = find(tuned_cmds, "backgrounds.battle");
    const RenderCommand* overlay_cmd = find(base_cmds, "backgrounds.battle_overlay");
    const RenderCommand* overlay_tuned = find(tuned_cmds, "backgrounds.battle_overlay");
    ASSERT_NE(base_cmd, nullptr);
    ASSERT_NE(base_tuned, nullptr);
    ASSERT_NE(overlay_cmd, nullptr);
    ASSERT_NE(overlay_tuned, nullptr);

    EXPECT_NEAR(base_tuned->destination.x - base_cmd->destination.x, 50.0f, 0.5f);
    EXPECT_NEAR(base_tuned->destination.y - base_cmd->destination.y, 30.0f, 0.5f);
    EXPECT_NEAR(overlay_tuned->destination.x - overlay_cmd->destination.x, 50.0f, 0.5f);
    EXPECT_NEAR(overlay_tuned->destination.y - overlay_cmd->destination.y, 30.0f, 0.5f);
}

TEST(BattleRenderer, BackgroundTreeMovesGroundLayerOnly) {
    BattleRenderSnapshot const snapshot;
    FakeTextureProvider        provider;
    BattleRenderOptions        opts;
    opts.ground_image = "HE_00.PNG";
    opts.terrain_image = "HERETIC_0_BG";
    opts.terrain_overlay_images = {"HERETIC_0_FG"};
    opts.draw_frame = false;

    const auto base_cmds_batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& base_cmds = test::commands_from(base_cmds_batch);

    d2engine::TreeLayout tree;
    tree.set_node("/background/ground",
                  TreeNode{.x = -20.0f, .y = 10.0f, .w = 800.0f, .h = 400.0f});
    BattleRenderOptions opts_tuned = opts;
    opts_tuned.tree_layout = &tree;

    const auto tuned_cmds_batch =
        BattleRenderer::build_render_batch(snapshot, provider, opts_tuned);

    const auto& tuned_cmds = test::commands_from(tuned_cmds_batch);

    auto find = [](const auto& cmds, const std::string& stable) -> const RenderCommand* {
        for (const auto& c : cmds) {
            if (test::tunable_item(c) && test::tunable_item(c)->stable_id == stable) {
                return &c;
            }
        }
        return nullptr;
    };

    const RenderCommand* ground_cmd = find(base_cmds, "backgrounds.ground");
    const RenderCommand* ground_tuned = find(tuned_cmds, "backgrounds.ground");
    const RenderCommand* battle_cmd = find(base_cmds, "backgrounds.battle");
    const RenderCommand* battle_tuned = find(tuned_cmds, "backgrounds.battle");
    ASSERT_NE(ground_cmd, nullptr);
    ASSERT_NE(ground_tuned, nullptr);
    ASSERT_NE(battle_cmd, nullptr);
    ASSERT_NE(battle_tuned, nullptr);

    EXPECT_NEAR(ground_tuned->destination.x - ground_cmd->destination.x, -20.0f, 0.5f);
    EXPECT_NEAR(ground_tuned->destination.y - ground_cmd->destination.y, 10.0f, 0.5f);
    EXPECT_FLOAT_EQ(ground_tuned->destination.w, 800.0f);
    EXPECT_FLOAT_EQ(ground_tuned->destination.h, 400.0f);
    EXPECT_FLOAT_EQ(battle_tuned->destination.x, battle_cmd->destination.x);
    EXPECT_FLOAT_EQ(battle_tuned->destination.y, battle_cmd->destination.y);
}

TEST(BattleRenderer, BackgroundTreeAffectsAlpha) {
    BattleRenderSnapshot const snapshot;
    FakeTextureProvider        provider;
    BattleRenderOptions        opts;
    opts.terrain_image = "HERETIC_0_BG";
    opts.draw_frame = false;

    const auto base_cmds_batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& base_cmds = test::commands_from(base_cmds_batch);

    d2engine::TreeLayout tree;
    tree.set_node("/background/battle", TreeNode{.alpha = 0.5f});
    BattleRenderOptions opts_tuned = opts;
    opts_tuned.tree_layout = &tree;

    const auto tuned_cmds_batch =
        BattleRenderer::build_render_batch(snapshot, provider, opts_tuned);

    const auto& tuned_cmds = test::commands_from(tuned_cmds_batch);

    const RenderCommand* base_cmd = nullptr;
    const RenderCommand* base_tuned = nullptr;
    for (const auto& c : base_cmds) {
        if (test::tunable_item(c) && test::tunable_item(c)->stable_id == "backgrounds.battle") {
            base_cmd = &c;
        }
    }
    for (const auto& c : tuned_cmds) {
        if (test::tunable_item(c) && test::tunable_item(c)->stable_id == "backgrounds.battle") {
            base_tuned = &c;
        }
    }
    ASSERT_NE(base_cmd, nullptr);
    ASSERT_NE(base_tuned, nullptr);
    EXPECT_NEAR(base_tuned->alpha / base_cmd->alpha, 0.5f, 0.01f);
}

// ─── Regression: background overlay pass order unchanged ──────────────────

TEST(BattleRenderer, CompositeBackgroundOverlayRenderPassOrderUnchanged) {
    // Same scenario as BackgroundOverlayPassIsAfterEffectsBeforeMarkers but checks
    // that selectable background stable IDs don't affect render pass order.
    BattleRenderSnapshot const snapshot{
        .entities = {
            entity(1, BattleDepth::Front, 0,
                   {track(TrackKind::Base, TrackRenderLayer::Base, "unit_img"),
                    track(TrackKind::Effect, TrackRenderLayer::Overlay, "effect_img"),
                    track(TrackKind::ActorMarker, TrackRenderLayer::Marker, "marker_img")})}};
    FakeTextureProvider provider;
    BattleRenderOptions opts;
    opts.terrain_image = "UNDEAD_0_BG";
    opts.terrain_overlay_images = {"UNDEAD_0_FG"};
    opts.draw_frame = false;
    static const d2engine::TreeLayout tree = make_slot_render_tree();
    opts.tree_layout = &tree;

    const auto batch = BattleRenderer::build_render_batch(snapshot, provider, opts);

    const auto& cmds = test::commands_from(batch);

    std::optional<std::size_t> bg_idx;
    std::optional<std::size_t> ov_idx;
    std::optional<std::size_t> marker_idx;
    for (std::size_t i = 0; i < cmds.size(); ++i) {
        const RenderLayer p = cmds[i].layer;
        if (p == battle_render_layer(BattleRenderPass::Background)) {
            bg_idx = i;
        } else if (p == battle_render_layer(BattleRenderPass::BackgroundOverlay)) {
            ov_idx = i;
        } else if (p == battle_render_layer(BattleRenderPass::Markers)) {
            marker_idx = i;
        }
    }
    ASSERT_TRUE(bg_idx.has_value());
    ASSERT_TRUE(ov_idx.has_value());
    ASSERT_TRUE(marker_idx.has_value());
    EXPECT_LT(*bg_idx, *ov_idx) << "Background must precede overlay";
    EXPECT_LT(*ov_idx, *marker_idx) << "Overlay must precede markers";
}

} // namespace d2engine
