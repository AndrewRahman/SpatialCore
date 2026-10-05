# SpatialCore

**The spatial audio rendering engine behind the Spatial Media Library.**

SpatialCore is a C++ static library that provides everything needed to spatialize audio objects in 3D space and render them to any output format — from binaural headphones to Dolby Atmos speaker arrays. It is the shared foundation that all [Spatial Media Library](https://spatialmedialab.org) plugins are built on.

## What SpatialCore Does

SpatialCore takes audio objects with 3D positions (azimuth, elevation, distance) and renders them to the listener through one of five rendering paths:

| Rendering Path | Output | How It Works |
|---------------|--------|-------------|
| **Direct Binaural HRTF** | 2ch headphones | Per-source HRTF convolution at exact 3D direction via SOFA files |
| **Simple Binaural** | 2ch headphones | Woodworth ITD+ILD model (low CPU, no convolution) |
| **Stereo Variants** | 2ch monitors | 5 mic simulation modes (Equal Power, VBAP, XY, MS, Blumlein) |
| **Discrete Surround** | 4-16ch speakers | Algorithm-based speaker gain computation (VBAP, VBIP, KNN, DBAP, MDAP) |
| **Ambisonics Output** | 4-49ch HOA | Spherical harmonic encoding (1st through 6th order, AmbiX: ACN channel order, SN3D, no Condon-Shortley phase) |

## Components

### Spatialization Algorithms (8)
- **Constant Power** — Cosine-distance all-speaker weighting, constant-power normalized (smooth, wide image)
- **VBAP** — Vector Base Amplitude Panning (Pulkki 1997); below the horizon on height layouts it uses the ITU-R BS.2127 (EAR) lower-hemisphere construction and matches the EAR reference renderer, except behind the listener on 5.1.4 (see docs/integration-guide.md)
- **VBIP** — Vector Base Intensity Panning (Pernaux, Boussard & Jot, DAFx-98) — VBAP gains raised to exponent 1/2 and renormalized, aiming the energy vector at the source; wider than VBAP. Single-band: the paper's high-frequency half at all frequencies (dual-band tracked in AndrewRahman/SpatialCore#20).
- **KNN** — K-Nearest Neighbor (inverse-distance-squared weighting)
- **DBAP** — Distance-Based Amplitude Panning (Lossius et al., ICMC 2009; effective rolloff 12.04 dB, no spatial blur)
- **MDAP** — Multiple-Direction Amplitude Panning (Pulkki, WASPAA 1999, source spread)
- **Ambisonics** — 3rd-order HOA with max-rE weighting (ACN/SN3D, no Condon-Shortley phase)
- **Direct Binaural** — Woodworth ITD+ILD (internal, for Simple profile)

### HRTF Binaural Rendering
- **HRTFDatabase** — SOFA file loading via libmysofa (from a file, from memory, or `loadFromBinaryData` for the embedded profiles), KD-tree HRIR lookup
- **PartitionedConvolver** — Real-time FFT overlap-save convolution
- **BinauralRenderer** — 12 per-source convolvers, double-buffered profile swap
- **HRTF Profiles:** 5 SOFA HRTF profiles ship; `profileIndex` is 0–5, where 0 = Simple (Woodworth) and 1–5 select the SOFA profiles. The five SOFA files are MIT KEMAR, SADIE II D2, CIPIC Subject003, HUTUBS PP2, and Bernschuetz KU100. The files are embedded in the library at build time (`SPATIALCORE_EMBED_ALL_HRTF`, default ON; OFF embeds MIT KEMAR only), so a plugin needs no install step. A file with the same name as a built-in, placed in the system shared folder (`/Library/Application Support/Spatial Media Lab/HRTF/` on macOS, `%ProgramData%\Spatial Media Lab\HRTF\` on Windows), replaces that built-in at the next switch. One engine call, `RenderEngine::setHRTFProfile (profileIndex)`, switches profile click-free in the background and `getHRTFProfileStatus()` reports the result. Profile 0 (Simple) is the Woodworth path with a rear head-shadow and up/down pinna cue bank, so front, back, above and below no longer sound alike.

### Output Format Support (23 formats)
- 1 Binaural (HRTF head model)
- 1 Stereo (5 mic simulation sub-modes)
- 15 Surround (Quad through 9.1.6 Atmos)
- 6 Ambisonics (FOA through 6th Order)

### Speaker Layouts
- 15 ITU-R BS.775/BS.2051 standard layouts with SMPTE channel ordering
- 16-speaker virtual array for binaural rendering (9 ear-level + 6 height + zenith)
- Automatic bus negotiation — format detected from DAW track I/O

### ADM-OSC Integration
- **Receive:** Parse `/adm/obj/N/azim|elev|dist|aed|xyz` messages, Cartesian-to-Polar conversion (ITU-R BS.2127-0)
- **Send:** Broadcast `/adm/obj/N/aed` at 30Hz with position-change gating

### Trajectory Engine
- 13 animation shapes (Orbit, Figure-8, Spiral, Heart, Helix, Bounce, Line, Cross, Square, Triangle, Infinity, Random, None)
- Origin-point architecture: knobs = live origin, trajectories compute animated position
- Forward/reverse direction per object

### Shared UI Components
- **SpatialMapComponent** — 2D top-down spatial map with object dragging, distance rings, elevation opacity encoding, trajectory glow trails
- **SMLLookAndFeel** — Dark theme (#0A0A14), cyan/purple/amber color system, 48px arc knobs
- **ReverseSlider** — Azimuth knob (clockwise rotation = clockwise on map)
- **IndicatorToggle** — Toggle pill with indicator dot
- **StyledButton** — Centered-text button

### Utility DSP
- `softClip()` — Soft saturation for delay input and feedback
- `outputLimiter()` — +2 dB (1.2589) tanh soft ceiling: `1.2589f * tanh(x / 1.2589f)`

## Integration

### Adding SpatialCore to a Plugin

```bash
# Add as git submodule.
# AndrewRahman/SpatialCore is the development remote and the URL to use today;
# Spatial-Media-Lab/SpatialCore is the post-proof destination and does not resolve yet.
git submodule add https://github.com/AndrewRahman/SpatialCore.git SpatialCore
```

```cmake
# In your CMakeLists.txt
add_subdirectory(SpatialCore)          # defines both SpatialCore + SpatialCoreUI targets

# DSP engine only:
target_link_libraries(YourPlugin PRIVATE SpatialCore)
# ...or DSP + shared UI (spatial map, SML look-and-feel, widgets):
target_link_libraries(YourPlugin PRIVATE SpatialCore SpatialCoreUI)
```

```cpp
// In your PluginProcessor.h
#include <SpatialCore/SpatialCore.h>
```

See [docs/integration-guide.md](docs/integration-guide.md) for the complete integration tutorial.

## Dependencies

- **JUCE 9.0.0** (C++17) — audio plugin framework
- **libmysofa** v1.3.5 (FetchContent) — SOFA file parsing
- **zlib** (system) — compression for SOFA files
- **Catch2** v3.7.1 (FetchContent, tests only) — unit testing

## Building

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### Local test gate

There are two test executables: `SpatialCoreTests` (DSP, I/O, OSC, trajectories, engine) and `SpatialCoreUITests` (the map and its fonts; the only target that links `SpatialCoreUI`). The gate builds every target in Debug and runs both executables, then builds every target in Release and runs `ctest`:

```bash
# Debug
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j8
./build/tests/SpatialCoreTests
./build/tests/SpatialCoreUITests

# Release
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j8
ctest --test-dir build-release/tests --output-on-failure
ctest --test-dir build-release/tests -N | grep 'ui:'
```

The last command is a listing check: it must print tests whose names start with `ui:` (CTest drops the space after the prefix), which proves the UI executable is in the run. Build every target before `ctest`. A Catch2 target that has not been built registers a failing `_NOT_BUILT` test, so a partial build fails for the wrong reason.

CI currently runs zero tests (this is fixed in Phase 6), so this local gate is the one that counts.

### Demo app

`SpatialCoreDemo` is a small JUCE app that shows how a plugin wires ADM-OSC, trajectories and the spatial map into `RenderEngine` (see `examples/demo/DemoComponent.cpp`). It is off by default and is only built in a top-level build, so a consumer that adds SpatialCore as a subdirectory never builds it:

```bash
cmake -S . -B build-demo -DSPATIALCORE_BUILD_EXAMPLES=ON -DSPATIALCORE_BUILD_TESTS=OFF
cmake --build build-demo --target SpatialCoreDemo -j8
```

Run it with no arguments for the interactive app. `--screenshots <dir>` writes `before-drag.png` and `after-drag.png` with no window and no audio device. `--selftest` drives the OSC, query, trajectory and map routes through `RenderEngine` on loopback and exits 0 when all four pass.

## Versioning

SpatialCore follows semantic versioning (`v1.0.0`). Each plugin pins to a specific version via its git submodule pointer.

- **Major:** Breaking API changes (new virtual methods, removed functions)
- **Minor:** New features (new algorithm, new output format, new UI widget)
- **Patch:** Bug fixes

### Updating SpatialCore in a consumer

A change to SpatialCore only reaches a plugin after you:
1. Commit + push the change in the SpatialCore repo.
2. In the consumer repo, `cd SpatialCore && git pull` (or checkout the tag), then `git add SpatialCore` in the consumer to bump the submodule pointer, and commit.

The consumer builds whatever commit its submodule pointer references — editing SpatialCore in place without bumping the pointer changes nothing in a clean build (this is what `build_version.sh` and CI both do).

## License

Dual-licensed:
- **[GPL-3.0](LICENSE)** — free for open-source use
- **Commercial License** — available from [Spatial Media Lab](https://spatialmedialab.org)

HRTF data from third-party sources under their respective licenses (MIT, Apache 2.0, CC BY, Public Domain).

## Credits

Part of the **Spatial Media Library** by [Spatial Media Lab](https://spatialmedialab.org).

*Making spatial media easy to create, open to explore, and greater to enjoy.*
