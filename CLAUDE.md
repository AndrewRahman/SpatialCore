# SpatialCore — Project Context for Claude

## Identity
- **Library:** SpatialCore — the shared spatial audio rendering engine for the Spatial Media Library
- **Organization:** Spatial Media Lab (spatialmedialab.org)
- **What it does:** Takes audio objects with 3D positions and renders them to any output format (binaural, stereo, surround, Ambisonics) via 7 spatialization algorithms
- **License:** GPL-3.0 + commercial (dual license)
- **GitHub:** `https://github.com/Spatial-Media-Lab/SpatialCore`

## Origin
SpatialCore was extracted from OpenSpatialDelay v1.0, where 68% of the codebase was marked as reusable spatial audio infrastructure. The extraction separated framework code (algorithms, HRTF, I/O, OSC, trajectories, UI) from delay-specific code (delay line, pitch shift, feedback, wobble).

## Architecture

### Core Components
| Component | Headers | Description |
|-----------|---------|-------------|
| Algorithms | `Algorithms/*.h` | 7 spatialization algorithms: VBAP, VBIP, KNN, DBAP, MDAP, Ambisonics, DirectBinaural |
| Binaural | `Binaural/*.h` | HRTFDatabase (SOFA/libmysofa), PartitionedConvolver (FFT overlap-save), BinauralRenderer (12 per-source convolvers) |
| I/O | `IO/*.h` | OutputFormatRegistry (22 formats), SpeakerLayout (13 ITU-R layouts), AmbisonicsCodec (SH eval, decode matrices) |
| OSC | `OSC/*.h` | ADM-OSC Receive (parse /adm/obj/N/), ADM-OSC Send (30Hz broadcast) |
| Trajectory | `Trajectory/*.h` | 13 shapes, origin-point architecture, forward/reverse |
| DSP | `DSP/*.h` | softClip(), outputLimiter() |
| UI | `UI/*.h` | SpatialMapComponent, SMLLookAndFeel, ReverseSlider, IndicatorToggle, StyledButton |

### Key Interfaces
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

## Build System
- **Framework:** JUCE 8, C++17, CMake 3.22+
- **Dependencies:** libmysofa v1.3.2 (FetchContent), zlib (system)
- **HRTF data:** 5 SOFA files embedded as BinaryData (Git LFS tracked)
- **Tests:** Catch2 v3.7.1 via FetchContent

## Versioning
Semantic versioning: `vMAJOR.MINOR.PATCH`
- Major: Breaking API changes (new virtual methods, removed functions)
- Minor: New features (new algorithm, output format, UI widget)
- Patch: Bug fixes

## Consumer Plugins
Plugins link SpatialCore as a git submodule and a CMake subdirectory:
```cmake
add_subdirectory(SpatialCore)
target_link_libraries(MyPlugin PRIVATE SpatialCore)
```

## Skills
Skills for SpatialCore development are in the `spatial-media-skills` repository:
- `spatialcore/spatial-audio-dsp` — algorithms, HRTF, coordinate systems
- `spatialcore/spatialcore-architecture` — how to build plugins on SpatialCore
- `spatialcore/adm-osc-integration` — ADM-OSC protocol
- `dsp/dsp-cookbook` — filters, saturation, smoothing
- `juce/juce-best-practices` — realtime safety, threading, APVTS
- `design/oiloil-ui-ux-guide` — UX principles, spacing, style

## Critical Rules
- NEVER allocate memory in any function called from processBlock
- NEVER modify the SpatializationAlgorithm interface without bumping the major version
- ALWAYS maintain backward compatibility with existing plugins when adding features
- ALWAYS run the full test suite before tagging a release
- HRTF profiles are embedded as BinaryData — adding/removing profiles requires rebuild of all consumer plugins
