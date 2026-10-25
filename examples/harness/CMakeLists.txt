# SpatialCore consumer harness -- a TEMPLATE for a consumer repository.
#
# This directory is never added to SpatialCore's own build. scripts/build-consumer.sh copies it into a
# fresh git repository beside a real `SpatialCore` git submodule (fetched from GitHub at the commit
# under test), which is exactly the layout a plugin such as OpenSpatialDelay has. The steps below
# mirror docs/integration-guide.md Steps 2-3: provide JUCE, add_subdirectory(SpatialCore), link the
# two targets SpatialCore and SpatialCoreUI. Nothing here points at SpatialCore's source folder.
cmake_minimum_required(VERSION 3.22)
project(SpatialCoreHarness VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# JUCE 9.0.0. OpenSpatialDelay uses a JUCE submodule instead; both are fine, because SpatialCore
# uses whichever JUCE targets exist when its CMakeLists.txt runs.
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# OpenSpatialDelay-like configuration: the consumer fetches libmysofa v1.3.2 itself (SpatialCore then
# skips its own v1.3.5 fetch because the mysofa-static target already exists) and builds its own
# targets with -ffast-math (never globally: SpatialCore refuses to compile under fast-math).
option(HARNESS_OSD_LIKE
    "Mirror OpenSpatialDelay: fetch libmysofa v1.3.2 here and build the harness with -ffast-math" OFF)

if(HARNESS_OSD_LIKE)
    set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
    # libmysofa v1.3.2 declares an old cmake_minimum_required, which CMake 4 rejects unless the
    # configure is run with -DCMAKE_POLICY_VERSION_MINIMUM=3.5 (SpatialCore sets that policy only
    # after this FetchContent has already run).
    FetchContent_Declare(
        libmysofa
        GIT_REPOSITORY https://github.com/hoene/libmysofa.git
        GIT_TAG        v1.3.2
    )
    FetchContent_MakeAvailable(libmysofa)
endif()

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/SpatialCore/CMakeLists.txt")
    message(FATAL_ERROR
        "SpatialCore/CMakeLists.txt is missing. Run scripts/build-consumer.sh, or: "
        "git submodule add https://github.com/AndrewRahman/SpatialCore.git SpatialCore")
endif()

# A consumed SpatialCore already defaults this to OFF; set explicitly here as OpenSpatialPanner does.
set(SPATIALCORE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(SpatialCore)

# The plugin (VST3) and the headless sound check (console app) share one processor source.
juce_add_plugin(HarnessPlugin
    COMPANY_NAME "Spatial Media Lab"
    PLUGIN_MANUFACTURER_CODE SMLb
    PLUGIN_CODE ScHn
    FORMATS VST3
    PRODUCT_NAME "SpatialCore Harness"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(HarnessPlugin PRIVATE src/HarnessPlugin.cpp)

target_compile_definitions(HarnessPlugin
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(HarnessPlugin
    PRIVATE
        SpatialCore
        SpatialCoreUI
        juce::juce_audio_utils
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

juce_add_console_app(HarnessCheck PRODUCT_NAME HarnessCheck)

target_sources(HarnessCheck PRIVATE src/SoundCheck.cpp src/HarnessPlugin.cpp)

target_compile_definitions(HarnessCheck
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_STANDALONE_APPLICATION=1)

target_link_libraries(HarnessCheck
    PRIVATE
        SpatialCore
        SpatialCoreUI
        juce::juce_audio_utils
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

if(HARNESS_OSD_LIKE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    foreach(_harness_target HarnessPlugin HarnessCheck)
        target_compile_options(${_harness_target} PRIVATE -ffast-math -Wno-nan-infinity-disabled)
    endforeach()
endif()
