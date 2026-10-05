include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1
)
FetchContent_MakeAvailable(Catch2)

add_executable(SpatialCoreTests
    Algorithms/SpatializationAlgorithmTests.cpp
    Algorithms/PanningLawTests.cpp
    IO/SpeakerLayoutTests.cpp
    IO/AmbisonicsCodecTests.cpp
    OSC/ADMOSCReceiverTests.cpp
    OSC/ADMOSCSenderTests.cpp
    OSC/OSCPortValidationTests.cpp
    Trajectory/TrajectoryTests.cpp
    Trajectory/DopplerTests.cpp
    Core/SpatialMathTests.cpp
    Core/CountsTests.cpp
    Core/VBAPTripletSelectionTests.cpp
    Binaural/SadieD2KU100Tests.cpp
    Binaural/CipicSubject003Tests.cpp
    Binaural/HutubsPP2Tests.cpp
    Binaural/BernschuetzKU100Tests.cpp
    Binaural/MitKemarLargePinnaTests.cpp
    Binaural/WoodworthFallbackTests.cpp
    Binaural/BinauralCueTests.cpp
    Binaural/PartitionedConvolverTests.cpp
    Binaural/BinauralRendererTests.cpp
    Binaural/ProfileLoudnessTests.cpp
    Binaural/HRTFEmbeddedTests.cpp
    Engine/RenderEngineTests.cpp
    Engine/TripleBufferIndexTests.cpp
    Engine/ProfileSwitchTests.cpp
    Engine/ControlRouteTests.cpp
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
#
# JUCE_MODAL_LOOPS_PERMITTED=1: juce::MessageManager::runDispatchLoopUntil only
# exists under this flag (JUCE 9.0.0 juce_MessageManager.h:99-105). A test needs
# it to pump the message loop so a real-UDP ADMOSCReceiver (MessageLoopCallback)
# delivers its messages (Phase 4, Plan 04-01).
target_compile_definitions(SpatialCoreTests PRIVATE
    SPATIALCORE_HRTF_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../HRTF"
    JUCE_UNIT_TESTS=1
    JUCE_MODAL_LOOPS_PERMITTED=1
)

# SpatialCoreUITests: the only target that links SpatialCoreUI (SpatialCoreTests deliberately
# does not). UI code is message-thread code, so the tests construct components without a window
# and paint them into an Image; the JUCE GUI initialiser is owned by a Catch2 listener in
# UI/UITestMain.cpp. No JUCE_MODAL_LOOPS_PERMITTED: UI tests never pump the message loop.
# The "ui: " ctest prefix keeps ctest names unique across the two Catch2 executables.
add_executable(SpatialCoreUITests
    UI/UITestMain.cpp
    UI/MapRouteTests.cpp
)

target_link_libraries(SpatialCoreUITests
    PRIVATE
        Catch2::Catch2WithMain
        SpatialCoreUI
        SpatialCore
)

target_compile_definitions(SpatialCoreUITests PRIVATE
    JUCE_UNIT_TESTS=1
)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)
catch_discover_tests(SpatialCoreTests)
catch_discover_tests(SpatialCoreUITests TEST_PREFIX "ui: ")
