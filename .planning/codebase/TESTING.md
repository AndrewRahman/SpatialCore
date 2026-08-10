# Testing Patterns

**Analysis Date:** 2026-08-10

**Verification method:** This document was produced by actually configuring, building, and running the test suite on branch `gsd-remap` (HEAD `8d1868e`), not by reading source alone. A previous mapping run analyzed a different branch and incorrectly reported ~5% coverage with binaural/HRTF/convolver/OSC/Ambisonics entirely untested — that claim is FALSE for this branch. See "Build & Run Evidence" below for raw output.

## Test Framework

**Runner:**
- Catch2 v3.7.1, fetched via CMake `FetchContent` (`tests/CMakeLists.txt`), not vendored/system-installed.
- Test executable target name: `SpatialCoreTests` (built via `add_executable(SpatialCoreTests ...)` in `tests/CMakeLists.txt`).
- Build option to enable the test target: `SPATIALCORE_BUILD_TESTS` (default `ON`, declared in root `CMakeLists.txt:279`), gating `add_subdirectory(tests)`.
- CTest integration via `catch_discover_tests(SpatialCoreTests)` — each Catch2 `TEST_CASE` is registered as an individual CTest test.

**Assertion Library:**
- Catch2's own `REQUIRE`/`CHECK` macros plus `catch2/matchers/catch_matchers_floating_point.hpp` (`Catch::Matchers::WithinAbs`) for float-tolerance comparisons — used pervasively for golden-value DSP checks.

**Run Commands (verified working):**
```bash
cmake -B build-map -DCMAKE_BUILD_TYPE=Release -DSPATIALCORE_BUILD_TESTS=ON
cmake --build build-map --target SpatialCoreTests -j4
./build-map/tests/SpatialCoreTests
```
Alternative (CTest, one process per TEST_CASE): `ctest --test-dir build-map`.

## Build & Run Evidence (2026-08-10)

**Configure:** succeeded cleanly (JUCE fetched/built `juceaide`, libmysofa fetched via `FetchContent`, ZLIB found system-wide). Only warning was a CMake deprecation notice from the vendored `mysofa` `CMakeLists.txt` (`Compatibility with CMake < 3.10 will be removed`) — not a SpatialCore issue.

**Build:** `cmake --build build-map --target SpatialCoreTests -j4` completed with `[100%] Built target SpatialCoreTests` and zero compiler errors/warnings surfaced in the tail output. All 16 test `.cpp` files compiled, along with the required JUCE modules pulled into the test binary (`juce_core`, `juce_audio_basics`, `juce_audio_formats` incl. FLAC codec objects, `juce_dsp`, `juce_osc`, `juce_events`).

**Run:** `./build-map/tests/SpatialCoreTests` produced:
```
Randomness seeded to: 2149913973
===============================================================================
All tests passed (1585 assertions in 144 test cases)
```
144 test cases, 1585 assertions, 100% pass, zero failures, zero skips.

**Git LFS status:** all 5 `.sofa` HRTF files under `HRTF/` are real binary content (not LFS pointer stubs) at the time of this run — `bernschuetz_ku100.sofa` (~19.7 MB), `cipic_subject_003.sofa` (~1.8 MB), `hutubs_pp2.sofa` (~1.7 MB), `mit_kemar_large_pinna.sofa` (~1.2 MB), `sadie_d2_ku100.sofa` (~36.6 MB). This means the per-profile Binaural golden-checksum tests ran against real SOFA data, not synthetic/mocked HRIRs.

## Test File Organization

**Location:** `tests/<Category>/<Module>Tests.cpp`, mirroring the `src/<Category>/` and `include/SpatialCore/<Category>/` layout (e.g. `src/Binaural/PartitionedConvolver.cpp` ↔ tests live in `tests/Binaural/*Tests.cpp` covering it indirectly via `BinauralRenderer`/HRTF profile tests and `RenderEngineTests.cpp`).

**Naming:** `<ModuleOrConcern>Tests.cpp`, one file per concern (not strictly one per source file — e.g. all 8 algorithms share one `SpatializationAlgorithmTests.cpp`; each of the 5 HRTF profiles gets its own file).

