add_library(libd2gamedata STATIC
    DbfGameDataIndex.cpp
    GlobalIdResolver.cpp
)
target_include_directories(libd2gamedata PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(libd2gamedata PUBLIC libd2res)
target_compile_options(libd2gamedata PRIVATE ${D2_COMPILE_WARNINGS} ${D2_COMPILE_WARNINGS_CLANG})
set_target_properties(libd2gamedata PROPERTIES OUTPUT_NAME d2gamedata)
