# Constraints

Synthesized: 2026-08-09
Paths are relative to repo root `/Users/andrewrahman/conductor/workspaces/SpatialCore/kelowna/`.

Source: `docs/superpowers/plans/2026-03-22-scaffold-spatialcore.md` (SPEC — highest-precedence
document in this ingest set; no ADRs exist). Per ingest guidance, SPEC items that the codebase map
shows as already implemented are recorded here as binding constraints, not as open requirements.

---

## Public header layout and module boundaries
- source: docs/superpowers/plans/2026-03-22-scaffold-spatialcore.md ("File Structure")
- type: api-contract
- content: Public headers under `include/SpatialCore/` in one subdirectory per module — `Core/` (SourcePosition.h, BinauralGains.h, Types.h), `Algorithms/` (SpatializationAlgorithm.h + VBAP, VBIP, KNN, DBAP, MDAP, Ambisonics, DirectBinaural), `Binaural/` (HRTFDatabase.h, PartitionedConvolver.h, BinauralRenderer.h), `IO/` (OutputFormat.h, OutputFormatRegistry.h, SpeakerLayout.h, AmbisonicsCodec.h), `OSC/` (ADMOSCReceiver.h, ADMOSCSender.h), `Trajectory/` (TrajectoryEngine.h), `DSP/` (Utilities.h), `UI/` (SpatialMapComponent.h, SMLLookAndFeel.h, ReverseSlider.h, IndicatorToggle.h, StyledButton.h), plus umbrella `SpatialCore.h`. Implementations mirror this layout under `src/`; tests mirror it under `tests/`.

## Root CMake build contract
- source: docs/superpowers/plans/2026-03-22-scaffold-spatialcore.md (Task 1, Step 1)
- type: api-contract
- content: `cmake_minimum_required(VERSION 3.22)`; `project(SpatialCore VERSION 1.0.0 LANGUAGES C CXX)`; C++17 required. JUCE resolved from an existing `juce::juce_core` target, else `../JUCE`, else `./JUCE`, else `find_package(JUCE REQUIRED)`. libmysofa fetched at GIT_TAG v1.3.2 with BUILD_STATIC_LIBS=ON / BUILD_SHARED_LIBS=OFF / BUILD_TESTS=OFF; `find_package(ZLIB REQUIRED)`. Target `SpatialCore` is STATIC. PUBLIC include dir `$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>` / `$<INSTALL_INTERFACE:include>`. PUBLIC links: juce_core, juce_audio_basics, juce_audio_formats, juce_dsp, juce_gui_basics, juce_osc. PRIVATE links: mysofa-static, ZLIB::ZLIB. `target_compile_features(SpatialCore PUBLIC cxx_std_17)`. Option `SPATIALCORE_BUILD_TESTS` defaults ON and adds `tests/`.

## Platform and optimization flags
- source: docs/superpowers/plans/2026-03-22-scaffold-spatialcore.md (Task 1, Step 1)
- type: nfr
- content: APPLE — `CMAKE_OSX_ARCHITECTURES "arm64"`, `CMAKE_OSX_DEPLOYMENT_TARGET "12.0"`, target options `-ffast-math -Wno-nan-infinity-disabled`. MSVC — `/Zc:preprocessor`, `_CRT_SECURE_NO_WARNINGS`, target option `/fp:fast`.

## Core type and constant schema
- source: docs/superpowers/plans/2026-03-22-scaffold-spatialcore.md (Task 1, Steps 2-4)
- type: schema
- content: `namespace spatialcore` — `static constexpr int MAX_SOURCES = 12;` and `static constexpr int MAX_SPEAKERS = 16;`. `struct SourcePosition { float azimuthRad = 0.0f; float elevationRad = 0.0f; float distance = 1.0f; };`. `struct BinauralGains { float leftGain = 0.0f; float rightGain = 0.0f; float leftDelaySamples = 0.0f; float rightDelaySamples = 0.0f; };`.

