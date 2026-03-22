#include <catch2/catch_test_macros.hpp>
#include <SpatialCore/Algorithms/VBAPAlgorithm.h>
#include <SpatialCore/Algorithms/VBIPAlgorithm.h>
#include <SpatialCore/Algorithms/KNNAlgorithm.h>
#include <SpatialCore/Algorithms/DBAPAlgorithm.h>
#include <SpatialCore/Algorithms/MDAPAlgorithm.h>
#include <SpatialCore/Algorithms/AmbisonicsAlgorithm.h>
#include <SpatialCore/Algorithms/DirectBinauralAlgorithm.h>
#include <memory>

using namespace spatialcore;

TEST_CASE("All algorithms instantiate and have names", "[algorithms]")
{
    std::unique_ptr<SpatializationAlgorithm> algos[] = {
        std::make_unique<VBAPAlgorithm>(),
        std::make_unique<VBIPAlgorithm>(),
        std::make_unique<KNNAlgorithm>(),
        std::make_unique<DBAPAlgorithm>(),
        std::make_unique<MDAPAlgorithm>(),
        std::make_unique<AmbisonicsAlgorithm>(),
        std::make_unique<DirectBinauralAlgorithm>(),
    };

    for (auto& algo : algos)
    {
        REQUIRE(algo->getName().isNotEmpty());
    }
}

TEST_CASE("DirectBinaural supports binaural, not surround", "[algorithms]")
{
    DirectBinauralAlgorithm algo;
    REQUIRE(algo.supportsBinauralDirect() == true);
    REQUIRE(algo.supportsSurround() == false);
}

TEST_CASE("Ambisonics supports SH domain", "[algorithms]")
{
    AmbisonicsAlgorithm algo;
    REQUIRE(algo.supportsSHDomain() == true);
}

TEST_CASE("VBAP supports surround, not binaural", "[algorithms]")
{
    VBAPAlgorithm algo;
    REQUIRE(algo.supportsBinauralDirect() == false);
    REQUIRE(algo.supportsSurround() == true);
}

TEST_CASE("computeGains zero-fills output (stub)", "[algorithms]")
{
    VBAPAlgorithm algo;
    SpeakerLayout layout;
    layout.numSpeakers = 4;
    std::vector<VBAPTriplet> triplets;
    float decodeMatrix[1][MAX_SPEAKERS] = {};
    LayoutContext ctx { layout, triplets, decodeMatrix, 0 };

    float gains[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    algo.computeGains({}, ctx, gains, 4);

    for (int i = 0; i < 4; ++i)
        REQUIRE(gains[i] == 0.0f);
}
