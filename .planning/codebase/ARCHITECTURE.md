<!-- refreshed: 2026-08-09 -->
# Architecture

**Analysis Date:** 2026-08-09

## System Overview

```text
┌─────────────────────────────────────────────────────────────────────────────┐
│                          Consumer Plugin / DAW                              │
└───────────────────────────────────────┬─────────────────────────────────────┘
                                        │
                                        ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                                 SpatialCore                                  │
│                        (Static Library: libSpatialCore.a)                    │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                               │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐    │
│  │   Core       │  │  Algorithms  │  │  Binaural    │  │     IO       │    │
│  │ (Types,      │  │  (7 algos)   │  │  (HRTF)      │  │  (22 formats)│    │
│  │  SourcePos)  │  │              │  │              │  │              │    │
│  └──────────────┘  └──────────────┘  └──────────────┘  └──────────────┘    │
│  `Core/*.h`        `Algorithms/*.h`  `Binaural/*.h`    `IO/*.h`             │
│                                                                               │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐    │
│  │     OSC      │  │ Trajectory   │  │     DSP      │  │      UI      │    │
│  │  (ADM-OSC)   │  │  (13 shapes) │  │  (softClip)  │  │  (Components)│    │
│  └──────────────┘  └──────────────┘  └──────────────┘  └──────────────┘    │
│  `OSC/*.h`        `Trajectory/*.h`   `DSP/*.h`         `UI/*.h`             │
│                                                                               │
└─────────────────────────────────────────────────────────────────────────────┘
         │                    │                    │                │
         ▼                    ▼                    ▼                ▼
    ┌─────────┐         ┌─────────┐         ┌─────────┐      ┌─────────┐
    │ Binaural│         │ Surround│         │Ambisonics│     │   UI    │
    │ Headphs │         │ Speakers│         │ (HOA)   │      │  Output │
    └─────────┘         └─────────┘         └─────────┘      └─────────┘
    2 channels         4-16 channels         4-49 channels   JUCE GUI
    (HRTF/Simple)      (Algorithm)           (SH domain)     Components
```

## Component Responsibilities

| Component | Responsibility | Location |
|-----------|----------------|----------|
| **Core Types** | Defines shared data structures (SourcePosition, ObjectState, BinauralProfile) and fundamental constants (MAX_SOURCES=12, MAX_SPEAKERS=16) | `include/SpatialCore/Core/Types.h` |
| **SpatializationAlgorithm** | Abstract base class defining the interface all 8 algorithms implement; defines LayoutContext and BinauralContext | `include/SpatialCore/Algorithms/SpatializationAlgorithm.h` |
| **Algorithm Implementations** | 8 concrete spatialization algorithms: VBAP, VBIP, KNN, DBAP, MDAP, Ambisonics, DirectBinaural, ConstantPower | `src/Algorithms/*.cpp`, `include/SpatialCore/Algorithms/*.h` |
| **HRTFDatabase** | SOFA file loading via libmysofa, KD-tree interpolation, ITD extraction | `src/Binaural/HRTFDatabase.cpp`, `include/SpatialCore/Binaural/HRTFDatabase.h` |
| **PartitionedConvolver** | Real-time FFT overlap-save convolution for HRTF application | `src/Binaural/PartitionedConvolver.cpp`, `include/SpatialCore/Binaural/PartitionedConvolver.h` |
| **BinauralRenderer** | Manages 12 per-source PartitionedConvolvers, profile management, per-sample HRIR updates | `src/Binaural/BinauralRenderer.cpp`, `include/SpatialCore/Binaural/BinauralRenderer.h` |
| **SharedFFTCache** | Process-global FFT singleton for HRTF convolution | `include/SpatialCore/Binaural/SharedFFTCache.h` |
| **OutputFormatRegistry** | 22 output format definitions (binaural, stereo, surround, Atmos, Ambisonics) with channel requirements and metadata | `src/IO/OutputFormatRegistry.cpp`, `include/SpatialCore/IO/OutputFormat.h` |
| **SpeakerLayout** | ITU-R speaker position definitions, VBAP triplet generation, 13 standard layouts | `src/IO/SpeakerLayout.cpp`, `include/SpatialCore/IO/SpeakerLayout.h` |
| **AmbisonicsCodec** | Spherical harmonic encoding/decoding, ACN/SN3D format support, max-rE weighting | `src/IO/AmbisonicsCodec.cpp`, `include/SpatialCore/IO/AmbisonicsCodec.h` |
| **ADMOSCReceiver** | ADM-OSC listener on port 4002, parses `/adm/obj/N/azim|elev|dist|aed|xyz` | `src/OSC/ADMOSCReceiver.cpp`, `include/SpatialCore/OSC/ADMOSCReceiver.h` |
| **ADMOSCSender** | Broadcasts object positions at 30Hz via ADM-OSC with position-change gating | `src/OSC/ADMOSCSender.cpp`, `include/SpatialCore/OSC/ADMOSCSender.h` |
| **TrajectoryEngine** | 13 animation shapes (Orbit, Circle, Spiral, etc.), origin-point architecture, forward/reverse | `src/Trajectory/TrajectoryEngine.cpp`, `include/SpatialCore/Trajectory/TrajectoryEngine.h` |
| **SpatialMapComponent** | 2D top-down spatial visualization, object dragging, distance rings, elevation encoding via opacity | `src/UI/SpatialMapComponent.cpp`, `include/SpatialCore/UI/SpatialMapComponent.h` |
| **SMLLookAndFeel** | Dark theme, cyan/purple/amber color palette, JUCE component styling | `src/UI/SMLLookAndFeel.cpp`, `include/SpatialCore/UI/SMLLookAndFeel.h` |
| **UI Widgets** | ReverseSlider (azimuth knob), IndicatorToggle, StyledButton | `src/UI/*.cpp`, `include/SpatialCore/UI/*.h` |
| **DSP Utilities** | softClip() (asymptotic saturation), outputLimiter() (tanh-based +2dB ceiling) | `include/SpatialCore/DSP/Utilities.h` |

## Pattern Overview

**Overall:** Layered architecture with plugin consumer at top, core library below, organized into functional domains (algorithms, binaural rendering, I/O, OSC, trajectory, UI, DSP).

**Key Characteristics:**
- **Lock-free audio path:** No malloc, locks, or logging in processBlock-called functions
- **Stateless algorithms:** All computation state passed in context structs; algorithms are pure functions
- **Virtual interface inheritance:** 8 concrete algorithms via SpatializationAlgorithm base class
- **Dual-buffered layout management:** Atomic swap for lock-free audio thread reads during format changes
- **Per-source HRTF convolution:** 12 independent PartitionedConvolvers; normalization via `targetRMS = 1/sqrt(irLen)`
- **Namespace isolation:** All code in `spatialcore::` namespace to avoid conflicts with consumer plugins

## Layers

**Core Abstractions:**
- Purpose: Define fundamental data types, constants, and math utilities shared by all components
- Location: `include/SpatialCore/Core/`, `src/Core/`
- Contains: Types.h (ObjectState, TrajectoryState, BinauralProfile), SourcePosition.h, SpatialMath.cpp
- Depends on: JUCE core only
- Used by: All higher layers

**Algorithms Layer:**
- Purpose: Provide 8 spatialization algorithms implementing SpatializationAlgorithm interface
- Location: `include/SpatialCore/Algorithms/`, `src/Algorithms/`
- Contains: Abstract base class + 8 concrete implementations (VBAP, VBIP, KNN, DBAP, MDAP, Ambisonics, DirectBinaural, ConstantPower)
- Depends on: Core abstractions, SpeakerLayout, math utilities
- Used by: Consumer plugins for computing speaker gains or binaural HRTF output

**Binaural Rendering Layer:**
- Purpose: HRTF-based spatial audio convolution for headphone output
- Location: `include/SpatialCore/Binaural/`, `src/Binaural/`
- Contains: HRTFDatabase (SOFA loading), PartitionedConvolver (FFT convolution), BinauralRenderer (12 convolvers), SharedFFTCache (process singleton)
- Depends on: libmysofa, JUCE DSP, Core abstractions
- Used by: Consumer plugins rendering to binaural output
- Note: Per-renderer HRTFDatabase eliminates shared-state race (issue #96)

**I/O Layer:**
- Purpose: Manage output formats, speaker layouts, Ambisonics encoding/decoding
- Location: `include/SpatialCore/IO/`, `src/IO/`
- Contains: OutputFormatRegistry (22 formats), SpeakerLayout (13 ITU-R layouts + virtual binaural), AmbisonicsCodec (SH domain)
- Depends on: Core abstractions
- Used by: Consumer plugins for output format negotiation and Ambisonics encoding

**OSC Integration Layer:**
- Purpose: ADM-OSC protocol for spatial audio object position communication
- Location: `include/SpatialCore/OSC/`, `src/OSC/`
- Contains: ADMOSCReceiver (listen on 4002), ADMOSCSender (broadcast 30Hz)
- Depends on: JUCE OSC, Core abstractions
- Used by: Plugins integrating with DAWs or external spatial controllers

**Trajectory Engine:**
- Purpose: Animated source trajectories (13 shapes) with origin-point architecture
- Location: `include/SpatialCore/Trajectory/`, `src/Trajectory/`
- Contains: TrajectoryEngine (shape evaluation, random noise generation, forward/reverse)
- Depends on: Core abstractions
- Used by: UI and plugins implementing animated spatial motion

**UI Layer:**
- Purpose: Shared JUCE GUI components for spatial audio editing
- Location: `include/SpatialCore/UI/`, `src/UI/`
- Contains: SpatialMapComponent (2D top-down view), SMLLookAndFeel (styling), ReverseSlider, IndicatorToggle, StyledButton
- Depends on: JUCE GUI, Trajectory engine, Core abstractions
- Used by: Consumer plugins building spatial audio UI

**DSP Utilities:**
- Purpose: Utility DSP functions for saturation and limiting
- Location: `include/SpatialCore/DSP/Utilities.h`
- Contains: softClip() (threshold 0.8), outputLimiter() (+2dB ceiling)
- Depends on: cmath only
- Used by: Consumer plugins for audio protection

## Data Flow

### Primary Rendering Path (Algorithm → Speaker/Binaural Output)

1. **Input:** Consumer plugin receives audio objects with 3D positions (azimuth, elevation, distance in SourcePosition struct)
2. **Algorithm Selection:** Consumer selects a SpatializationAlgorithm concrete class
3. **Gain Computation:** Algorithm.computeGains() reads SourcePosition and LayoutContext (speaker positions, VBAP triplets, Ambisonics decode matrix)
   - 2D layouts: Uses VBAP or nearest-speaker fallback
   - 3D layouts with triplets: Full 3D VBAP via triplet inversion
   - Ambisonics: Spherical harmonic encoding
4. **Output Routing:**
   - **Surround output:** Send speaker gains directly to DAW output channels
   - **Binaural output:** Route per-source accumulation buffers through BinauralRenderer.renderSourceBuffers()
5. **HRTF Convolution (Binaural only):**
   - updateSourceHRIR() called at block boundaries when position changes (KD-tree + in-place FFT, realtime-safe)
   - renderSourceBuffers() applies 12 PartitionedConvolvers (overlap-save FFT convolution)
   - Interleaved L/R channels output to 2-channel bus

**State Management:**
- ObjectState and TrajectoryState maintained per source in consumer plugin
- BinauralRenderer holds per-profile normGain for consistent levels across HRTF profiles
- PartitionedConvolver state (FFT history) reset on playback restart via BinauralRenderer.reset()
- Profile swaps use dual-buffered layout management (atomic swap for lock-free audio thread reads)

### ADM-OSC Receive Path

1. ADMOSCReceiver connects on port 4002
2. Parses `/adm/obj/N/azim`, `/adm/obj/N/elev`, `/adm/obj/N/dist`, `/adm/obj/N/aed`, `/adm/obj/N/xyz`
3. Converts Cartesian (x,y,z) to Polar (azimuth, elevation, distance) via ITU-R BS.2127-0
4. Notifies Listener interface with admPositionReceived(objectIndex, azimuthDeg, elevationDeg, distance)
5. Consumer plugin updates ObjectState with received positions

### Trajectory Animation Path

1. TrajectoryEngine.tick(ObjectInput) evaluates current shape at phase [0..1]
2. Returns TrajectoryResult with azimuth, elevation, distance computed from origin + trajectory offset
3. Consumer plugin blends origin (manual knob) + trajectory output each sample
4. SpatialMapComponent visualizes trajectory as glow trail around object

## Key Abstractions

**SpatializationAlgorithm:**
- Purpose: Abstract interface for spatialization computation
- Examples: `VBAPAlgorithm`, `AmbisonicsAlgorithm`, `DirectBinauralAlgorithm`
- Pattern: Virtual method override; stateless (all state in LayoutContext/BinauralContext)
- Methods: computeGains(), supportsBinauralDirect(), supportsSurround(), supportsSHDomain(), getName()

**LayoutContext:**
- Purpose: Bundle speaker layout, VBAP triplets, Ambisonics decode matrix as immutable algorithm input
- Contains: SpeakerLayout ref, triplets vector, ambiDecodeMatrix pointer, ambiNumSpeakers
- Usage: Passed to algorithm.computeGains() for gain computation

**SpeakerLayout:**
- Purpose: Represent physical or virtual speaker configuration with azimuth/elevation positions and SMPTE channel ordering
- Contains: numSpeakers, lfeChannelIndex, speakers[MAX_SPEAKERS], totalChannels
- Factory functions: Layouts::get5_1(), Layouts::get7_1_4(), etc. (13 ITU-R standard layouts)

**OutputFormat enum + OutputFormatInfo:**
- Purpose: Enumerate 22 supported output configurations and their metadata
- Covers: Binaural (1), Stereo (1), Surround (7), Atmos (7), Ambisonics (6)
- Metadata: requiredChannels, hasLFE, hasHeight, ambiOrder

**HRTFDatabase:**
- Purpose: Load and interpolate SOFA HRTF files
- Wraps: libmysofa for KD-tree HRIR lookup
- Supports: 6 embedded profiles (Simple Woodworth, MIT KEMAR, SADIE II D2, CIPIC003, HUTUBS PP2, Bernschuetz KU100)
- Methods: loadFromMemory(), getInterpolatedHRIR(), getAlignedHRIR() (ITD-free)

**TrajectoryEngine:**
- Purpose: Evaluate 13 parameterized animation shapes
- Shapes: None, Bounce, Circle, Cross, Figure8, Heart, Helix, Infinity, Line, Orbit, Random, Spiral, Square, Triangle
- Input: TrajectoryEngine::ObjectInput (shape, speed, reverse, origin position)
- Output: TrajectoryResult (azDeg, elDeg, dist + bool flags indicating which dimensions are controlled)

## Entry Points

**Library Export (include/SpatialCore/SpatialCore.h):**
- Location: `include/SpatialCore/SpatialCore.h`
- Triggers: `#include <SpatialCore/SpatialCore.h>` in consumer plugin
- Responsibilities: Umbrella header aggregating all public API headers (Core, Algorithms, Binaural, IO, OSC, Trajectory, DSP, UI)

**Build Integration (CMakeLists.txt):**
- Location: `/Users/andrewrahman/conductor/workspaces/SpatialCore/kelowna/CMakeLists.txt`
- Triggers: `add_subdirectory(SpatialCore)` in consumer plugin CMakeLists.txt
- Responsibilities: Fetches JUCE 8, libmysofa v1.3.2, zlib; builds libSpatialCore.a; defines target_link_libraries

**Test Harness (tests/CMakeLists.txt):**
- Location: `tests/CMakeLists.txt`
- Triggers: `cmake --build build --target SpatialCoreTests` or via CTest
- Responsibilities: Fetches Catch2 v3.7.1, compiles test executables, discovers tests via Catch2

## Architectural Constraints

- **Threading:** Single-threaded audio processing (JUCE plugin lifecycle). OSC receiver runs on internal JUCE message thread. Dual-buffering (atomic swap) protects audio thread from profile changes.
- **Global state:** SharedFFTCache is process-global singleton (FFT buffer shared across all PartitionedConvolvers in the process). Per-renderer HRTFDatabase eliminates per-source race conditions.
- **Circular imports:** No circular dependencies by design. Algorithms depend on Core + IO, but IO and algorithms never depend on each other.
- **Max sources/speakers:** Hardcoded MAX_SOURCES=12, MAX_SPEAKERS=16 as compile-time constants (enables stack allocation in audio thread).
- **Real-time safety:** No malloc/new, no blocking I/O, no logging in any function called from processBlock (audio path is lock-free).
- **FFT buffer size:** PartitionedConvolver uses fixed FFT size (typically 2048) for HRTF convolution. No dynamic resizing.

## Anti-Patterns

### Triplet Missing for 3D Layout

**What happens:** If a 3D speaker layout (has height speakers) is passed to VBAP without precomputed VBAP triplets, the code falls back to nearestSpeaker3DFallback().

**Why it's wrong:** 3D VBAP without triplets routes to height speakers using 2D math, producing spatial artifacts. The triplets must be precomputed via buildVBAPTripletsForLayout().

**Do this instead:** Always call buildVBAPTripletsForLayout() before passing 3D layouts to VBAP algorithm. See `src/Algorithms/VBAPAlgorithm.cpp:15-20` for the guard and `include/SpatialCore/IO/SpeakerLayout.h:56-57` for the function.

### Shared HRTF State Race Condition

**What happens:** Old code (before issue #96) used a single shared HRTFDatabase across all PartitionedConvolvers. Timer thread loading a SOFA file raced with audio thread HRIR lookups, causing crashes or garbage audio.

**Why it's wrong:** Audio thread is real-time, cannot wait for I/O. Shared mutable state violates lock-free requirements.

**Do this instead:** BinauralRenderer holds its own HRTFDatabase instance. Profile loading happens on timer thread; audio thread only reads prepared HRIRs. See `src/Binaural/BinauralRenderer.h:23`.

### Direct FFT on Audio Thread

**What happens:** If HRTF convolution tried to compute FFTs inside processBlock, real-time performance would stall.

**Why it's wrong:** FFTs are expensive O(N log N) and cannot meet strict audio thread deadlines.

**Do this instead:** PartitionedConvolver precomputes IR FFTs during updateSourceHRIR() (called at block boundaries during position changes, not every sample). processBlock only does overlap-save accumulation (cheap). See `src/Binaural/PartitionedConvolver.cpp`.

## Error Handling

**Strategy:** Minimal error handling in audio path. Validation and error reporting deferred to setup/configuration time.

**Patterns:**
- HRTFDatabase.loadFromMemory() returns bool (true if SOFA parse succeeds, false on error)
- OutputFormatRegistry returns OutputFormatInfo by value; invalid format returns zeroed struct
- Algorithm.computeGains() assumes valid SourcePosition and LayoutContext; no validation
- ADMOSCReceiver.connect() returns bool (true if UDP bind succeeds)
- No exceptions thrown; all errors communicated via return codes or boolean flags

## Cross-Cutting Concerns

**Logging:** None in audio path (lock-free requirement). Console output only in setup/test code.

**Validation:** Input validation (null checks, bounds checks) happens in public API entry points (HRTFDatabase, OutputFormatRegistry, ADMOSCReceiver), not in hot audio path. Algorithm.computeGains() assumes valid inputs.

**Authentication:** No authentication; OSC receiver listens on localhost 4002 by default (consumer plugin chooses port).

**Coordinate Conversion:** Polar (azimuth [radians], elevation [radians], distance [meters]) is the canonical internal format. UI and OSC work in degrees; conversion happens at boundaries:
- SourcePosition uses radians (internal canonical)
- ObjectState uses degrees (UI-facing)
- ADMOSCReceiver converts Cartesian → Polar (ITU-R BS.2127-0)
- SpatialMapComponent uses degrees (UI display)

---

*Architecture analysis: 2026-08-09*
