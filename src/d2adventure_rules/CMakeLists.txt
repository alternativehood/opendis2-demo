add_library(libd2adventure_rules STATIC
    AdventureNavigationMap.cpp
    AdventureNavigationMapBuilder.cpp
    AdventurePathfinder.cpp
    AdventureRoutePrefix.cpp
    AdventureRoutePreviewBuilder.cpp
    AdventureMovementPolicy.cpp
    AdventureMovementProfile.cpp
)
target_include_directories(libd2adventure_rules PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(libd2adventure_rules PUBLIC libd2runtime libd2game_data)
target_compile_options(libd2adventure_rules PRIVATE
    ${D2_COMPILE_WARNINGS}
    ${D2_COMPILE_WARNINGS_CLANG}
)
set_target_properties(libd2adventure_rules PROPERTIES
    OUTPUT_NAME d2adventure_rules
)
