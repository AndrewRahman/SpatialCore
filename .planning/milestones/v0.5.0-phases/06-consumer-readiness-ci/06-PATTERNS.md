# Phase 6: Consumer Readiness & CI - Pattern Map

**Mapped:** 2026-10-17
**Files analyzed:** 22 (new + modified)
**Analogs found:** 18 / 22 (4 are config/CI/script files with no in-repo analog beyond the current ci.yml)

All analog paths below were confirmed git-tracked source (no gitignored mirrors).

## Line-ending map (preserve on edit)

| File | Endings |
|------|---------|
| `tests/CMakeLists.txt`, `README.md`, `.gitignore` | **CRLF** |
| `docs/integration-guide.md` (pinned LF in `.gitattributes`), `CLAUDE.md`, `CMakeLists.txt`, `examples/CMakeLists.txt`, all `src/`, `include/`, other `tests/*.cpp`, `.planning/*.md` | LF |
| All new files (workflows, harness, scripts, new tests) | LF |

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match |
|-------------------|------|-----------|----------------|-------|
| `CMakeLists.txt` (modify: `enable_testing`, `SPATIALCORE_BUILD_TESTS` default, `SPATIALCORE_ENABLE_COVERAGE`) | config | build | itself, lines 355-364 | exact |
| `tests/CMakeLists.txt` (modify, CRLF: new test sources, coverage UI objects) | config | build | itself, lines 9-40, 85-100, 105-108 | exact |
| `include/SpatialCore/SpatialCore.h` (modify: W2/W3) | utility (umbrella) | n/a | itself | exact |
| `include/SpatialCore/SpatialCoreUI.h` (new) | utility (umbrella) | n/a | `include/SpatialCore/SpatialCore.h` | role-match |
| `include/SpatialCore/Engine/RenderEngine.h` (modify: counter + accessor) | service (facade) | request-response | same file, `getOversizedBlockCount` | exact |
| `src/Engine/RenderEngine.cpp` (modify: detector in `renderBlock`) | service | request-response | same file lines 756, 837, 1866, 1912 | exact |
| `tests/Engine/UnsuppliedGainsTests.cpp` (new) | test | request-response | `tests/Engine/RealtimeSafetyTests.cpp` (`contextFor`, counter CHECKs), `tests/Support/RouteRenderRig.h` | role-match |
| `tests/Core/UmbrellaTests.cpp` (new, source-scan) | test | file-I/O | `tests/UI/FontProvenanceTests.cpp:177-245`; `tests/Core/CountsTests.cpp` | role-match |
| `tests/Binaural/PartitionedConvolverTests.cpp` (modify: `[osd50]`) | test | transform | same file lines 85-125 (pending-IR) | exact |
| `tests/Binaural/SadieD2KU100Tests.cpp` or new `[osd89]` case | test | file-I/O | same file ~line 67 | role-match |
| `examples/harness/CMakeLists.txt` (new) | config | build | `examples/CMakeLists.txt`; RESEARCH "Consumer harness CMake" | role-match |
| `examples/harness/src/HarnessProcessor.{h,cpp}` (new) | service | streaming | `tests/Support/RouteRenderRig.h`; `examples/demo/DemoComponent.cpp` | role-match |
| `examples/harness/src/SoundCheck.cpp` (new) | test/console | batch | `examples/demo/Main.cpp` (`--selftest`); `RealtimeSafetyTests.cpp` format matrix | role-match |
| `examples/harness/scripts/build-consumer.sh` (new) | script | batch | RESEARCH Pattern 1 sketch | no in-repo analog |
| `scripts/coverage-report.py` (new) | utility | transform | RESEARCH aggregation sketch | no analog |
| `.github/workflows/ci.yml` (modify/rewrite) | config | CI | itself | exact |
| `.github/workflows/nightly.yml` (new) | config | CI | README "Realtime-safety check" steps 1-5 | no analog |
| `.gitignore` (modify, CRLF: `build-cov/`) | config | n/a | existing `build-tsan` entry | exact |
| `docs/integration-guide.md`, `README.md`, `CLAUDE.md`, `.planning/REQUIREMENTS.md`, `.planning/ROADMAP.md` (modify) | docs | n/a | existing lines at README:130,166 / guide:509 / CLAUDE.md:141 | exact |

## Pattern Assignments

### `CMakeLists.txt` (config, build)

