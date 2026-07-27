cmake_minimum_required(VERSION 3.25)
project(opendis2 CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(D2_COMPILE_WARNINGS
    $<IF:$<CXX_COMPILER_ID:MSVC>,
        /W4;/WX;/utf-8;/permissive-;/sdl;/w14242;/w14254;/w14263;/w14265;/w14287;/we4289;/w14296;/w14311;/w14545;/w14546;/w14547;/w14549;/w14555;/w14640;/w14826;/w14928,
        -Wall;-Wextra;-Werror;-Wpedantic;-Wconversion;-Wsign-conversion;-Wshadow;-Wold-style-cast;-Woverloaded-virtual;-Wnon-virtual-dtor;-Wdouble-promotion;-Wformat=2;-Wimplicit-fallthrough;-Wcast-align;-Wundef;-Wswitch-enum;-Wunused;-Wunused-function;-Wunused-variable;-Wunused-parameter;-Wno-missing-field-initializers>
)

set(D2_COMPILE_WARNINGS_CLANG
    $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:
        -Wunused-private-field;-Wunreachable-code;-Wno-missing-designated-field-initializers>
)

if(WIN32)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

option(D2_ENABLE_CLANG_TIDY "Run clang-tidy during build" OFF)
option(D2_ENABLE_CPPCHECK   "Run cppcheck during build"   OFF)
option(D2_ENABLE_IWYU       "Run include-what-you-use"    OFF)
option(D2_ENABLE_ENGINE     "Build SDL3 visual engine prototype" ON)
option(D2_USE_FETCHCONTENT_FALLBACK "Fall back to FetchContent for unavailable dependencies" OFF)

if(D2_ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy REQUIRED)
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
endif()

# -------------------------------------------------------------------------
# SDL3 — optional visual engine dependency
# Must be found before imgui_lib creation (imgui_lib links SDL3::SDL3).
# -------------------------------------------------------------------------
if(D2_ENABLE_ENGINE)
    find_package(SDL3 3.4 CONFIG)
    if(NOT SDL3_FOUND)
        message(FATAL_ERROR
            "SDL3 not found.\n"
            "  Use the vcpkg manifest workflow:\n"
            "    cp .env.example .env\n"
            "    edit VCPKG_ROOT in .env\n"
            "    make build"
        )
    endif()
    find_package(SDL3_ttf CONFIG REQUIRED)
    find_package(SDL3_image CONFIG REQUIRED)
    find_package(SDL3_mixer CONFIG REQUIRED)
    find_package(BZip2 REQUIRED)
    find_package(unofficial-brotli CONFIG REQUIRED)
    find_program(D2_SHADERCROSS_EXECUTABLE NAMES shadercross
        HINTS "${VCPKG_INSTALLED_DIR}/${VCPKG_HOST_TRIPLET}/tools/sdl3-shadercross")
endif()

# -------------------------------------------------------------------------
# Dependencies — try system/vcpkg packages first, FetchContent as fallback
# -------------------------------------------------------------------------
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)
set(SPDLOG_USE_STD_FORMAT ON CACHE BOOL "" FORCE)

set(_fetch_deps)

# Helper: try find_package(CONFIG) first; if not found and fallback is ON,
# declare FetchContent. Emits a clear error when fallback is OFF.
# Also checks ${_fname}_FOUND for packages whose Config file creates a
# namespaced target (e.g. nlohmann_json_schema_validator::validator).
macro(_d2_dep_fetch _fname _target _repo _tag)
    if(NOT TARGET ${_target})
        find_package(${_fname} QUIET CONFIG)
    endif()
    if(NOT TARGET ${_target} AND NOT ${_fname}_FOUND)
        if(D2_USE_FETCHCONTENT_FALLBACK)
            FetchContent_Declare(${_fname}
                GIT_REPOSITORY ${_repo}
                GIT_TAG        ${_tag}
                GIT_SHALLOW    TRUE
            )
            list(APPEND _fetch_deps ${_fname})
        else()
            message(FATAL_ERROR
                "${_fname} not found.\n"
                "  Use the vcpkg manifest workflow:\n"
                "    cp .env.example .env\n"
                "    edit VCPKG_ROOT in .env\n"
                "    make build")
        endif()
    endif()
endmacro()

# Dependencies that support find_package CONFIG
_d2_dep_fetch(CLI11 CLI11::CLI11
    https://github.com/CLIUtils/CLI11.git
    6c7b07a878ad834957b98d0f9ce1dbe0cb204fc9  # v2.4.2
)
# nlohmann_json — use release archive URL instead of git clone
if(NOT TARGET nlohmann_json::nlohmann_json)
    find_package(nlohmann_json QUIET CONFIG)
endif()
if(NOT TARGET nlohmann_json::nlohmann_json AND NOT nlohmann_json_FOUND)
    if(D2_USE_FETCHCONTENT_FALLBACK)
        FetchContent_Declare(nlohmann_json
            URL      https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
            URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
        )
        list(APPEND _fetch_deps nlohmann_json)
    else()
        message(FATAL_ERROR
            "nlohmann_json not found.\n"
            "  Use the vcpkg manifest workflow:\n"
            "    cp .env.example .env\n"
            "    edit VCPKG_ROOT in .env\n"
            "    make build")
    endif()
endif()
_d2_dep_fetch(GTest GTest::gtest
    https://github.com/google/googletest.git
    f8d7d77c06936315286eb55f8de22cd23c188571  # v1.14.0
)
_d2_dep_fetch(spdlog spdlog::spdlog
    https://github.com/gabime/spdlog.git
    79524ddd08a4ec981b7fea76afd08ee05f83755d  # v1.17.0
)
_d2_dep_fetch(nlohmann_json_schema_validator nlohmann_json_schema_validator
    https://github.com/pboettch/json-schema-validator.git
    d0a18559ae6573e88d47febde77251f7282e8ce5  # v2.3.0
)
_d2_dep_fetch(magic_enum magic_enum::magic_enum
    https://github.com/Neargye/magic_enum.git
    1384769c66bd16ec9bb1353f45fe8ec8ccc12dbd  # v0.9.8
)

