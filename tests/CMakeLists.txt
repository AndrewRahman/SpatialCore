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
    Trajectory/TrajectoryEngineTests.cpp
    DSP/UtilitiesTests.cpp
)

target_link_libraries(SpatialCoreTests
    PRIVATE
        Catch2::Catch2WithMain
        SpatialCore
)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)
catch_discover_tests(SpatialCoreTests)