## SpeakerLayout schema and ITU-R layout factories
- source: docs/superpowers/plans/2026-03-22-scaffold-spatialcore.md (Task 2, Step 1)
- type: schema
- content: `struct VirtualSpeaker { float azimuthRad; float elevationRad; };`. `struct VBAPTriplet { int i, j, k; float inv[3][3]; };`. `struct SpeakerLayout { int numSpeakers; int lfeChannelIndex = -1; int totalChannels; struct Speaker { float azimuthRad; float elevationRad; int channelIndex; }; Speaker speakers[MAX_SPEAKERS]; };`. Predefined ITU-R BS.775/BS.2051 factories in `namespace Layouts`: getQuad, get5_0, get5_1, get7_0, get7_1, getOctaphonic, get5_1_2, get5_1_4, get7_1_2, get7_1_4, get7_1_6, get9_1_4, get9_1_6, getVirtualBinaural16.

## OutputFormat enumeration and metadata schema
- source: docs/superpowers/plans/2026-03-22-scaffold-spatialcore.md (Task 2, Step 2)
- type: schema
- content: `enum class OutputFormat` with Binaural, Stereo, Quad, Surround_5_0, Surround_5_1, Surround_7_0, Surround_7_1, Octaphonic, Atmos_5_1_2, Atmos_5_1_4, Atmos_7_1_2, Atmos_7_1_4, Atmos_7_1_6, Atmos_9_1_4, Atmos_9_1_6, SML_13_1, Ambi_FOA, Ambi_SOA, Ambi_HOA, Ambi_4OA, Ambi_5OA, Ambi_6OA, NumFormats (22 formats). `struct OutputFormatInfo { OutputFormat format; const char* name; const char* shortName; int requiredChannels; bool hasLFE; bool hasHeight; bool isAmbisonicsOutput; int ambiOrder; bool isStereoVariant; };`.

## OutputFormatRegistry API
- source: docs/superpowers/plans/2026-03-22-scaffold-spatialcore.md (Task 2, Step 3)
- type: api-contract
- content: All-static class: `getInfo(OutputFormat)`, `getAllFormats()`, `getNumFormats()`, `detectFromChannelCount(int)`, `getLayoutForFormat(OutputFormat)`, `getTripletsForFormat(OutputFormat)`; private `initLayouts()`.

## AmbisonicsCodec API and order limits
- source: docs/superpowers/plans/2026-03-22-scaffold-spatialcore.md (Task 2, Step 4)
- type: api-contract
- content: `MAX_AMBI_ORDER = 6`; `MAX_AMBI_CHANNELS = (MAX_AMBI_ORDER + 1)^2 = 49`. Static methods: `encode(const SourcePosition&, int order, float* shCoeffs, int numCoeffs)`, `getDecodeMatrix(int order, int numSpeakers, const float* speakerAzimuths, const float* speakerElevations, float* decodeMatrix)`, `evaluateSH(int l, int m, float azimuthRad, float elevationRad)`, `applyMaxREWeights(float* shCoeffs, int order)`.

## SpatializationAlgorithm interface and context structs
- source: docs/superpowers/plans/2026-03-22-scaffold-spatialcore.md (Task 3, Step 1); duplicated in docs/development-roadmap.md ("Key Interface (Frozen)")
- type: api-contract
- content: `struct LayoutContext { const SpeakerLayout& layout; const std::vector<VBAPTriplet>& triplets; const float (*ambiDecodeMatrix)[MAX_SPEAKERS]; int ambiNumSpeakers; };` and `struct BinauralContext { int profileIndex; double sampleRate; };`. Abstract class `SpatializationAlgorithm` with virtual dtor; pure virtual `computeGains(const SourcePosition&, const LayoutContext&, float* outputGains, int numSpeakers) const`; `supportsBinauralDirect()` default false; `computeBinauralGains(const SourcePosition&, const BinauralContext&) const` default `{}`; `supportsSurround()` default true; `supportsSHDomain()` default false; pure virtual `juce::String getName() const`. Concrete algorithms inherit and override (e.g. VBAPAlgorithm returns "VBAP").

## Binaural module responsibilities
- source: docs/superpowers/plans/2026-03-22-scaffold-spatialcore.md (Task 4, Steps 1-2)
- type: api-contract
- content: HRTFDatabase wraps libmysofa. PartitionedConvolver performs FFT overlap-save. BinauralRenderer manages 12 per-source convolver pairs. HRTFDatabase must NOT reference BinaryData; `loadFromMemory()` accepts raw data pointers provided by the consumer.

