# opendis2-dev-scenario-gen — Dev-only: scenario generator

add_executable(opendis2-dev-scenario-gen
    main.cpp
)

target_link_libraries(opendis2-dev-scenario-gen PRIVATE libd2buildinfo libd2engine CLI11::CLI11)
target_compile_definitions(opendis2-dev-scenario-gen PRIVATE OPENDIS2_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_compile_options(opendis2-dev-scenario-gen PRIVATE ${D2_COMPILE_WARNINGS} ${D2_COMPILE_WARNINGS_CLANG})