# lodepng — single-file PNG codec
if(NOT TARGET lodepng_lib)
    # 1. Try find_package CONFIG (vcpkg provides lodepng or lodepng::lodepng)
    find_package(lodepng QUIET CONFIG)
    if(TARGET lodepng::lodepng)
        add_library(lodepng_lib INTERFACE)
        target_link_libraries(lodepng_lib INTERFACE lodepng::lodepng)
    elseif(TARGET lodepng)
        add_library(lodepng_lib INTERFACE)
        target_link_libraries(lodepng_lib INTERFACE lodepng)
    else()
        # 2. Try find_path (system/vcpkg header+source path)
        find_path(lodepng_include NAMES lodepng.h)
        find_file(lodepng_source NAMES lodepng.cpp
            HINTS "${lodepng_include}" "${lodepng_include}/../src"
            NO_CACHE)
        if(lodepng_include AND lodepng_source)
            add_library(lodepng_lib STATIC "${lodepng_source}")
            target_include_directories(lodepng_lib SYSTEM PUBLIC "${lodepng_include}")
            target_compile_options(lodepng_lib PRIVATE
                $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wno-unused-variable -Wno-unused-parameter>)
        elseif(D2_USE_FETCHCONTENT_FALLBACK)
            FetchContent_Declare(lodepng
                GIT_REPOSITORY https://github.com/lvandeve/lodepng.git
                GIT_TAG        ed6fe5825c6a4fbb7f58ab35a4231c7543cd452a
            )
            list(APPEND _fetch_deps lodepng)
        else()
        message(FATAL_ERROR
            "lodepng not found.\n"
            "  Use the vcpkg manifest workflow:\n"
            "    cp .env.example .env\n"
            "    edit VCPKG_ROOT in .env\n"
            "    make build")
        endif()
    endif()
endif()

# gif.h — single-header animated GIF encoder (no vcpkg port, vendored)
set(_gifh_vendored "${CMAKE_CURRENT_SOURCE_DIR}/third_party/gif-h")
if(NOT TARGET gifh_lib)
    if(EXISTS "${_gifh_vendored}/gif.h")
        add_library(gifh_lib INTERFACE)
        target_include_directories(gifh_lib SYSTEM INTERFACE "${_gifh_vendored}")
    else()
        find_path(gifh_include NAMES gif.h)
        if(gifh_include)
            add_library(gifh_lib INTERFACE)
            target_include_directories(gifh_lib SYSTEM INTERFACE "${gifh_include}")
        elseif(D2_USE_FETCHCONTENT_FALLBACK)
            FetchContent_Declare(gifh
                GIT_REPOSITORY https://github.com/charlietangora/gif-h.git
                GIT_TAG        70b645280d5e687f5217177c9cfa2889b0a2ad5f
                GIT_SHALLOW    TRUE
            )
            list(APPEND _fetch_deps gifh)
        else()
            message(FATAL_ERROR
                "gif-h not found (vendored copy missing).\n"
                "  Fix: restore third_party/gif-h/gif.h from git")
        endif()
    endif()
endif()
unset(_gifh_vendored)

# stb — single-header utility library (no CMake CONFIG)
if(NOT TARGET stb_lib)
    find_path(stb_include NAMES stb_image.h)
    if(stb_include)
        add_library(stb_lib INTERFACE)
        target_include_directories(stb_lib SYSTEM INTERFACE "${stb_include}")
    elseif(D2_USE_FETCHCONTENT_FALLBACK)
        FetchContent_Declare(stb
            GIT_REPOSITORY https://github.com/nothings/stb.git
            GIT_TAG        31c1ad37456438565541f4919958214b6e762fb4
            GIT_SHALLOW    TRUE
        )
        list(APPEND _fetch_deps stb)
    else()
        message(FATAL_ERROR
            "stb not found.\n"
            "  Use the vcpkg manifest workflow:\n"
            "    cp .env.example .env\n"
            "    edit VCPKG_ROOT in .env\n"
            "    make build")
    endif()
endif()

# Dear ImGui — debug/tuning UI (only when D2_ENABLE_ENGINE=ON)
if(D2_ENABLE_ENGINE AND NOT TARGET imgui_lib)
    find_package(imgui QUIET CONFIG)
    if(TARGET imgui::imgui)
        # Provided by find_package/vcpkg — create alias wrapper.
        # vcpkg flattens backend headers (imgui_impl_sdl3.h) to include root,
        # but project includes <backends/imgui_impl_*.h>. Create a backends/
        # dir in the build tree bridging the mismatch.
        get_target_property(_imgui_incs imgui::imgui INTERFACE_INCLUDE_DIRECTORIES)
        add_library(imgui_lib INTERFACE)
        target_link_libraries(imgui_lib INTERFACE imgui::imgui)
        foreach(_imgui_inc IN LISTS _imgui_incs)
            if(IS_DIRECTORY "${_imgui_inc}")
                set(_backends_dir "${CMAKE_CURRENT_BINARY_DIR}/backends")
                file(MAKE_DIRECTORY "${_backends_dir}")
                foreach(_hdr imgui_impl_sdl3.h imgui_impl_sdlrenderer3.h)
                    if(EXISTS "${_imgui_inc}/${_hdr}" AND
                       NOT EXISTS "${_backends_dir}/${_hdr}")
                        file(CREATE_LINK "${_imgui_inc}/${_hdr}"
                            "${_backends_dir}/${_hdr}" SYMBOLIC COPY_ON_ERROR)
                    endif()
                endforeach()
                target_include_directories(imgui_lib INTERFACE "${CMAKE_CURRENT_BINARY_DIR}")
                unset(_backends_dir)
            endif()
        endforeach()
        unset(_imgui_incs)
    elseif(D2_USE_FETCHCONTENT_FALLBACK)
        FetchContent_Declare(imgui
            GIT_REPOSITORY https://github.com/ocornut/imgui.git
            GIT_TAG        dbb5eeaadffb6a3ba6a60de1290312e5802dba5a
            GIT_SHALLOW    TRUE
        )
        list(APPEND _fetch_deps imgui)
    else()
        message(FATAL_ERROR
            "imgui not found.\n"
            "  Use the vcpkg manifest workflow:\n"
            "    cp .env.example .env\n"
            "    edit VCPKG_ROOT in .env\n"
            "    make build")
    endif()
endif()

# Fetch all declared dependencies
if(_fetch_deps)
    FetchContent_MakeAvailable(${_fetch_deps})
endif()
unset(_fetch_deps)

# --- Wrapper library targets ---
# These wrap raw FetchContent/file sources into proper CMake targets.
# When deps come from find_package, SOURCE_DIR may not be set; we skip
# include dir configuration in that case (imported targets carry their own).

# lodepng: create wrapper from FetchContent source (if not already created)
if(NOT TARGET lodepng_lib)
    add_library(lodepng_lib STATIC ${lodepng_SOURCE_DIR}/lodepng.cpp)
    target_include_directories(lodepng_lib SYSTEM PUBLIC ${lodepng_SOURCE_DIR})
    target_compile_options(lodepng_lib PRIVATE
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wno-unused-variable -Wno-unused-parameter>
    )
endif()

# gif.h: header-only wrapper (if not already created)
if(NOT TARGET gifh_lib)
    add_library(gifh_lib INTERFACE)
    target_include_directories(gifh_lib SYSTEM INTERFACE ${gifh_SOURCE_DIR})
endif()

# stb: header-only wrapper (if not already created)
if(NOT TARGET stb_lib)
    add_library(stb_lib INTERFACE)
    target_include_directories(stb_lib SYSTEM INTERFACE ${stb_SOURCE_DIR})
endif()

