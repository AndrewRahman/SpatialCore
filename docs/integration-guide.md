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
    // HRTF profiles: no HRTFDatabase or renderer glue of your own. See "HRTF profiles" below.

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
> three-slot layout handoff (`TripleBufferIndex`: the audio thread's layout slot is
> never written, so back-to-back `setOutputFormat()` calls are safe). Call `engine.prepare(sampleRate, maxBlock)` and
> `engine.setOutputFormat(fmt)` in `prepareToPlay`; then per block fill a `RenderSources`
> (the per-object mono signals produced by YOUR effect DSP) plus a `RenderBlockContext`
> (per-block format/layout snapshot) and call `engine.renderBlock(sources, ctx, buffer)`.
> Set `ctx.engineDerivesDispatch = true` (with `ctx.engineComputesGains = true`) instead of
> deriving `isBinaural` / `isStereoVariant` / `isAmbiOutput` / `activeFormat` / `ambiOrder`
> from your own format state: the engine then takes all five from the same per-block
> layout snapshot it renders, so a format switch landing mid-block cannot tear them
> apart (SC-16). Call `setOutputFormat()` from the message thread only, and treat
> `getActiveLayout()` / `getActiveOutputFormat()` as the writer-thread view, not for the
> audio thread.
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
radians, azimuth 0 = front, positive azimuth toward the listener's left (AmbiX +Y; +x in
SpatialCore's internal Cartesian frame, where x = cos(el) sin(az) and y = cos(el) cos(az) is
front), elevation 0 = horizon, positive up. This is the AmbiX convention.

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
  change the sound above the horizon. On a consumer-defined layout whose ear-level ring leaves a
  gap of 179 degrees or more between neighbouring speakers (180 or more, with a 1-degree numerical
  margin; no shipped layout has one, the widest shipped gap is 140 degrees), the first third of the gap stays on the speaker at that edge, the
  middle third pans between the two edge speakers and the last third stays on the other one, so
  every below-horizon direction is still audible. VBAP and VBIP put no gain on an elevated speaker for a source
  at or below -1 degree; MDAP's spread ring can still reach one just below the horizon. Binaural and
  Ambisonics output keep the true negative elevation.
- **3D VBAP picks the minimum-gain-sum triplet** (the tightest enclosing triangle). Above the
  horizon on height layouts, where two triangulations of a coplanar speaker quad tie exactly, float
  rounding decides, so a small azimuth move can jump the gains; this is tracked in
  AndrewRahman/SpatialCore#22 and not fixed. Below the horizon there is no such tie: regular
  triplets are tried first, then the nadir cap, then the pair regions, and regions of one kind meet
  only on shared edges, where they give the same gains. If no triplet encloses a finite direction
  (measured never to happen with the list `RenderEngine` builds), the triplet with the largest
  minimum gain is used, negatives clamped to 0, renormalised; if that clamps to all zeros, the
  nearest speaker gets unity. That path never asserts and is never silent for a non-empty list. A
  consumer calling `computeVBAPGains3D` directly must pass `buildVBAPTripletsForLayout` followed by
  `appendLowerHemisphereTriplets`: the first alone encloses no below-horizon direction, so every
  one of them would take this fallback instead of the EAR construction.

### Non-finite positions

- `RenderEngine::renderBlock` holds the last finite azimuth, elevation and distance per object
  slot (an index into `RenderSources::objects`), field by field, before every render path. The
  held values belong to the slot, not to an object: they update on every block whether or not the
  slot is live, and only the constructor and `prepare()` reset them. A field that has not been
  finite in that slot since the last `prepare()` renders as azimuth 0, elevation 0, distance 0.5.
  A slot reused for a different object whose first update leaves a field non-finite renders that
  field at the slot's last finite value, which may be the previous occupant's, not at the default,
  so send a complete finite position when you assign a slot.
