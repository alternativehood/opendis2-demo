#include <gtest/gtest.h>

#include <fstream>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string read_source(const char* relative_path) {
    std::ifstream const in(std::string{OPENDIS2_SOURCE_DIR} + "/" + relative_path);
    std::ostringstream  out;
    out << in.rdbuf();
    return out.str();
}

void expect_absent(const std::string& text, const std::vector<const char*>& needles) {
    for (const char* needle : needles) {
        EXPECT_EQ(text.find(needle), std::string::npos) << needle;
    }
}

void expect_source_exists(const char* path) {
    EXPECT_FALSE(read_source(path).empty()) << path;
}

std::string source_line_containing(const std::string& text, const char* needle) {
    const auto pos = text.find(needle);
    if (pos == std::string::npos) {
        return {};
    }
    const auto begin = text.rfind('\n', pos);
    const auto end = text.find('\n', pos);
    return text.substr(begin == std::string::npos ? 0 : begin + 1,
                       end == std::string::npos ? std::string::npos : end - begin - 1);
}

std::vector<std::filesystem::path> source_files_under(const char* relative_dir) {
    std::vector<std::filesystem::path> result;
    const auto root = std::filesystem::path{OPENDIS2_SOURCE_DIR} / relative_dir;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const auto ext = entry.path().extension().string();
        if (ext == ".cpp" || ext == ".hpp") {
            result.push_back(entry.path());
        }
    }
    return result;
}

std::string read_absolute_source(const std::filesystem::path& path) {
    std::ifstream const in(path);
    std::ostringstream  out;
    out << in.rdbuf();
    return out.str();
}

} // namespace

TEST(BattleArchitectureBoundaries, CoreBattleHeadersStayPlatformFree) {
    for (const char* path : {
             "src/d2engine/battle_view/battle_ids.hpp",
             "src/d2engine/battle_view/battle_scene.hpp",
             "src/d2engine/battle_view/battle_unit.hpp",
             "src/d2engine/battle_view/battle_visual_event.hpp",
             "src/d2engine/battle_view/visual_command.hpp",
             "src/d2engine/battle_view/visual_track.hpp",
             "src/d2engine/battle_view/command_timeline.hpp",
             "src/d2engine/battle_view/battle_animation_engine.hpp",
             "src/d2engine/battle_view/battle_animation_scripts.hpp",
         }) {
        expect_absent(read_source(path), {"#include <SDL", "../app/", "\"app/"});
    }
}

TEST(BattleArchitectureBoundaries, ViewerHelpersStayOutsideBattleCore) {
    expect_source_exists("src/d2engine/app/battle_viewer_controller.hpp");
    expect_source_exists("src/d2engine/app/battle_viewer_renderer.hpp");
    expect_source_exists("src/d2engine/app/debug_overlay_renderer.hpp");
    expect_source_exists("src/d2engine/app/battle_tuning_controller.hpp");

    for (const char* path : {
             "src/d2engine/battle_view/battle_renderer.cpp",
             "src/d2engine/battle_view/battle_renderer.hpp",
             "src/d2engine/battle_view/battle_scene.cpp",
             "src/d2engine/battle_view/battle_scene.hpp",
             "src/d2engine/battle_view/battle_presenter.cpp",
             "src/d2engine/battle_view/battle_presenter.hpp",
         }) {
        expect_absent(read_source(path), {"battle_viewer_controller", "battle_viewer_renderer",
                                          "debug_overlay_renderer", "battle_tuning_controller",
                                          "battle_bootstrap_runner"});
    }
}

TEST(BattleArchitectureBoundaries, ApplicationDelegatesViewerRuntimeDetails) {
    const std::string application = read_source("src/d2engine/app/application.cpp");
    // Application must NOT contain battle-specific viewer details — they belong to BattleScreen
    expect_absent(application, {"BattleViewerController::map_key", "BattleViewerRenderer::render",
                                "DebugOverlayRenderer::draw", "show_debug_overlay"});
    expect_absent(application,
                  {"Application::handle_key", "SDL_RenderDebugText", "BattleRenderOptions{"});
    // These live in BattleScreen instead
    const std::string battle_screen = read_source("src/d2engine/app/battle_screen.cpp");
    EXPECT_NE(battle_screen.find("BattleScreenInputHandler::handle("), std::string::npos);
    EXPECT_NE(battle_screen.find("BattleViewerRenderer::render"), std::string::npos);
    EXPECT_NE(battle_screen.find("DebugOverlayRenderer::draw"), std::string::npos);
    EXPECT_NE(battle_screen.find("show_debug_overlay"), std::string::npos);
}