# nlohmann-json-schema-validator — supports two target names depending on source:
#   - FetchContent: nlohmann_json_schema_validator (bare)
#   - vcpkg:        nlohmann_json_schema_validator::validator
# We create an alias wrapper that adds the include path when from FetchContent.
if(NOT TARGET nlohmann_json_schema_validator_lib)
    add_library(nlohmann_json_schema_validator_lib INTERFACE)
    if(TARGET nlohmann_json_schema_validator::validator)
        target_link_libraries(nlohmann_json_schema_validator_lib INTERFACE nlohmann_json_schema_validator::validator)
    elseif(DEFINED nlohmann_json_schema_validator_SOURCE_DIR)
        target_include_directories(nlohmann_json_schema_validator_lib SYSTEM INTERFACE
            ${nlohmann_json_schema_validator_SOURCE_DIR}/src)
        target_link_libraries(nlohmann_json_schema_validator_lib INTERFACE nlohmann_json_schema_validator)
    else()
        target_link_libraries(nlohmann_json_schema_validator_lib INTERFACE nlohmann_json_schema_validator)
    endif()
endif()

# magic_enum: header-only
# vcpkg 0.9.8+ installs headers to magic_enum/ subdirectory;
# project code includes <magic_enum.hpp> (flat path).
if(NOT TARGET magic_enum_lib)
    add_library(magic_enum_lib INTERFACE)
    if(DEFINED magic_enum_SOURCE_DIR)
        target_include_directories(magic_enum_lib SYSTEM INTERFACE
            ${magic_enum_SOURCE_DIR}/include
            ${magic_enum_SOURCE_DIR}/include/magic_enum
        )
    endif()
    if(TARGET magic_enum::magic_enum)
        target_link_libraries(magic_enum_lib INTERFACE magic_enum::magic_enum)
    endif()
    # If the header isn't reachable via the imported target's include dirs,
    # try adding the magic_enum/ subdirectory (vcpkg 0.9.8+ layout).
    find_path(_magic_enum_hdr NAMES magic_enum.hpp magic_enum/magic_enum.hpp)
    if(_magic_enum_hdr AND NOT EXISTS "${_magic_enum_hdr}/magic_enum.hpp")
        target_include_directories(magic_enum_lib SYSTEM INTERFACE "${_magic_enum_hdr}/magic_enum")
    endif()
    unset(_magic_enum_hdr)
endif()

# Dear ImGui: compile the core + SDL3 + SDL3_renderer backends
# Only built from FetchContent when D2_ENABLE_ENGINE=ON and imgui::imgui not found.
if(D2_ENABLE_ENGINE AND NOT TARGET imgui_lib)
    if(NOT DEFINED imgui_SOURCE_DIR)
        message(FATAL_ERROR "imgui_SOURCE_DIR not set — ImGui must be provided via "
                            "find_package or FetchContent")
    endif()
    set(IMGUI_DIR ${imgui_SOURCE_DIR})
    add_library(imgui_lib STATIC
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
        ${IMGUI_DIR}/backends/imgui_impl_sdlrenderer3.cpp
    )
    target_include_directories(imgui_lib PUBLIC
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
    )
    if(TARGET SDL3::SDL3)
        target_link_libraries(imgui_lib PUBLIC SDL3::SDL3)
    endif()
    target_compile_options(imgui_lib PRIVATE
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wno-old-style-cast -Wno-sign-conversion -Wno-double-promotion>
    )
endif()

