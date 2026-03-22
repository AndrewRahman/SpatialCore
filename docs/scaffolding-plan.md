# SpatialCore — Scaffolding Plan

## Context

SpatialCore is currently a **documentation-only shell** — the architecture is fully designed but no source code exists yet. All spatial audio code lives in OpenSpatialDelay v1.0. Rather than wait for OSD to ship, we'll clone the OSD repo as a reference, then scaffold SpatialCore's build system and directory structure so it's ready to receive extracted code.

---

## Plan

### Step 1: Clone OpenSpatialDelay as Reference
- Clone `github.com/AndrewRahman/OpenSpatialDelay` into `.context/` (gitignored) so we can reference the source code during scaffolding
- Study the existing code structure to understand what modules exist, how they're organized, and what dependencies they use

### Step 2: Analyze OSD Source Code
- Map every file in OSD to a SpatialCore module (Algorithms, Binaural, IO, OSC, Trajectory, DSP, UI)
- Identify the exact headers, classes, and dependencies that belong to SpatialCore vs plugin-specific code
- Note the HRTF SOFA files, their locations, and how they're currently embedded
- Document the JUCE modules used by SpatialCore code

### Step 3: Create Directory Structure
```
SpatialCore/
├── CMakeLists.txt
├── Algorithms/          (7 spatialization algorithm implementations)
├── Binaural/            (HRTFDatabase, PartitionedConvolver, BinauralRenderer)
├── IO/                  (OutputFormatRegistry, SpeakerLayout, AmbisonicsCodec)
├── OSC/                 (ADM-OSC send/receive)
├── Trajectory/          (13 trajectory shapes)
├── DSP/                 (softClip, outputLimiter utilities)
├── UI/                  (SpatialMapComponent, SMLLookAndFeel, widgets)
└── Tests/
    └── CMakeLists.txt
```

### Step 4: Build the CMakeLists.txt
- Static library target `SpatialCore`
- C++17 standard
- FetchContent for libmysofa v1.3.2 and Catch2 v3.7.1
- System zlib dependency
- JUCE provided by consumer (SpatialCore uses `target_link_libraries` to specify which JUCE modules it needs, but the consumer's CMake brings JUCE in)
- BinaryData target for HRTF SOFA files
- Proper include paths so consumers just do `target_link_libraries(MyPlugin PRIVATE SpatialCore)`

### Step 5: Extract Core Headers and Interfaces
- Copy the `SpatializationAlgorithm` base class and key interfaces from OSD
- Copy struct definitions (SourcePosition, LayoutContext, etc.) that form the public API
- Place extracted headers in the correct module directories
- Ensure headers compile standalone

### Step 6: Set Up Catch2 Test Scaffold
- One test file per module (AlgorithmTests.cpp, BinauralTests.cpp, etc.)
- Basic smoke tests where possible (e.g., algorithm gain computation, coordinate transforms)
- Test target that builds and runs

### Step 7: Verify Build
- `cmake -B build` succeeds
- `cmake --build build` produces the library
- Test target compiles and any scaffold tests pass

---

## Key Files to Create/Modify
- `CMakeLists.txt` — top-level build system
- `Algorithms/*.h` — algorithm interface + implementations
- `Binaural/*.h` — HRTF/convolver headers
- `IO/*.h` — output format/speaker layout headers
- `OSC/*.h` — ADM-OSC headers
- `Trajectory/*.h` — trajectory shape headers
- `DSP/*.h` — utility DSP headers
- `UI/*.h` — spatial map component and widget headers
- `Tests/CMakeLists.txt` — test build config
- `Tests/*Tests.cpp` — per-module test files

## Key Files to Reference (from OSD clone)
- OSD's `PluginProcessor.h/cpp` — contains most SpatialCore code
- OSD's `PluginEditor.h/cpp` — contains UI components
- OSD's `CMakeLists.txt` — dependency versions, JUCE module list
- OSD's SOFA files — HRTF data locations

---

## Verification
1. `cmake -B build -DCMAKE_BUILD_TYPE=Release` completes without errors
2. `cmake --build build` produces a library artifact
3. Test target compiles and runs (even if tests are minimal)
4. Headers can be included from a hypothetical consumer without errors
