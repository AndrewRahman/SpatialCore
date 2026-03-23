# SpatialCore Library Scaffolding Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Create the complete directory structure, build system, public API headers, stub implementations, and test infrastructure for SpatialCore — a JUCE 8 C++17 static library extracted from OpenSpatialDelay.

**Architecture:** SpatialCore is a static library with 7 modules (Algorithms, Binaural, IO, OSC, Trajectory, DSP, UI). All public headers live under `include/SpatialCore/`. Implementations live under `src/`. Tests live under `tests/`. The library compiles as a JUCE module-style static library that consumer plugins link via `add_subdirectory()`.

**Tech Stack:** JUCE 8, C++17, CMake 3.22+, libmysofa v1.3.2 (FetchContent), zlib (system), Catch2 v3.7.1 (FetchContent)

---

## File Structure

```
SpatialCore/
├── CMakeLists.txt                          # Root build: static lib + test target
├── include/
│   └── SpatialCore/
│       ├── SpatialCore.h                   # Umbrella include
│       ├── Core/
│       │   ├── SourcePosition.h            # SourcePosition struct
│       │   ├── BinauralGains.h             # BinauralGains struct
│       │   └── Types.h                     # Forward declarations, constants
│       ├── Algorithms/
│       │   ├── SpatializationAlgorithm.h   # Abstract base class
│       │   ├── VBAPAlgorithm.h
│       │   ├── VBIPAlgorithm.h
│       │   ├── KNNAlgorithm.h
│       │   ├── DBAPAlgorithm.h
│       │   ├── MDAPAlgorithm.h
│       │   ├── AmbisonicsAlgorithm.h
│       │   └── DirectBinauralAlgorithm.h
│       ├── Binaural/
│       │   ├── HRTFDatabase.h
│       │   ├── PartitionedConvolver.h
│       │   └── BinauralRenderer.h
│       ├── IO/
│       │   ├── OutputFormat.h              # OutputFormat enum + OutputFormatInfo
│       │   ├── OutputFormatRegistry.h      # Format lookup/registration
│       │   ├── SpeakerLayout.h             # SpeakerLayout + VBAPTriplet structs
│       │   └── AmbisonicsCodec.h           # SH eval + decode matrices
│       ├── OSC/
│       │   ├── ADMOSCReceiver.h
│       │   └── ADMOSCSender.h
│       ├── Trajectory/
│       │   └── TrajectoryEngine.h          # computeTrajectory() + TrajectoryResult
│       ├── DSP/
│       │   └── Utilities.h                 # softClip(), outputLimiter()
│       └── UI/
│           ├── SpatialMapComponent.h
│           ├── SMLLookAndFeel.h
│           ├── ReverseSlider.h
│           ├── IndicatorToggle.h
│           └── StyledButton.h
├── src/
│   ├── Algorithms/
│   │   ├── VBAPAlgorithm.cpp
│   │   ├── VBIPAlgorithm.cpp
│   │   ├── KNNAlgorithm.cpp
│   │   ├── DBAPAlgorithm.cpp
│   │   ├── MDAPAlgorithm.cpp
│   │   ├── AmbisonicsAlgorithm.cpp
│   │   └── DirectBinauralAlgorithm.cpp
│   ├── Binaural/
│   │   ├── HRTFDatabase.cpp
│   │   ├── PartitionedConvolver.cpp
│   │   └── BinauralRenderer.cpp
│   ├── IO/
│   │   ├── OutputFormatRegistry.cpp
│   │   ├── SpeakerLayout.cpp
│   │   └── AmbisonicsCodec.cpp
│   ├── OSC/
│   │   ├── ADMOSCReceiver.cpp
│   │   └── ADMOSCSender.cpp
│   ├── Trajectory/
│   │   └── TrajectoryEngine.cpp
│   └── UI/
│       ├── SpatialMapComponent.cpp
│       ├── SMLLookAndFeel.cpp
│       ├── ReverseSlider.cpp
│       ├── IndicatorToggle.cpp
│       └── StyledButton.cpp
├── tests/
│   ├── CMakeLists.txt                      # Test target config
│   ├── .gitkeep                             # (TestMain.cpp not needed — Catch2WithMain provides main)
│   ├── Algorithms/
│   │   └── SpatializationAlgorithmTests.cpp
│   ├── IO/
│   │   └── SpeakerLayoutTests.cpp
│   ├── Trajectory/
│   │   └── TrajectoryEngineTests.cpp
│   └── DSP/
│       └── UtilitiesTests.cpp
├── CLAUDE.md
├── README.md
└── docs/
```