## Per-File Coverage Map

All 16 files below are enumerated via `find tests -name "*.cpp"` and each is explicitly wired into `tests/CMakeLists.txt`'s `add_executable(SpatialCoreTests ...)` source list — none are orphaned/unbuilt.

| File | TEST_CASEs | What it covers |
|---|---|---|
| `tests/Core/SpatialMathTests.cpp` | 13 | `softClip`, `outputLimiter`, `distanceAttenuation` in `src/Core/SpatialMath.cpp` — non-finite input handling, threshold/asymptote behavior, odd symmetry, golden values, denominator clamping |
| `tests/Algorithms/SpatializationAlgorithmTests.cpp` | 18 | All 8 `SpatializationAlgorithm` implementations (`ConstantPower`, `VBAP`, `VBIP`, `KNN`, `DBAP`, `MDAP`, `Ambisonics`, `DirectBinaural`) in `src/Algorithms/*.cpp` — capability flags (`supportsBinauralDirect`/`supportsSurround`/`supportsSHDomain`), golden gain vectors, symmetry, spread |
| `tests/IO/SpeakerLayoutTests.cpp` | 13 | `OutputFormatRegistry` (23-format table, channel counts, display names, `detectFromChannelCount`) and `SpeakerLayout` (speaker/channel counts, height flags, VBAP triplets, channel-index overlap) in `src/IO/OutputFormatRegistry.cpp` / `src/IO/SpeakerLayout.cpp` |
| `tests/IO/AmbisonicsCodecTests.cpp` | 7 | `AmbisonicsCodec` in `src/IO/AmbisonicsCodec.cpp` — `evaluateSH` (orders 0/1, crossfeed), `encode` order truncation, `getDecodeMatrix` well-formedness/determinism for 7.1.4, `applyMaxREWeights` |
| `tests/OSC/ADMOSCReceiverTests.cpp` | 18 | `ADMOSCReceiver` in `src/OSC/ADMOSCReceiver.cpp` — ADM `/adm/obj/N/...` and OSD-alias `/osd/obj/N/...` address parsing (azim/elev/dist/aed/xyz), per-object and global param forwarding, edge cases (out-of-range object, unknown property, malformed address), listener removal, all 12 objects addressable. Uses the `JUCE_UNIT_TESTS=1`-gated synchronous test entry point `testProcessOSCMessage()` |
| `tests/OSC/ADMOSCSenderTests.cpp` | 3 | `ADMOSCSender` in `src/OSC/ADMOSCSender.cpp` — 1-based object numbering in `/adm/obj/N/aed`, disconnected-state no-op, out-of-range index guard |
| `tests/OSC/OSCPortValidationTests.cpp` | 5 | `oscPortsConflict` in `src/OSC/OSCPortValidation.cpp` — loopback vs non-loopback port-conflict detection, bind-gating behavior |
| `tests/Trajectory/TrajectoryTests.cpp` | 29 | `TrajectoryEngine` in `src/Trajectory/TrajectoryEngine.cpp` — all 13 trajectory shapes (None, Orbit, Bounce, Circle, Cross, Figure8, Heart, Helix, Infinity, Line, Random, Spiral, Square, Triangle) at characteristic phases, axis-control flags, azimuth wrapping |
| `tests/Trajectory/DopplerTests.cpp` | 9 | `DopplerVelocity` in `src/Trajectory/DopplerVelocity.cpp` — stationary/approaching/receding pitch shift, EMA smoothing, reset/clearDisabled state, semitone clamping to ±12 |
| `tests/Binaural/WoodworthFallbackTests.cpp` | 4 | Woodworth ITD/ILD analytic fallback path (CPU-lite binaural option, no SOFA file) — center/hard-left/hard-right gain+delay, negative control confirming no SOFA involvement |
| `tests/Binaural/BernschuetzKU100Tests.cpp` | 3 | `HRTFDatabase` (`src/Binaural/HRTFDatabase.cpp`) loading `HRTF/bernschuetz_ku100.sofa` — synchronous load success, golden HRIR checksum at az=90°, negative control (unloaded DB has no data) |
| `tests/Binaural/CipicSubject003Tests.cpp` | 3 | Same pattern as above against `HRTF/cipic_subject_003.sofa` |
| `tests/Binaural/HutubsPP2Tests.cpp` | 3 | Same pattern as above against `HRTF/hutubs_pp2.sofa` |
| `tests/Binaural/MitKemarLargePinnaTests.cpp` | 4 | Same pattern against `HRTF/mit_kemar_large_pinna.sofa`, plus an ITD pass-through check confirming raw SOFA delay is preserved verbatim |
| `tests/Binaural/SadieD2KU100Tests.cpp` | 3 | Same pattern against `HRTF/sadie_d2_ku100.sofa` |
| `tests/Engine/RenderEngineTests.cpp` | 9 | `RenderEngine` in `src/Engine/RenderEngine.cpp` — direct-binaural HRTF branch (finite non-silent output, oversized-block heap-corruption survival), simple Woodworth binaural branch (live + zero-gain), stereo-variant branch, Ambisonics branch (W-channel), discrete-surround branch (channel routing), `setOutputFormat` swap visibility, escape hatches exposing internal `BinauralRenderer` instances |