TEST(BattleArchitectureBoundaries, DebugTuningEnabledShowsBattleScreenOverlay) {
    const std::string battle_input = read_source("src/d2engine/app/battle_screen_input.cpp");
    const std::string battle_screen = read_source("src/d2engine/app/battle_screen.cpp");

    // ToggleTuning is decoded in battle_screen_input.cpp (Shift+D)
    EXPECT_NE(battle_input.find("Key::D && has_modifier(key->modifiers, KeyModifier::Shift)"),
              std::string::npos);
    EXPECT_NE(battle_input.find("Key::D && has_modifier(key->modifiers, KeyModifier::Ctrl)"),
              std::string::npos);

    // Tuning toggle applied in battle_screen.cpp
    EXPECT_NE(battle_screen.find("tuning_.toggle_enabled()"), std::string::npos);

    // Debug HUD toggle within tuning gate
    EXPECT_NE(battle_screen.find("debug_hud_visible_ = !debug_hud_visible_"), std::string::npos);

    const std::string gate = source_line_containing(battle_screen, "show_debug_overlay");
    EXPECT_NE(gate.find("tuning_.enabled()"), std::string::npos);
    EXPECT_NE(gate.find("debug_hud_visible_"), std::string::npos);
    EXPECT_EQ(gate.find("draw_debug_slot_anchors_"), std::string::npos);
    EXPECT_EQ(gate.find("debug_overlay_enabled_"), std::string::npos);

    EXPECT_NE(battle_screen.find("draw_debug_slot_anchors_"), std::string::npos);
    EXPECT_NE(battle_screen.find("DebugOverlayRenderer::draw"), std::string::npos);
}

TEST(BattleArchitectureBoundaries, BattleScreenHandleInputHasNoDirectKeyMapping) {
    const std::string battle_screen = read_source("src/d2engine/app/battle_screen.cpp");
    const std::string handle_fn =
        source_line_containing(battle_screen, "BattleScreenInputHandler::handle");
    EXPECT_NE(handle_fn.find("BattleScreenInputHandler::handle"), std::string::npos);

    // BattleScreen::handle_input must NOT contain direct key interpretation
    expect_absent(battle_screen, {"key_to_edit_action"});

    // Must delegate to BattleScreenInputHandler
    EXPECT_NE(battle_screen.find("BattleScreenInputHandler::handle(event, context)"),
              std::string::npos);
}

TEST(BattleArchitectureBoundaries, ApplicationHandleApplicationInputHasNoBattleMapping) {
    const std::string application_cpp = read_source("src/d2engine/app/application.cpp");
    const std::string application_hpp = read_source("src/d2engine/app/application.hpp");
    const std::string combined = application_cpp + application_hpp;

    // Application must NOT contain handle_application_input — it was removed;
    // unhandled InputEvent is simply discarded.
    expect_absent(combined, {"handle_application_input", "Application::handle_"});

    // No direct key interpretation in Application (except universal Ctrl+Shift+D debug toggle)
    // Key::D is allowed for the universal Debug UI toggle, but must be paired with Ctrl+Shift
    expect_absent(combined, {"Key::S", "ToggleTuning", "toggle_enabled", "BattleScreenAction",
                             "AdventureUiAction"});
}

TEST(BattleArchitectureBoundaries, UpdatePathDoesNotPrepareStagedTextures) {
    const std::string application = read_source("src/d2engine/app/application.cpp");
    // process_staged_texture_prepare blocked main thread 90-120ms/frame causing 1 FPS
    expect_absent(application, {"process_staged_texture_prepare", "staged_cpu_prepare",
                                "staged_prepare_index_"});
    // update() must not decode sprites: decode happens in warm_up_battle_startup before autoplay
    const auto update_start = application.find("void Application::update(");
    const auto update_end = application.find("void Application::", update_start + 1);
    ASSERT_NE(update_start, std::string::npos);
    ASSERT_NE(update_end, std::string::npos);
    const std::string update_body = application.substr(update_start, update_end - update_start);
    EXPECT_EQ(update_body.find("decode_sprite"), std::string::npos)
        << "Application::update() must not call decode_sprite";
    EXPECT_EQ(update_body.find("TexturePrepareService"), std::string::npos)
        << "Application::update() must not construct TexturePrepareService";
}

