# ── libd2adventure_render ─────────────────────────────────────────────────────
# Generic adventure render infrastructure: render phases, isometric projection,
# depth ordering, render graph, map geometry, terrain compositing.
#
# Domain boundaries:
#   - NO SDL
#   - NO raw SG/FF parsing
#   - NO direct MqdbContainer access
#   - NO application/CLI logic

add_library(libd2adventure_render STATIC
    adventure_frame_composer.cpp
    adventure_actor_primitive_builder.cpp
    adventure_banner_placement.cpp
    adventure_banner_primitive_builder.cpp
    capital_contributor.cpp
    contained_stack_presentation_builder.cpp
    contained_stack_presentation_contributor.cpp
    city_contributor.cpp
    contained_stack_shield_asset_catalog.cpp
    stack_banner_asset_catalog.cpp
    iso_depth_resolver.cpp
    map_geometry.cpp
    map_preparer.cpp
    site_contributor.cpp
    render_graph.cpp
    ruin_contributor.cpp
    resource_node_contributor.cpp
    treasure_contributor.cpp
    terrain/adventure_terrain_border_shape_resolver.cpp
    terrain/adventure_terrain_surface.cpp
    terrain/adventure_terrain_variant_hash.cpp
    terrain/terrain_asset_catalog.cpp
)
target_include_directories(libd2adventure_render PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(libd2adventure_render
    PUBLIC libd2res libd2runtime
    PRIVATE libd2log)
target_compile_options(libd2adventure_render PRIVATE ${D2_COMPILE_WARNINGS} ${D2_COMPILE_WARNINGS_CLANG})
set_target_properties(libd2adventure_render PROPERTIES OUTPUT_NAME d2adventure_render)