**Total:** 144 TEST_CASE blocks executed (150 `TEST_CASE` string occurrences found by grep include a few in multi-line macro continuations/comments; the Catch2 runtime count of 144 is authoritative), 1585 assertions, all passing.

## Modules With No Dedicated Test File

Based on the file list above cross-referenced against `src/`, the following have no dedicated test file (though some are exercised indirectly through `RenderEngineTests.cpp` or `PartitionedConvolver`-adjacent Binaural tests):

- `src/Binaural/PartitionedConvolver.cpp` — no direct `PartitionedConvolverTests.cpp`. Exercised only indirectly through `BinauralRenderer`/`RenderEngine` integration tests (e.g. `RenderEngineTests.cpp`'s direct-binaural branch tests), not via unit tests targeting the convolver's dual-slot crossfade state machine, `SharedFFTCache`, or `setIR` transition logic in isolation.
- `src/Binaural/BinauralRenderer.cpp` — no dedicated `BinauralRendererTests.cpp`; covered only indirectly via `RenderEngineTests.cpp` and the per-profile HRTF database tests.
- `src/UI/*.cpp` (all 10 files: `GlobalTapDrawer`, `IOSectionComponent`, `IndicatorToggle`, `OSCSectionComponent`, `ObjectPanel`, `PresetBrowser`, `ReverseSlider`, `SMLLookAndFeel`, `SpatialMapComponent`, `StyledButton`) — zero test coverage. These are compiled into the separate `SpatialCoreUI` target, deliberately excluded from the DSP-only `SpatialCore` library that `SpatialCoreTests` links against, so they are architecturally out of scope for the current unit-test suite.

## Test Structure

**Suite Organization (from `tests/Binaural/WoodworthFallbackTests.cpp`):**
```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <SpatialCore/Algorithms/DirectBinauralAlgorithm.h>
#include <SpatialCore/Core/Types.h>
#include <array>

using namespace spatialcore;
using Catch::Matchers::WithinAbs;

namespace
{
    // Verbatim from Source/PluginProcessor.cpp, moved to <SpatialCore/Core/Types.h>.
    const std::array<BinauralProfile, 5> kBinauralProfiles = {{ ... }};

    SourcePosition makeSource (float azDeg, float elDeg, float distance)
    {
        return { juce::degreesToRadians (azDeg), juce::degreesToRadians (elDeg), distance };
    }
}

TEST_CASE ("Woodworth fallback — center source (az=0) produces equal, undelayed L/R gains", "[binaural][woodworth]")
{
    // ... REQUIRE / CHECK with WithinAbs matchers
}
```

**Patterns:**
- File-level anonymous `namespace { ... }` blocks hold shared test fixtures/helpers (constant tables, position-builder free functions) scoped to that file only — no shared test-utility header observed across files.
- Catch2 tag conventions: bracketed tags like `[binaural][woodworth]`, `[algorithms][vbap][golden]`, `[osc][edge]`, `[trajectory][characterization]` classify tests by module and intent (`golden` = exact reference-value comparison, `edge` = boundary/invalid-input, `characterization` = documents current behavior, `negative-control`/`own control` = binaural test-methodology markers distinguishing "no real DSP happened" checks from "real DSP happened correctly" checks).
- Long, descriptive `TEST_CASE` name strings function as executable documentation (e.g. `"MIT KEMAR Large Pinna — ITD pass-through: raw SOFA delay is zero (preserved verbatim, D-09)"`), often citing a plan/decision ID (`D-09`, `08-02`).

## Mocking

**Framework:** None. No mocking library (no GoogleMock, no Trompeloeil, no hand-rolled mock macros) is used anywhere in `tests/`.

**Patterns:** Tests exercise real implementations directly — real `HRTFDatabase` loading real `.sofa` files from disk (via `SPATIALCORE_HRTF_DIR` compile definition pointing at `HRTF/`), real `TrajectoryEngine`/`DopplerVelocity` state machines, real `RenderEngine` instances processing real audio buffers. This is a characterization/golden-value testing style rather than isolated-unit-with-mocks style.

**What to Mock:** Nothing is currently mocked; there is no test seam/interface designed for mocking in this codebase.

**What NOT to Mock:** SOFA/HRTF loading is deliberately tested against real binary data files (Git-LFS-tracked) rather than synthetic stand-ins, to catch real-world parsing/format regressions — see the "own control" language in `WoodworthFallbackTests.cpp` explaining the per-profile testing methodology.

## Fixtures and Factories

**Test Data:**
```cpp
// tests/Binaural/WoodworthFallbackTests.cpp
SourcePosition makeSource (float azDeg, float elDeg, float distance)
{
    return { juce::degreesToRadians (azDeg), juce::degreesToRadians (elDeg), distance };
}
```
Small free-function factories like `makeSource` are defined per-file in an anonymous namespace, not shared across test files.

**Location:** No `tests/fixtures/` or `tests/support/` directory exists. Real HRTF binary fixtures live in `HRTF/*.sofa` at repo root (Git LFS), referenced at test-compile time via the `SPATIALCORE_HRTF_DIR` preprocessor definition set in `tests/CMakeLists.txt`.

## Coverage Tooling

**No coverage tooling is configured.** There is no `gcov`/`lcov`/`llvm-cov`/`--coverage` compiler flag, no `codecov.yml`, and no coverage target in any `CMakeLists.txt` in this repo. No coverage percentage can be measured from the current build configuration — any specific percentage figure (including any previously reported "~5%") is not derived from actual instrumented coverage data and should not be treated as a real coverage measurement. The only quantitative signal available is the TEST_CASE/assertion count reported above (144 cases / 1585 assertions, verified by execution).

## Test Types

**Unit Tests:** The bulk of the suite — `Core`, `Algorithms`, `IO`, `OSC`, `Trajectory` tests exercise individual pure functions/classes in isolation with golden values and edge cases.

**Integration Tests:** `tests/Engine/RenderEngineTests.cpp` exercises `RenderEngine` end-to-end across multiple output branches (binaural, stereo, Ambisonics, discrete-surround) with real audio buffers, and the `Binaural/*ProfileTests.cpp` files integration-test `HRTFDatabase` against real on-disk SOFA files.

**E2E Tests:** Not used — no plugin-host-level or DAW-level test harness exists in this repo (SpatialCore is a library, not a plugin; end-to-end plugin testing is a concern of consumer projects like OpenSpatialDelay).

## Common Patterns

**Float-tolerance assertions:**
```cpp
REQUIRE_THAT (gain, WithinAbs (expected, 0.001f));
```
Used throughout golden-value DSP tests instead of exact equality, given floating-point computation.

**Negative-control pattern (Binaural suite specific):**
Each of the 5 HRTF-profile test files includes a "negative control: unloaded database has no data" `TEST_CASE`, verifying the test methodology itself — i.e. that a genuinely-unloaded `HRTFDatabase` produces detectably-empty state, so that the "golden checksum" tests in the same file can be trusted to mean the SOFA data actually loaded.

---

*Testing analysis: 2026-08-10*