TEST(BattleArchitectureBoundaries, BattleScreenHasNoPresentationOrBlockingPreloadAuthority) {
    const std::string battle_screen = read_source("src/d2engine/app/battle_screen.cpp");
    const std::string battle_header = read_source("src/d2engine/app/battle_screen.hpp");
    const std::string combined = battle_screen + battle_header;

    expect_absent(combined,
                  {"SdlFramePresenter", "SDL_RenderPresent", "AdventureScreen", "ScreenManager",
                   "preload_complete", ".wait()", "std::async", "std::thread"});
    EXPECT_NE(combined.find("BattleStartupSession"), std::string::npos);
    EXPECT_NE(combined.find("begin_incremental_preload"), std::string::npos);
}

TEST(BattleArchitectureBoundaries, ScreenManagerUsesPoliciesInsteadOfConcreteScreens) {
    const std::string manager = read_source("src/d2engine/app/screen_manager.cpp");
    const std::string manager_header = read_source("src/d2engine/app/screen_manager.hpp");
    expect_absent(manager, {"AdventureScreen", "BattleScreen", "StackInfoScreen", "name()"});
    EXPECT_NE(manager.find("stack_policy()"), std::string::npos);
    EXPECT_NE(manager_header.find("std::deque<"), std::string::npos);
}

TEST(BattleArchitectureBoundaries, AnimationEngineAvoidsViewerAssetSources) {
    for (const char* path : {
             "src/d2engine/battle_view/battle_animation_engine.cpp",
             "src/d2engine/battle_view/battle_animation_engine.hpp",
             "src/d2engine/battle_view/battle_animation_scripts.cpp",
             "src/d2engine/battle_view/battle_animation_scripts.hpp",
         }) {
        expect_absent(read_source(path),
                      {"raw_resource_loader", "game_texture_cache", "raw_ff_animation_catalog"});
    }
}

TEST(BattleArchitectureBoundaries, BattleAdaptersLiveOutsideCoreBattleView) {
    expect_source_exists("src/d2engine/battle_view/battle_render_command_builder.hpp");
    expect_source_exists("src/d2engine/battle_view/battle_render_command_builder.cpp");
    expect_source_exists("src/d2engine/battle_adapters/raw_animation_role_resolver_factory.hpp");
    expect_source_exists("src/d2engine/battle_adapters/raw_animation_role_resolver_factory.cpp");
    expect_source_exists("src/d2engine/battle_adapters/raw_ff_animation_catalog.hpp");
    expect_source_exists("src/d2engine/battle_adapters/raw_ff_animation_catalog.cpp");
    expect_source_exists("src/d2engine/battle_adapters/sdl_battle_renderer.hpp");
    expect_source_exists("src/d2engine/battle_adapters/sdl_battle_renderer.cpp");
    expect_source_exists("src/d2engine/battle_adapters/sdl_battle_texture_provider.hpp");
    expect_source_exists("src/d2engine/battle_adapters/sdl_battle_texture_provider.cpp");
    // Bootstrapper lives in app layer
    expect_source_exists("src/d2engine/app/battle_scenario_bootstrapper.hpp");
    expect_source_exists("src/d2engine/app/battle_scenario_bootstrapper.cpp");

    for (const char* path : {
             "src/d2engine/battle_view/animation_catalog.hpp",
             "src/d2engine/battle_view/battle_animation_engine.cpp",
             "src/d2engine/battle_view/battle_animation_engine.hpp",
             "src/d2engine/battle_view/battle_animation_scripts.cpp",
             "src/d2engine/battle_view/battle_animation_scripts.hpp",
             "src/d2engine/battle_view/battle_presenter.cpp",
             "src/d2engine/battle_view/battle_presenter.hpp",
             "src/d2engine/battle_view/battle_renderer.cpp",
             "src/d2engine/battle_view/battle_renderer.hpp",
             "src/d2engine/battle_view/battle_scene.cpp",
             "src/d2engine/battle_view/battle_scene.hpp",
             "src/d2engine/battle_view/battle_texture_provider.hpp",
             "src/d2engine/battle_view/command_timeline.cpp",
             "src/d2engine/battle_view/command_timeline.hpp",
             "src/d2engine/battle_view/visual_track.hpp",
         }) {
        expect_absent(read_source(path), {"battle_adapters/", "raw_ff_animation_catalog",
                                          "sdl_battle_texture_provider"});
    }
}

