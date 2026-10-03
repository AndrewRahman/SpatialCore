# SpatialCore Integration Guide

How to build a new Spatial Media Library plugin using SpatialCore.

## Prerequisites

- JUCE 9.0.0 (C++17)
- CMake 3.22+
- A C++17 compiler (Clang on macOS, MSVC on Windows)

## Step 1: Create Your Plugin Repository

```bash
mkdir OpenSpatialYourEffect
cd OpenSpatialYourEffect
git init
```

## Step 2: Add SpatialCore and JUCE as Submodules

```bash
git submodule add https://github.com/AndrewRahman/SpatialCore.git SpatialCore
git submodule add https://github.com/juce-framework/JUCE.git JUCE
git submodule update --init --recursive   # SpatialCore's HRTF .sofa are Git-LFS
```

> **Remote topology.** `AndrewRahman/SpatialCore` is the deliberate development remote and the
> URL to use today. `Spatial-Media-Lab/SpatialCore` is the post-proof destination: SpatialCore,
> OpenSpatialDelay-on-SpatialCore, and OpenSpatialPanner move to the organisation together once
> the pipeline is proven, so the migration is gated on proof rather than on a date. The
> organisation URL does not resolve yet — do not substitute it.

## Step 3: CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.22)
project(OpenSpatialYourEffect VERSION 0.1.0)

# Add JUCE and SpatialCore
add_subdirectory(JUCE)
add_subdirectory(SpatialCore)

# Create plugin target
juce_add_plugin(OpenSpatialYourEffect
    COMPANY_NAME "Spatial Media Lab"
    PLUGIN_MANUFACTURER_CODE SMLb
    PLUGIN_CODE YrFx
    FORMATS VST3 AU
    PRODUCT_NAME "OpenSpatialYourEffect"
    IS_SYNTH FALSE          # TRUE for instruments (synth, sampler)
    NEEDS_MIDI_INPUT FALSE  # TRUE for instruments
)

# Link SpatialCore (DSP) and SpatialCoreUI (shared spatial map + look-and-feel).
# Both targets are defined by add_subdirectory(SpatialCore). Drop SpatialCoreUI
# only if your plugin builds its entire UI from scratch (Step 5 uses it).
target_link_libraries(OpenSpatialYourEffect PRIVATE SpatialCore SpatialCoreUI)

# Source files
target_sources(OpenSpatialYourEffect PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)
```

## Step 4: PluginProcessor — Using SpatialCore

Your processor integrates SpatialCore by:
1. **Keeping** all spatial parameters (outputFormat, algorithm, hrtfProfile, per-object azimuth/elevation/distance)
2. **Adding** your effect-specific parameters (delay time, filter cutoff, etc.)
3. **Implementing** your DSP in processBlock, calling SpatialCore for spatialization

### Minimal Example

```cpp
#include <SpatialCore/SpatialCore.h>

class YourProcessor : public juce::AudioProcessor {
public:
    // SpatialCore components (from framework)
    spatialcore::BinauralRenderer binauralRenderer;
    spatialcore::SpatializationAlgorithm* algorithms[8];
    spatialcore::HRTFDatabase hrtfDb;

    // Your effect-specific DSP
    // ... (delay lines, filters, grain engines, etc.)

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override {
        // 1. Your effect-specific DSP per object
        for (int obj = 0; obj < numEnabledObjects; ++obj) {
            float monoSample = yourEffectProcess(obj);  // YOUR DSP

            // 2. Get object position from parameters
            spatialcore::SourcePosition pos = {
                azimuthRad[obj], elevationRad[obj], distance[obj]
            };

            // 3. SpatialCore spatializes the object
            // (binaural HRTF, surround gains, or Ambisonics encoding)
            spatialize(obj, monoSample, pos);  // SPATIALCORE
        }

        // 4. Mix dry/wet and output
        mixOutput(buffer);
    }
};
```

> **The real render entry point is `spatialcore::RenderEngine`.** The snippet above is
> conceptual — in practice you do NOT hand-roll `spatialize()` or dispatch render paths
> yourself. `RenderEngine` (`#include <SpatialCore/SpatialCore.h>`, header
> `Engine/RenderEngine.h`) owns all five render paths (direct-binaural HRTF, simple
> binaural, stereo variants, Ambisonics, discrete surround) and the glitch-free
> double-buffered layout swap. Call `engine.prepare(sampleRate, maxBlock)` and
> `engine.setOutputFormat(fmt)` in `prepareToPlay`; then per block fill a `RenderSources`
> (the per-object mono signals produced by YOUR effect DSP) plus a `RenderBlockContext`
> (per-block format/layout snapshot) and call `engine.renderBlock(sources, ctx, buffer)`.
> See `include/SpatialCore/Engine/RenderEngine.h` for the exact struct and signature.

