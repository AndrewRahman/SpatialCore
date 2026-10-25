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
| **Ambisonics Output** | 4-49ch HOA | Plain AmbiX spherical harmonic encoding (1st through 6th order: ACN channel order, SN3D, no Condon-Shortley phase), with no near-field filter and no max-rE weighting |

## Components

### Spatialization Algorithms (8)
- **Constant Power** — Cosine-distance all-speaker weighting, constant-power normalized (smooth, wide image)
- **VBAP** — Vector Base Amplitude Panning (Pulkki 1997); below the horizon on height layouts it uses the ITU-R BS.2127 (EAR) lower-hemisphere construction and matches the EAR reference renderer, except behind the listener on 5.1.4 (see docs/integration-guide.md)
- **VBIP** — Vector Base Intensity Panning (Pernaux, Boussard & Jot, DAFx-98) — VBAP gains raised to exponent 1/2 and renormalized, aiming the energy vector at the source; wider than VBAP. Single-band: the paper's high-frequency half at all frequencies (dual-band tracked in AndrewRahman/SpatialCore#20).
- **KNN** — K-Nearest Neighbor (inverse-distance-squared weighting)
- **DBAP** — Distance-Based Amplitude Panning (Lossius et al., ICMC 2009; effective rolloff 12.04 dB, no spatial blur)
- **MDAP** — Multiple-Direction Amplitude Panning (Pulkki, WASPAA 1999, source spread)
- **Ambisonics** — 3rd-order HOA speaker decode with max-rE weighting (ACN/SN3D, no Condon-Shortley phase). The max-rE belongs to this speaker decode; the Ambisonics output formats are plain AmbiX and carry no weighting
- **Direct Binaural** — Woodworth ITD+ILD (internal, for Simple profile)