TEST(BattleArchitectureBoundaries, RenderCoreAvoidsSdlRendererBackend) {
    for (const char* path : {
             "src/d2engine/battle_view/battle_renderer.cpp",
             "src/d2engine/battle_view/battle_renderer.hpp",
             "src/d2engine/battle_view/battle_render_command_builder.cpp",
             "src/d2engine/battle_view/battle_render_command_builder.hpp",
             "src/d2engine/battle_view/render/battle_background_command_builder.cpp",
             "src/d2engine/battle_view/render/battle_debug_command_builder.cpp",
             "src/d2engine/battle_view/render/battle_render_binding_helpers.cpp",
             "src/d2engine/battle_view/render/battle_render_level_resolver.cpp",
             "src/d2engine/battle_view/render/battle_render_tree_helpers.cpp",
             "src/d2engine/battle_view/render/battle_ui_command_builder.cpp",
             "src/d2engine/battle_view/render/battle_unit_track_command_builder.cpp",
         }) {
        expect_absent(read_source(path),
                      {"SDL3/SDL.h", "renderer2d.hpp", "SDL_Texture", "static_cast<SDL_Texture",
                       "draw_texture(", "draw_texture_rotated("});
    }
}

TEST(BattleArchitectureBoundaries, GenericRenderContainsNoBattleTuningVocabulary) {
    const std::vector<const char*> forbidden = {
        "debug_item",
        "DebugRenderableItem",
        "BindingOwnerKind",
        "BindingRole",
        "ConfigBinding",
        "UnitVisualProfile",
        "UnitVisualLayerProfile",
        "UnitVisualLayerDefaultProfile",
        "EffectProfile",
        "EffectDefaultProfile",
        "SpriteProfile",
        "LifecycleProfile",
        "UnitIdle",
        "UnitAttack",
        "UnitHit",
        "UnitBase",
        "TargetTeam",
        "Corpse",
        "DeathFx",
        "ReviveSmall",
        "ReviveLarge",
        "SelectionMarker",
        "TargetMarker",
        "CombatFrame",
        "BattleRenderPass",
        "BattleSlotCoord",
        "Attacker",
        "Defender",
    };
    const std::vector<const char*> forbidden_includes = {
        "#include \"../battle_view", "#include \"battle_view", "#include <d2battle",
        "#include \"d2battle",       "#include <d2runtime",    "#include \"d2runtime",
        "#include <d2game",          "#include \"d2game",
    };

    for (const auto& path : source_files_under("src/d2engine/render")) {
        const std::string text = read_absolute_source(path);
        for (const char* token : forbidden) {
            EXPECT_EQ(text.find(token), std::string::npos) << path << " contains " << token;
        }
        for (const char* include : forbidden_includes) {
            EXPECT_EQ(text.find(include), std::string::npos) << path << " contains " << include;
        }
    }
}

TEST(BattleArchitectureBoundaries, TerrainAssetCatalogStaysAssetOnly) {
    const std::string header =
        read_source("src/d2adventure_render/terrain/terrain_asset_catalog.hpp");
    const std::string impl =
        read_source("src/d2adventure_render/terrain/terrain_asset_catalog.cpp");
    const std::string combined = header + impl;

    expect_absent(combined, {"battle_view", "d2battle", "AdventureWorldState", "ScenarioTemplate",
                             "SDL3/", "SDL.h"});

    const std::string cmake = read_source("CMakeLists.txt");
    const auto        target_pos = cmake.find("opendis2-dev-terrain-assets-dump");
    ASSERT_NE(target_pos, std::string::npos);
    const auto target_end =
        cmake.find("target_compile_options(opendis2-dev-terrain-assets-dump", target_pos);
    ASSERT_NE(target_end, std::string::npos);
    const std::string target_block = cmake.substr(target_pos, target_end - target_pos);
    expect_absent(target_block, {"libd2engine", "libd2battle", "battle_viewer"});
}

TEST(BattleArchitectureBoundaries, AdventureTerrainRuntimeStaysPure) {
    for (const char* path : {
             "src/d2runtime/AdventureTerrain.hpp",
             "src/d2runtime/AdventureTerrain.cpp",
             "src/d2runtime/AdventureTerrainDecoder.hpp",
             "src/d2runtime/AdventureTerrainDecoder.cpp",
             "src/d2runtime/AdventureWorldState.hpp",
         }) {
        expect_absent(read_source(path),
                      {"d2adventure_render/terrain/terrain_asset_catalog.hpp",
                       "d2engine/assets/adventure_terrain_asset_resolver.hpp", "SDL3/", "SDL.h",
                       "battle_view", "AdventureScreen", "SdlRenderCommandRenderer"});
    }
}

