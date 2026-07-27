add_library(libd2analysis STATIC
    ScenarioGlobalIdReport.cpp
)
target_include_directories(libd2analysis PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(libd2analysis PUBLIC libd2gamedata libd2scenario)
target_compile_options(libd2analysis PRIVATE ${D2_COMPILE_WARNINGS} ${D2_COMPILE_WARNINGS_CLANG})
set_target_properties(libd2analysis PROPERTIES OUTPUT_NAME d2analysis)
