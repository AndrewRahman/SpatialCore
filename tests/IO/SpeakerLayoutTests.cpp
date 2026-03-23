#include <catch2/catch_test_macros.hpp>
#include <SpatialCore/IO/OutputFormatRegistry.h>

using namespace spatialcore;

TEST_CASE("OutputFormatRegistry: 22 formats registered", "[io]")
{
    REQUIRE(OutputFormatRegistry::getNumFormats() == 22);
}

TEST_CASE("OutputFormatRegistry: Binaural is 2 channels", "[io]")
{
    auto& info = OutputFormatRegistry::getInfo(OutputFormat::Binaural);
    REQUIRE(info.requiredChannels == 2);
    REQUIRE(info.hasLFE == false);
}

TEST_CASE("OutputFormatRegistry: 7.1.4 Atmos is 12 channels with LFE and height", "[io]")
{
    auto& info = OutputFormatRegistry::getInfo(OutputFormat::Atmos_7_1_4);
    REQUIRE(info.requiredChannels == 12);
    REQUIRE(info.hasLFE == true);
    REQUIRE(info.hasHeight == true);
}

TEST_CASE("OutputFormatRegistry: HOA is 3rd order Ambisonics with 16 channels", "[io]")
{
    auto& info = OutputFormatRegistry::getInfo(OutputFormat::Ambi_HOA);
    REQUIRE(info.requiredChannels == 16);
    REQUIRE(info.isAmbisonicsOutput == true);
    REQUIRE(info.ambiOrder == 3);
}

TEST_CASE("OutputFormatRegistry: detectFromChannelCount", "[io]")
{
    REQUIRE(OutputFormatRegistry::detectFromChannelCount(2) == OutputFormat::Binaural);
}