TEST(BattleArchitectureBoundaries, TerrainPreviewAvoidsBattleAndSdlWindow) {
    const std::string cmake = read_source("CMakeLists.txt");
    const auto        target_pos = cmake.find("libd2terrain_calibration_export");
    ASSERT_NE(target_pos, std::string::npos);
    const auto target_end = cmake.find("PRIVATE ${D2_COMPILE_WARNINGS}", target_pos);
    ASSERT_NE(target_end, std::string::npos);
    const std::string target_block = cmake.substr(target_pos, target_end - target_pos);
    expect_absent(target_block, {"libd2battle", "opendis2-dev-battle-viewer", "battle_viewer"});

    for (const char* path : {
             "src/opendis2_terrain_preview/terrain_preview_image.hpp",
             "src/opendis2_terrain_preview/terrain_preview_image.cpp",
         }) {
        expect_absent(read_source(path), {"SDL_CreateWindow", "AdventureMapRenderer", "battle_view",
                                          "BattleRenderer", "opendis2-dev-battle-viewer"});
    }
}

TEST(BattleArchitectureBoundaries, AdventureTerrainSurfaceComposerAvoidsPresentationLayers) {
    for (const char* path : {
             "src/d2adventure_render/terrain/adventure_terrain_surface.hpp",
             "src/d2adventure_render/terrain/adventure_terrain_surface.cpp",
         }) {
        expect_absent(read_source(path),
                      {"SDL3/", "SDL.h", "battle_view", "AdventureScreen", "AdventureMapRenderer"});
    }
}

TEST(BattleArchitectureBoundaries, AnimationRoleResolverAvoidsRawLoader) {
    for (const char* path : {
             "src/d2engine/battle_view/animation_role_resolver.cpp",
             "src/d2engine/battle_view/animation_role_resolver.hpp",
         }) {
        expect_absent(read_source(path), {"raw_resource_loader.hpp", "RawResourceLoader"});
    }
}

TEST(BattleArchitectureBoundaries, CoreRuntimeAvoidsBackendCaches) {
    for (const char* path : {
             "src/d2engine/battle_view/battle_animation_engine.cpp",
             "src/d2engine/battle_view/battle_animation_engine.hpp",
             "src/d2engine/battle_view/battle_animation_scripts.cpp",
             "src/d2engine/battle_view/battle_animation_scripts.hpp",
             "src/d2engine/battle_view/battle_presenter.cpp",
             "src/d2engine/battle_view/battle_presenter.hpp",
             "src/d2engine/battle_view/battle_renderer.cpp",
             "src/d2engine/battle_view/battle_renderer.hpp",
             "src/d2engine/battle_view/battle_scene.cpp",
             "src/d2engine/battle_view/battle_scene.hpp",
         }) {
        expect_absent(read_source(path), {"raw_resource_loader", "game_texture_cache"});
    }
}

TEST(BattleArchitectureBoundaries, BattleInterfacesRemainInCore) {
    const std::string animation_catalog =
        read_source("src/d2engine/battle_view/animation_catalog.hpp");
    EXPECT_NE(animation_catalog.find("class IAnimationCatalog"), std::string::npos);
    expect_absent(animation_catalog, {"battle_adapters/", "raw_ff_animation_catalog"});

    const std::string texture_provider =
        read_source("src/d2engine/battle_view/battle_texture_provider.hpp");
    EXPECT_NE(texture_provider.find("using IBattleTextureProvider = ITextureProvider"),
              std::string::npos);
    expect_absent(texture_provider,
                  {"battle_adapters/", "sdl_battle_texture_provider", "game_texture_cache"});

    const std::string generic_texture_provider =
        read_source("src/d2engine/render/texture_provider.hpp");
    EXPECT_NE(generic_texture_provider.find("class ITextureProvider"), std::string::npos);
    expect_absent(generic_texture_provider, {"battle_view/", "battle_adapters/",
                                             "sdl_battle_texture_provider", "game_texture_cache"});
}

TEST(BattleArchitectureBoundaries, PresenterSubmissionApiIsSemanticOnly) {
    const std::string header = read_source("src/d2engine/battle_view/battle_presenter.hpp");
    const std::string signature = source_line_containing(header, "submit_visual_step");

    EXPECT_NE(signature.find("BattleVisualStep"), std::string::npos);
    expect_absent(signature, {"SDL", "BattleRenderCommand", "RendererCommand", "RawResourceLoader",
                              "VisualEntityId", "TrackId"});
}