## ADM-OSC protocol contract
- source: docs/superpowers/plans/2026-03-22-scaffold-spatialcore.md (Task 5, Step 1); restated in docs/development-roadmap.md (Phase 1 module table)
- type: protocol
- content: ADMOSCReceiver parses `/adm/obj/N/{azim,elev,dist,aed,xyz}`. ADMOSCSender broadcasts `/adm/obj/N/aed` at 30 Hz with position-change gating.

## TrajectoryEngine API and shape enumeration
- source: docs/superpowers/plans/2026-03-22-scaffold-spatialcore.md (Task 6, Step 1)
- type: api-contract
- content: `enum class TrajectoryShape { None, Bounce, Circle, Cross, Figure8, Heart, Helix, Infinity, Line, Orbit, Random, Spiral, Square, Triangle, NumShapes }` (13 shapes plus None). `struct TrajectoryResult { float azDeg; float elDeg; float dist; bool controlsAz; bool controlsEl; bool controlsDist; };`. Static API: `compute(TrajectoryShape shape, float phase, float baseAzDeg, float baseElDeg, float baseDist, bool reverse = false)`, `getNumShapes()`, `getShapeName(TrajectoryShape)`.

## DSP utilities behavior contract
- source: docs/superpowers/plans/2026-03-22-scaffold-spatialcore.md (Task 7, Step 1)
- type: api-contract
- content: Header-only inline functions in `spatialcore::DSP`. `softClip(float)` — non-finite input returns 0.0f; threshold 0.8f; above/below threshold applies `threshold + (x - threshold) / (1 + (x - threshold)^2)` (mirrored for negatives); otherwise passthrough. `outputLimiter(float)` — non-finite input returns 0.0f; ceiling 1.2589f (+2 dB) applied as a hard clamp (`if (x > ceiling) return ceiling; if (x < -ceiling) return -ceiling; return x;`).
- note: The implemented `outputLimiter` uses `ceiling * tanh(x / ceiling)` rather than a hard clamp. See INGEST-CONFLICTS.md.

## UI component decoupling
- source: docs/superpowers/plans/2026-03-22-scaffold-spatialcore.md (Task 8, Step 1)
- type: api-contract
- content: UI headers carry full class declarations matching OpenSpatialDelay signatures but decoupled from OpenSpatialDelayProcessor — SpatialMapComponent uses an abstract Listener interface instead of a processor pointer.

## Umbrella header contract
- source: docs/superpowers/plans/2026-03-22-scaffold-spatialcore.md (Task 9, Step 1)
- type: api-contract
- content: `include/SpatialCore/SpatialCore.h` aggregates every public header across Core, Algorithms (base + 7 implementations), Binaural, IO, OSC, Trajectory, DSP, UI. Consumers include only this header.

## Test target contract
- source: docs/superpowers/plans/2026-03-22-scaffold-spatialcore.md (Task 10, Steps 1-2)
- type: api-contract
- content: `tests/CMakeLists.txt` fetches Catch2 at GIT_TAG v3.7.1 and builds executable `SpatialCoreTests` from Algorithms/SpatializationAlgorithmTests.cpp, IO/SpeakerLayoutTests.cpp, Trajectory/TrajectoryEngineTests.cpp, DSP/UtilitiesTests.cpp. Links `Catch2::Catch2WithMain` and `SpatialCore`. Appends `${catch2_SOURCE_DIR}/extras` to CMAKE_MODULE_PATH, includes CTest and Catch, and calls `catch_discover_tests(SpatialCoreTests)`. No TestMain.cpp — Catch2WithMain supplies `main()`.

## Build verification gate
- source: docs/superpowers/plans/2026-03-22-scaffold-spatialcore.md (Task 11)
- type: nfr
- content: `cmake -B build -DCMAKE_BUILD_TYPE=Release` must configure (finding JUCE, fetching libmysofa and Catch2); `cmake --build build --config Release` must compile the static library without errors; `./build/SpatialCoreTests` must pass all tests.

## Realtime safety of the audio path
- source: docs/development-roadmap.md ("Critical Design Rules", rule 1); CLAUDE.md ("Design Principles", "Critical Rules")
- type: nfr
- content: No malloc, locks, or logging in any function called from processBlock. Algorithms are stateless — all computation state lives in context structs. Layout changes use dual-buffered atomic swap so the audio thread reads lock-free.
