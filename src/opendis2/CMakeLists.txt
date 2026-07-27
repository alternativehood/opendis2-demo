# opendis2 — Production executable (future game entry point)

add_executable(opendis2
    main.cpp
    adventure_launcher.cpp
    adventure_scenario_loader.cpp
    adventure_startup_screen.cpp
    headless_frontend.cpp
)

target_link_libraries(opendis2 PRIVATE libd2buildinfo libd2game libd2runtime libd2engine CLI11::CLI11 libd2scenario)
target_compile_definitions(opendis2 PRIVATE OPENDIS2_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_compile_options(opendis2 PRIVATE ${D2_COMPILE_WARNINGS} ${D2_COMPILE_WARNINGS_CLANG})

# ── Config deployment ────────────────────────────────────────────────────────
# Copy configs/ to one canonical runtime location.
# macOS: app bundle Resources/
# Other: next to binary
if(APPLE)
    set_target_properties(opendis2 PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.opendis2.app"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
        MACOSX_BUNDLE_ICON_FILE "icon.icns"
    )

    set(_d2_icon_src "${CMAKE_SOURCE_DIR}/assets/app/icon.png")
    if(EXISTS "${_d2_icon_src}")
        add_custom_command(TARGET opendis2 POST_BUILD
            COMMAND "${CMAKE_SOURCE_DIR}/tools/make-icns.sh"
                "${_d2_icon_src}"
                "$<TARGET_BUNDLE_CONTENT_DIR:opendis2>/Resources/icon.icns"
            COMMENT "Generating macOS .icns from assets/app/icon.png"
        )
    endif()
    unset(_d2_icon_src)

    add_custom_command(TARGET opendis2 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/configs"
            "$<TARGET_BUNDLE_CONTENT_DIR:opendis2>/Resources/configs"
        COMMENT "Copying configs/ into app bundle Resources/"
    )
else()
    add_custom_command(TARGET opendis2 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/configs"
            "$<TARGET_FILE_DIR:opendis2>/configs"
        COMMENT "Copying configs/ to runtime directory"
    )
endif()
