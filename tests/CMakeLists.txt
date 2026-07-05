include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1
)
FetchContent_MakeAvailable(Catch2)

add_executable(SpatialCoreTests
    Algorithms/SpatializationAlgorithmTests.cpp
    IO/SpeakerLayoutTests.cpp
    IO/AmbisonicsCodecTests.cpp
    OSC/ADMOSCReceiverTests.cpp
    OSC/ADMOSCSenderTests.cpp
    Trajectory/TrajectoryTests.cpp
    Trajectory/DopplerTests.cpp
    Core/SpatialMathTests.cpp
    Binaural/SadieD2KU100Tests.cpp
    Binaural/CipicSubject003Tests.cpp
    Binaural/HutubsPP2Tests.cpp
    Binaural/BernschuetzKU100Tests.cpp
    Binaural/MitKemarLargePinnaTests.cpp
    Binaural/WoodworthFallbackTests.cpp
)

target_link_libraries(SpatialCoreTests
    PRIVATE
        Catch2::Catch2WithMain
        SpatialCore
)

# SPATIALCORE_HRTF_DIR: absolute path to the Git-LFS-tracked raw .sofa files
# (SpatialCore/HRTF/), so the per-profile Binaural tests can synchronously
# load each profile via HRTFDatabase::loadFromFile() (Phase 8 Plan 08-04).
#
# JUCE_UNIT_TESTS=1: exposes ADMOSCReceiver::testProcessOSCMessage() (Phase 8
# Plan 08-05), the same synchronous test-entry-point guard OSD's own
# OpenSpatialDelayTests target uses (see root CMakeLists.txt).
target_compile_definitions(SpatialCoreTests PRIVATE
    SPATIALCORE_HRTF_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../HRTF"
    JUCE_UNIT_TESTS=1
)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)
catch_discover_tests(SpatialCoreTests)