TEST(BattleArchitectureBoundaries, ScenarioPlayerStaysSemanticAndBackendFree) {
    expect_source_exists("src/d2engine/battle_view/battle_scenario_executor.hpp");
    const std::string source = read_source("src/d2engine/battle_view/battle_scenario_executor.cpp");
    expect_absent(source, {"SDL", "battle_renderer", "raw_resource_loader", "RawResourceLoader",
                           "BattleRenderCommand", "RendererCommand", "VisualEntityId", "TrackId"});

    EXPECT_NE(source.find("BattleScenarioPlayer"), std::string::npos);
    expect_absent(source, {"BattleVisualScript", "BattleVisualSequencePlayer",
                           "battle_visual_script", "battle_visual_sequence_player"});
}

TEST(BattleArchitectureBoundaries, TimedBatchRuntimeBoundariesHold) {
    for (const char* path : {
             "src/d2engine/battle_view/battle_scenario_executor.cpp",
             "src/d2engine/battle_view/battle_scenario_executor.hpp",
         }) {
        expect_absent(read_source(path), {"SDL", "raw_resource_loader", "battle_renderer",
                                          "VisualEntityId", "TrackId"});
    }

    for (const char* path : {
             "src/d2engine/battle_view/battle_animation_engine.cpp",
             "src/d2engine/battle_view/battle_animation_engine.hpp",
             "src/d2engine/battle_view/battle_presenter.cpp",
             "src/d2engine/battle_view/battle_presenter.hpp",
         }) {
        expect_absent(read_source(path), {"nlohmann/json", "json::parse"});
    }

    expect_absent(read_source("src/d2engine/battle_view/battle_renderer.cpp"),
                  {"script version", "sequence_id", "step_id", "envelope"});
}

TEST(BattleArchitectureBoundaries, CueFanoutRuntimeBoundariesHold) {
    for (const char* path : {
             "src/d2engine/battle_view/battle_animation_engine.cpp",
             "src/d2engine/battle_view/battle_animation_engine.hpp",
         }) {
        expect_absent(read_source(path),
                      {"SDL", "raw_resource_loader", "game_texture_cache", "battle_renderer",
                       "BattleRenderCommand", "RendererCommand", "VisualEntityId", "TrackId",
                       "nlohmann/json", "json::parse"});
    }

    for (const char* path : {
             "src/d2engine/battle_view/battle_presenter.cpp",
             "src/d2engine/battle_view/battle_presenter.hpp",
         }) {
        expect_absent(read_source(path),
                      {"SDL", "raw_resource_loader", "game_texture_cache", "battle_renderer",
                       "BattleRenderCommand", "RendererCommand", "nlohmann/json", "json::parse"});
    }
}

TEST(BattleArchitectureBoundaries, BattleScenarioExecutorStaysRendererFree) {
    const std::string source = read_source("src/d2engine/battle_view/battle_scenario_executor.cpp");
    expect_absent(source,
                  {"SDL", "battle_renderer", "raw_resource_loader", "VisualEntityId", "TrackId",
                   "VisualCommand", "BattleRenderCommand", "GameTextureCache", "Renderer2D"});
    EXPECT_NE(source.find("BattleVisualStep"), std::string::npos);
}

TEST(BattleArchitectureBoundaries, EffectPlacementRuntimeStaysBackendFree) {
    const std::string event_header =
        read_source("src/d2engine/battle_view/battle_visual_event.hpp");
    EXPECT_NE(event_header.find("struct BattleEffectStarted"), std::string::npos);
    expect_absent(event_header, {"SDL", "RawResourceLoader", "VisualEntityId", "TrackId",
                                 "BattleRenderCommand", "RendererCommand"});

    for (const char* path : {
             "src/d2engine/battle_view/battle_animation_scripts.cpp",
             "src/d2engine/battle_view/battle_animation_scripts.hpp",
         }) {
        expect_absent(read_source(path), {"SDL", "raw_resource_loader", "RawResourceLoader",
                                          "game_texture_cache", "GameTextureCache"});
    }
}