# -------------------------------------------------------------------------
# libd2log — unified logging facade (thin spdlog wrapper)
# -------------------------------------------------------------------------
add_library(libd2log STATIC
    src/d2log/log.cpp
)
target_include_directories(libd2log PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(libd2log PUBLIC spdlog::spdlog)
target_compile_options(libd2log PRIVATE ${D2_COMPILE_WARNINGS} ${D2_COMPILE_WARNINGS_CLANG})
set_target_properties(libd2log PROPERTIES OUTPUT_NAME d2log)

# -------------------------------------------------------------------------
# libd2buildinfo — build timestamp and project version (zero-dependency)
# -------------------------------------------------------------------------
add_library(libd2buildinfo STATIC
    src/d2buildinfo/build_info.cpp
)
target_include_directories(libd2buildinfo PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(libd2buildinfo PRIVATE ${D2_COMPILE_WARNINGS} ${D2_COMPILE_WARNINGS_CLANG})
set_target_properties(libd2buildinfo PROPERTIES OUTPUT_NAME d2buildinfo)

# Force rebuild on every build so __DATE__/__TIME__ stay current.
add_custom_target(touch_build_info ALL
    COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_SOURCE_DIR}/src/d2buildinfo/build_info.cpp
    VERBATIM
)
add_dependencies(libd2buildinfo touch_build_info)

# -------------------------------------------------------------------------
# libd2res — core parsing library
# -------------------------------------------------------------------------
add_library(libd2res STATIC
    src/d2res/byte_reader.cpp
    src/d2res/mqdb.cpp
    src/d2res/special_files.cpp
    src/d2res/hexdump.cpp
    src/d2res/opt_index.cpp
    src/d2res/opt_images.cpp
    src/d2res/opt_anims.cpp
    src/d2res/opt_maps.cpp
    src/d2res/rgba_buffer.cpp
    src/d2res/image_decoder.cpp
    src/d2res/png_metadata.cpp
    src/d2res/raw_inventory.cpp
    src/d2res/gif_encoder.cpp
    src/d2res/anim_decoder.cpp
    src/d2res/atlas_packer.cpp
    src/d2res/wdb_decoder.cpp
    src/d2res/wdb_audio_payload.cpp
    src/d2res/game_scanner.cpp
    src/d2res/dbf_reader.cpp
    src/d2res/dlg_parser.cpp
    src/d2res/dat_parser.cpp
    src/d2res/wdt_reader.cpp
    src/d2res/image_comparer.cpp
    src/d2res/platform/hash.cpp
    src/d2res/platform/glob.cpp
)
if(WIN32)
    target_sources(libd2res PRIVATE src/d2res/mapped_file_windows.cpp)
else()
    target_sources(libd2res PRIVATE src/d2res/mapped_file_posix.cpp)
endif()
target_include_directories(libd2res PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(libd2res PUBLIC nlohmann_json::nlohmann_json lodepng_lib gifh_lib stb_lib libd2log)
target_compile_options(libd2res PRIVATE ${D2_COMPILE_WARNINGS} ${D2_COMPILE_WARNINGS_CLANG})
set_target_properties(libd2res PROPERTIES OUTPUT_NAME d2res)

# -------------------------------------------------------------------------
# libd2asset — runtime asset package reader
# -------------------------------------------------------------------------
add_library(libd2asset STATIC
    src/d2asset/asset_manifest.cpp
    src/d2asset/atlas_manifest.cpp
    src/d2asset/animation_manifest.cpp
    src/d2asset/sound_manifest.cpp
    src/d2asset/data_table_manifest.cpp
    src/d2asset/asset_link_manifest.cpp
    src/d2asset/asset_database.cpp
    src/d2asset/engine_contract.cpp
)
target_include_directories(libd2asset PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(libd2asset PUBLIC nlohmann_json::nlohmann_json nlohmann_json_schema_validator_lib libd2log)
target_compile_options(libd2asset PRIVATE ${D2_COMPILE_WARNINGS} ${D2_COMPILE_WARNINGS_CLANG})
set_target_properties(libd2asset PROPERTIES OUTPUT_NAME d2asset)

# -------------------------------------------------------------------------
# libd2scenario — .sg scenario parser library
# -------------------------------------------------------------------------
add_subdirectory(src/d2scenario)

# -------------------------------------------------------------------------
# libd2gamedata — game data indexing and global ID resolution library
# -------------------------------------------------------------------------
add_subdirectory(src/d2gamedata)

# -------------------------------------------------------------------------
# libd2analysis — scenario/diagnostics reporting layer (depends on both d2gamedata and d2scenario)
# -------------------------------------------------------------------------
add_subdirectory(src/d2analysis)

# -------------------------------------------------------------------------
# libd2runtime — runtime game state (depends on d2scenario)
# -------------------------------------------------------------------------
add_subdirectory(src/d2runtime)

# -------------------------------------------------------------------------
# libd2game — logical game layer (depends on d2runtime)
# -------------------------------------------------------------------------
add_subdirectory(src/d2game)

# -------------------------------------------------------------------------
# libd2adventure_render — adventure render/infrastructure library
# -------------------------------------------------------------------------
add_subdirectory(src/d2adventure_render)

# -------------------------------------------------------------------------
# libd2game_data — D2 game data tables (GameDataRegistry, UnitDef, AttackDef)
#
# Lives here (not inside d2engine) because it has zero SDL dependency and is
# needed by headless battle rules, integration tests, and the engine alike.
# Links libd2res PRIVATE for DBF reading; the public API is pure-data types.
# -------------------------------------------------------------------------
add_library(libd2game_data STATIC
    src/d2engine/assets/game_data_registry.cpp
    src/d2engine/assets/unit_def.cpp
)
target_include_directories(libd2game_data PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(libd2game_data SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/d2engine)
target_link_libraries(libd2game_data PUBLIC libd2log)
target_link_libraries(libd2game_data PRIVATE libd2res)
target_compile_options(libd2game_data PRIVATE ${D2_COMPILE_WARNINGS} ${D2_COMPILE_WARNINGS_CLANG})

add_subdirectory(src/d2adventure_rules)

# -------------------------------------------------------------------------
# libd2battle_rules — headless battle rules engine
#
# Authoritative gameplay battle core. No SDL, no UI, no renderer.
# Depends on libd2runtime (world state), libd2game_data (defs), libd2log.
# -------------------------------------------------------------------------
add_library(libd2battle_rules STATIC
    src/d2battle_rules/battle_bootstrap.cpp
    src/d2battle_rules/battle_initiative.cpp
    src/d2battle_rules/battle_formation.cpp
    src/d2battle_rules/battle_action_validate.cpp
    src/d2battle_rules/battle_valid_actions.cpp
    src/d2battle_rules/battle_apply.cpp
    src/d2battle_rules/battle_outcomes.cpp
    src/d2battle_rules/battle_validate.cpp
    src/d2battle_rules/battle_fingerprint.cpp
    src/d2battle_rules/attack_support.cpp

    src/d2battle_rules/detail/battle_turn.cpp
    src/d2battle_rules/detail/battle_status.cpp
    src/d2battle_rules/detail/battle_apply_internal.cpp
    src/d2battle_rules/detail/battle_derived.cpp
    src/d2battle_rules/detail/battle_formation.cpp
    src/d2battle_rules/detail/drain_overflow.cpp
    src/d2battle_rules/detail/effect_dispatch.cpp
    src/d2battle_rules/detail/resolved_attack_context.cpp
    src/d2battle_rules/detail/healing_primitive.cpp
    src/d2battle_rules/detail/drain.cpp
    src/d2battle_rules/detail/damage_effect.cpp
    src/d2battle_rules/detail/unit_effects.cpp
    src/d2battle_rules/detail/petrify.cpp
    src/d2battle_rules/detail/heal.cpp
    src/d2battle_rules/detail/cure.cpp
    src/d2battle_rules/detail/revive.cpp
    src/d2battle_rules/detail/attack_target_enumeration.cpp
    src/d2battle_rules/detail/bundle_support.cpp
)
target_include_directories(libd2battle_rules PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(libd2battle_rules PUBLIC libd2runtime libd2game_data libd2log)
target_compile_options(libd2battle_rules PRIVATE ${D2_COMPILE_WARNINGS} ${D2_COMPILE_WARNINGS_CLANG})

# libd2battle_terminal — headless battle terminal presentation library
# -------------------------------------------------------------------------
add_library(libd2battle_terminal STATIC
    src/opendis2_battle/forced_action_policy.cpp
    src/opendis2_battle/terminal_view.cpp
    src/opendis2_battle/attack_presentation_labels.cpp
)
target_include_directories(libd2battle_terminal PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(libd2battle_terminal PUBLIC libd2battle_rules libd2game_data)
target_compile_options(libd2battle_terminal PRIVATE ${D2_COMPILE_WARNINGS} ${D2_COMPILE_WARNINGS_CLANG})

# -------------------------------------------------------------------------
# libd2battle_sweep — batch battle sweep library
# -------------------------------------------------------------------------
add_library(libd2battle_sweep STATIC
    src/opendis2_battle_sweep/stack_catalog.cpp
    src/opendis2_battle_sweep/stack_pair_generator.cpp
    src/opendis2_battle_sweep/random_action_policy.cpp
    src/opendis2_battle_sweep/battle_log_writer.cpp
    src/opendis2_battle_sweep/battle_simulator.cpp
    src/opendis2_battle_sweep/sweep_runner.cpp
)
target_include_directories(libd2battle_sweep PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(libd2battle_sweep PUBLIC libd2battle_rules libd2battle_terminal libd2scenario libd2runtime libd2game_data libd2log)
target_compile_options(libd2battle_sweep PRIVATE ${D2_COMPILE_WARNINGS} ${D2_COMPILE_WARNINGS_CLANG})

# -------------------------------------------------------------------------
# opendis2-dev-battle — headless stack battle sweep
# -------------------------------------------------------------------------
add_executable(opendis2-dev-battle
    src/opendis2_battle/main.cpp
)
target_link_libraries(opendis2-dev-battle PRIVATE libd2battle_sweep libd2buildinfo CLI11::CLI11)
target_compile_options(opendis2-dev-battle PRIVATE ${D2_COMPILE_WARNINGS} ${D2_COMPILE_WARNINGS_CLANG})

# -------------------------------------------------------------------------
# opendis2-dev-extractor — development extractor CLI (no final user binary yet)
# -------------------------------------------------------------------------
add_executable(opendis2-dev-extractor
    src/cli/main.cpp
    src/cli/commands_list.cpp
    src/cli/commands_extract.cpp
    src/cli/commands_special.cpp
    src/cli/commands_extract_images.cpp
    src/cli/commands_extract_anim.cpp
    src/cli/commands_extract_atlas.cpp
    src/cli/commands_extract_sounds.cpp
    src/cli/commands_extract_viewer.cpp
    src/cli/commands_scan.cpp
    src/cli/commands_extract_dbf.cpp
    src/cli/commands_extract_dlg.cpp
    src/cli/commands_extract_dat.cpp
    src/cli/commands_extract_wdt.cpp
    src/cli/commands_extract_all.cpp
    src/cli/commands_compare_images.cpp
    src/cli/commands_research.cpp
    src/cli/extraction_orchestrator.cpp
    src/cli/asset_reference_resolver.cpp
    src/cli/runtime_package.cpp
    src/cli/manifest_builder.cpp
    src/cli/commands_sg_inspect.cpp
)
target_link_libraries(opendis2-dev-extractor PRIVATE libd2buildinfo libd2res libd2asset libd2scenario libd2gamedata libd2analysis CLI11::CLI11)
target_compile_options(opendis2-dev-extractor PRIVATE ${D2_COMPILE_WARNINGS} ${D2_COMPILE_WARNINGS_CLANG})

# -------------------------------------------------------------------------
# Tests
# -------------------------------------------------------------------------
set(DISCIPLES2_GAME_ROOT "" CACHE STRING
    "Absolute path to the Disciples II game root for integration tests (e.g. ${DISCIPLES2_GAME_ROOT} or /path/to/disciples2)")

# Strip surrounding quotes (Makefile include .env preserves them as literal chars)
# so paths with spaces like "/path/to/Disciples II/" work correctly.
set(_d2_game_root "${DISCIPLES2_GAME_ROOT}")
string(REGEX REPLACE "^\"(.*)\"$" "\\1" _d2_game_root "${_d2_game_root}")

enable_testing()

add_executable(opendis2-dev-tests
    tests/unit/test_byte_reader.cpp
    tests/unit/test_classify_record.cpp
    tests/unit/test_hexdump.cpp
    tests/unit/test_opt_index.cpp
    tests/unit/test_opt_images.cpp
    tests/unit/test_opt_anims.cpp
    tests/unit/test_rgba_buffer.cpp
    tests/unit/test_compositor.cpp
    tests/unit/test_sanitize_filename.cpp
    tests/unit/test_image_decoder.cpp
    tests/unit/test_png_metadata.cpp
    tests/unit/test_raw_inventory.cpp
    tests/unit/test_gif_encoder.cpp
    tests/unit/test_anim_decoder.cpp
    tests/unit/test_atlas_packer.cpp
    tests/unit/test_wdb_decoder.cpp
    tests/unit/test_wdb_audio_payload.cpp
    tests/unit/test_game_scanner.cpp
    tests/unit/test_dbf_reader.cpp
    tests/unit/test_dlg_parser.cpp
    tests/unit/test_dat_parser.cpp
    tests/unit/test_wdt_reader.cpp
    tests/unit/test_extract_all.cpp
    tests/unit/test_image_comparer.cpp
    tests/unit/test_platform_hash.cpp
    tests/unit/test_platform_glob.cpp
    tests/unit/test_manifest_builder.cpp
    tests/unit/test_asset_database.cpp
    tests/unit/test_schema_sync.cpp
    tests/unit/test_engine_contract.cpp
    tests/unit/test_runtime_package.cpp
    tests/unit/test_data_table_manifest.cpp
    tests/unit/test_asset_link_manifest.cpp
    tests/unit/test_battle_fx_research.cpp
    tests/unit/test_adventure_terrain_decoder.cpp
    tests/unit/test_sg_parser.cpp
    tests/unit/test_d2gamedata.cpp
    tests/unit/test_architecture_guardrails.cpp
    tests/unit/test_adventure_world_builder.cpp
    tests/unit/test_adventure_resource_node_builder.cpp
    tests/unit/test_tree_hash.cpp
    tests/unit/test_game_session.cpp
    tests/unit/test_game_session_adventure_movement.cpp
    tests/unit/test_adventure_route_preview.cpp
    tests/unit/test_battle_rules.cpp
    tests/unit/test_battle_terminal_view.cpp
    tests/unit/test_battle_forced_action_policy.cpp
    tests/unit/test_battle_sweep.cpp
    tests/unit/test_battle_heal_cure_revive.cpp
    tests/unit/test_battle_composite_attacks.cpp
    tests/unit/test_adventure_movement_policy.cpp
    tests/unit/test_adventure_movement_direction.cpp
    tests/unit/test_adventure_route_prefix.cpp
    tests/unit/test_adventure_navigation_map.cpp
    tests/unit/test_adventure_pathfinder.cpp
    src/cli/commands_extract_images.cpp
    src/cli/commands_extract_anim.cpp
    src/cli/commands_extract_atlas.cpp
    src/cli/commands_extract_sounds.cpp
    src/cli/commands_extract_viewer.cpp
    src/cli/commands_scan.cpp
    src/cli/commands_extract_dbf.cpp
    src/cli/commands_extract_dlg.cpp
    src/cli/commands_extract_dat.cpp
    src/cli/commands_extract_wdt.cpp
    src/cli/commands_extract_all.cpp
    src/cli/commands_compare_images.cpp
    src/cli/commands_research.cpp
    src/cli/extraction_orchestrator.cpp
    src/cli/asset_reference_resolver.cpp
    src/cli/runtime_package.cpp
    src/cli/manifest_builder.cpp
    src/d2res/extractor/png_decode.cpp
)
target_link_libraries(opendis2-dev-tests PRIVATE libd2buildinfo libd2res libd2asset libd2scenario libd2runtime libd2game libd2gamedata libd2analysis libd2battle_rules libd2battle_terminal libd2battle_sweep libd2game_data libd2adventure_rules GTest::gtest_main)
target_include_directories(opendis2-dev-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(opendis2-dev-tests PRIVATE ${D2_COMPILE_WARNINGS} ${D2_COMPILE_WARNINGS_CLANG})
target_compile_definitions(opendis2-dev-tests PRIVATE
    DISCIPLES2_GAME_ROOT="${_d2_game_root}"
    OPENDIS2_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

if(D2_ENABLE_ENGINE)
    target_sources(opendis2-dev-tests PRIVATE
        tests/unit/test_opendis2_cli.cpp
        tests/unit/test_animation_player.cpp
        tests/unit/test_animation_time_controller.cpp
        tests/unit/test_layered_animation_player.cpp
        tests/unit/test_rect.cpp
        tests/unit/test_camera2d.cpp
        tests/unit/test_render_tree.cpp
        tests/unit/test_terrain_asset_catalog.cpp
        tests/unit/test_battle_render_tree_contract.cpp
        tests/unit/test_battle_slot.cpp
        tests/unit/test_tree_layout.cpp
        tests/unit/test_tree_layout_debug_overlay.cpp
        tests/unit/test_battle_anchor_resolver.cpp
        tests/unit/test_battle_animation_engine.cpp
        tests/unit/test_battle_animation_scripts.cpp
        tests/unit/test_visual_effect_resolver.cpp
        tests/unit/test_battle_renderer.cpp
        tests/unit/test_battle_warmup_plan.cpp
        tests/unit/test_lifecycle_visual_tuning.cpp
        tests/unit/test_layer_default_profiles.cpp
        tests/unit/test_battle_architecture_boundaries.cpp
        tests/unit/test_audio_service.cpp
        tests/unit/test_audio_architecture.cpp
        tests/unit/test_debug_sound_catalog.cpp
        tests/unit/test_animation_role.cpp
        tests/unit/test_animation_role_resolver.cpp
        tests/unit/test_battle_unit.cpp
        tests/unit/test_battle_scene.cpp
        tests/unit/test_battle_presenter.cpp
        tests/unit/test_battle_scenario.cpp
        tests/unit/test_battle_viewer_cli.cpp
        tests/unit/test_battle_viewer_action.cpp
        tests/unit/test_debug_command_handler.cpp
        tests/unit/test_battle_visual_event.cpp
        tests/unit/test_unit_attack_summary.cpp
        tests/unit/test_formation_packer.cpp
        tests/unit/test_scenario_gen_validation.cpp
        tests/unit/test_command_timeline.cpp
        tests/unit/test_visual_track.cpp
        tests/unit/test_visual_entity.cpp
        tests/unit/test_game_data_registry.cpp
        tests/unit/test_asset_runtime.cpp
        tests/unit/test_animation_asset_preloader.cpp
        tests/unit/test_bounded_worker_pool.cpp
        tests/unit/test_cpu_staging_reclaimer.cpp
        tests/unit/test_mapped_file.cpp
        tests/unit/test_ff_asset_store_init.cpp
        tests/unit/test_portrait_manifest.cpp
        tests/unit/test_portrait_rendering.cpp
        tests/unit/test_png_decode.cpp
        tests/unit/test_sdl_texture.cpp
        tests/unit/test_texture_cache.cpp
        tests/unit/test_shared_app_runtime.cpp
        tests/unit/test_text_box_renderer.cpp
        tests/unit/test_screen_manager.cpp
        tests/unit/test_screen_architecture.cpp
        tests/unit/test_headless_frontend.cpp
        src/opendis2/headless_frontend.cpp
        tests/unit/test_prepared_texture_upload_queue.cpp
        tests/unit/test_adventure_screen_input.cpp
        tests/unit/test_adventure_selection_builder.cpp
        tests/unit/test_adventure_pick_index.cpp
        tests/unit/test_adventure_sprite_metadata.cpp
        tests/unit/test_adventure_boat_presentation.cpp
        tests/unit/test_adventure_boat_visual.cpp
        tests/unit/test_adventure_stack_actor_request.cpp
        tests/unit/test_adventure_facing.cpp
        tests/unit/test_adventure_unit_idle_animation.cpp
        tests/unit/test_stack_inspection.cpp
        tests/unit/test_stack_inspection_resolved.cpp
        tests/unit/test_stack_info_asset_plan.cpp
        tests/unit/test_stack_info_screen.cpp
        tests/unit/test_debug_input_capture.cpp
        tests/unit/test_sdl_input_backend.cpp
        tests/unit/test_sdl_input_backend_coordinates.cpp
        tests/unit/test_adventure_terrain_asset_resolver.cpp
        tests/unit/test_adventure_terrain_border_shape_resolver.cpp
        tests/unit/test_adventure_terrain_surface.cpp
        tests/unit/test_capital_asset_catalog.cpp
        tests/unit/test_capital_visual_resolver.cpp
        tests/unit/test_capital_contributor.cpp
        tests/unit/test_stack_banner_asset_catalog.cpp
        tests/unit/test_contained_stack_presentation_contributor.cpp
        tests/unit/test_contained_stack_shield_asset_catalog.cpp
        tests/unit/test_adventure_stack_interaction_masks.cpp
        tests/unit/test_terrain_preview_image.cpp
        tests/unit/test_adventure_map_rendering.cpp
        tests/unit/test_site_rendering.cpp
        tests/unit/test_city_rendering.cpp
        tests/unit/test_road_rendering.cpp
        tests/unit/test_terrain_calibration_exporter.cpp
        tests/unit/test_cursor_controller.cpp
        tests/unit/test_upscale_settings.cpp
        tests/unit/test_sdl_frame_presenter.cpp
        tests/unit/test_adventure_screen_pointer.cpp
        tests/unit/test_adventure_startup.cpp
        tests/unit/test_adventure_banner_pairing.cpp
        src/opendis2/adventure_scenario_loader.cpp
        src/opendis2/adventure_startup_screen.cpp
        tests/unit/test_resource_node_asset_catalog.cpp
        tests/unit/test_ruin_asset_catalog.cpp
        tests/unit/test_treasure_asset_catalog.cpp
        tests/unit/test_resource_node_contributor.cpp
        tests/unit/test_ruin_world_builder.cpp
        tests/unit/test_ruin_rendering.cpp
        tests/unit/test_treasure_rendering.cpp
        tests/unit/test_adventure_map_cell_picking.cpp
        tests/unit/test_adventure_route_preview_presentation.cpp
        tests/unit/test_adventure_route_preview_visual_resources.cpp
        tests/unit/test_adventure_movement_presentation_controller.cpp
        tests/unit/test_adventure_movement_visual_plan.cpp
        tests/unit/test_adventure_actor_graph_updater.cpp
        tests/unit/test_adventure_header_self_containment.cpp
        tests/unit/test_adventure_movement_click_controller.cpp
        src/opendis2_terrain_preview/terrain_preview_image.cpp
    )
    target_link_libraries(opendis2-dev-tests PRIVATE libd2engine libd2adventure_render libd2terrain_calibration_export CLI11::CLI11)
    target_include_directories(opendis2-dev-tests SYSTEM PRIVATE ${SDL3_INCLUDE_DIRS})
    target_include_directories(opendis2-dev-tests SYSTEM PRIVATE ${SDL3_image_INCLUDE_DIRS})
    target_include_directories(opendis2-dev-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
endif()

include(GoogleTest)
gtest_discover_tests(opendis2-dev-tests
    PROPERTIES TIMEOUT 3
)

# ---------------------------------------------------------------------------
# Integration tests — require DISCIPLES2_GAME_ROOT + real game data
# Run with: make test-integration  or  ctest -L integration
# ---------------------------------------------------------------------------
# Full-game validation binary (excluded from CTest — may exceed 3s per test)
# Run via: make validate-full-game
add_executable(opendis2-dev-full-game-validation
    tests/integration/test_extract_all_game_integration.cpp
    tests/validation/test_landmark_full_game_geometry.cpp
    src/cli/commands_extract_all.cpp
    src/cli/commands_extract_images.cpp
    src/cli/commands_extract_anim.cpp
    src/cli/commands_extract_atlas.cpp
    src/cli/commands_extract_sounds.cpp
    src/cli/commands_extract_viewer.cpp
    src/cli/commands_extract_dbf.cpp
    src/cli/commands_extract_dlg.cpp
    src/cli/commands_extract_dat.cpp
    src/cli/commands_extract_wdt.cpp
    src/cli/commands_scan.cpp
    src/cli/commands_compare_images.cpp
    src/cli/extraction_orchestrator.cpp
    src/cli/asset_reference_resolver.cpp
    src/cli/runtime_package.cpp
    src/cli/manifest_builder.cpp
)
if(NOT D2_ENABLE_ENGINE)
    set_target_properties(opendis2-dev-full-game-validation PROPERTIES EXCLUDE_FROM_ALL TRUE)
endif()
target_link_libraries(opendis2-dev-full-game-validation PRIVATE libd2buildinfo libd2engine libd2res libd2asset GTest::gtest_main CLI11::CLI11)
target_include_directories(opendis2-dev-full-game-validation PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(opendis2-dev-full-game-validation PRIVATE ${D2_COMPILE_WARNINGS} ${D2_COMPILE_WARNINGS_CLANG})
target_compile_definitions(opendis2-dev-full-game-validation PRIVATE
    DISCIPLES2_GAME_ROOT="${_d2_game_root}"
)

# ---------------------------------------------------------------------------
# Integration test binary — requires DISCIPLES2_GAME_ROOT + game data.
# Every individual test must complete within TIMEOUT 3.
# Exhaustive full-game/full-container validation must run outside CTest.
# Run via: make test-integration  or  ctest -L integration
# ---------------------------------------------------------------------------
set(_d2_int_test_srcs
    tests/integration/test_mqdb_batunits.cpp
    tests/integration/test_mqdb_isounit.cpp
    tests/integration/test_mqdb_battle_wdb.cpp
    tests/integration/test_mqdb_payload_sizes.cpp
    tests/integration/test_mqdb_interf.cpp
    tests/integration/test_special_batunits.cpp
    tests/integration/test_list_special.cpp
    tests/integration/test_opt_index_batunits.cpp
    tests/integration/test_opt_images_batunits.cpp
    tests/integration/test_opt_anims_batunits.cpp
    tests/integration/test_opt_maps_batunits.cpp
    tests/integration/test_decode_image_batunits.cpp
    tests/integration/test_decode_image_capital.cpp
    tests/integration/test_extract_images_capital.cpp
    tests/integration/test_extract_images_interf.cpp
    tests/integration/test_anim_decoder_batunits.cpp
    tests/integration/test_extract_anim_batunits.cpp
    tests/integration/test_extract_anim_gif.cpp
    tests/integration/test_extract_anim_pattern.cpp
    tests/integration/test_extract_atlas_capital.cpp
    tests/integration/test_extract_atlas_interf.cpp
    tests/integration/test_extract_anim_atlas.cpp
    tests/integration/test_wdb_decoder_battle.cpp
    tests/integration/test_extract_sounds_battle.cpp
    tests/integration/test_runtime_sound_battle.cpp
    tests/integration/test_runtime_data_tables.cpp
    tests/integration/test_adventure_stack_real_scenario.cpp
    tests/integration/test_mountain_asset_catalog.cpp
    tests/integration/test_road_asset_catalog.cpp
    tests/integration/test_landmark_animation_geometry.cpp
    tests/integration/test_capital_asset_catalog.cpp
    tests/integration/test_capital_visual_resolver_real_scenario.cpp
    tests/integration/test_stack_banner_asset_catalog.cpp
    tests/integration/test_ruin_asset_catalog.cpp
    tests/integration/test_contained_stack_shield_asset_catalog.cpp
    tests/integration/test_extract_viewer.cpp
    tests/integration/test_scan_game.cpp
    tests/integration/test_dbf_reader_gunits.cpp
    tests/integration/test_dlg_parser_interf.cpp
    tests/integration/test_extract_dbf_gunits.cpp
    tests/integration/test_extract_dlg_interf.cpp
    tests/integration/test_extract_all_game.cpp
    tests/integration/test_compare_images_reference.cpp
    tests/integration/test_animation_asset_preloader.cpp
    tests/integration/test_game_data_registry_real.cpp
)
set(_d2_int_test_cli_srcs
    src/cli/commands_extract_images.cpp
    src/cli/commands_extract_anim.cpp
    src/cli/commands_extract_atlas.cpp
    src/cli/commands_extract_sounds.cpp
    src/cli/commands_extract_viewer.cpp
    src/cli/commands_scan.cpp
    src/cli/commands_extract_dbf.cpp
    src/cli/commands_extract_dlg.cpp
    src/cli/commands_extract_dat.cpp
    src/cli/commands_extract_wdt.cpp
    src/cli/commands_extract_all.cpp
    src/cli/commands_compare_images.cpp
    src/cli/commands_research.cpp
    src/cli/extraction_orchestrator.cpp
    src/cli/asset_reference_resolver.cpp
    src/cli/runtime_package.cpp
    src/cli/manifest_builder.cpp
    src/d2res/extractor/png_decode.cpp
)
add_executable(opendis2-dev-integration-tests
    ${_d2_int_test_srcs}
    ${_d2_int_test_cli_srcs}
)
if(NOT D2_ENABLE_ENGINE)
    set_target_properties(opendis2-dev-integration-tests PROPERTIES EXCLUDE_FROM_ALL TRUE)
endif()
target_link_libraries(opendis2-dev-integration-tests PRIVATE
    libd2buildinfo libd2res libd2asset libd2scenario
    libd2runtime libd2game libd2gamedata libd2analysis libd2game_data
    GTest::gtest_main CLI11::CLI11
)
target_include_directories(opendis2-dev-integration-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(opendis2-dev-integration-tests PRIVATE ${D2_COMPILE_WARNINGS} ${D2_COMPILE_WARNINGS_CLANG})
target_compile_definitions(opendis2-dev-integration-tests PRIVATE
    DISCIPLES2_GAME_ROOT="${_d2_game_root}"
    OPENDIS2_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)
if(D2_ENABLE_ENGINE)
    target_sources(opendis2-dev-integration-tests PRIVATE
        tests/integration/test_raw_resource_loader.cpp
        tests/integration/test_debug_sound_catalog.cpp
        tests/integration/test_sdl_mixer_real_data.cpp
        tests/integration/test_boat_corpus.cpp
        tests/integration/test_boat_water_stack.cpp
        tests/integration/test_city_pipeline.cpp
        tests/integration/test_site_pipeline.cpp
        tests/integration/test_resource_node_asset_catalog.cpp
        tests/integration/test_resource_node_real_scenario.cpp
        tests/integration/test_ruin_pipeline.cpp
        tests/integration/test_ruin_asset_catalog.cpp
        tests/integration/test_treasure_pipeline.cpp
    )
    target_link_libraries(opendis2-dev-integration-tests PRIVATE
        libd2engine libd2adventure_render libd2terrain_calibration_export libd2assets_runtime
        SDL3_mixer::SDL3_mixer CLI11::CLI11
    )
    target_include_directories(opendis2-dev-integration-tests SYSTEM PRIVATE
        ${SDL3_INCLUDE_DIRS} ${SDL3_image_INCLUDE_DIRS} ${SDL3_mixer_INCLUDE_DIRS}
    )
    target_include_directories(opendis2-dev-integration-tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
endif()
gtest_discover_tests(opendis2-dev-integration-tests
    PROPERTIES LABELS "integration" TIMEOUT 3
)

# ---------------------------------------------------------------------------
# Lint targets — delegate to tools/ scripts
# ---------------------------------------------------------------------------
set(D2_TOOLS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tools")

# Shell-based check: no raw diagnostic prints in src/
add_test(NAME NoRawDiagnosticPrints
    COMMAND "${D2_TOOLS_DIR}/check_no_raw_prints.sh"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
set_tests_properties(NoRawDiagnosticPrints PROPERTIES TIMEOUT 3)

# Architecture guardrail: d2gamedata must not include d2scenario headers
add_test(NAME Guardrail_NoD2ScenarioInGamedata
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/guardrail_no_d2scenario_in_gamedata.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/d2gamedata"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
set_tests_properties(Guardrail_NoD2ScenarioInGamedata PROPERTIES TIMEOUT 3)

# Architecture guardrail: CLI must not own DBF scanning logic
add_test(NAME Guardrail_CLINoDbfScanning
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/guardrail_cli_no_dbf_scanning.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/cli"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
set_tests_properties(Guardrail_CLINoDbfScanning PROPERTIES TIMEOUT 3)

# Architecture guardrail: DbfGameDataIndex naming must be consistent
add_test(NAME Guardrail_DbfIndexNaming
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/guardrail_dbfindex_naming.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/unit"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
set_tests_properties(Guardrail_DbfIndexNaming PROPERTIES TIMEOUT 3)

# Architecture guardrail: production must not include test/test-support headers
add_test(NAME Guardrail_NoTestDepsInProduction
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/guardrail_no_test_deps_in_production.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
set_tests_properties(Guardrail_NoTestDepsInProduction PROPERTIES TIMEOUT 3)

# Architecture guardrail: exactly one headless battle executable (sweep-only, no legacy interactive)
add_test(NAME Guardrail_BattleArchitecture
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/guardrail_battle_architecture.py"
            "${CMAKE_CURRENT_SOURCE_DIR}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
set_tests_properties(Guardrail_BattleArchitecture PROPERTIES TIMEOUT 3)

# Architecture guardrail: composite attack bundle invariants and regressions
add_test(NAME Guardrail_BattleAttackBundle
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/guardrail_battle_attack_bundle.py"
            "${CMAKE_CURRENT_SOURCE_DIR}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
set_tests_properties(Guardrail_BattleAttackBundle PROPERTIES TIMEOUT 3)

# -------------------------------------------------------------------------
# Engine targets — conditionally included when D2_ENABLE_ENGINE is ON
# -------------------------------------------------------------------------
if(D2_ENABLE_ENGINE)
    add_subdirectory(src/d2engine)
    add_subdirectory(src/opendis2_battle_scenario_gen)
    add_subdirectory(src/opendis2)

    add_executable(opendis2-dev-terrain-assets-dump
        src/opendis2_terrain_assets_dump/main.cpp
    )
    target_link_libraries(opendis2-dev-terrain-assets-dump PRIVATE libd2buildinfo libd2assets_runtime CLI11::CLI11)
    target_compile_options(opendis2-dev-terrain-assets-dump
        PRIVATE ${D2_COMPILE_WARNINGS} ${D2_COMPILE_WARNINGS_CLANG})

    add_executable(opendis2-dev-grborder-atlas
        src/opendis2_grborder_atlas/main.cpp
    )
    target_link_libraries(opendis2-dev-grborder-atlas
        PRIVATE libd2buildinfo libd2assets_runtime CLI11::CLI11 lodepng_lib)
    target_compile_options(opendis2-dev-grborder-atlas
        PRIVATE ${D2_COMPILE_WARNINGS} ${D2_COMPILE_WARNINGS_CLANG})

    add_library(libd2terrain_calibration_export STATIC
        src/opendis2_terrain_calibration_export/terrain_calibration_exporter.cpp
        src/opendis2_terrain_preview/terrain_preview_image.cpp
    )
    target_include_directories(libd2terrain_calibration_export PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(libd2terrain_calibration_export
        PUBLIC libd2runtime libd2assets_runtime
        PRIVATE libd2scenario libd2res nlohmann_json::nlohmann_json lodepng_lib)
    target_compile_options(libd2terrain_calibration_export
        PRIVATE ${D2_COMPILE_WARNINGS} ${D2_COMPILE_WARNINGS_CLANG})

    add_executable(opendis2-dev-terrain-calibration-export
        src/opendis2_terrain_calibration_export/main.cpp
    )
    target_link_libraries(opendis2-dev-terrain-calibration-export
        PRIVATE libd2buildinfo libd2terrain_calibration_export CLI11::CLI11)
    target_compile_options(opendis2-dev-terrain-calibration-export
        PRIVATE ${D2_COMPILE_WARNINGS} ${D2_COMPILE_WARNINGS_CLANG})

    # ── opendis2 smoke tests ──────────────────────────────────────────
    add_test(NAME OpenDis2_Help
        COMMAND opendis2 --help)
    set_tests_properties(OpenDis2_Help PROPERTIES
        LABELS "cli" TIMEOUT 3)

    add_test(NAME OpenDis2_Version
        COMMAND opendis2 --version)
    set_tests_properties(OpenDis2_Version PROPERTIES
        LABELS "cli" TIMEOUT 3)

    add_test(NAME OpenDis2_BattleViewerHelp
        COMMAND opendis2 battle-viewer --help)
    set_tests_properties(OpenDis2_BattleViewerHelp PROPERTIES
        LABELS "cli" TIMEOUT 3)

    add_test(NAME OpenDis2_InvalidOption
        COMMAND opendis2 --bogus)
    set_tests_properties(OpenDis2_InvalidOption PROPERTIES
        WILL_FAIL TRUE
        LABELS "cli" TIMEOUT 3)

    add_test(NAME OpenDis2_AdventureHelp
        COMMAND opendis2 adventure --help)
    set_tests_properties(OpenDis2_AdventureHelp PROPERTIES
        LABELS "cli" TIMEOUT 3)
endif()
