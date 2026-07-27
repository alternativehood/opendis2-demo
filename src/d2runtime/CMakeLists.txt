add_library(libd2runtime STATIC
    AdventureMovementDirection.cpp
    AdventureActorAnimationResolver.cpp
    AdventureGroundClassifier.cpp
    AdventureStackPresentationResolver.cpp
    AdventureTerrain.cpp
    AdventureTerrainDecoder.cpp
    AdventureWorldBuilder.cpp
)
target_include_directories(libd2runtime PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(libd2runtime PUBLIC libd2scenario)
target_compile_options(libd2runtime PRIVATE ${D2_COMPILE_WARNINGS} ${D2_COMPILE_WARNINGS_CLANG})
set_target_properties(libd2runtime PROPERTIES OUTPUT_NAME d2runtime)