## Step 5: PluginEditor — Using SpatialCore UI

```cpp
#include <SpatialCore/UI/SpatialMapComponent.h>
#include <SpatialCore/UI/SMLLookAndFeel.h>

class YourEditor : public juce::AudioProcessorEditor {
public:
    spatialcore::SMLLookAndFeel smlLookAndFeel;
    spatialcore::SpatialMapComponent spatialMap;

    // Your effect-specific controls
    // ... (knobs, buttons, etc.)

    YourEditor(YourProcessor& p) : AudioProcessorEditor(p) {
        setLookAndFeel(&smlLookAndFeel);
        addAndMakeVisible(spatialMap);
        // Add your controls...
        setSize(820, 580);  // Standard SML plugin size
    }
};
```

## Step 6: Build and Test

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

If your plugin's own CMake defines a post-build copy step, it installs the AU and VST3 to `~/Library/Audio/Plug-Ins/` on macOS. SpatialCore itself is a static library and defines no such step — that belongs to the consumer plugin (see OpenSpatialDelay's `scripts/build_version.sh` for the reference install flow).

## Architecture Pattern

Every SML plugin follows the same architecture:

```
┌─────────────────────────────────────────────┐
│                YOUR PLUGIN                   │
│  ┌────────────────────────────────────────┐  │
│  │  Plugin-Specific DSP (32%)             │  │
│  │  - Your effect engine                  │  │
│  │  - Your parameters                     │  │
│  │  - Your render methods                 │  │
│  └────────────┬───────────────────────────┘  │
│               │ calls                        │
│  ┌────────────▼───────────────────────────┐  │
│  │  SpatialCore (68%)                     │  │
│  │  - 8 spatialization algorithms         │  │
│  │  - HRTF binaural rendering             │  │
│  │  - 23 output formats                   │  │
│  │  - ADM-OSC send/receive                │  │
│  │  - Trajectory engine                   │  │
│  │  - Spatial map UI                      │  │
│  │  - SML LookAndFeel                     │  │
│  └────────────────────────────────────────┘  │
└─────────────────────────────────────────────┘
```

## Ambisonics and panning conventions

What a consumer can rely on when it reads SpatialCore's output. Each statement restates a code
docblock; if the two ever disagree, the code is right and this section is the defect.

### Ambisonics channel convention

From the `evalSH` docblock in `include/SpatialCore/Core/SpatialMath.h`: real spherical harmonics,
ACN channel order (`acn = l*l + l + m`; m > 0 uses cos(m*az), m < 0 uses sin(|m|*az)), SN3D
normalisation (the sum over m of Y_lm^2 is 1 at every order l), no Condon-Shortley phase, angles in
radians, azimuth 0 = front, positive azimuth toward +Y (left), elevation 0 = horizon, positive up.
This is the AmbiX convention.