- A consumer that calls `SpatializationAlgorithm::computeGains` directly (as OpenSpatialDelay
  does) bypasses that hold and is protected by the algorithm layer instead: every algorithm returns
  finite gains or silence and never hangs. Every algorithm except DBAP returns silence for a
  non-finite azimuth or elevation (ConstantPower, VBAP, VBIP, MDAP, KNN, Ambisonics and
  DirectBinaural), with one exception: on a flat layout VBAP, VBIP and MDAP pan by azimuth only, so
  a non-finite elevation there is ignored and the source pans normally. DBAP never goes silent and
  always returns unit power: a non-finite azimuth or elevation gives equal gains (1/sqrt(N)) on
  every speaker, and a non-finite distance (NaN, +Inf or -Inf) is treated as 0.5, the
  `SourcePosition` default, so the source still pans by its direction (a finite distance is clamped
  to -1000..1000 so the arithmetic cannot overflow). The 2D VBAP path wraps a
  huge finite azimuth with a bounded `std::remainder` instead of looping.
- These guards live in SpatialCore `.cpp` files, so compiling the consumer with `-ffast-math` does
  not disable them. SpatialCore's own sources must not be compiled with fast-math, and the build
  enforces it: every guarded source includes `src/Core/FloatSemanticsGuard.h`, which stops with
  `#error` under `-ffast-math`, `-ffinite-math-only` or MSVC `/fp:fast`, including when the flag
  arrives through `CMAKE_CXX_FLAGS` or `add_compile_options` before `add_subdirectory(SpatialCore)`.
  Apply fast-math per consumer target (`target_compile_options`), as OpenSpatialDelay does.

## HRTF profiles

SpatialCore carries its own HRTF data and does its own profile switching. A consumer plugin writes
no loader, no file path, no timer and no renderer swap.

**Profile numbers.** They are the numbers a plugin stores in saved sessions, so they never change.

| Profile | Name | What it is |
|---------|------|------------|
| 0 | Simple (Low CPU) | the Woodworth model, no file and no convolution |
| 1 | Immersive | SADIE II D2 |
| 2 | Natural | CIPIC Subject003 |
| 3 | Precise | HUTUBS PP2 |
| 4 | Spatial | Bernschuetz KU100 |
| 5 | Studio Reference | MIT KEMAR large pinna (always embedded) |

**One call.**

```cpp
engine.setHRTFProfile (3);   // message thread; returns at once
```

An engine-owned background worker loads the profile into the renderer the audio thread is not using,
the audio thread picks it up at a block boundary, and the two crossfade (at least 4096 samples, so
also at 32-sample host blocks). The old profile keeps playing until the new one is ready. If several
requests arrive, the latest wins. A request made before the first `prepareToPlay` is held and served
after it. If a request cannot be met (an unknown number, a file that cannot be found) the current
profile keeps playing and nothing is muted.

**Showing the result.** `engine.getHRTFProfileStatus()` is lock-free and safe from any thread; it
returns the requested profile, the active profile, the load state, where the data came from (simple
model, shared folder or built-in) and any problem. Turn it into a line of text for your UI on the
message thread with the free function `spatialcore::describeHRTFProfileStatus (status)`, for example
"profile 3 ready (built-in)" or "profile 9 failed: no such profile". It allocates, so never call it on
the audio thread.

**Let the engine choose the path.** Set `ctx.engineSelectsHRTF = true` together with
`ctx.engineComputesGains = true` in your `RenderBlockContext`. The engine then decides between the
Simple path and the HRTF path from the profile that is actually active and, during a switch between
Simple and an HRTF profile, blends the two, so profile 0 is as click-free as the others. Your own
`useHRTF` is ignored. Both flags default to `false`, so an existing plugin renders exactly as before.
Do not mix `setHRTFProfile` with the older `swapActiveRenderer` / `getPrepareRendererIndex` /
`getBinauralRenderer` loading on the same engine; pick one way.

**Where the data lives.** The five profiles are compiled into SpatialCore (a `SpatialCoreHRTFData`
library linked privately), so there is no install step and no path to set. The CMake option
`SPATIALCORE_EMBED_ALL_HRTF` (default `ON`) embeds all five; `OFF` embeds only profile 5 and is meant
for a future installer that supplies the rest.