TEST(BattleArchitectureBoundaries, AoeScriptEventModelStaysRendererFree) {
    for (const char* path : {
             "src/d2engine/battle_view/battle_visual_event.hpp",
             "src/d2engine/battle_view/battle_scenario_data.hpp",
         }) {
        expect_absent(read_source(path), {"SDL", "renderer", "RawResourceLoader", "VisualEntityId",
                                          "TrackId", "clip", "anchor", "layer"});
    }
}

TEST(BattleArchitectureBoundaries, PresenterScriptsAvoidParallelAssetArrays) {
    for (const char* path : {
             "src/d2engine/battle_view/battle_animation_scripts.cpp",
             "src/d2engine/battle_view/battle_animation_scripts.hpp",
             "src/d2engine/battle_view/battle_presenter.cpp",
             "src/d2engine/battle_view/battle_presenter.hpp",
         }) {
        expect_absent(read_source(path), {"unit_roles[", "death_assets[", "cached_animations_",
                                          "death_assets_", "unit_types_"});
    }
}

TEST(BattleArchitectureBoundaries, RendererDoesNotOwnAnimationMutation) {
    for (const char* path : {
             "src/d2engine/battle_view/battle_renderer.cpp",
             "src/d2engine/battle_view/battle_renderer.hpp",
             "src/d2engine/battle_view/battle_anchor_resolver.cpp",
             "src/d2engine/battle_view/battle_anchor_resolver.hpp",
         }) {
        expect_absent(read_source(path), {"command_timeline", "CommandTimeline",
                                          "battle_animation_engine", "BattleAnimationEngine"});
    }
}

TEST(BattleArchitectureBoundaries, VisualTuningHasNoPermanentDebugOverrideLayer) {
    for (const char* path : {
             "src/d2engine/app/battle_tuning_state.hpp",
             "src/d2engine/app/battle_tuning_controller.cpp",
             "src/d2engine/app/application.cpp",
             "src/d2engine/battle_view/battle_render_command_builder.cpp",
             "src/d2engine/battle_view/battle_renderer.hpp",
         }) {
        expect_absent(read_source(path), {"debug_overrides", "DebugTransformOverride",
                                          "debug_offset", "DebugPlacementOverride"});
    }
}

TEST(BattleArchitectureBoundaries, BattleSceneHasNoRawIndexMutationAliases) {
    expect_absent(read_source("src/d2engine/battle_view/battle_scene.hpp"),
                  {"replace_animation(std::size_t", "set_unit_alpha(std::size_t",
                   "toggle_pause(std::size_t", "step_frame(std::size_t", "move_unit(std::size_t",
                   "add_track(std::size_t", "set_track_visibility(std::size_t",
                   "set_track_alpha(std::size_t", "replace_track_clip(std::size_t",
                   "unit_position_offset(std::size_t"});
}

TEST(BattleArchitectureBoundaries, BattleSceneHasNoPresentationOrCentroidApi) {
    expect_absent(read_source("src/d2engine/battle_view/battle_scene.hpp"),
                  {"toggle_layers", "background_visible", "frame_visible",
                   "compute_defender_centroid", "compute_tucha_centroid"});
}

// ── Hard regression: no v2 runtime remnants ───────────────────────────

TEST(BattleArchitectureBoundaries, NoScriptPlayerInApplication) {
    expect_absent(read_source("src/d2engine/app/application.cpp"), {"script_player_"});
}

TEST(BattleArchitectureBoundaries, NoBattleVisualScriptPlayerInProduction) {
    for (const char* path : {
             "src/d2engine/app/application.cpp",
             "src/d2engine/app/application.hpp",
             "src/d2engine/app/debug_overlay_renderer.cpp",
             "src/d2engine/app/debug_overlay_renderer.hpp",
             "src/d2engine/battle_view/battle_scenario_executor.cpp",
             "src/d2engine/battle_view/battle_scenario_executor.hpp",
         }) {
        expect_absent(read_source(path), {"BattleVisualSequencePlayer", "BattleVisualScriptPlayer",
                                          "battle_visual_sequence_player", "battle_visual_script"});
    }
}

TEST(BattleArchitectureBoundaries, NoLoadBattleVisualScriptInProduction) {
    expect_absent(read_source("src/d2engine/app/application.cpp"), {"load_battle_visual_script"});
}

// ── Core/interface boundary: runtime must not depend on presenter/factory ──

