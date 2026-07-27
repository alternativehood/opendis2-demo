add_library(libd2game STATIC
    AdventureUnitMovementProfileCatalog.cpp
    GameSession.cpp
)
target_include_directories(libd2game PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(libd2game PUBLIC libd2runtime libd2adventure_rules libd2game_data)
target_compile_options(libd2game PRIVATE ${D2_COMPILE_WARNINGS} ${D2_COMPILE_WARNINGS_CLANG})
set_target_properties(libd2game PROPERTIES OUTPUT_NAME d2game)
