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

# --- JUCE (must be provided by consumer or found) ---
if(NOT TARGET juce::juce_core)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE/CMakeLists.txt")
        add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../JUCE" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
        add_subdirectory(JUCE EXCLUDE_FROM_ALL)
    else()
        find_package(JUCE REQUIRED)
    endif()
endif()

# --- libmysofa ---
include(FetchContent)
FetchContent_Declare(
    mysofa
    GIT_REPOSITORY https://github.com/hoene/libmysofa.git
    GIT_TAG v1.3.2
)
set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(mysofa)

find_package(ZLIB REQUIRED)

# --- SpatialCore static library ---
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
    # UI
    src/UI/SpatialMapComponent.cpp
    src/UI/SMLLookAndFeel.cpp
    src/UI/ReverseSlider.cpp
    src/UI/IndicatorToggle.cpp
    src/UI/StyledButton.cpp
)

target_include_directories(SpatialCore
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(SpatialCore
    PUBLIC
        juce::juce_core
        juce::juce_audio_basics
        juce::juce_audio_formats
        juce::juce_dsp
        juce::juce_gui_basics
        juce::juce_osc
    PRIVATE
        mysofa-static
        ZLIB::ZLIB
)

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
