# SpatialCore — Project Context for Claude

## Identity
- **Library:** SpatialCore — the shared spatial audio rendering engine for the Spatial Media Library
- **Organization:** Spatial Media Lab (spatialmedialab.org)
- **What it does:** Takes audio objects with 3D positions and renders them to any output format (binaural, stereo, surround, Ambisonics) via 8 spatialization algorithms
- **License:** GPL-3.0 + commercial (dual license)
- **GitHub:** `https://github.com/Spatial-Media-Lab/SpatialCore`

## Origin
SpatialCore was extracted from OpenSpatialDelay v1.0, where 68% of the codebase was marked as reusable spatial audio infrastructure. The extraction separated framework code (algorithms, HRTF, I/O, OSC, trajectories, UI) from delay-specific code (delay line, pitch shift, feedback, wobble).

## Architecture

### Core Components
| Component | Headers | Description |
|-----------|---------|-------------|
| Algorithms | `Algorithms/*.h` | 8 spatialization algorithms: ConstantPower, VBAP, VBIP, KNN, DBAP, MDAP, Ambisonics, DirectBinaural |
| Binaural | `Binaural/*.h` | SharedFFTCache (process-global FFT singleton), HRTFDatabase (SOFA/libmysofa), PartitionedConvolver (FFT overlap-save), BinauralRenderer (12 per-source convolvers) |
| Engine | `Engine/RenderEngine.h` | `RenderEngine` — the consumer-facing render facade. Owns the 5 render paths (direct-binaural HRTF, simple binaural Woodworth, stereo variants, Ambisonics HOA, discrete surround), the glitch-free double-buffered output-format/HRTF-profile swap, and (opt-in, SC-13) per-object gain computation via `RenderBlockContext::engineComputesGains` |
| Core | `Core/SpatialMath.h` | `softClip()`, `outputLimiter()` (tanh soft ceiling), `distanceAttenuation()`, plus the shared position/gain types in `Core/Types.h` |
| I/O | `IO/*.h` | OutputFormatRegistry (23 formats), SpeakerLayout (15 ITU-R layouts), AmbisonicsCodec (SH eval, decode matrices) |
| OSC | `OSC/*.h` | ADM-OSC Receive (parse /adm/obj/N/), ADM-OSC Send (30Hz broadcast) |
| Trajectory | `Trajectory/*.h` | 13 shapes, origin-point architecture, forward/reverse |
| UI | `UI/*.h` | SpatialMapComponent, SMLLookAndFeel, ReverseSlider, IndicatorToggle, StyledButton |

### Key Interfaces

**`RenderEngine` — the consumer-facing render surface.** A consumer plugin drives
rendering through this facade only:
```cpp
class RenderEngine {
public:
    void prepare(double sampleRate, int maxBlockSize);
    void renderBlock(const RenderSources& sources,
                      const RenderBlockContext& blockCtx,
                      float* const* outChannels, int numOutCh);
    void setOutputFormat(OutputFormat format);
    // ...
};
```
`RenderBlockContext::engineComputesGains` (default `false`, SC-13) is the opt-in flag
that lets `RenderEngine` compute `objChannelGains`/`objGains` internally instead of
requiring the consumer to precompute them. Stereo-variant gains (`objGainL`/
`objGainR`) always stay consumer-side — stereo gain math is not a
`SpatializationAlgorithm` concern.

**`SpatializationAlgorithm` — engine-internal.** Not a consumer-facing interface;
reached only through `RenderEngine`, never dispatched directly by a consumer:
```cpp
// Abstract base — all algorithms implement this
class SpatializationAlgorithm {
    virtual void computeGains(const SourcePosition& source,
                              const LayoutContext& ctx,
                              float* outputGains, int numSpeakers) const = 0;
    virtual bool supportsBinauralDirect() const { return false; }
    virtual bool supportsSurround() const { return true; }
    virtual bool supportsSHDomain() const { return false; }
    virtual juce::String getName() const = 0;
};
```

### Design Principles
- **Lock-free audio path:** No malloc, locks, or logging in any function called from processBlock
- **Stateless algorithms:** All computation state in context structs, algorithms are pure functions
- **Dual-buffered layouts:** Atomic swap for lock-free audio thread reads during format changes
- **Per-source HRTF:** 12 independent PartitionedConvolvers for direct binaural rendering
- **Self-calibrating normalization:** `targetRMS = 1/sqrt(irLen)` ensures consistent levels across HRTF profiles
- **Facade boundary (SC-13):** consumers drive rendering through `RenderEngine` and do not dispatch algorithms or build `LayoutContext`s themselves — with one stated exception: stereo-variant gains (`objGainL`/`objGainR`) are computed consumer-side always, because that math is not a `SpatializationAlgorithm`

## Build System
- **Framework:** JUCE 9.0.0, C++17, CMake 3.22+
- **Dependencies:** libmysofa v1.3.2 (FetchContent), zlib (system)
- **HRTF data:** 5 SOFA files (Git LFS tracked), loaded at runtime via `HRTFDatabase::loadFromFile` — no BinaryData compilation step is involved. Consumer plugins resolve the files bundle-relative in shipped builds (e.g. OSD: `<bundle>/Contents/Resources/HRTF/`), with a source-tree fallback for dev/test/CI. See OpenSpatialDelay's `Source/PluginProcessor.cpp` `loadHRTFProfileIntoRenderer` for the reference resolver + its packaging scripts for the release-time file placement.
- **Tests:** Catch2 v3.7.1 via FetchContent

## Versioning
Semantic versioning: `vMAJOR.MINOR.PATCH`
- Major: Breaking API changes (new virtual methods, removed functions)
- Minor: New features (new algorithm, output format, UI widget)
- Patch: Bug fixes

## Consumer Plugins
Plugins link SpatialCore as a git submodule and a CMake subdirectory. `add_subdirectory(SpatialCore)` exposes two link targets: `SpatialCore` (DSP) and `SpatialCoreUI` (shared spatial map + SML look-and-feel + font BinaryData).
```cmake
add_subdirectory(SpatialCore)
target_link_libraries(MyPlugin PRIVATE SpatialCore SpatialCoreUI)
```
A SpatialCore change reaches a consumer only after: commit + push here, then bump the consumer's submodule pointer (`git add SpatialCore && git commit` in the consumer). Consumers today: OpenSpatialDelay (live) and OpenSpatialPanner (next).

## Skills
15 JUCE/DSP skills are available at project level in `.claude/skills/` — they load automatically in this repo.

## Critical Rules
- NEVER allocate memory in any function called from processBlock
- NEVER modify the SpatializationAlgorithm interface without bumping the major version
- ALWAYS maintain backward compatibility with existing plugins when adding features
- ALWAYS run the full test suite before tagging a release
- HRTF profiles are Git-LFS-tracked raw `.sofa` files loaded at runtime, not BinaryData — adding/removing profiles updates the consumer plugin's packaging copy step (and the runtime resolver's filename switch), not a BinaryData rebuild