TEST(BattleArchitectureBoundaries, CoreRuntimeDoesNotIncludePresenterOrFactory) {
    const std::string runtime_cpp =
        read_source("src/d2engine/battle_view/battle_scenario_runtime.cpp");
    expect_absent(runtime_cpp,
                  {"battle_presenter.hpp", "battle_unit_factory.hpp",
                   "battle_debug_scene_controller.hpp", "battle_selection_controller.hpp",
                   "debug_battle_outcome_resolver.hpp"});
    const std::string runtime_hpp =
        read_source("src/d2engine/battle_view/battle_scenario_runtime.hpp");
    expect_absent(runtime_hpp, {"battle_presenter.hpp", "battle_unit_factory.hpp",
                                "class BattlePresenter", "class BattleUnitFactory"});
}

TEST(BattleArchitectureBoundaries, CoreExecutorDoesNotIncludePresenter) {
    const std::string executor_cpp =
        read_source("src/d2engine/battle_view/battle_scenario_executor.cpp");
    expect_absent(executor_cpp, {"battle_presenter.hpp", "battle_unit_factory.hpp"});
}

TEST(BattleArchitectureBoundaries, ScreensDoNotConstructAssetInfrastructure) {
    for (const auto& path : source_files_under("src/d2engine/app")) {
        const auto filename = path.filename().string();
        if (filename == "application.cpp" || filename == "application.hpp" ||
            filename == "app_runtime_context.cpp" || filename == "app_runtime_context.hpp") {
            continue;
        }
        const std::string source = read_absolute_source(path);
        expect_absent(source,
                      {"make_unique<AssetRuntime", "AssetRuntime{", "make_unique<FfAssetStore",
                       "FfAssetStore{", "make_unique<GameTextureCache", "GameTextureCache{",
                       "std::thread", "hardware_concurrency"});
    }
}

TEST(BattleArchitectureBoundaries, AssetRuntimeLayerStaysSdlFree) {
    const std::string cmake = read_source("src/d2engine/CMakeLists.txt");
    const auto        begin = cmake.find("target_link_libraries(libd2assets_runtime");
    ASSERT_NE(begin, std::string::npos);
    const auto end = cmake.find("target_compile_options(libd2assets_runtime", begin);
    ASSERT_NE(end, std::string::npos);
    const auto link_block = cmake.substr(begin, end - begin);
    expect_absent(link_block, {"SDL3::SDL3", "SDL3_image::SDL3_image", "SDL3_ttf::SDL3_ttf"});

    for (const auto& path : source_files_under("src/d2engine/assets")) {
        expect_absent(read_absolute_source(path), {"<SDL3/", "<SDL3_image/"});
    }
}

TEST(BattleArchitectureBoundaries, ScreenInputHandlersAreSdlFree) {
    for (const char* path : {
             "src/d2engine/app/adventure_screen_input.hpp",
             "src/d2engine/app/adventure_screen_input.cpp",
             "src/d2engine/app/battle_screen_input.hpp",
             "src/d2engine/app/battle_screen_input.cpp",
             "src/d2engine/app/battle_tuning_controller.hpp",
             "src/d2engine/app/battle_tuning_controller.cpp",
             "src/d2engine/app/battle_screen_action.hpp",
             "src/d2engine/app/battle_screen_action.cpp",
             "src/d2engine/app/battle_viewer_controller.hpp",
             "src/d2engine/app/battle_viewer_controller.cpp",
         }) {
        expect_absent(read_source(path),
                      {"<SDL3/", "SDL_Event", "SDL_Keycode", "SDL_Keymod", "SDLK_", "SDL_EVENT_KEY",
                       "SDL_EVENT_MOUSE", "SDL_BUTTON_"});
    }
}

TEST(BattleArchitectureBoundaries, NoSdlReverseConversionsInScreenCode) {
    for (const char* path : {
             "src/d2engine/app/battle_screen.cpp",
             "src/d2engine/app/adventure_screen.cpp",
             "src/d2engine/app/battle_viewer_controller.cpp",
             "src/d2engine/app/battle_viewer_controller.hpp",
         }) {
        expect_absent(read_source(path), {"to_sdl_keycode", "to_sdl_mod"});
    }
}

TEST(BattleArchitectureBoundaries, AdventureRenderLayerStaysPure) {
    const std::string cmake = read_source("src/d2adventure_render/CMakeLists.txt");
    expect_absent(cmake, {"libd2assets_runtime", "libd2render_sdl", "SDL3"});
    for (const auto& path : source_files_under("src/d2adventure_render")) {
        expect_absent(read_absolute_source(path),
                      {"d2engine/assets", "d2engine/render", "<SDL3/", "<SDL3_image/"});
    }
}
