# Testing Patterns

**Analysis Date:** 2026-08-09

## Test Framework

**Runner:**
- Catch2 v3.7.1 (via FetchContent from GitHub)
- Config: `tests/CMakeLists.txt`
- Integration: CTest with automatic test discovery via `catch_discover_tests()`

**Assertion Library:**
- Catch2 built-in macros: `REQUIRE()`, `CHECK()`
- Floating-point: `Catch::Approx()` for near-equality checks
- Matchers: `catch_matchers_string.hpp` for string assertions

**Run Commands:**
```bash
# Build tests (from build directory)
cmake --build . --target SpatialCoreTests

# Run all tests
./tests/SpatialCoreTests

# Run tests with tag filter
./tests/SpatialCoreTests "[dsp]"

# Run with verbose output
./tests/SpatialCoreTests -v

# Run through CTest (from build directory)
ctest --output-on-failure
```

## Test File Organization

**Location:**
- Tests are co-located in `tests/` directory mirroring `src/` structure
- Directory structure:
  ```
  tests/
  ├── Algorithms/
  │   └── SpatializationAlgorithmTests.cpp
  ├── IO/
  │   └── SpeakerLayoutTests.cpp
  ├── Trajectory/
  │   └── TrajectoryEngineTests.cpp
  └── DSP/
      └── UtilitiesTests.cpp
  ```

**Naming:**
- Convention: `{ComponentName}Tests.cpp` (e.g., `SpatializationAlgorithmTests.cpp`, `UtilitiesTests.cpp`)
- Each component gets its own test file
- Tests added to `tests/CMakeLists.txt` in source list

**Build Configuration:**
- All tests compiled into single executable: `SpatialCoreTests`
- Optional build: `SPATIALCORE_BUILD_TESTS` CMake flag (default `ON`)
- Tests run as part of CTest pipeline

## Test Structure

**Suite Organization:**
```cpp
#include <catch2/catch_test_macros.hpp>
#include <SpatialCore/Algorithms/VBAPAlgorithm.h>

using namespace spatialcore;

TEST_CASE("All algorithms instantiate and have names", "[algorithms]")
{
    // Arrange
    std::unique_ptr<SpatializationAlgorithm> algos[] = {
        std::make_unique<VBAPAlgorithm>(),
        std::make_unique<VBIPAlgorithm>(),
    };

    // Act & Assert
    for (auto& algo : algos)
    {
        REQUIRE(algo->getName().isNotEmpty());
    }
}
```

**Patterns:**
- Setup: Create objects/fixtures at beginning of test
- Teardown: Automatic (stack-allocated objects destroyed at scope end)
- Assertion: REQUIRE() for test failure conditions (must pass or test fails)
- Multiple assertions: Each test focuses on one behavior

## Mocking

**Framework:** None used (tests use real objects)

**Patterns:**
- Real object instantiation preferred: `std::make_unique<VBAPAlgorithm>()`
- Create minimal valid context structs for algorithm tests:
  ```cpp
  SpeakerLayout layout;
  layout.numSpeakers = 4;
  std::vector<VBAPTriplet> triplets;
  float decodeMatrix[1][MAX_SPEAKERS] = {};
  LayoutContext ctx { layout, triplets, decodeMatrix, 0 };
  ```
- No mocking framework (Catch2 provides no built-in mocks)

**What to Mock:**
- Nothing - tests instantiate real components
- Use fixture objects and minimal data instead

**What NOT to Mock:**
- Algorithms - test with real implementations
- Mathematical functions - test exact behavior
- Data structures - create real instances

## Fixtures and Factories

**Test Data:**
- Inline fixtures in each test (no shared test data factories)
- Example from `UtilitiesTests.cpp`:
  ```cpp
  TEST_CASE("softClip: passthrough below threshold", "[dsp]")
  {
      REQUIRE(spatialcore::DSP::softClip(0.0f) == Approx(0.0f));
      REQUIRE(spatialcore::DSP::softClip(0.5f) == Approx(0.5f));
      REQUIRE(spatialcore::DSP::softClip(-0.5f) == Approx(-0.5f));
      REQUIRE(spatialcore::DSP::softClip(0.79f) == Approx(0.79f));
  }
  ```

**Location:**
- Fixtures defined in test cases themselves
- No separate fixture files or base classes
- Use `std::make_unique<T>()` for object creation

**Minimal Context Pattern:**
```cpp
TEST_CASE("computeGains produces valid output", "[algorithms]")
{
    VBAPAlgorithm algo;
    SpeakerLayout layout;
    layout.numSpeakers = 4;
    std::vector<VBAPTriplet> triplets;
    float decodeMatrix[1][MAX_SPEAKERS] = {};
    LayoutContext ctx { layout, triplets, decodeMatrix, 0 };

    float gains[4] = { -1.0f, -1.0f, -1.0f, -1.0f };
    algo.computeGains({}, ctx, gains, 4);

    for (int i = 0; i < 4; ++i)
        REQUIRE(gains[i] >= 0.0f);
}
```

## Coverage

**Requirements:** None enforced (no coverage threshold configured)

**View Coverage:**
- Not configured; coverage requires separate tool (gcov, llvm-cov)
- No GitHub Actions or CI coverage uploads

