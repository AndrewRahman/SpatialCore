---
name: spatialcore-architecture
description: >
  How to build a new Spatial Media Library plugin on SpatialCore. Covers the 68/32 framework split,
  integration patterns (git submodule + CMake), 5 rendering paths, parameter layout conventions,
  and the extraction guide for separating framework code from plugin-specific DSP.
  Use when creating a new SML plugin, extracting SpatialCore from OpenSpatialDelay, refactoring
  the framework boundary, or onboarding to the SpatialCore architecture.
allowed-tools: Read,Write,Edit,Bash,Glob,Grep,WebFetch,WebSearch
metadata:
  category: Development & Engineering
  pairs-with:
    - skill: spatial-audio-dsp
      reason: SpatialCore wraps the algorithms and HRTF infrastructure described in spatial-audio-dsp
    - skill: juce-best-practices
      reason: SpatialCore enforces JUCE realtime safety patterns
    - skill: plugin-architecture-patterns
      reason: Plugin-specific code follows the clean architecture patterns
  tags:
    - spatialcore
    - framework
    - architecture
    - juce
    - audio-plugin
    - extraction
---

# SpatialCore Architecture

How to build plugins on SpatialCore — the shared spatial audio rendering engine for the Spatial Media Library.

## What SpatialCore Is

SpatialCore is a C++ static library extracted from OpenSpatialDelay. It contains **68% of the codebase** — everything needed to spatialize audio objects in 3D and render to any output format. Plugin authors write only the **32% that is unique** to their effect or instrument.

## The 68/32 Split

### SpatialCore Provides (68%)

| Component | What It Does |
|-----------|-------------|
| 7 Spatialization Algorithms | VBAP, VBIP, KNN, DBAP, MDAP, Ambisonics (HOA), DirectBinaural (Woodworth) |
| HRTF Binaural Rendering | HRTFDatabase, PartitionedConvolver, BinauralRenderer (12 per-source convolvers) |
| 6 HRTF Profiles | Simple (Woodworth), MIT KEMAR, SADIE II D2, CIPIC Subject003, HUTUBS PP2, Bernschuetz KU100 |
| 22 Output Formats | 1 Binaural + 1 Stereo (5 modes) + 13 Surround (Quad–9.1.6) + 6 Ambisonics (FOA–6OA) |
| 13 Speaker Layouts | ITU-R BS.775/BS.2051, SMPTE channel ordering |
| Bus Negotiation | Automatic format detection from DAW track I/O |
| ADM-OSC Send/Receive | `/adm/obj/N/` message parsing, 30Hz position broadcast |
| Trajectory Engine | 13 shapes, origin-point architecture, forward/reverse |
| Spatial Map UI | 2D top-down map, object dragging, distance rings, elevation opacity, trajectory trails |
| SMLLookAndFeel | Dark theme, arc knobs, IndicatorToggle, StyledButton, ComboBox/PopupMenu rendering |
| Utility DSP | softClip(), outputLimiter() |

### Plugin Author Writes (32%)

| Component | Examples |
|-----------|---------|
| DSP Engine | Delay line, reverb tank, grain cloud, synth voices, filter bank |
| Effect Parameters | Delay time, feedback, grain density, oscillator type, filter cutoff |
| Render Methods | How your DSP feeds into SpatialCore's spatialization |
| Modulation | LFOs, envelopes, wobble, effect-specific automation |
| UI Controls | Right panel knobs, bottom panel per-object controls |

## Integration Pattern

### Git Submodule
```bash
git submodule add https://github.com/Spatial-Media-Lab/SpatialCore.git SpatialCore
```

### CMake
```cmake
add_subdirectory(SpatialCore)
target_link_libraries(YourPlugin PRIVATE SpatialCore)
```

### Include
```cpp
#include <SpatialCore/SpatialCore.h>
```

## 5 Rendering Paths

Every SML plugin dispatches to one of these based on output format:

| Path | Output | Method |
|------|--------|--------|
| `renderDirectBinauralHRTF` | 2ch (HRTF profiles 1–5) | Per-source HRTF convolution |
| `renderSimpleBinauralWoodworth` | 2ch (profile 0) | Woodworth ITD+ILD, no convolution |
| `renderStereoVariant` | 2ch (stereo mode) | 5 mic simulation sub-modes |
| `renderAmbisonicsOutput` | 4–49ch | SH encoding per object |
| `renderDiscreteSurround` | 4–16ch | Algorithm speaker gains + LFE |

Your plugin implements a per-object DSP function (like `readObjectSample()`) that each rendering path calls. SpatialCore handles the spatialization after your DSP produces mono output per object.

## Parameter Layout Convention

### Keep from SpatialCore
- `outputFormat`, `algorithm`, `hrtfProfile`
- `admOscEnabled`, `admOscSendEnabled`, OSC ports
- Per-object: `object{N}_azimuth`, `object{N}_elevation`, `object{N}_distance`
- Per-object: `object{N}_trajectoryShape`, `object{N}_trajectorySpeed`, `object{N}_trajectoryDirection`
- `dryWet`, `inputGain`, `outputGain`

### Replace with Your Own
- Effect-specific globals (delay time, feedback, grain density, etc.)
- Effect-specific per-object params (pitch shift, filter cutoff, etc.)

## Creating a New Plugin — Checklist

1. Clone the template from `spatial-media-lab/templates/spatialcore-effect/` or `spatialcore-instrument/`
2. Add SpatialCore and JUCE as git submodules
3. Set unique `PLUGIN_CODE` (4 chars) and `PLUGIN_MANUFACTURER_CODE` (`SMLb`)
4. Define your effect-specific parameters in APVTS
5. Implement your DSP engine (the 32%)
6. Wire your per-object DSP into the 5 rendering paths
7. Add your UI controls to the right panel and bottom panel
8. Create factory presets in PresetData
9. Write Catch2 tests
10. Build, install, test in Reaper

## Versioning

SpatialCore uses semantic versioning. Plugins pin to a version via git submodule pointer.

- **Major:** Breaking API changes (new virtual methods on SpatializationAlgorithm)
- **Minor:** New features (new algorithm, output format, UI widget)
- **Patch:** Bug fixes

## Key Design Constraints

- `processBlock` is **lock-free** — no malloc, locks, or logging
- Algorithms are **stateless** — all state in context structs
- Layout changes use **double-buffered atomic swap** for thread safety
- HRTF profile loading is **background threaded** (60Hz timer)
- Position updates are **smoothed** (LinearSmoothedValue, 10ms ramp)