**Analog:** itself, lines 355-364 (current tail):
```cmake
option(SPATIALCORE_BUILD_EXAMPLES "Build the SpatialCore demo app in examples/" OFF)
if(PROJECT_IS_TOP_LEVEL AND SPATIALCORE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# --- Tests ---
option(SPATIALCORE_BUILD_TESTS "Build SpatialCore tests" ON)
if(SPATIALCORE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
```
Apply: default `${PROJECT_IS_TOP_LEVEL}` (needs user call, RESEARCH A7); add `if(PROJECT_IS_TOP_LEVEL) enable_testing() endif()` before `add_subdirectory(tests)`; add `option(SPATIALCORE_ENABLE_COVERAGE ... OFF)` gated on `PROJECT_IS_TOP_LEVEL` and Clang/AppleClang, adding `-fprofile-instr-generate -fcoverage-mapping` compile and `-fprofile-instr-generate` link flags. Follow the `PROJECT_IS_TOP_LEVEL` gating idiom already at lines 183, 290, 327, 356. Do not apply `-ffast-math` globally (`FloatSemanticsGuard.h` #errors).

### `tests/CMakeLists.txt` (config, CRLF)

**Analog:** itself. Source lists are alphabetic-by-area `add_executable(SpatialCoreTests ...)` (lines 9-40); append `Engine/UnsuppliedGainsTests.cpp` and `Core/UmbrellaTests.cpp`. UI target (lines 85-100) has `SPATIALCORE_SOURCE_DIR` define. Discovery tail:
```cmake
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)
catch_discover_tests(SpatialCoreTests)
catch_discover_tests(SpatialCoreUITests TEST_PREFIX "ui: ")
```
RTGuard guard (line ~73) is `if (APPLE AND NOT CMAKE_CXX_FLAGS MATCHES "-fsanitize")`; coverage flags do not match, trap still builds. Coverage-only `OBJECT` library of `src/UI/*.cpp` added to `SpatialCoreUITests` via `$<TARGET_OBJECTS:...>` (RESEARCH Pitfall 4). Edit with CRLF preserved.

### `include/SpatialCore/SpatialCoreUI.h` (new) and `SpatialCore.h` (modify)

**Analog:** `include/SpatialCore/SpatialCore.h`. Shape: `#pragma once`, comment header, then `#include <SpatialCore/<Area>/<Name>.h>` grouped by `// Area` comments. Fix W3: lines 3-5 comment ("UI headers are deliberately NOT included here: link SpatialCoreUI...") should point at the new `SpatialCoreUI.h`; the "Phase 8 Plan 08-06" comment on the Engine group is stale. Add to the OSC group (currently only ADMOSCReceiver.h, ADMOSCSender.h):
```cpp
#include <SpatialCore/OSC/ADMPositionQuery.h>
#include <SpatialCore/OSC/OSCPortValidation.h>
```
UI umbrella includes the 10 headers in `include/SpatialCore/UI/`: GlobalTapDrawer, IOSectionComponent, IndicatorToggle, OSCSectionComponent, ObjectPanel, PresetBrowser, ReverseSlider, SMLLookAndFeel, SpatialMapComponent, StyledButton.

### `RenderEngine.h` / `RenderEngine.cpp` (INTG-02 detector)

**Analog:** same files, existing D-11 oversized-block counter.

Header declaration + doc style (`RenderEngine.h:431-436`):
```cpp
// Diagnostic (D-11, D-04): how many renderBlock() calls carried more samples than
// prepare()'s maxBlockSize. ... It does not allocate and does not assert ...  Any thread, lock-free.
int getOversizedBlockCount() const noexcept;
```
Member (`RenderEngine.h:864-870`) sits beside the `static_assert (std::atomic<int>::is_always_lock_free, ...)` that already covers it:
```cpp
// D-11: renderBlock() calls longer than preparedMaxBlock_ (relaxed, lock-free).
std::atomic<int> oversizedBlockCount_ { 0 };
```
Count site (`RenderEngine.cpp:755-756`):
```cpp
if (sources.numSamples > preparedMaxBlock_)
    oversizedBlockCount_.fetch_add (1, std::memory_order_relaxed);
```
Count + Debug assert (`RenderEngine.cpp:1862-1867`):
```cpp
legacyRouteWrongThreadCount_.fetch_add (1, std::memory_order_relaxed);
jassertfalse;
```
Accessor (`RenderEngine.cpp:1912-1915`):
```cpp
int RenderEngine::getOversizedBlockCount() const noexcept
{
    return oversizedBlockCount_.load (std::memory_order_relaxed);
}
```
**Placement:** immediately after `const RenderBlockContext& ctx = *dispatchCtx;` (`RenderEngine.cpp:837`), guarded by `! blockCtx.engineComputesGains`, so the post-fill context is read (engineSelectsHRTF fills `objGains` at ~820-826; ProfileSwitchTests:1546 case must count 0). Path-aware: Simple-binaural reads `objGains`, stereo variant reads `objGainL/R`, discrete surround reads `objChannelGains[t][sp < numSpeakers]`; HRTF and Ambisonics read none. Active object = `objectLive[t]`, non-null `monoBuffers[t]` and `tapFadeGainPerSample[t]`, tap fade `> 0` at first/last sample. Suggested name `getUnsuppliedGainsBlockCount()`. No logging, no allocation. Doc comment in header per D-06 (count-only in Release).

### `tests/Engine/UnsuppliedGainsTests.cpp` (new, tag `[intg02]`)

**Analog:** `tests/Engine/RealtimeSafetyTests.cpp` for the counter CHECK style (`CHECK (engine->getOversizedBlockCount() == 1);` lines 378-525) and `contextFor` (lines 909-945) which shows the "flag-off consumer supplies constants" shape the detector must NOT fire on:
```cpp
if (! ctx.engineComputesGains)
{
    const int channels = std::min (info.requiredChannels, MAX_SPEAKERS);
    for (int t = 0; t < MAX_SOURCES; ++t)
    {
        ctx.objGains[t] = BinauralGains { 0.5f, 0.5f, 0.0f, 0.0f };
        ctx.objGainL[t] = 0.5f;  ctx.objGainR[t] = 0.5f;
        for (int ch = 0; ch < channels; ++ch) ctx.objChannelGains[t][ch] = 0.25f;
    }
}
```
Engine setup: `RenderEngine engine; engine.prepare (rate, block); engine.setOutputFormat (...)` (see `RouteRenderRig.h:30-40`). Header block: `#include <catch2/catch_test_macros.hpp>`, `#include <SpatialCore/Engine/RenderEngine.h>`, `#include "../Support/Watchdog.h"` (MultiInstanceTests.cpp:1-12). Misuse tests: Debug `jassertfalse` only logs (no debugger), so a deliberate-misuse test is safe; copy the comment convention at `ProfileSwitchTests.cpp:1664`. Negative cases: HRTF/Ambisonics flag-off (0), flag on (0), engineSelectsHRTF zero gains (0). Match reference from RouteRenderRig for rig reuse.

### `tests/Core/UmbrellaTests.cpp` (new, source-scan)

**Analog:** `tests/UI/FontProvenanceTests.cpp:179,238-245` (reads `SPATIALCORE_SOURCE_DIR`, enumerates files):
```cpp
const juce::File root (SPATIALCORE_SOURCE_DIR);
for (auto* sub : { "src/UI", "include/SpatialCore/UI" })
    for (auto& f : root.getChildFile (sub).findChildFiles (juce::File::findFiles, true, "*.cpp;*.h"))
        files.push_back (f);
```
`SpatialCoreTests` defines `SPATIALCORE_SOURCE_DIR` (tests/CMakeLists.txt:~49). `Core/CountsTests.cpp` is the file-style analog (Catch2 include, `using namespace spatialcore;`, anonymous-namespace helpers, derived-not-hard-coded counts). Rule: every `include/SpatialCore/**/*.h` appears in `SpatialCore.h` or `SpatialCoreUI.h` or a named allow-list.

### `tests/Binaural/PartitionedConvolverTests.cpp` (add `[osd50]`)

**Analog:** same file. Header (lines 1-12): Catch2, `PartitionedConvolver.h`, `BinauralMetrics.h`, `../TestNumerics.h`, `using namespace spatialcore; using namespace spatialcore::test;`. Helpers `whiteNoise`, `directConvolve`, `convolveInChunks`, `blockPlans()`. Closest pattern is the pending-IR test (lines ~85-125):
```cpp
PartitionedConvolver conv;
conv.prepare (512, kIRLength);
conv.setIR (irA.data(), kIRLength);
...
conv.setIR (irB.data(), kIRLength);          // starts a transition (Warmup)
conv.process (input.data() + 2048, out.data() + 2048, 37);
...
CHECK (nonFinite == 0);
CHECK (worstVsC <= 2.0e-6);
CHECK (worstVsB > 1.0e-3);   // proves C, not B, won
```
Code under test (OSD#50, `src/Binaural/PartitionedConvolver.cpp`): dual slots at ~50-54; `setIR` first-IR direct load (~92-100), mid-transition deferral to `pendingIR` (~102-109), Idle load into inactive slot + Warmup (~111-118). The #50 test should assert the swap is click-free: no step discontinuity at the IR change (use `maxAbsStep` from `BinauralMetrics.h`, as the BUG-02 test does at line ~170) and output transitions A to B. Cite `Spatial-Media-Lab/OpenSpatialDelay#50` in the comment (BUG-02 header at line 130 is the citation style). Show red by temporarily removing the dual-slot crossfade (D-17, Phase 5 D-07), restore, record evidence.

Existing regression citations to verify red: #234 `PartitionedConvolverTests.cpp:130`, #96 `Engine/MultiInstanceTests.cpp:20`, #131 `UI/MultiMapTests.cpp:3` and `Engine/MultiInstanceTests.cpp:25`, #89 only a comment at `SadieD2KU100Tests.cpp:67` (needs a direct test of the onset-detection fallback).

### `examples/harness/` (new consumer template)

**Analog:** `examples/CMakeLists.txt` (juce target idiom):
```cmake
juce_add_gui_app(SpatialCoreDemo PRODUCT_NAME "SpatialCoreDemo" NEEDS_WEB_BROWSER FALSE NEEDS_CURL FALSE)
target_sources(SpatialCoreDemo PRIVATE demo/Main.cpp demo/DemoComponent.cpp)
target_compile_definitions(SpatialCoreDemo PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 ...)
target_link_libraries(SpatialCoreDemo PRIVATE SpatialCoreUI SpatialCore juce::juce_audio_devices)
```
Differences: harness is its own CMake project (NOT `add_subdirectory`'d from `examples/CMakeLists.txt`), FetchContent JUCE 9.0.0, sets `SPATIALCORE_BUILD_TESTS OFF CACHE BOOL "" FORCE`, `add_subdirectory(SpatialCore)` on a real submodule, `juce_add_plugin(HarnessPlugin ... FORMATS VST3)` + `juce_add_console_app(HarnessCheck)` sharing `HarnessProcessor.cpp`, linking `SpatialCore SpatialCoreUI`. Full verified CMake in RESEARCH "Code Examples". Needs `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` if it fetches libmysofa v1.3.2.

**Processor/render pattern:** `tests/Support/RouteRenderRig.h` (a consumer-side glue that sets `engineComputesGains` + `engineDerivesDispatch`, constructs `RenderSources` with `monoBuffers`, `tapFadeGainPerSample` (all 1.0), per-object `distGain` buffers, calls `engine.renderBlock`). Extend with the other opt-ins and HRTF calls per RESEARCH "Harness processor calls": `setHRTFProfile (5)`, `engineSelectsHRTF`, `engineFadesFormatSwitch`, `setAlgorithmIndex (kAlgorithmIndexVBAP)`, `getBlockLayout()` after `renderBlock`, `waitForHRTFProfileIdle`, `setSharedHRTFFolderForTesting (emptyTempDir)`. Size output by `OutputFormatRegistry::getInfo (fmt).requiredChannels` (can exceed `MAX_SPEAKERS`), iterate to `NUM_OUTPUT_FORMATS`, assert finite + non-silent + `getUnsuppliedGainsBlockCount() == 0`.

**Console-main pattern:** `examples/demo/Main.cpp` `--selftest` mode (exit code driven); the harness `SoundCheck.cpp` returns non-zero on failure.

### `examples/harness/scripts/build-consumer.sh`, `scripts/coverage-report.py`

No in-repo analog; use the RESEARCH Pattern 1 shell sketch (mktemp, `git init`, `git submodule add https://github.com/AndrewRahman/SpatialCore.git`, `git -C SpatialCore fetch --depth 1 origin "$SHA"`, `checkout --detach FETCH_HEAD`, `lfs pull`, configure, build, run) and the RESEARCH stdlib-only aggregation (`python3 -I`, keys `src/<Area>` and `include/<Area>`, UI reported separately). Use `set -euo pipefail`. Never reference the local source path in the harness CMake.

### `.github/workflows/ci.yml` (modify) and `nightly.yml` (new)

**Analog:** current `ci.yml`. Reusable parts:
```yaml
on:
  push: ...
  pull_request: ...
  workflow_dispatch:
...
- name: Checkout repository
  uses: actions/checkout@v4
  with:
    lfs: true
...
- name: Configure CMake
  run: cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -Wno-dev
```
LFS guard script (ci.yml lines ~21-37) is GNU-only (`stat -c%s`); make it portable (`stat -f%z` on macOS). Linux apt dependency list (ci.yml ~39-47) is copied as-is to `linux-tests`. Defects to fix (D-12): `ctest` at build root with `working-directory: build` (use `ctest --test-dir build --no-tests=error`), stale comments (Phase 8/9, "suite is empty of new tests"), `actions/checkout@v4` to v7, branch list lacks the work branch. Jobs: `macos-gate` (runs `macos-26`; Debug build + both executables; Release `ctest`; `ctest -N | grep 'ui:'`; `build-consumer.sh $GITHUB_SHA`), `linux-tests` (initially `continue-on-error: true`), `windows-build` (build only), `coverage`. Unique job names across workflows; `concurrency` + `permissions: contents: read`; `lfs: false` + `actions/cache` on `.git/lfs` keyed `hashFiles('HRTF/*.sofa')`; explicit `timeout-minutes`. Key on exit code, not "Assertion" text (Pitfall 10). `nightly.yml`: README "Realtime-safety check" steps 1-5 (README lines ~132-160, TSan flags in the README block verbatim), `schedule` with off-minute cron + `workflow_dispatch`, must be merged to `main` first.

### Docs (README CRLF, others LF)

Replace "CI runs zero tests" at `README.md:130` ("CI currently runs zero tests ... local gate is the one that counts") and `README.md:166`, `docs/integration-guide.md:509`, `CLAUDE.md:141` (Tests bullet), and user memory `spatialcore-ship-and-verify-flow.md`. Integration guide gets the D-07 "set these flags" block near lines ~119-158 and ~350. Amend REQUIREMENTS (INTG-01 two-header wording, CI-01 scope note, TEST-01 UI exclusion) and ROADMAP SC-1. Keep `0.x` wording (pre-1.0).

## Shared Patterns

### Misuse counting (lock-free, Debug-assert only)
**Source:** `src/Engine/RenderEngine.cpp:756, 1862-1867, 1912-1915`; `RenderEngine.h:436, 864-870`
**Apply to:** the INTG-02 detector. Relaxed `fetch_add`, `jassertfalse` after counting, never log or allocate on the audio thread; accessor `noexcept`, any thread.

### Regression test provenance
**Source:** `tests/Binaural/PartitionedConvolverTests.cpp:130`, `tests/Engine/MultiInstanceTests.cpp:20-25`
**Apply to:** `[osd50]`, `[osd89]`. Cite `Spatial-Media-Lab/OpenSpatialDelay#N` in a header comment, show red against old behaviour first, record evidence.

### Source-scan rules via `SPATIALCORE_SOURCE_DIR`
**Source:** `tests/UI/FontProvenanceTests.cpp:238-245`; define at `tests/CMakeLists.txt` (both executables)
**Apply to:** `UmbrellaTests.cpp` and any doc-truth check.

### Consumer-side RenderEngine driving
**Source:** `tests/Support/RouteRenderRig.h:1-60`
**Apply to:** `HarnessProcessor.cpp`, `SoundCheck.cpp`, `UnsuppliedGainsTests.cpp`.

### Top-level-only build features
**Source:** `CMakeLists.txt:183, 290, 327, 356` (`if(PROJECT_IS_TOP_LEVEL ...)`)
**Apply to:** `enable_testing()`, coverage option, `SPATIALCORE_BUILD_TESTS` default.

## No Analog Found

| File | Role | Data Flow | Reason |
|------|------|-----------|--------|
| `examples/harness/scripts/build-consumer.sh` | script | batch | No shell scripts in repo; use RESEARCH Pattern 1 |
| `scripts/coverage-report.py` | utility | transform | No Python tooling; stdlib-only sketch in RESEARCH |
| `.github/workflows/nightly.yml` | config | CI | No scheduled workflow; derive from README steps 1-5 |
| macOS/Windows jobs in `ci.yml` | config | CI | Only a Linux job exists |

## Metadata

**Analog search scope:** `CMakeLists.txt`, `tests/**`, `include/SpatialCore/**`, `src/Engine`, `src/Binaural`, `examples/**`, `.github/workflows`, `README.md`
**Pattern extraction date:** 2026-10-17