### HRTF Binaural Rendering
- **HRTFDatabase** — SOFA file loading via libmysofa (from a file, from memory, or `loadFromBinaryData` for the embedded profiles), KD-tree HRIR lookup
- **PartitionedConvolver** — Real-time FFT overlap-save convolution
- **BinauralRenderer** — one per-source convolver pair per object slot (`MAX_SOURCES` = 32), double-buffered profile swap
- **HRTF Profiles:** 5 SOFA HRTF profiles ship; `profileIndex` is 0–5, where 0 = Simple (Woodworth) and 1–5 select the SOFA profiles. The five SOFA files are MIT KEMAR, SADIE II D2, CIPIC Subject003, HUTUBS PP2, and Bernschuetz KU100. The files are embedded in the library at build time (`SPATIALCORE_EMBED_ALL_HRTF`, default ON; OFF embeds MIT KEMAR only), so a plugin needs no install step. A file with the same name as a built-in, placed in the system shared folder (`/Library/Application Support/Spatial Media Lab/HRTF/` on macOS, `%ProgramData%\Spatial Media Lab\HRTF\` on Windows), replaces that built-in at the next switch. One engine call, `RenderEngine::setHRTFProfile (profileIndex)`, switches profile click-free in the background and `getHRTFProfileStatus()` reports the result. Profile 0 (Simple) is the Woodworth path with a rear head-shadow and up/down pinna cue bank, so front, back, above and below no longer sound alike.

### Output Format Support (23 formats)
- 1 Binaural (HRTF head model)
- 1 Stereo (5 mic simulation sub-modes)
- 15 Surround (Quad through 9.1.6 Atmos)
- 6 Ambisonics (FOA through 6th Order)

### Speaker Layouts
- 15 ITU-R BS.775/BS.2051 standard layouts with SMPTE channel ordering
- Binaural output uses no virtual speaker array: each source is rendered directly (per-source HRTF convolution, or the Simple Woodworth path)
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
#include <SpatialCore/SpatialCoreUI.h>   // only when you link SpatialCoreUI
```

SpatialCore's own tests are not built in a consumer's build unless that build sets `SPATIALCORE_BUILD_TESTS=ON`.

See [docs/integration-guide.md](docs/integration-guide.md) for the complete integration tutorial.

## Dependencies

- **JUCE 9.0.0** (C++17) — audio plugin framework
- **libmysofa** v1.3.5 (FetchContent) — SOFA file parsing
- **zlib** (system on macOS and Linux) — compression for SOFA files. Windows has no system zlib, so a Windows build supplies it itself: CI installs the vcpkg package `zlib:x64-windows-static-md` and passes `ZLIB_INCLUDE_DIR` and `ZLIB_LIBRARY` (the static library is named `zs.lib`, a name CMake's FindZLIB does not search) at configure time; see the `windows-build` job in `.github/workflows/ci.yml`.
- **Catch2** v3.7.1 (FetchContent, tests only) — unit testing

## Building

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### Local test gate

There are two test executables: `SpatialCoreTests` (DSP, I/O, OSC, trajectories, engine) and `SpatialCoreUITests` (the map and its fonts; the only target that links `SpatialCoreUI`).

**Which tests to run when (decision P7-D01, 2026-10-25).** The moment decides the tier: quick checks while you work, the whole Release suite before `main`, and everything else once, when you prepare a release. This section is the one place the tiers and their commands are written; CLAUDE.md, the integration guide and the planning handoffs point here. P7-D01 replaces the everyday-gate wording of D-19 (2026-10-24); D-18 and the release-only part of D-19 stand.

#### No test run: documentation and planning changes

A change that touches only Markdown, `.planning/` files or other documentation needs no test run. A comment edit inside a `.h` or `.cpp` file still compiles, so it takes the light tier.

#### Light tier: while iterating

Build the Release tree, then run only the tests for the area you changed. It is much quicker than the whole suite.

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release   # first time only
cmake --build build-release --target SpatialCoreTests -j8
./build-release/tests/SpatialCoreTests --filenames-as-tags "[#BinauralRendererTests]~[.]"
```

`--filenames-as-tags` gives every test case a tag made from its file name (its short form `-#` is not used because zsh with EXTENDED_GLOB treats it as a pattern), so `[#BinauralRendererTests]` selects the cases in `tests/Binaural/BinauralRendererTests.cpp`. Several files are joined with commas, each with its own `~[.]`. `~[.]` keeps hidden capture cases out, because a filter that names a hidden case's file runs it, and the `[.][ambi-maxre-capture]` case fails without its `SC_CAPTURE_FILE` variable.

To run every test file of one folder:

```bash
area=Binaural; ./build-release/tests/SpatialCoreTests --filenames-as-tags "$(for f in tests/$area/*.cpp; do printf '[#%s]~[.],' "$(basename "$f" .cpp)"; done | sed 's/,$//')"
```

| You changed | Run |
|-------------|-----|
| `Algorithms/` | `area=Algorithms` |
| `Binaural/` | `area=Binaural` |
| `Core/` | `area=Core` |
| `Engine/` | `area=Engine` (it includes the 20-second `[rt]` trap) |
| `IO/` | `area=IO` |
| `OSC/` | `area=OSC`, or `"[osc]"`, which is on every case in `tests/OSC/` |
| `Trajectory/` | `area=Trajectory` |
| `UI/` | `cmake --build build-release --target SpatialCoreUITests -j8`, then `./build-release/tests/SpatialCoreUITests` (the whole UI executable) |
| `CMakeLists.txt`, an umbrella header (`SpatialCore.h`, `SpatialCoreUI.h`) or code several areas use | the full Release `ctest` of the next section |

The area tags (`[binaural]`, `[engine]`, `[io]` and the like) are not on every case in their folder: `[binaural]` is on 25 of the 70 cases in `tests/Binaural/` (counted 2026-10-25), so a tag run can miss the test that covers your change. Use the file tags.

#### Before merging or pushing to `main`

Run the whole Release suite. `main` has no CI run and no branch protection, so this is the only check that catches a break in an area you did not touch. It takes about 2 minutes, including the 20-second `[rt]` trap.

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j8
ctest --test-dir build-release/tests --output-on-failure
ctest --test-dir build-release/tests -N | grep 'ui:'
```

The last command is a listing check: it must print tests whose names start with `ui:` (CTest drops the space after the prefix), which proves the UI executable is in the run. Build every target before `ctest`. A Catch2 target that has not been built registers a failing `_NOT_BUILT` test, so a partial build fails for the wrong reason.

#### Full tier: only when preparing a release

Run all of these once, when you prepare a release (a release candidate or the final release):

1. The Debug build with both executables (about 8 minutes for `SpatialCoreTests`):

   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
   cmake --build build -j8
   ./build/tests/SpatialCoreTests
   ./build/tests/SpatialCoreUITests
   ```

2. The Release `ctest` above (it includes the `[rt]` trap).
3. The realtime-safety check below, steps 2-5 (the `build-tsan` tree).
4. Coverage ("Coverage" below).
5. The consumer harness ("Consumer harness" below; the commit must be pushed first).
6. The speaker-rig and DAW listening checks, by ear (the open ones are listed in `.planning/ROADMAP.md`).
7. With the user's go-ahead, a manual CI run (next paragraph).

CI (`.github/workflows/ci.yml`) runs this same gate only for a public release: on its own when a release tag is pushed (a release candidate such as `v1.0.0-rc1`, or the final `v1.0.0`), or when started by hand (Actions, "SpatialCore CI", Run workflow). A push to a branch or a pull request starts no GitHub build (D-20, 2026-10-24): `macos-gate` on GitHub's Apple Silicon Mac (it also builds and runs the consumer harness, see "Consumer harness" below), `linux-tests` (the Release suite on Ubuntu; the allocation-trap cases report skipped there), `windows-build` (an MSVC build of every target, no test run) and `coverage` (see "Coverage" below). Run the local gate before you push anyway, because a failure is quicker to fix here. `ctest --test-dir build-release` from the build root now finds the tests too (the `build-release/tests` form above still works). `main` has no branch protection yet, so nothing on GitHub stops a push whose CI would fail: this local gate is the safety net before every push to `main`, which is pushed to directly as before. Protection that requires the CI jobs may be added later.

#### Realtime-safety check (before a release tag)

The everyday gate above is unchanged. This second check proves that nothing `RenderEngine::renderBlock` reaches allocates, takes a lock or races with another thread. Run steps 2-5 only before a release tag (or by choice after a large change to audio-thread code), not on every edit, not at the end of every phase and not on a timer. Step 1, the 20-second `[rt]` trap, is cheap and already runs inside the everyday Release `ctest` gate. Proved on macOS (Apple Silicon) only. On Linux the whole suite runs in CI, but the allocation and lock trap is built on macOS only, so its cases are skipped there; Windows is built in CI and its tests are not run. On Linux and Windows JUCE 9.0.0 selects its fallback FFT, whose `perform()` takes a `SpinLock` (`juce_FFT.cpp:115`), so the "no lock" part of this proof does not carry over to those platforms (see the note in `Binaural/SharedFFTCache.h`). No claim covers a real DAW.

**Why it exists and when to run it.** Audio code must never pause. The host asks the plugin for a chunk of sound about every 10 milliseconds, and if the code grabs memory or waits for a lock at that moment, the chunk arrives late and the listener hears a click or a dropout. These faults come and go, so they are easy to miss when you listen to a test. A 2026-08-09 audit found real bugs of exactly this kind: two places in `BinauralRenderer` could allocate memory in a Release build (RTSF-01), and the trajectory values were read and written by different threads without protection (RTSF-02). Phase 5 fixed them and added this check (this section was added in commit `947fa58`, 2026-10-06) with the rule "every phase end and before a release tag". The ThreadSanitizer runs (steps 2-5) are slow because ThreadSanitizer watches every memory access, which makes the suite 5 to 15 times slower: about 17 minutes for the subset and about 32 minutes for the whole suite on this Mac, about 100 minutes on GitHub. For a pre-1.0 library with a few dozen users, the trade is: the 20-second trap (step 1) catches exactly the click-causing bugs and costs almost nothing, so it runs in every gate; the ThreadSanitizer runs go before a release only, or by choice after a large change to audio-thread code (decision of 2026-10-24, STATE.md D-18). The `realtime-nightly` CI job has no schedule any more: it runs on its own when a release tag is pushed (a release candidate or the final release), or when you start it by hand (Actions, "SpatialCore CI", Run workflow, `realtime` on). The last full pass was CI run 39058330902 on 2026-10-17.

```bash
# 1. Allocation and lock trap, Release (macOS only, skipped elsewhere; also part of the Release ctest).
#    The trap is armed in any macOS tree built without a sanitizer, Debug included; the HRIR-lookup
#    cases assert allocations in Release only.
./build-release/tests/SpatialCoreTests "[rt]"

# 2. ThreadSanitizer tree (third tree, git-ignored)
cmake -S . -B build-tsan -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_C_FLAGS="-fsanitize=thread -fno-omit-frame-pointer" \
  -DCMAKE_CXX_FLAGS="-fsanitize=thread -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread" \
  -DCMAKE_SHARED_LINKER_FLAGS="-fsanitize=thread"
# Optional: -DFETCHCONTENT_FULLY_DISCONNECTED=ON plus -DFETCHCONTENT_SOURCE_DIR_JUCE=..., _CATCH2=... and _MYSOFA=...
# pointing at an existing tree's _deps/*-src avoids a second download.
cmake --build build-tsan -j8

# 3. Thread-interaction subset ([tsan]: every [hrtf-switch] case (including the misuse and in-use ones), the [sc16] and [sc17] cases, [fftcache], [multi] and [trajectory][threads])
SPATIALCORE_TEST_TIMEOUT_SCALE=10 TSAN_OPTIONS="halt_on_error=0 second_deadlock_stack=1" ./build-tsan/tests/SpatialCoreTests "[tsan]" > /tmp/sc-tsan.log 2>&1; echo "exit=$?"
grep -c "WARNING: ThreadSanitizer" /tmp/sc-tsan.log

# 4. The UI suite under ThreadSanitizer
SPATIALCORE_TEST_TIMEOUT_SCALE=10 TSAN_OPTIONS="halt_on_error=0 second_deadlock_stack=1" ./build-tsan/tests/SpatialCoreUITests > /tmp/sc-tsan-ui.log 2>&1; echo "exit=$?"
grep -c "WARNING: ThreadSanitizer" /tmp/sc-tsan-ui.log

# 5. The whole SpatialCoreTests suite under ThreadSanitizer
SPATIALCORE_TEST_TIMEOUT_SCALE=10 TSAN_OPTIONS="halt_on_error=0 second_deadlock_stack=1" ./build-tsan/tests/SpatialCoreTests > /tmp/sc-tsan-full.log 2>&1; echo "exit=$?" >> /tmp/sc-tsan-full.log
tail -1 /tmp/sc-tsan-full.log
grep -c "WARNING: ThreadSanitizer" /tmp/sc-tsan-full.log
```

A run passes only when its exit status is 0 and its warning count is 0. The ThreadSanitizer tree is `RelWithDebInfo` because an `-O0` instrumented SADIE HRTF load takes longer than the engine's 15 s shutdown wait. The allocation trap is not built in that tree (ThreadSanitizer's allocator hides allocations from it), so its cases report skipped there and Catch2's summary line counts them as skipped instead of saying "All tests passed"; the recorded exit status is the verdict. Measured run times on an Apple Silicon Mac, each run alone: `[rt]` in Release about 20 s, the `[tsan]` subset about 17 min (992 s, 40 cases), the UI suite under ThreadSanitizer about 45 s, the whole suite under ThreadSanitizer about 32 min (1938 s, 386 cases of which 43 are skipped trap cases). Run the last one in the background and wait for it. CI runs steps 1-5 as the `realtime-nightly` job, on a release-tag push or a manual run only (no schedule, no branch-push trigger); on a GitHub macOS runner the measured times were about 46 s for `[rt]`, 1887 s for the `[tsan]` subset, 58 s for the UI suite and 3183 s for the whole suite (100 minutes for the job, including both builds). Run them (by hand, or through the manual CI run) before a release tag.

### Demo app

`SpatialCoreDemo` is a small JUCE app that shows how a plugin wires ADM-OSC, trajectories and the spatial map into `RenderEngine` (see `examples/demo/DemoComponent.cpp`). It is off by default and is only built in a top-level build, so a consumer that adds SpatialCore as a subdirectory never builds it:

```bash
cmake -S . -B build-demo -DSPATIALCORE_BUILD_EXAMPLES=ON -DSPATIALCORE_BUILD_TESTS=OFF
cmake --build build-demo --target SpatialCoreDemo -j8
```

Run it with no arguments for the interactive app. `--screenshots <dir>` writes `before-drag.png` and `after-drag.png` with no window and no audio device. `--selftest` drives the OSC, query, trajectory and map routes through `RenderEngine` on loopback and exits 0 when all four pass.

### Consumer harness

`examples/harness/` is a throwaway plugin that consumes SpatialCore the way OpenSpatialDelay does: through a real git submodule of this repository, fetched from GitHub at a pushed commit. It links both `SpatialCore` and `SpatialCoreUI`, includes both umbrella headers, uses the embedded HRTF data, switches on every opt-in `RenderEngine` feature, and ends with a headless sound check of every output format. The commit must be pushed first, because the harness never reads the local source folder:

```bash
git push origin HEAD
examples/harness/scripts/build-consumer.sh "$(git rev-parse HEAD)"
```

To reuse downloaded sources instead of fetching JUCE and libmysofa again, add a `--` and the pass-through flags, for example `-- -DFETCHCONTENT_SOURCE_DIR_JUCE=$PWD/build/_deps/juce-src -DFETCHCONTENT_SOURCE_DIR_MYSOFA=$PWD/build/_deps/mysofa-src`. Set `HARNESS_OSD_LIKE=1` for OpenSpatialDelay's own configuration (its libmysofa v1.3.2 and fast-math on its own targets). A pass ends with `HarnessCheck: PASS`. The `macos-gate` CI job runs it on a release-tag or manual run. Details are in [examples/harness/README.md](examples/harness/README.md).

### Coverage

`scripts/coverage.sh` builds an instrumented Debug tree (`build-cov/`, git-ignored with `SPATIALCORE_ENABLE_COVERAGE=ON`, Clang or Apple Clang only), runs both test executables and writes the report to `build-cov/coverage/summary.md` (HTML pages under `build-cov/coverage/html/`). It takes about 10 minutes on an Apple Silicon Mac. The engine (`include/` and `src/` outside the UI) and the UI are reported separately.

The measured figures on 2026-10-17 (CI run 39074814315 on `310fb93`): engine 94.19% of lines (3847 of 4084), UI 25.89% (419 of 1618). The target lives in `scripts/coverage-targets.json`: the engine must stay at 93% of lines or more, and the `coverage` CI job fails below it. The UI figure is shown and is not a rule. Reports never leave GitHub: the table is on the run's summary page and the full report is the `coverage-report` artifact.

## Versioning

SpatialCore follows semantic versioning. It is pre-1.0 (currently `0.5.0`) because no public release uses it yet; it becomes `1.0.0` when OpenSpatialDelay ships a release built on it (that release is OpenSpatialDelay 2.0.0). Until then the minor number tracks the milestone phase and the API may change between minors. Each plugin pins to a specific commit via its git submodule pointer.

From 1.0.0 on:

- **Major:** Breaking API changes (new virtual methods, removed functions)
- **Minor:** New features (new algorithm, new output format, new UI widget)
- **Patch:** Bug fixes

### Updating SpatialCore in a consumer

A change to SpatialCore only reaches a plugin after you:
1. Commit + push the change in the SpatialCore repo.
2. In the consumer repo, `cd SpatialCore && git pull` (or checkout the tag), then `git add SpatialCore` in the consumer to bump the submodule pointer, and commit.

The consumer builds whatever commit its submodule pointer references — editing SpatialCore in place without bumping the pointer changes nothing in a clean build (this is what `build_version.sh` and CI both do).

## License

Licensed under **[GPL-3.0](LICENSE)**. SpatialCore is open source and is not sold; there is no commercial licence.

HRTF data from third-party sources under their respective licenses (MIT, Apache 2.0, CC BY, Public Domain).

## Credits

Part of the **Spatial Media Library** by [Spatial Media Lab](https://spatialmedialab.org).

*Making spatial media easy to create, open to explore, and greater to enjoy.*