---

### Task 1: Root CMakeLists.txt + Core Types

**Files:**
- Create: `CMakeLists.txt`
- Create: `include/SpatialCore/Core/Types.h`
- Create: `include/SpatialCore/Core/SourcePosition.h`
- Create: `include/SpatialCore/Core/BinauralGains.h`

- [ ] **Step 1: Create root CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.22)
project(SpatialCore VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0")
endif()

if(MSVC)
    add_compile_options(/Zc:preprocessor)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

# --- JUCE (must be provided by consumer or found) ---
if(NOT TARGET juce::juce_core)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE/CMakeLists.txt")
        add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../JUCE" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
        add_subdirectory(JUCE EXCLUDE_FROM_ALL)
    else()
        find_package(JUCE REQUIRED)
    endif()
endif()

# --- libmysofa ---
include(FetchContent)
FetchContent_Declare(
    mysofa
    GIT_REPOSITORY https://github.com/hoene/libmysofa.git
    GIT_TAG v1.3.2
)
set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(mysofa)

find_package(ZLIB REQUIRED)

# --- SpatialCore static library ---
add_library(SpatialCore STATIC
    # Algorithms
    src/Algorithms/VBAPAlgorithm.cpp
    src/Algorithms/VBIPAlgorithm.cpp
    src/Algorithms/KNNAlgorithm.cpp
    src/Algorithms/DBAPAlgorithm.cpp
    src/Algorithms/MDAPAlgorithm.cpp
    src/Algorithms/AmbisonicsAlgorithm.cpp
    src/Algorithms/DirectBinauralAlgorithm.cpp
    # Binaural
    src/Binaural/HRTFDatabase.cpp
    src/Binaural/PartitionedConvolver.cpp
    src/Binaural/BinauralRenderer.cpp
    # IO
    src/IO/OutputFormatRegistry.cpp
    src/IO/SpeakerLayout.cpp
    src/IO/AmbisonicsCodec.cpp
    # OSC
    src/OSC/ADMOSCReceiver.cpp
    src/OSC/ADMOSCSender.cpp
    # Trajectory
    src/Trajectory/TrajectoryEngine.cpp
    # UI
    src/UI/SpatialMapComponent.cpp
    src/UI/SMLLookAndFeel.cpp
    src/UI/ReverseSlider.cpp
    src/UI/IndicatorToggle.cpp
    src/UI/StyledButton.cpp
)

target_include_directories(SpatialCore
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(SpatialCore
    PUBLIC
        juce::juce_core
        juce::juce_audio_basics
        juce::juce_audio_formats
        juce::juce_dsp
        juce::juce_gui_basics
        juce::juce_osc
    PRIVATE
        mysofa-static
        ZLIB::ZLIB
)

target_compile_features(SpatialCore PUBLIC cxx_std_17)

if(APPLE)
    target_compile_options(SpatialCore PRIVATE -ffast-math -Wno-nan-infinity-disabled)
elseif(MSVC)
    target_compile_options(SpatialCore PRIVATE /fp:fast)
endif()

# --- Tests ---
option(SPATIALCORE_BUILD_TESTS "Build SpatialCore tests" ON)
if(SPATIALCORE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
```

- [ ] **Step 2: Create Core/Types.h**

```cpp
#pragma once

#include <juce_core/juce_core.h>

namespace spatialcore
{

static constexpr int MAX_SOURCES = 12;
static constexpr int MAX_SPEAKERS = 16;

} // namespace spatialcore
```

- [ ] **Step 3: Create Core/SourcePosition.h**

```cpp
#pragma once

namespace spatialcore
{

struct SourcePosition
{
    float azimuthRad   = 0.0f;
    float elevationRad = 0.0f;
    float distance     = 1.0f;
};

} // namespace spatialcore
```

- [ ] **Step 4: Create Core/BinauralGains.h**

```cpp
#pragma once

namespace spatialcore
{

struct BinauralGains
{
    float leftGain        = 0.0f;
    float rightGain       = 0.0f;
    float leftDelaySamples  = 0.0f;
    float rightDelaySamples = 0.0f;
};

} // namespace spatialcore
```

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt include/SpatialCore/Core/
git commit -m "feat: add root CMakeLists.txt and core data types"
```

---

### Task 2: I/O System — SpeakerLayout, OutputFormat, AmbisonicsCodec

**Files:**
- Create: `include/SpatialCore/IO/SpeakerLayout.h`
- Create: `include/SpatialCore/IO/OutputFormat.h`
- Create: `include/SpatialCore/IO/OutputFormatRegistry.h`
- Create: `include/SpatialCore/IO/AmbisonicsCodec.h`
- Create: `src/IO/SpeakerLayout.cpp`
- Create: `src/IO/OutputFormatRegistry.cpp`
- Create: `src/IO/AmbisonicsCodec.cpp`

- [ ] **Step 1: Create IO/SpeakerLayout.h**

```cpp
#pragma once

#include <SpatialCore/Core/Types.h>

namespace spatialcore
{

struct VirtualSpeaker
{
    float azimuthRad   = 0.0f;
    float elevationRad = 0.0f;
};

struct VBAPTriplet
{
    int i = 0, j = 0, k = 0;
    float inv[3][3] = {};
};

struct SpeakerLayout
{
    int numSpeakers    = 0;
    int lfeChannelIndex = -1;
    int totalChannels  = 0;

    struct Speaker
    {
        float azimuthRad   = 0.0f;
        float elevationRad = 0.0f;
        int   channelIndex = 0;
    };

    Speaker speakers[MAX_SPEAKERS] = {};
};

// Predefined ITU-R BS.775/BS.2051 layouts
namespace Layouts
{
    SpeakerLayout getQuad();
    SpeakerLayout get5_0();
    SpeakerLayout get5_1();
    SpeakerLayout get7_0();
    SpeakerLayout get7_1();
    SpeakerLayout getOctaphonic();
    SpeakerLayout get5_1_2();
    SpeakerLayout get5_1_4();
    SpeakerLayout get7_1_2();
    SpeakerLayout get7_1_4();
    SpeakerLayout get7_1_6();
    SpeakerLayout get9_1_4();
    SpeakerLayout get9_1_6();
    SpeakerLayout getVirtualBinaural16();
} // namespace Layouts

} // namespace spatialcore
```

- [ ] **Step 2: Create IO/OutputFormat.h**

```cpp
#pragma once

namespace spatialcore
{

enum class OutputFormat
{
    Binaural = 0,
    Stereo,
    Quad,
    Surround_5_0,
    Surround_5_1,
    Surround_7_0,
    Surround_7_1,
    Octaphonic,
    Atmos_5_1_2,
    Atmos_5_1_4,
    Atmos_7_1_2,
    Atmos_7_1_4,
    Atmos_7_1_6,
    Atmos_9_1_4,
    Atmos_9_1_6,
    SML_13_1,
    Ambi_FOA,
    Ambi_SOA,
    Ambi_HOA,
    Ambi_4OA,
    Ambi_5OA,
    Ambi_6OA,
    NumFormats
};

struct OutputFormatInfo
{
    OutputFormat format;
    const char*  name;
    const char*  shortName;
    int          requiredChannels;
    bool         hasLFE;
    bool         hasHeight;
    bool         isAmbisonicsOutput;
    int          ambiOrder;
    bool         isStereoVariant;
};

} // namespace spatialcore
```

- [ ] **Step 3: Create IO/OutputFormatRegistry.h**

```cpp
#pragma once

#include <SpatialCore/IO/OutputFormat.h>
#include <SpatialCore/IO/SpeakerLayout.h>
#include <vector>

namespace spatialcore
{

class OutputFormatRegistry
{
public:
    static const OutputFormatInfo& getInfo(OutputFormat format);
    static const OutputFormatInfo* getAllFormats();
    static int getNumFormats();

    static OutputFormat detectFromChannelCount(int numChannels);
    static const SpeakerLayout& getLayoutForFormat(OutputFormat format);
    static const std::vector<VBAPTriplet>& getTripletsForFormat(OutputFormat format);

private:
    static void initLayouts();
};

} // namespace spatialcore
```

- [ ] **Step 4: Create IO/AmbisonicsCodec.h**

```cpp
#pragma once

#include <SpatialCore/Core/SourcePosition.h>

namespace spatialcore
{

class AmbisonicsCodec
{
public:
    static constexpr int MAX_AMBI_ORDER = 6;
    static constexpr int MAX_AMBI_CHANNELS = (MAX_AMBI_ORDER + 1) * (MAX_AMBI_ORDER + 1); // 49

    static void encode(const SourcePosition& source, int order,
                       float* shCoeffs, int numCoeffs);

    static void getDecodeMatrix(int order, int numSpeakers,
                                const float* speakerAzimuths,
                                const float* speakerElevations,
                                float* decodeMatrix);

    static float evaluateSH(int l, int m, float azimuthRad, float elevationRad);

    static void applyMaxREWeights(float* shCoeffs, int order);
};

} // namespace spatialcore
```

- [ ] **Step 5: Create stub implementations**

Create `src/IO/SpeakerLayout.cpp`, `src/IO/OutputFormatRegistry.cpp`, `src/IO/AmbisonicsCodec.cpp` with empty/stub function bodies.

- [ ] **Step 6: Commit**

```bash
git add include/SpatialCore/IO/ src/IO/
git commit -m "feat: add I/O module — OutputFormat, SpeakerLayout, AmbisonicsCodec"
```

---

### Task 3: Algorithm Interface + 7 Implementations

**Files:**
- Create: `include/SpatialCore/Algorithms/SpatializationAlgorithm.h`
- Create: `include/SpatialCore/Algorithms/{VBAP,VBIP,KNN,DBAP,MDAP,Ambisonics,DirectBinaural}Algorithm.h`
- Create: `src/Algorithms/{VBAP,VBIP,KNN,DBAP,MDAP,Ambisonics,DirectBinaural}Algorithm.cpp`

- [ ] **Step 1: Create SpatializationAlgorithm.h (abstract base)**

```cpp
#pragma once

#include <SpatialCore/Core/Types.h>
#include <SpatialCore/Core/SourcePosition.h>
#include <SpatialCore/Core/BinauralGains.h>
#include <SpatialCore/IO/SpeakerLayout.h>
#include <juce_core/juce_core.h>
#include <vector>

namespace spatialcore
{

struct LayoutContext
{
    const SpeakerLayout&             layout;
    const std::vector<VBAPTriplet>&  triplets;
    const float (*ambiDecodeMatrix)[MAX_SPEAKERS];
    int ambiNumSpeakers;
};

struct BinauralContext
{
    int    profileIndex;
    double sampleRate;
};

class SpatializationAlgorithm
{
public:
    virtual ~SpatializationAlgorithm() = default;

    virtual void computeGains(const SourcePosition& source,
                              const LayoutContext& ctx,
                              float* outputGains,
                              int numSpeakers) const = 0;

    virtual bool supportsBinauralDirect() const { return false; }
    virtual BinauralGains computeBinauralGains(const SourcePosition& /*source*/,
                                               const BinauralContext& /*ctx*/) const { return {}; }

    virtual bool supportsSurround() const { return true; }
    virtual bool supportsSHDomain() const { return false; }
    virtual juce::String getName() const = 0;
};

} // namespace spatialcore
```

- [ ] **Step 2: Create 7 algorithm headers**

Each header declares a concrete algorithm class inheriting from `SpatializationAlgorithm`. Example for VBAP:

```cpp
#pragma once
#include <SpatialCore/Algorithms/SpatializationAlgorithm.h>

namespace spatialcore
{
class VBAPAlgorithm : public SpatializationAlgorithm
{
public:
    void computeGains(const SourcePosition& source, const LayoutContext& ctx,
                      float* outputGains, int numSpeakers) const override;
    juce::String getName() const override { return "VBAP"; }
};
} // namespace spatialcore
```

- [ ] **Step 3: Create 7 stub .cpp files**

Each cpp includes its header and provides a stub `computeGains()` that zero-fills the output array.

- [ ] **Step 4: Commit**

```bash
git add include/SpatialCore/Algorithms/ src/Algorithms/
git commit -m "feat: add algorithm interface and 7 stub implementations"
```

---

### Task 4: Binaural System — HRTFDatabase, PartitionedConvolver, BinauralRenderer

**Files:**
- Create: `include/SpatialCore/Binaural/HRTFDatabase.h`
- Create: `include/SpatialCore/Binaural/PartitionedConvolver.h`
- Create: `include/SpatialCore/Binaural/BinauralRenderer.h`
- Create: `src/Binaural/HRTFDatabase.cpp`
- Create: `src/Binaural/PartitionedConvolver.cpp`
- Create: `src/Binaural/BinauralRenderer.cpp`

- [ ] **Step 1: Create Binaural headers**

HRTFDatabase wraps libmysofa, PartitionedConvolver does FFT overlap-save, BinauralRenderer manages 12 per-source convolver pairs. Full class declarations match OSD signatures. Note: HRTFDatabase stubs must NOT reference BinaryData (SOFA files are out-of-scope for scaffolding — they'll be added during Phase 1 extraction). The stub `loadFromMemory()` accepts raw data pointers that consumers provide.

- [ ] **Step 2: Create stub implementations**

Each cpp provides stub/no-op implementations that compile. No BinaryData dependency — HRTF SOFA files will be embedded later during extraction from OSD.

- [ ] **Step 3: Commit**

```bash
git add include/SpatialCore/Binaural/ src/Binaural/
git commit -m "feat: add binaural rendering module stubs"
```

---

### Task 5: OSC Module — ADM-OSC Receive + Send

**Files:**
- Create: `include/SpatialCore/OSC/ADMOSCReceiver.h`
- Create: `include/SpatialCore/OSC/ADMOSCSender.h`
- Create: `src/OSC/ADMOSCReceiver.cpp`
- Create: `src/OSC/ADMOSCSender.cpp`

- [ ] **Step 1: Create OSC headers**

ADMOSCReceiver: parses `/adm/obj/N/{azim,elev,dist,aed,xyz}` messages.
ADMOSCSender: broadcasts `/adm/obj/N/aed` at 30Hz with position-change gating.

- [ ] **Step 2: Create stub implementations**

- [ ] **Step 3: Commit**

```bash
git add include/SpatialCore/OSC/ src/OSC/
git commit -m "feat: add ADM-OSC receive/send module stubs"
```

---

### Task 6: Trajectory Engine

**Files:**
- Create: `include/SpatialCore/Trajectory/TrajectoryEngine.h`
- Create: `src/Trajectory/TrajectoryEngine.cpp`

- [ ] **Step 1: Create TrajectoryEngine.h**

```cpp
#pragma once

namespace spatialcore
{

enum class TrajectoryShape
{
    None = 0,
    Bounce, Circle, Cross, Figure8, Heart, Helix,
    Infinity, Line, Orbit, Random, Spiral, Square, Triangle,
    NumShapes
};

struct TrajectoryResult
{
    float azDeg  = 0.0f;
    float elDeg  = 0.0f;
    float dist   = 1.0f;
    bool controlsAz   = false;
    bool controlsEl   = false;
    bool controlsDist  = false;
};

class TrajectoryEngine
{
public:
    static TrajectoryResult compute(TrajectoryShape shape, float phase,
                                    float baseAzDeg, float baseElDeg, float baseDist,
                                    bool reverse = false);
    static int getNumShapes();
    static const char* getShapeName(TrajectoryShape shape);
};

} // namespace spatialcore
```

- [ ] **Step 2: Create stub implementation**

- [ ] **Step 3: Commit**

```bash
git add include/SpatialCore/Trajectory/ src/Trajectory/
git commit -m "feat: add trajectory engine module stub"
```

---

### Task 7: DSP Utilities

**Files:**
- Create: `include/SpatialCore/DSP/Utilities.h`

- [ ] **Step 1: Create Utilities.h with inline implementations**

```cpp
#pragma once

#include <cmath>
#include <juce_core/juce_core.h>

namespace spatialcore
{
namespace DSP
{

inline float softClip(float x)
{
    if (!std::isfinite(x))
        return 0.0f;

    const float threshold = 0.8f;
    if (x > threshold)
        return threshold + (x - threshold) / (1.0f + (x - threshold) * (x - threshold));
    if (x < -threshold)
        return -threshold + (x + threshold) / (1.0f + (x + threshold) * (x + threshold));
    return x;
}

inline float outputLimiter(float x)
{
    if (!std::isfinite(x))
        return 0.0f;
    const float ceiling = 1.2589f;  // +2 dB hard ceiling
    if (x > ceiling)
        return ceiling;
    if (x < -ceiling)
        return -ceiling;
    return x;
}

} // namespace DSP
} // namespace spatialcore
```

- [ ] **Step 2: Commit**

```bash
git add include/SpatialCore/DSP/
git commit -m "feat: add DSP utility functions (softClip, outputLimiter)"
```

---

### Task 8: UI Components

**Files:**
- Create: `include/SpatialCore/UI/SpatialMapComponent.h`
- Create: `include/SpatialCore/UI/SMLLookAndFeel.h`
- Create: `include/SpatialCore/UI/ReverseSlider.h`
- Create: `include/SpatialCore/UI/IndicatorToggle.h`
- Create: `include/SpatialCore/UI/StyledButton.h`
- Create: `src/UI/SpatialMapComponent.cpp`
- Create: `src/UI/SMLLookAndFeel.cpp`
- Create: `src/UI/ReverseSlider.cpp`
- Create: `src/UI/IndicatorToggle.cpp`
- Create: `src/UI/StyledButton.cpp`

- [ ] **Step 1: Create UI headers**

Full class declarations matching OSD signatures, but decoupled from OpenSpatialDelayProcessor (SpatialMapComponent uses an abstract Listener interface instead of processor pointer).

- [ ] **Step 2: Create stub implementations**

- [ ] **Step 3: Commit**

```bash
git add include/SpatialCore/UI/ src/UI/
git commit -m "feat: add shared UI component stubs"
```

---

### Task 9: Umbrella Header

**Files:**
- Create: `include/SpatialCore/SpatialCore.h`

- [ ] **Step 1: Create umbrella include**

```cpp
#pragma once

// Core
#include <SpatialCore/Core/Types.h>
#include <SpatialCore/Core/SourcePosition.h>
#include <SpatialCore/Core/BinauralGains.h>

// Algorithms
#include <SpatialCore/Algorithms/SpatializationAlgorithm.h>
#include <SpatialCore/Algorithms/VBAPAlgorithm.h>
#include <SpatialCore/Algorithms/VBIPAlgorithm.h>
#include <SpatialCore/Algorithms/KNNAlgorithm.h>
#include <SpatialCore/Algorithms/DBAPAlgorithm.h>
#include <SpatialCore/Algorithms/MDAPAlgorithm.h>
#include <SpatialCore/Algorithms/AmbisonicsAlgorithm.h>
#include <SpatialCore/Algorithms/DirectBinauralAlgorithm.h>

// Binaural
#include <SpatialCore/Binaural/HRTFDatabase.h>
#include <SpatialCore/Binaural/PartitionedConvolver.h>
#include <SpatialCore/Binaural/BinauralRenderer.h>

// IO
#include <SpatialCore/IO/OutputFormat.h>
#include <SpatialCore/IO/OutputFormatRegistry.h>
#include <SpatialCore/IO/SpeakerLayout.h>
#include <SpatialCore/IO/AmbisonicsCodec.h>

// OSC
#include <SpatialCore/OSC/ADMOSCReceiver.h>
#include <SpatialCore/OSC/ADMOSCSender.h>

// Trajectory
#include <SpatialCore/Trajectory/TrajectoryEngine.h>

// DSP
#include <SpatialCore/DSP/Utilities.h>

// UI
#include <SpatialCore/UI/SpatialMapComponent.h>
#include <SpatialCore/UI/SMLLookAndFeel.h>
#include <SpatialCore/UI/ReverseSlider.h>
#include <SpatialCore/UI/IndicatorToggle.h>
#include <SpatialCore/UI/StyledButton.h>
```

- [ ] **Step 2: Commit**

```bash
git add include/SpatialCore/SpatialCore.h
git commit -m "feat: add umbrella header"
```

---

### Task 10: Test Infrastructure

**Files:**
- Create: `tests/CMakeLists.txt`
- Note: No TestMain.cpp needed — `Catch2::Catch2WithMain` provides `main()`
- Create: `tests/Algorithms/SpatializationAlgorithmTests.cpp`
- Create: `tests/IO/SpeakerLayoutTests.cpp`
- Create: `tests/Trajectory/TrajectoryEngineTests.cpp`
- Create: `tests/DSP/UtilitiesTests.cpp`

- [ ] **Step 1: Create tests/CMakeLists.txt**

```cmake
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
```

- [ ] **Step 2: Create test files with basic smoke tests**

Example — DSP/UtilitiesTests.cpp:
```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <SpatialCore/DSP/Utilities.h>

TEST_CASE("softClip: passthrough below threshold", "[dsp]")
{
    REQUIRE(spatialcore::DSP::softClip(0.5f) == Catch::Approx(0.5f));
}

TEST_CASE("softClip: NaN returns zero", "[dsp]")
{
    REQUIRE(spatialcore::DSP::softClip(std::nanf("")) == 0.0f);
}

TEST_CASE("outputLimiter: passthrough below threshold", "[dsp]")
{
    REQUIRE(spatialcore::DSP::outputLimiter(0.5f) == Catch::Approx(0.5f));
}
```

- [ ] **Step 3: Commit**

```bash
git add tests/
git commit -m "feat: add Catch2 test infrastructure with smoke tests"
```

---

### Task 11: Build Verification

- [ ] **Step 1: Run CMake configure**

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
```

Expected: Configuration succeeds, finds JUCE, fetches libmysofa and Catch2.

- [ ] **Step 2: Build library**

```bash
cmake --build build --config Release
```

Expected: SpatialCore static library compiles without errors.

- [ ] **Step 3: Run tests**

```bash
cmake --build build --target SpatialCoreTests
./build/SpatialCoreTests
```

Expected: All smoke tests pass.

- [ ] **Step 4: Fix any build errors and re-verify**

- [ ] **Step 5: Commit any fixes**

---

### Task 12: Update Documentation

- [ ] **Step 1: Update docs/development-roadmap.md**

Update "What Exists" section to reflect the new scaffolding: directory structure, CMakeLists.txt, public API headers, stub implementations, test infrastructure. Update "What Does NOT Exist Yet" accordingly (still missing: actual algorithm implementations, HRTF data files, real test coverage).

- [ ] **Step 2: Commit**

```bash
git add docs/development-roadmap.md
git commit -m "docs: update roadmap to reflect scaffolding completion"
```