**Overriding a built-in.** A file in the system shared folder whose name is exactly a built-in's file
name replaces that built-in at the next switch to that profile, with no restart:

- macOS: `/Library/Application Support/Spatial Media Lab/HRTF/`
- Windows: `%ProgramData%\Spatial Media Lab\HRTF\` (implemented, not yet verified on Windows)

The built-in file names are `sadie_d2_ku100.sofa`, `cipic_subject_003.sofa`, `hutubs_pp2.sofa`,
`bernschuetz_ku100.sofa` and `mit_kemar_large_pinna.sofa`. Any other file in the folder is ignored,
whether the name match is case-sensitive follows the file system, and no per-user folder is looked at.
SpatialCore only reads the folder; it never creates or writes it. A missing folder or file falls back
to the built-in copy silently. A file that is there but unusable (empty, half-copied, a Git LFS stub,
not a SOFA file, or larger than 256 MB) falls back to the built-in copy and the status says
"unreadable, used built-in". Until a dedicated custom-file feature exists, this is also how to try your
own HRTF: name your file like the profile it should replace.

**Git LFS.** The `.sofa` files in `HRTF/` are Git LFS objects. A checkout without them cannot build:
CMake stops at configure time with "is not a real SOFA/HDF5 file (a Git LFS pointer?). Run: git lfs
pull". SpatialCore's own tests still read the source-tree files through `SPATIALCORE_HRTF_DIR`.

**Threading.** `setHRTFProfile`, `waitForHRTFProfileIdle` and `setSharedHRTFFolderForTesting` are
message-thread only. `getHRTFProfileStatus` is safe from any thread. The audio thread only does atomic
operations (loads, stores, exchanges) for the profile handoff: no lock, no allocation, no file access.
That guarantee covers the handoff, not every line of the render path: a few defensive
`jassertfalse` plus `resize` guards remain in the binaural renderer, reachable only if `prepare()`
was not called with a large enough block size (tracked as RTSF-01). `prepare()` itself may block
while a profile load in flight finishes, and must not overlap `setHRTFProfile` or `renderBlock`.

**Moving an existing plugin over.** Delete your HRTF loading glue and any HRTF BinaryData of your own,
call `setHRTFProfile` from your profile parameter, and set `engineSelectsHRTF` and
`engineComputesGains`. While a plugin still embeds its own copy as well, both sets are linked (about
2 x 58 MB); the plugin's own copy goes when its glue does.

**The Simple profile sounds different.** Profile 0 now applies a position-blended rear head-shadow and
a pinna elevation cue bank before the Woodworth gains, so a source behind, above or below the listener
no longer sounds the same as one in front. Ear-level sources in the front half are unchanged. If your
plugin has regression recordings of the Simple path, expect them to differ for sources outside the
front half at ear level, and say so in your release notes.

**Fades and warm-up are timed in samples, for every consumer.** Two more behaviours changed without any
opt-in flag, and both only lengthen a transition at small block sizes (they never shorten one):

- The renderer crossfade of a profile swap lasts `max (8 blocks, kMinRendererXfadeSamples = 4096)`
  samples, fixed when the fade starts. At blocks of 512 samples and above it is the old 8-block fade;
  at 256 and below it is longer (at least 85 ms at 48 kHz).
- A convolver's warm-up before a new IR is faded in now also waits until the new slot has convolved at
  least one IR length of samples. A 558-sample KEMAR IR at 512-sample calls therefore warms for two
  calls where it used to warm for one, and the convolver crossfade is never shorter than
  `PartitionedConvolver::kMinCrossfadeSamples` (2048).

Steady-state output is unchanged by these two; a recording that captures a profile swap will differ
during the transition. Treat the Simple-path change above and these two as one release-note item, and
bump the minor version when you ship them.

## What to Keep vs Replace

| Keep from SpatialCore | Replace with Your DSP |
|----------------------|----------------------|
| Output format detection & bus negotiation | Your effect engine |
| Algorithm selection & dispatch | Your per-object processing |
| HRTF profile loading & convolution (one call: `setHRTFProfile`) | Your modulation/feedback/filters |
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
