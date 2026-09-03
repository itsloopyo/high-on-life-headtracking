# SPDX-License-Identifier: MIT
# Copyright (c) 2026 itsloopyo

cmake_minimum_required(VERSION 3.20)
project(HighOnLifeHeadTracking VERSION 0.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS -DUNICODE -D_UNICODE)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(MSVC)
    add_compile_options(/permissive- /Zc:__cplusplus /MP)
endif()

# MinHook, for the GetPlayerViewPoint and presentation-gate trampolines.
# cameraunlock-core's hook manager links against this `minhook` target.
include(FetchContent)
# Pinned to the commit v1.3.3 points at, not to the tag. A git tag is a mutable
# ref: whoever controls the upstream repo can move it, and every clean
# configure - a CI runner, a fresh contributor checkout - would then fetch and
# compile different code under the same version number, into a DLL that runs
# inside the player's game process.
FetchContent_Declare(
    minhook_src
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG        9fbd087432700d73fc571118d6a9697a36443d88  # v1.3.3
)
FetchContent_MakeAvailable(minhook_src)
add_library(minhook STATIC
    ${minhook_src_SOURCE_DIR}/src/hook.c
    ${minhook_src_SOURCE_DIR}/src/buffer.c
    ${minhook_src_SOURCE_DIR}/src/trampoline.c
    ${minhook_src_SOURCE_DIR}/src/hde/hde64.c
    ${minhook_src_SOURCE_DIR}/src/hde/hde32.c
)
target_include_directories(minhook PUBLIC ${minhook_src_SOURCE_DIR}/include)
if(MSVC)
    target_compile_options(minhook PRIVATE /wd4267 /wd4244 /wd4127 /wd4131)
endif()

set(CAMERAUNLOCK_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CAMERAUNLOCK_BUILD_HOOKS ON CACHE BOOL "" FORCE)
set(CAMERAUNLOCK_BUILD_UNREAL ON CACHE BOOL "" FORCE)
add_subdirectory(cameraunlock-core/cpp ${CMAKE_BINARY_DIR}/cameraunlock-core)

set(MOD_SOURCES
    src/dllmain.cpp
    src/headtracking_mod.cpp
    src/view_hook.cpp
    src/view_diagnostics.cpp
    src/frame_pose.cpp
    src/presentation_gate.cpp
    src/game_state.cpp
    src/camera_boundary.cpp
    src/aim_point.cpp
    src/xs_camera.cpp
    src/xs_widgets.cpp
    src/ads_state.cpp
    src/mod_hotkeys.cpp
    src/config.cpp
    src/rva_hook.cpp
    src/builds/build_registry.cpp
    src/builds/steam_offsets.cpp
)

add_library(HighOnLifeHeadTracking SHARED ${MOD_SOURCES})

# The log banner names the version, so an attached HeadTracking.log says which
# build produced it. release.ps1 stamps the project version above.
target_compile_definitions(HighOnLifeHeadTracking PRIVATE
    HOL_HT_VERSION="${PROJECT_VERSION}"
)

target_include_directories(HighOnLifeHeadTracking PRIVATE
    src
    cameraunlock-core/cpp/include
)

target_link_libraries(HighOnLifeHeadTracking PRIVATE
    cameraunlock
    cameraunlock_hooks
    cameraunlock_unreal
    minhook
    ws2_32
    psapi
)

# Output as .asi for Ultimate ASI Loader.
set_target_properties(HighOnLifeHeadTracking PROPERTIES
    SUFFIX ".asi"
    PREFIX ""
)

# Behaviour locks for the parts of the mod that are pure functions - the
# tracker-to-Unreal camera boundary and the presentation caller gate. A separate
# executable linking none of the hook or Windows machinery, so `pixi run test`
# runs with no game installed.
option(HOL_BUILD_TESTS "Build the behaviour tests" ON)
if(HOL_BUILD_TESTS)
    enable_testing()

    # Every suite is one executable over the same two include roots, so the only
    # thing a new one states is its sources and whether it needs core's library.
    function(hol_add_test name)
        cmake_parse_arguments(ARG "" "" "SOURCES;LIBS" ${ARGN})
        add_executable(${name} ${ARG_SOURCES})
        target_include_directories(${name} PRIVATE
            src
            tests
            cameraunlock-core/cpp/include
        )
        if(ARG_LIBS)
            target_link_libraries(${name} PRIVATE ${ARG_LIBS})
        endif()
        add_test(NAME ${name} COMMAND ${name})
    endfunction()

    hol_add_test(hol_boundary_tests
        SOURCES tests/boundary_tests.cpp src/camera_boundary.cpp)

    hol_add_test(hol_config_tests
        SOURCES tests/config_tests.cpp src/config.cpp
        LIBS cameraunlock)

    # The aim-down-sights behaviour, in two suites the shared ADS spec asks
    # every mod for. Both are header-only maths - no hooks, no game, no Windows
    # machinery - so they run on a machine with no copy of High On Life.
    hol_add_test(hol_ads_pose_tests SOURCES tests/ads_pose_tests.cpp)
    hol_add_test(hol_ads_gate_tests SOURCES tests/ads_gate_tests.cpp)

    # The gate between head movement and the game's own decisions. Its matching
    # half is pure, so a fake module range and a few fabricated frames exercise
    # it with no game.
    hol_add_test(hol_presentation_gate_tests
        SOURCES tests/presentation_gate_tests.cpp src/presentation_gate.cpp
        LIBS cameraunlock cameraunlock_unreal)

    # The guarded reads both the gameplay gate and the ADS gate decide from.
    # They read plain process memory through core's SEH-guarded helpers, so a
    # fake module range and a few local bytes exercise them with no game.
    hol_add_test(hol_ue_probe_tests
        SOURCES tests/ue_probe_tests.cpp
        LIBS cameraunlock cameraunlock_unreal)
endif()