`spatialcore::evalSH` is the single spherical-harmonic evaluator in SpatialCore;
`AmbisonicsCodec::evaluateSH` forwards to it, and both names stay public. Orders 4-6 were corrected
to true SN3D in Phase 2 (22 constants; AndrewRahman/SpatialCore#11), so a consumer decoding 4OA-6OA
output with an AmbiX decoder now gets correct levels. Orders 0-3 did not change.

### Panning behaviour a consumer can observe

- **VBIP is the textbook law** (Pernaux, Boussard & Jot, DAFx-98): the VBAP gains are raised to
  exponent 1/2 and renormalised to unit power, which aims the energy vector at the source. It is
  wider than VBAP between speakers. It is single-band: the paper's VBIP half (above 700 Hz) applies
  at all frequencies, and dual-band VBAP/VBIP is tracked in AndrewRahman/SpatialCore#20.
- **Below the horizon on height layouts** (no shipped layout has speakers below ear level), VBAP,
  VBIP and MDAP use the ITU-R BS.2127 (EAR) lower-hemisphere construction: a virtual speaker at -30
  degrees under each ear-level speaker plus a virtual nadir, downmixed onto the ear-level speakers
  and power-normalised. Between the horizon and the -30 degree ring each pair of neighbouring
  ear-level speakers is one pan region, as in EAR, so a source keeps exactly the gains its azimuth
  gets at 0 degrees: VBAP on 7.1.4 at azimuth 60 gives 0.7071 / 0.7071 on M+030 / M+090 at every
  elevation from 0 to -30, with no lean toward either speaker. Where neighbouring speakers are far
  apart the region reaches lower (behind the listener on 5.1.x, down to about -59 degrees). Below it
  a source blends to equal gain on the whole ear-level ring at -90 (the nadir at 1/sqrt(n) to each
  of the n ear-level speakers). VBAP's lower-hemisphere gains match PyPI ear 2.1.0, the BS.2127
  reference renderer, to float precision on every shipped height layout, with one deliberate
  exception: on 5.1.4 behind the listener (azimuths beyond 110 degrees either side, between M+110
  and M-110), EAR also feeds the rear height speakers U+135 / U-135, even at and just above the
  horizon; SpatialCore keeps the horizon pan on M+110 / M-110 there, because matching EAR would
  change the sound above the horizon. VBAP and VBIP put no gain on an elevated speaker for a source
  at or below -1 degree; MDAP's spread ring can still reach one just below the horizon. Binaural and
  Ambisonics output keep the true negative elevation.
- **3D VBAP picks the minimum-gain-sum triplet** (the tightest enclosing triangle). Above the
  horizon on height layouts, where two triangulations of a coplanar speaker quad tie exactly, float
  rounding decides, so a small azimuth move can jump the gains; this is tracked in
  AndrewRahman/SpatialCore#22 and not fixed. Below the horizon there is no such tie: regular
  triplets are tried first, then the nadir cap, then the pair regions, and regions of one kind meet
  only on shared edges, where they give the same gains. If no triplet encloses a finite direction
  (measured never to happen on a shipped layout), the triplet with the largest minimum gain is used,
  negatives clamped to 0, renormalised.

### Non-finite positions

- `RenderEngine::renderBlock` holds the last finite azimuth, elevation and distance per object,
  field by field, before every render path. A field that has never been finite renders as azimuth
  0, elevation 0, distance 0.5.
- A consumer that calls `SpatializationAlgorithm::computeGains` directly (as OpenSpatialDelay
  does) bypasses that hold and is protected by the algorithm layer instead: every algorithm returns
  finite gains or silence and never hangs. VBAP, VBIP, MDAP, KNN and DirectBinaural return silence
  for a non-finite direction, and the 2D VBAP path wraps a huge finite azimuth with a bounded
  `std::remainder` instead of looping.
- These guards live in SpatialCore `.cpp` files, so compiling the consumer with `-ffast-math` does
  not disable them. SpatialCore's own sources must not be compiled with fast-math.

## What to Keep vs Replace

| Keep from SpatialCore | Replace with Your DSP |
|----------------------|----------------------|
| Output format detection & bus negotiation | Your effect engine |
| Algorithm selection & dispatch | Your per-object processing |
| HRTF profile loading & convolution | Your modulation/feedback/filters |
| Speaker layout activation | Your tempo sync / timing |
| ADM-OSC receive/send | Your effect-specific parameters |
| Trajectory animation | Your UI controls (right panel, bottom panel) |
| Spatial map component | |
| LookAndFeel & shared widgets | |
| Soft clipper & output limiter | |

## Naming Convention

All SML plugins follow this naming:
- **Repository:** `OpenSpatial{Effect}` (e.g., `OpenSpatialChorus`)
- **Plugin name:** `OpenSpatial{Effect}` (shown in DAW)
- **Plugin code:** Unique 4-char code (e.g., `OsCh` for Chorus)
- **Manufacturer code:** `SMLb` (Spatial Media Lab)

## Testing

Use Catch2 for unit tests:
```cmake
# In CMakeLists.txt
FetchContent_Declare(Catch2 GIT_REPOSITORY https://github.com/catchorg/Catch2.git GIT_TAG v3.7.1)
FetchContent_MakeAvailable(Catch2)

add_executable(YourPluginTests tests/YourTests.cpp)
target_link_libraries(YourPluginTests PRIVATE Catch2::Catch2WithMain SpatialCore)
```

## Preset System

Follow the OpenSpatialDelay pattern:
- Store presets at `~/Library/Audio/Presets/OpenSpatial{Effect}/`
- Use `PresetData` struct with JSON serialization
- Factory presets in C++ source, installed via build-time CLI tool
- User presets in `User/` subfolder, never overwritten