**Target Coverage:**
- Core algorithms: Full coverage expected (< 5 lines of uncovered code)
- I/O registry: All format entries tested via parametric tests
- DSP utilities: Edge cases tested (NaN, infinity, symmetry)
- Trajectory engine: All 14 shapes tested

## Test Types

**Unit Tests:**
- Scope: Individual functions and classes
- Approach: Create minimal context, call function, verify output
- Example: `SpatializationAlgorithmTests.cpp` tests each algorithm's `computeGains()` independently

**Integration Tests:**
- Scope: Multiple components working together
- Approach: Chain algorithm output to format registry
- Example: Not currently present (could test full spatial rendering pipeline)

**E2E Tests:**
- Framework: Not used
- Would require: Full JUCE plugin instantiation and audio processing
- Not implemented in current test suite

## Common Patterns

**Floating-Point Testing:**
```cpp
using Catch::Approx;

TEST_CASE("outputLimiter: asymptotically approaches +2dB ceiling", "[dsp]")
{
    float ceiling = 1.2589f;
    float result = spatialcore::DSP::outputLimiter(5.0f);
    REQUIRE(result > ceiling * 0.99f);   // Within 1% of expected
    REQUIRE(result <= ceiling);          // Doesn't exceed ceiling
}

// With margin parameter
TEST_CASE("outputLimiter: near-passthrough for small values", "[dsp]")
{
    REQUIRE(spatialcore::DSP::outputLimiter(0.5f) == Approx(0.5f).margin(0.05f));
}
```

**Error Testing:**
```cpp
TEST_CASE("softClip: NaN returns zero", "[dsp]")
{
    REQUIRE(spatialcore::DSP::softClip(std::nanf("")) == 0.0f);
}

TEST_CASE("softClip: infinity returns zero", "[dsp]")
{
    REQUIRE(spatialcore::DSP::softClip(std::numeric_limits<float>::infinity()) == 0.0f);
    REQUIRE(spatialcore::DSP::softClip(-std::numeric_limits<float>::infinity()) == 0.0f);
}

TEST_CASE("softClip: odd symmetry", "[dsp]")
{
    REQUIRE(spatialcore::DSP::softClip(1.5f) == Approx(-spatialcore::DSP::softClip(-1.5f)));
}
```

**Static Method Testing:**
```cpp
TEST_CASE("OutputFormatRegistry: 23 formats registered", "[io]")
{
    REQUIRE(OutputFormatRegistry::getNumFormats() == 23);
}

TEST_CASE("OutputFormatRegistry: detectFromChannelCount", "[io]")
{
    REQUIRE(OutputFormatRegistry::detectFromChannelCount(2) == OutputFormat::Binaural);
}
```

**Algorithm Capability Testing:**
```cpp
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
```

## Test Tags

Used for grouping and filtering tests:

| Tag | Component | Tests |
|-----|-----------|-------|
| `[algorithms]` | Spatialization algorithms | `SpatializationAlgorithmTests.cpp` - VBAP, VBIP, KNN, DBAP, MDAP, Ambisonics, DirectBinaural instantiation and capabilities |
| `[io]` | Output formats and speaker layouts | `SpeakerLayoutTests.cpp` - OutputFormatRegistry format counts, channel requirements, format detection |
| `[trajectory]` | Trajectory engine shapes | `TrajectoryEngineTests.cpp` - Trajectory shape computation, base position fallback, degenerate cases |
| `[dsp]` | DSP utilities | `UtilitiesTests.cpp` - softClip() and outputLimiter() functions with edge cases (NaN, infinity, symmetry) |

## Current Test Coverage

**Algorithms (`tests/Algorithms/SpatializationAlgorithmTests.cpp`):**
- 7 algorithms tested: VBAP, VBIP, KNN, DBAP, MDAP, Ambisonics, DirectBinaural
- Tests: instantiation, name availability, capability flags, gain output bounds

**I/O (`tests/IO/SpeakerLayoutTests.cpp`):**
- OutputFormatRegistry: 23 format entries
- Tests: format count, channel requirements, LFE presence, height speaker presence, Ambisonics order, channel detection

**Trajectory (`tests/Trajectory/TrajectoryEngineTests.cpp`):**
- 14 trajectory shapes (None, Orbit, Triangle, etc.)
- Tests: shape enumeration, shape names, base position fallback, degenerate cases (zero-radius circle)

**DSP (`tests/DSP/UtilitiesTests.cpp`):**
- softClip(): passthrough, saturation, odd symmetry, NaN/infinity handling (5 tests)
- outputLimiter(): zero passthrough, near-passthrough, ceiling asymptotic behavior, odd symmetry, NaN handling (4+ tests)

## Running Tests

**From Build Directory:**
```bash
# Build and run
cmake --build . --target SpatialCoreTests
./tests/SpatialCoreTests

# Run specific tag
./tests/SpatialCoreTests "[algorithms]"

# Verbose output
./tests/SpatialCoreTests "[dsp]" -v

# Via CTest
ctest --output-on-failure
```

**In CI/GitHub Actions:**
- Tests run automatically on push/PR
- All tests must pass before merge
- Test discovery via CTest integration

---

*Testing analysis: 2026-08-09*
