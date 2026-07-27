# ── libd2render_core ──────────────────────────────────────────────────────────
# Generic render/tuning/animation primitives. No SDL, assets, battle, game, or runtime deps.

add_library(libd2render_core STATIC
    animation/animation_player.cpp
    render/render_command.cpp
    render/render_tuning.cpp
    render/render_tree.cpp
    render/upscale_settings.cpp
)
target_include_directories(libd2render_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(libd2render_core PUBLIC libd2log nlohmann_json::nlohmann_json)
target_compile_options(libd2render_core PRIVATE ${D2_COMPILE_WARNINGS} ${D2_COMPILE_WARNINGS_CLANG})

# ── libd2battle_core ──────────────────────────────────────────────────────────
# Pure battle domain primitives: animation engine, scene, timeline, executor,
# runtime. No SDL, no libd2res, no GameTextureCache, no Renderer2D, no
# RawResourceLoader, no GameDataRegistry, no app-layer deps.
# Link-time guard: only libd2log + nlohmann_json (needed by render_tree.hpp).

add_library(libd2battle_core STATIC
    battle_view/battle_debug_binding.cpp
    battle_view/battle_render_tree_contract.cpp
    battle_view/battle_slot.cpp
    battle_view/battle_scene.cpp
    battle_view/command_timeline.cpp
    battle_view/visual_entity.cpp
    battle_view/battle_animation_engine.cpp
    battle_view/battle_animation_scripts.cpp
    battle_view/animation_role_resolver.cpp
    battle_view/visual_effect_resolver.cpp
    battle_view/battle_anchor_resolver.cpp
    battle_view/battle_scenario_executor.cpp
    battle_view/battle_scenario_runtime.cpp
)
target_include_directories(libd2battle_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(libd2battle_core PUBLIC libd2render_core libd2log magic_enum_lib)
target_compile_options(libd2battle_core PRIVATE ${D2_COMPILE_WARNINGS} ${D2_COMPILE_WARNINGS_CLANG})

# ── libd2game_data ────────────────────────────────────────────────────────────
# Now defined in root CMakeLists.txt (zero SDL dep — needed by headless battle).
# This comment remains as documentation of the dependency graph.

# ── libd2assets_runtime ───────────────────────────────────────────────────────
# Sole high-level owner/access/cache for original game .ff assets.
# Houses FfAssetStore (central FF container access), terrain compositing,
# asset catalog building, and RuntimeAssetContext (asset package reader).
# Lives below libd2render_sdl and libd2battle_d2_adapter so those targets
# can declare an honest direct dependency.

add_library(libd2assets_runtime STATIC
    assets/adventure_terrain_asset_resolver.cpp
    assets/capital_asset_catalog_builder.cpp
    assets/capital_visual_resolver.cpp
    assets/contained_stack_shield_asset_catalog_builder.cpp
    assets/stack_banner_asset_catalog_builder.cpp
    assets/animation_asset_preloader.cpp
    assets/asset_runtime.cpp
    assets/bounded_worker_pool.cpp
    assets/cpu_staging_reclaimer.cpp
    assets/ff_asset_store.cpp
    assets/image_asset_key.cpp
    assets/adventure_stack_actor_request_resolver.cpp
    assets/adventure_actor_visual_conversion.cpp
    assets/iso_actor_visual_resolver.cpp
    assets/runtime_asset_context.cpp
    assets/treasure_asset_catalog_builder.cpp
    assets/terrain_asset_catalog_builder.cpp
    assets/tree_asset_catalog_builder.cpp
    assets/mountain_asset_catalog_builder.cpp
    assets/city_asset_catalog_builder.cpp
    assets/landmark_asset_catalog_builder.cpp
    assets/road_asset_catalog_builder.cpp
    assets/site_asset_catalog_builder.cpp
    assets/ruin_asset_catalog_builder.cpp
    assets/resource_node_asset_catalog_builder.cpp
    assets/ruin_asset_catalog_builder.cpp
    assets/render_graph_asset_collector.cpp
)
target_include_directories(libd2assets_runtime PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(libd2assets_runtime
    PUBLIC libd2adventure_render libd2res libd2asset libd2log libd2runtime libd2game_data
           nlohmann_json::nlohmann_json)
target_compile_options(libd2assets_runtime PRIVATE ${D2_COMPILE_WARNINGS} ${D2_COMPILE_WARNINGS_CLANG})

# ── libd2portrait_data ────────────────────────────────────────────────────────
# Portrait manifest loading and index. Lives below libd2battle_sdl_renderer so
# portrait_render_item can declare an honest dependency.
# portrait_render_item.hpp exposes PortraitManifestIndex in its public API,
# so this target is PUBLIC in libd2battle_sdl_renderer.

add_library(libd2portrait_data STATIC
    assets/portrait_manifest.cpp
    assets/portrait_manifest_index.cpp
    assets/events_portrait_manifest.cpp
)
target_include_directories(libd2portrait_data PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(libd2portrait_data PUBLIC libd2log)
target_link_libraries(libd2portrait_data PRIVATE libd2res libd2assets_runtime)
target_compile_options(libd2portrait_data PRIVATE ${D2_COMPILE_WARNINGS} ${D2_COMPILE_WARNINGS_CLANG})

# ── libd2battle_d2_adapter ────────────────────────────────────────────────────
# D2-specific adapters: raw FF animation catalog, role resolver factory,
# and BattleUnitFactory (depends on GameDataRegistry). Does NOT depend on SDL3.

add_library(libd2battle_d2_adapter STATIC
    battle_view/battle_unit_factory.cpp
    battle_adapters/raw_animation_role_resolver_factory.cpp
    battle_adapters/raw_ff_animation_catalog.cpp
)
target_include_directories(libd2battle_d2_adapter PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(libd2battle_d2_adapter PUBLIC libd2battle_core libd2log)
target_link_libraries(libd2battle_d2_adapter PRIVATE libd2game_data libd2assets_runtime libd2res)
target_compile_options(libd2battle_d2_adapter PRIVATE ${D2_COMPILE_WARNINGS} ${D2_COMPILE_WARNINGS_CLANG})

# ── libd2render_sdl ───────────────────────────────────────────────────────────
# SDL + Renderer2D render infrastructure: texture management, PNG decode.
# Declares libd2assets_runtime directly: game_texture_cache and texture_cache
# use RawResourceLoader/RuntimeAssetContext in their public headers.

set(D2_FSR_SHADER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/render/shaders")
set(D2_FSR_SHADER_THIRD_PARTY_DIR "${CMAKE_SOURCE_DIR}/third_party")
set(D2_FSR_SHADER_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/fsr_shaders")
set(D2_FSR_SHADER_DEPENDENCIES
    "${D2_FSR_SHADER_THIRD_PARTY_DIR}/fidelityfx-fsr/ffx-fsr/ffx_a.h"
    "${D2_FSR_SHADER_THIRD_PARTY_DIR}/fidelityfx-fsr/ffx-fsr/ffx_fsr1.h")
set(D2_FSR_SHADER_HEADERS)

function(d2_embed_fsr_shader pass format extension)
    set(source "${D2_FSR_SHADER_SOURCE_DIR}/${pass}.frag.hlsl")
    set(binary "${D2_FSR_SHADER_GENERATED_DIR}/${pass}.${extension}")
    set(header "${D2_FSR_SHADER_GENERATED_DIR}/${pass}_${extension}.hpp")
    string(REPLACE "-" "_" symbol "${pass}_${extension}")
    add_custom_command(
        OUTPUT "${header}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${D2_FSR_SHADER_GENERATED_DIR}"
        COMMAND "${D2_SHADERCROSS_EXECUTABLE}" "${source}" -s HLSL -d "${format}" -t fragment
                -I "${D2_FSR_SHADER_THIRD_PARTY_DIR}" -o "${binary}"
        COMMAND "${CMAKE_COMMAND}" -DINPUT="${binary}" -DOUTPUT="${header}" -DSYMBOL="${symbol}"
                -P "${CMAKE_SOURCE_DIR}/cmake/embed_shader_binary.cmake"
        DEPENDS "${source}" ${D2_FSR_SHADER_DEPENDENCIES}
        VERBATIM)
    list(APPEND D2_FSR_SHADER_HEADERS "${header}")
    set(D2_FSR_SHADER_HEADERS "${D2_FSR_SHADER_HEADERS}" PARENT_SCOPE)
endfunction()

if(D2_SHADERCROSS_EXECUTABLE)
    d2_embed_fsr_shader(fsr_easu SPIRV spv)
    d2_embed_fsr_shader(fsr_easu DXIL dxil)
    d2_embed_fsr_shader(fsr_easu MSL msl)
    d2_embed_fsr_shader(fsr_rcas SPIRV spv)
    d2_embed_fsr_shader(fsr_rcas DXIL dxil)
    d2_embed_fsr_shader(fsr_rcas MSL msl)
    add_custom_target(d2_fsr_shaders DEPENDS ${D2_FSR_SHADER_HEADERS})
else()
    set(D2_FSR_SHADER_HEADERS
        "${D2_FSR_SHADER_SOURCE_DIR}/precompiled/fsr_easu_spv.hpp"
        "${D2_FSR_SHADER_SOURCE_DIR}/precompiled/fsr_easu_dxil.hpp"
        "${D2_FSR_SHADER_SOURCE_DIR}/precompiled/fsr_easu_msl.hpp"
        "${D2_FSR_SHADER_SOURCE_DIR}/precompiled/fsr_rcas_spv.hpp"
        "${D2_FSR_SHADER_SOURCE_DIR}/precompiled/fsr_rcas_dxil.hpp"
        "${D2_FSR_SHADER_SOURCE_DIR}/precompiled/fsr_rcas_msl.hpp")
    add_custom_target(d2_fsr_shaders DEPENDS ${D2_FSR_SHADER_HEADERS})
    message(STATUS "SDL_shadercross unavailable; using checked-in FSR shader binaries")
endif()

add_library(libd2render_sdl STATIC
    render/renderer2d.cpp
    render/sdl_render_command_renderer.cpp
    render/sdl_texture.cpp
    render/texture_cache.cpp
    render/game_texture_cache.cpp
    render/render_asset_runtime.cpp
    render/fsr_embedded_shader.cpp
    render/sdl_frame_presenter.cpp
    ${D2_FSR_SHADER_HEADERS}
    render/text/text_box_renderer.cpp
)
target_include_directories(libd2render_sdl PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_include_directories(libd2render_sdl SYSTEM PUBLIC ${SDL3_INCLUDE_DIRS})
target_include_directories(libd2render_sdl SYSTEM PUBLIC ${SDL3_image_INCLUDE_DIRS})
target_include_directories(libd2render_sdl PRIVATE
    "${D2_FSR_SHADER_GENERATED_DIR}"
    "${D2_FSR_SHADER_SOURCE_DIR}/precompiled")
add_dependencies(libd2render_sdl d2_fsr_shaders)
target_link_libraries(libd2render_sdl PUBLIC
    SDL3::SDL3 SDL3_ttf::SDL3_ttf SDL3_image::SDL3_image libd2render_core
    libd2assets_runtime libd2log
    BZip2::BZip2 unofficial::brotli::brotlidec unofficial::brotli::brotlicommon)
target_link_libraries(libd2render_sdl PRIVATE libd2res)
target_compile_definitions(libd2render_sdl
    PRIVATE D2ENGINE_FONT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/render/font")
target_compile_options(libd2render_sdl PRIVATE ${D2_COMPILE_WARNINGS} ${D2_COMPILE_WARNINGS_CLANG})

# ── libd2battle_sdl_renderer ──────────────────────────────────────────────────
# Battle presentation, render-command building, SDL rendering.
# Presenter, debug/selection controllers, and warmup live here — not in core.
# portrait_render_item.hpp exposes PortraitManifestIndex => libd2portrait_data PUBLIC.

add_library(libd2battle_sdl_renderer STATIC
    battle_view/battle_render_command_builder.cpp
    battle_view/render/battle_background_command_builder.cpp
    battle_view/render/battle_debug_command_builder.cpp
    battle_view/render/battle_render_binding_helpers.cpp
    battle_view/render/battle_render_level_resolver.cpp
    battle_view/render/battle_render_tree_helpers.cpp
    battle_view/render/battle_ui_command_builder.cpp
    battle_view/render/battle_unit_track_command_builder.cpp
    battle_view/battle_renderer.cpp
    battle_view/battle_startup_texture_warmup.cpp
    battle_view/battle_presenter.cpp
    battle_view/battle_debug_scene_controller.cpp
    battle_view/battle_selection_controller.cpp
    battle_view/debug_battle_outcome_resolver.cpp
    battle_view/portrait_render_item.cpp
    battle_adapters/sdl_battle_renderer.cpp
    battle_adapters/sdl_battle_texture_provider.cpp
)
target_include_directories(libd2battle_sdl_renderer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_include_directories(libd2battle_sdl_renderer SYSTEM PUBLIC ${SDL3_INCLUDE_DIRS})
target_link_libraries(libd2battle_sdl_renderer PUBLIC
    libd2battle_core libd2render_sdl libd2portrait_data SDL3::SDL3 SDL3_ttf::SDL3_ttf libd2log
    BZip2::BZip2 unofficial::brotli::brotlidec unofficial::brotli::brotlicommon)
target_compile_options(libd2battle_sdl_renderer PRIVATE ${D2_COMPILE_WARNINGS} ${D2_COMPILE_WARNINGS_CLANG})

# ── libd2engine ───────────────────────────────────────────────────────────────
# Application, SDL event loop, config, assets, HUD orchestration.
# Composition root: depends on all lower targets.

add_library(libd2engine STATIC
    audio/null_audio_service.cpp
    audio/sdl_mixer_audio_service.cpp
    audio/audio_runtime_factory.cpp
    assets/debug_sound_catalog.cpp
    app/animation_time_controller.cpp
    app/application.cpp
    app/adventure_animation_helpers.cpp
    app/app_runtime_context.cpp
    app/adventure_interaction_mask.cpp
    app/adventure_pick_index.cpp
    app/adventure_loading_screen.cpp
    app/adventure_screen.cpp
    app/adventure_actor_graph_updater.cpp
    app/adventure_movement_click_controller.cpp
    app/adventure_route_preview_presentation.cpp
    app/adventure_movement_visual_plan.cpp
    app/adventure_movement_presentation_controller.cpp
    app/adventure_screen_input.cpp
    app/adventure_selection_builder.cpp
    app/adventure_visual_resources.cpp
    app/cursor_controller.cpp
    app/battle_screen.cpp
    app/battle_screen_action.cpp
    app/battle_screen_input.cpp
    app/battle_tuning_binding_resolver.cpp
    app/battle_tuning_config.cpp
    app/battle_viewer_controller.cpp
    app/battle_viewer_renderer.cpp
    app/debug_command_handler.cpp
    app/debug_overlay_renderer.cpp
    app/battle_tuning_controller.cpp
    app/debug_ui_renderer.cpp
    app/screen_manager.cpp
    app/screen_config_store.cpp
    app/tree_layout_editor.cpp
    app/tree_layout_debug_overlay.cpp
    app/upscale_controller.cpp
    app/stack_inspection.cpp
    app/stack_info_asset_plan.cpp
    app/stack_info_screen.cpp
    app/stack_info_screen_input.cpp
    input/sdl_input_backend.cpp
    platform/sdl_context.cpp
    platform/executable_path.cpp
    render/adventure_render_state.cpp
    app/battle_scenario_bootstrapper.cpp
    app/battle_scenario.cpp
)
if(APPLE)
    target_sources(libd2engine PRIVATE platform/macos_display_p3_output.mm)
    target_link_libraries(libd2engine PRIVATE "-framework QuartzCore" "-framework CoreGraphics")
endif()
target_include_directories(libd2engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_include_directories(libd2engine SYSTEM PUBLIC ${SDL3_INCLUDE_DIRS})
target_link_libraries(libd2engine PUBLIC
    libd2render_core
    libd2battle_core
    libd2battle_d2_adapter
    libd2battle_sdl_renderer
    libd2render_sdl
    libd2game_data
    libd2portrait_data
    libd2game
    libd2buildinfo SDL3::SDL3 SDL3_mixer::SDL3_mixer libd2res libd2log nlohmann_json::nlohmann_json
    nlohmann_json_schema_validator_lib magic_enum_lib imgui_lib)
target_compile_definitions(libd2engine PRIVATE
    D2ENGINE_ICON_FILE="${CMAKE_SOURCE_DIR}/assets/app/icon.png")
target_compile_options(libd2engine PRIVATE ${D2_COMPILE_WARNINGS} ${D2_COMPILE_WARNINGS_CLANG})
set_target_properties(libd2engine PROPERTIES OUTPUT_NAME d2engine)
