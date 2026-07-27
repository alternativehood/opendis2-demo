add_library(libd2scenario STATIC
    SgParser.cpp
    SgRecordReader.cpp
)
target_include_directories(libd2scenario PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(libd2scenario PUBLIC libd2log)
target_compile_options(libd2scenario PRIVATE ${D2_COMPILE_WARNINGS} ${D2_COMPILE_WARNINGS_CLANG})
set_target_properties(libd2scenario PROPERTIES OUTPUT_NAME d2scenario)