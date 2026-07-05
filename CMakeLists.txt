cmake_minimum_required(VERSION 3.22)
cmake_policy(SET CMP0077 NEW)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)
project(SpatialCore VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0")
endif()

if(MSVC)
    add_compile_options(/Zc:preprocessor)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

# --- JUCE (provided by consumer, local checkout, or fetched) ---
include(FetchContent)
if(NOT TARGET juce::juce_core)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE/CMakeLists.txt")
        add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../JUCE" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
        add_subdirectory(JUCE EXCLUDE_FROM_ALL)
    else()
        find_package(JUCE QUIET)
        if(NOT JUCE_FOUND)
            FetchContent_Declare(
                JUCE
                GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
                GIT_TAG        8.0.3
                GIT_SHALLOW    TRUE
            )
            FetchContent_MakeAvailable(JUCE)
        endif()
    endif()
endif()

# --- libmysofa (skip fetch if a consumer already provides the mysofa-static
# target — e.g. OSD's own top-level FetchContent of the same hoene/libmysofa
# v1.3.2 tag under the name "libmysofa"; both declarations produce the same
# "mysofa-static" CMake target, so fetching twice is a duplicate-target error
# when SpatialCore is add_subdirectory()'d into a consumer that already
# fetched it) ---
if(NOT TARGET mysofa-static)
    FetchContent_Declare(
        mysofa
        GIT_REPOSITORY https://github.com/hoene/libmysofa.git
        GIT_TAG v1.3.2
    )
    set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(mysofa)
endif()

find_package(ZLIB REQUIRED)

# --- SpatialCore static library (DSP-ONLY — no GUI modules, no UI sources) ---
# NOTE: src/UI/*.cpp (SpatialMapComponent, SMLLookAndFeel, ReverseSlider,
# IndicatorToggle, StyledButton) is deliberately NOT compiled into this target.
# UI lands in a separate SpatialCoreUI CMake target in Phase 9 (D-03 scope split).
add_library(SpatialCore STATIC
    # Algorithms
    src/Algorithms/VBAPAlgorithm.cpp
    src/Algorithms/VBIPAlgorithm.cpp
    src/Algorithms/KNNAlgorithm.cpp
    src/Algorithms/DBAPAlgorithm.cpp
    src/Algorithms/MDAPAlgorithm.cpp
    src/Algorithms/AmbisonicsAlgorithm.cpp
    src/Algorithms/DirectBinauralAlgorithm.cpp
    src/Algorithms/ConstantPowerAlgorithm.cpp
    # Core
    src/Core/SpatialMath.cpp
    # Binaural
    src/Binaural/HRTFDatabase.cpp
    src/Binaural/PartitionedConvolver.cpp
    src/Binaural/BinauralRenderer.cpp
    # IO
    src/IO/OutputFormatRegistry.cpp
    src/IO/SpeakerLayout.cpp
    src/IO/AmbisonicsCodec.cpp
    # OSC
    src/OSC/ADMOSCReceiver.cpp
    src/OSC/ADMOSCSender.cpp
    # Trajectory
    src/Trajectory/TrajectoryEngine.cpp
)

target_include_directories(SpatialCore
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# DSP-only link set (JUCE-Independence Boundary, 08-PATTERNS.md): no
# juce_gui_basics, no juce_audio_utils/juce_audio_plugin_client. Only
# juce_core, juce_dsp, juce_osc, plus the audio-basics/formats modules the
# transplanted DSP bodies (HRTFDatabase SOFA I/O) require.
#
# NOTE on the standalone-vs-consumed split below: JUCE module targets
# (juce::juce_core etc.) are CMake INTERFACE libraries whose .cpp/.mm unity
# sources get compiled directly into whichever CONCRETE target links them
# (target_sources(... INTERFACE ...) propagates sources, not just symbols/
# include-dirs) — PUBLIC or PRIVATE both trigger this, only the downstream
# propagation differs. When SpatialCore is add_subdirectory()'d into OSD,
# OSD's own plugin/test/harness targets ALSO independently compile full
# copies of the same JUCE module sources (existing pre-Phase-8 pattern, one
# copy per standalone JUCE "app" target). Linking libSpatialCore.a (which
# would otherwise carry its OWN third copy) into those targets collides at
# final link with thousands of duplicate symbols. So: in consumed mode, link
# the juce:: module targets INTERFACE-only (SpatialCore's own .cpp files
# still get the needed include dirs + compile definitions via
# $<BUILD_INTERFACE:...INTERFACE_INCLUDE_DIRECTORIES...> below, without a
# second compiled copy of the module sources ending up in libSpatialCore.a —
# the consumer's own module compilation is the single source of truth for
# those symbols, per D-06's "consumer's JUCE wins" rule). In standalone mode
# (SpatialCore's own CI / SpatialCoreTests, no consumer present), there is no
# other target to supply the compiled module code, so link PUBLIC as before.
set(_spatialcore_juce_modules
    juce::juce_core
    juce::juce_audio_basics
    juce::juce_audio_formats
    juce::juce_dsp
    juce::juce_osc
)

if(PROJECT_IS_TOP_LEVEL)
    target_link_libraries(SpatialCore PUBLIC ${_spatialcore_juce_modules})
else()
    target_link_libraries(SpatialCore INTERFACE ${_spatialcore_juce_modules})
    foreach(_scjm IN LISTS _spatialcore_juce_modules)
        target_include_directories(SpatialCore PRIVATE
            $<TARGET_PROPERTY:${_scjm},INTERFACE_INCLUDE_DIRECTORIES>)
        target_compile_definitions(SpatialCore PRIVATE
            $<TARGET_PROPERTY:${_scjm},INTERFACE_COMPILE_DEFINITIONS>)
    endforeach()
endif()

target_link_libraries(SpatialCore
    PRIVATE
        mysofa-static
        ZLIB::ZLIB
)

# JUCE_USE_CURL=0: avoids juce_core's optional libcurl dependency (WebInputStream/
# URL download support), which SpatialCore's DSP code never uses. Without this,
# Linux builds (standalone CI, ubuntu-latest) fail to link with "undefined
# reference to curl_easy_init" etc. — matches OSD's own root CMakeLists.txt,
# which sets this on every one of its targets for the identical reason.
# PUBLIC so it also applies to SpatialCoreTests (propagates via the
# SpatialCore link dependency) and to any other target compiling these JUCE
# module sources transitively through SpatialCore.
target_compile_definitions(SpatialCore PUBLIC JUCE_USE_CURL=0)

target_compile_features(SpatialCore PUBLIC cxx_std_17)

if(APPLE)
    target_compile_options(SpatialCore PRIVATE -ffast-math -Wno-nan-infinity-disabled)
elseif(MSVC)
    target_compile_options(SpatialCore PRIVATE /fp:fast)
endif()

# --- Tests ---
option(SPATIALCORE_BUILD_TESTS "Build SpatialCore tests" ON)
if(SPATIALCORE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
