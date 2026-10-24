---
phase: 06-consumer-readiness-ci
plan: 06
subsystem: infra
tags: [github-actions, ubuntu-24.04, windows-2025, msvc, vcpkg, catch2, ci, juce-fft]

requires:
  - phase: 06-consumer-readiness-ci
    provides: "06-05 macos-gate and realtime-nightly jobs, LFS cache and HRTF guard, SHA-pinned actions"
provides:
  - "linux-tests CI job: ubuntu-24.04, GCC 13.3, Release build of every target, ctest from the build root under xvfb, 416 tests, ui: listing check"
  - "windows-build CI job: windows-2025 (Visual Studio 2026, MSVC 19.51), Release build of SpatialCore, SpatialCoreUI, SpatialCoreTests and SpatialCoreUITests, no test run (D-08)"
  - "MSVC compile fixes for the test targets (NOMINMAX in FreeUdpPort.h, three no-capture lambdas)"
  - "Sourced answer to the Phase 5 fallback-FFT question for Plan 06-09"
affects: [06-07 coverage, 06-09 docs (SharedFFTCache.h, CLAUDE.md, README Windows prerequisites), 06-10 branch-protection]

actuals:
  tokens: 4600
  tasks: 2
  commits: 6

plan_head_before: 599e0b0658db5318f997c04403a63114b0059177
plan_head_after: 422c30845e97a910891f91e3e12368f6272b99e2

tech-stack:
  added: [vcpkg zlib 1.3.2 (CI only, x64-windows-static-md triplet), xvfb (CI only)]
  patterns:
    - "Windows job builds each target in its own cmake call so one compile error does not hide the others"
    - "A timing case discards an attempt in which its own calling thread paused, instead of widening a bound"

key-files:
  created: []
  modified:
    - .github/workflows/ci.yml
    - tests/Support/FreeUdpPort.h
    - tests/Binaural/BinauralTestUtilities.h
    - tests/Binaural/PartitionedConvolverTests.cpp
    - tests/Binaural/BinauralRendererTests.cpp
    - tests/OSC/ADMOSCSenderTests.cpp

key-decisions:
  - "Linux passed on its first run: 416 of 416 tests, no Linux platform failure; no golden or tolerance moved"
  - "Windows has no system zlib: the job installs zlib:x64-windows-static-md with the runner's vcpkg and names zs.lib to FindZLIB; all five HRTF profiles still embed with MSVC (SPATIALCORE_EMBED_ALL_HRTF stays ON)"
  - "No library source changed, so no macOS sound or golden can have moved; every fix is in tests or the workflow"
  - "windows-2025 is pinned instead of windows-latest (the README maps all three labels to the same image today)"

patterns-established:
  - "Evidence for a green gate is the push run of the final commit with all three platform jobs, none continue-on-error"

requirements-completed: [CI-01]

coverage:
  - id: D1
    description: "linux-tests runs the whole suite on ubuntu-24.04 in Release (416 tests, 44 skipped because the allocation trap is macOS-only) and is green with the ui: tests in the run"
    requirement: CI-01
    verification:
      - kind: integration
        ref: "https://github.com/AndrewRahman/SpatialCore/actions/runs/39070985243 job linux-tests: '100% tests passed, 0 tests failed out of 416', 'ui: tests listed: 17', 'JUCE checkout: 9.0.0'"
        status: pass
    human_judgment: false
  - id: D2
    description: "windows-build compiles SpatialCore, SpatialCoreUI, SpatialCoreTests and SpatialCoreUITests in Release with MSVC (Windows-only branches included) and runs no test"
    requirement: CI-01
    verification:
      - kind: integration
        ref: "https://github.com/AndrewRahman/SpatialCore/actions/runs/39070985243 job windows-build: four targets linked (SpatialCore.lib, SpatialCoreUI.lib, SpatialCoreTests.exe, SpatialCoreUITests.exe), job success"
        status: pass
    human_judgment: false
  - id: D3
    description: "No job is continue-on-error and macos-gate, linux-tests and windows-build are all green on one pushed commit"
    requirement: CI-01
    verification:
      - kind: integration
        ref: "run 39070985243 (push of 422c308): macos-gate success, linux-tests success, windows-build success; grep -c continue-on-error .github/workflows/ci.yml prints 0"
        status: pass
    human_judgment: false
  - id: D4
    description: "Fallback-FFT answer from JUCE 9.0.0 source with file and line numbers (for Plan 06-09)"
    requirement: CI-01
    verification:
      - kind: other
        ref: "juce_FFT.cpp lines 89-115, 175-193, 439 and the engine ranking at 92, 446, 570-572, 745, 832, checked against the Linux and Windows configure logs (no FFTW, MKL or IPP flag present)"
        status: pass
    human_judgment: false

duration: 1h 44m
completed: 2026-10-17
status: complete
---

# Phase 6 Plan 06: Linux test and Windows build CI jobs Summary

**Every push now runs the whole 416-test suite on Ubuntu 24.04 (green on the first try) and builds all four targets with MSVC on Windows (after a vcpkg zlib install and three MSVC test-source fixes), with no job in continue-on-error and macOS still the full gate.**

## Performance

- **Duration:** about 1h 44m wall clock (20:14Z to 21:58Z, 2026-10-17), mostly waiting on hosted runners
- **Tasks:** 2
- **Files modified:** 6 (`.github/workflows/ci.yml` and five test files; no library source)

## Accomplishments

- `linux-tests`: `ubuntu-24.04`, GCC 13.3.0, CMake 3.31.6, Release build of every target in 4 min 38 s, `ctest --test-dir build-release --no-tests=error` under `xvfb-run -a` in 58 s, then the `ui:` listing check (17 listed).
- `windows-build`: `windows-2025`, MSVC 19.51.36260 under Visual Studio 18 2026, four targets in 13 min 13 s, no test or test executable is run.
- The Phase 5 fallback-FFT question is answered from JUCE's own source (below).

## CI Evidence

| Run | Commit | Result |
|---|---|---|
| [39065979298](https://github.com/AndrewRahman/SpatialCore/actions/runs/39065979298) | c65f79c | cancelled after reading: linux-tests all steps green (416/416), windows-build configure failed (no zlib) |
| [39066785312](https://github.com/AndrewRahman/SpatialCore/actions/runs/39066785312) | 1a380cf | cancelled after reading: windows configure failed (`zs.lib` not found by FindZLIB) |
| [39067025723](https://github.com/AndrewRahman/SpatialCore/actions/runs/39067025723) | 8942ff7 | cancelled after reading: windows configure green, build red (MSVC test-source errors) |
| [39068118485](https://github.com/AndrewRahman/SpatialCore/actions/runs/39068118485) | d90b281 | cancelled after reading: windows build green (13 min), linux green |
| [39069330986](https://github.com/AndrewRahman/SpatialCore/actions/runs/39069330986) | 5e17ff1 | linux and windows green, macos-gate red (wall-clock sender case, see deviation 4) |
| [39070985243](https://github.com/AndrewRahman/SpatialCore/actions/runs/39070985243) | 422c308 | **success: macos-gate, linux-tests, windows-build all green, no continue-on-error** |

Final run 39070985243: `linux-tests` 7 min (21:27:02Z to 21:34:07Z), `windows-build` 16 min (21:27:02Z to 21:43:02Z), `macos-gate` 29 min (21:27:08Z to 21:55:57Z; Debug both executables, Release `100% tests passed out of 416`, `ui: tests listed: 17`, `HarnessCheck: PASS (23 formats, 12 algorithms, profile 5 embedded)`). Saved logs: `/tmp/sc-p6-06-linux.log` (and `/tmp/sc-p6-06-final-*.log`).

### Linux counts

`100% tests passed, 0 tests failed out of 416`, total test time 58.24 s. 44 of the 416 report Skipped: 41 `Realtime:` cases, `INTG-02: a misuse block allocates and locks nothing on the render thread`, and the two `[rt][scratch]` BinauralRenderer cases that call `skipUnlessRealtimeTrap()`. All 44 skip because the allocation and lock trap is built on macOS only (`tests/CMakeLists.txt:73`); each skips, none passes with zero counts. Pinned images: `ubuntu-24.04`, `windows-2025` (the runner-images README maps `windows-latest`, `windows-2025` and `windows-2025-vs2026` to the same image today; the label is pinned so a later `-latest` move cannot change it).

## Triage table (every Linux and Windows failure seen)

| # | Platform | Failure | Root cause | Fix | macOS result after fix |
|---|---|---|---|---|---|
| 0 | Linux | none: all 416 tests passed on the first run (run 39065979298) | n/a | none (xvfb was added up front for the UI tests, not proven necessary: the first run already used it, so whether a display is required is unmeasured) | n/a |
| 1 | Windows | Configure: `Could NOT find ZLIB (missing: ZLIB_LIBRARY ZLIB_INCLUDE_DIR)`; libmysofa's MSVC branch ran `nuget install zlib` and got `Unable to find package 'zlib'` | Windows has no system zlib (it is a system library on Linux and macOS, README and CLAUDE.md say "zlib (system)"), and libmysofa v1.3.5's MSVC branch expects a nuget package that does not exist | `vcpkg install zlib:x64-windows-static-md` with the runner's vcpkg (zlib 1.3.2, static library, dynamic C runtime as JUCE uses), `CFLAGS`/`CXXFLAGS=-I<vcpkg include>` because libmysofa's MSVC branch adds no zlib include path | n/a (workflow only) |
| 2 | Windows | Configure: `Could NOT find ZLIB (missing: ZLIB_LIBRARY)` after the include folder was found | zlib 1.3.2 names its static Windows library `zs.lib`, a name CMake 4.4's FindZLIB does not search | `-DZLIB_INCLUDE_DIR` and `-DZLIB_LIBRARY=<root>/lib/zs.lib` | n/a (workflow only) |
| 3 | Windows | `tests/Engine/ControlRouteTests.cpp`: C4002 `too many arguments for function-like macro invocation 'min'`/`'max'` at `juce_FloatVectorOperations.h:129-138`, then C1201 | `tests/Support/FreeUdpPort.h` includes `winsock2.h` (which includes `windows.h`) before any JUCE header and defined no `NOMINMAX`, so the `min`/`max` macros hit JUCE's member functions of those names | `#ifndef NOMINMAX / #define NOMINMAX` before the include, with the reason beside it (`d90b281`) | Release ctest 416 of 416 |
| 4 | Windows | C3493 `'kRelTol' cannot be implicitly captured` (`BinauralTestUtilities.h:134`), `'kNumSamples'` (`PartitionedConvolverTests.cpp:296`), `'kBlock'` (`BinauralRendererTests.cpp:640`), plus the C2064/C3536/C2100 errors that cascade from them | MSVC 19.51 rejects a use of a local `constexpr` variable inside a lambda with no capture list that GCC, Clang and Apple Clang accept. None of the three is odr-used (all are passed or multiplied by value), so this is MSVC being stricter than the other three compilers here | the three locals became `static constexpr` (same value, no capture needed), reason beside each (`d90b281`) | Release ctest 416 of 416 |
| 5 | macOS (new) | macos-gate Debug SpatialCoreTests: `tests/OSC/ADMOSCSenderTests.cpp:299 CHECK( total >= 25 )`, 24 messages over 0.948 s (run 39069330986) | see deviation 4 | the case measures the longest gap between its own calls and repeats an attempt in which the calling thread paused (`422c308`) | Debug `[osc][send]` 19 cases, Release 416 of 416, and run 39070985243 green |

Windows libs and flags: SPATIALCORE_EMBED_ALL_HRTF stayed ON: MSVC compiled the generated BinaryData for all five SOFA profiles (about 58 MB) without error, so the Windows job needs no `-DSPATIALCORE_EMBED_ALL_HRTF=OFF`. Windows warnings (C4820, C4711, C4242, C4244 and others) are not errors and were not touched.

**Windows finding for OpenSpatialDelay and Plan 06-09:** a Windows consumer must supply zlib itself. SpatialCore links `ZLIB::ZLIB` (`CMakeLists.txt:60`, `:198`) and libmysofa v1.3.5's MSVC branch only runs `nuget install zlib`, which finds no package today. CI's recipe (vcpkg `zlib:x64-windows-static-md`, `ZLIB_LIBRARY` pointing at `zs.lib`, the include folder on the C compiler's command line) is the working one. 06-09 should say so beside the "zlib (system)" lines in README.md:100 and CLAUDE.md:139, which are true only on Linux and macOS.

## Fallback-FFT finding (Phase 5 D-13, for Plan 06-09)

Source: JUCE 9.0.0, `modules/juce_dsp/frequency/juce_FFT.cpp` (the Debug tree's `build/_deps/juce-src`).

- `FFTFallback` (line 89) declares `static constexpr int priority = -1; // this should have the least priority of all engines` (line 92). Its `perform()` (line 107) takes `const SpinLock::ScopedLockType sl (processLock);` at **line 115** (member `SpinLock processLock;` at line 434) before every transform, skipped only for `size == 1`.
- The real-only transforms that `PartitionedConvolver` calls (`src/Binaural/PartitionedConvolver.cpp:75, 145, 156`) reach the same lock: `performRealOnlyForwardTransform (Complex<float>*, float*)` (line 175) calls `perform (...)` at line 181, and `performRealOnlyInverseTransform` (line 183) calls it at line 193. The scratch buffer is `alloca` below 256 KiB (`maxFFTScratchSpaceToAlloca`, line 130) and a `HeapBlock` allocation at or above it (an FFT of order 15 or more; the convolver's orders are well below that).
- Engine selection: `FFT::Engine::createBestEngineForPlatform` (line 58) walks the engines in priority order (the list is sorted at line 54) and returns the first that creates an instance. Above the fallback: Apple vDSP, priority 5 (line 446, only `(JUCE_MAC || JUCE_IOS) && JUCE_USE_VDSP_FRAMEWORK`, line 443); FFTW, priority 10 shared or 3 static (lines 570-572, only with `JUCE_DSP_USE_SHARED_FFTW` or `JUCE_DSP_USE_STATIC_FFTW`, line 550); Intel MKL, priority 8 (line 745, only with `JUCE_DSP_USE_INTEL_MKL`, line 742); Intel IPP, priority 9 (line 832, only when Visual Studio defines one of `_IPP_SEQUENTIAL_STATIC`, `_IPP_SEQUENTIAL_DYNAMIC`, `_IPP_PARALLEL_STATIC` or `_IPP_PARALLEL_DYNAMIC`). `juce_dsp.h:157, 173, 188` default the MKL, shared-FFTW and static-FFTW switches to 0.
- This repository sets none of them (`git grep` for `JUCE_DSP_USE` and `_IPP_` in `CMakeLists.txt`, `tests/CMakeLists.txt` and `examples` finds nothing), and neither the Linux nor the Windows configure and build log mentions FFTW, MKL or IPP.

Conclusion: on Linux and Windows as built here, JUCE uses the fallback FFT. Its `perform()` takes a `SpinLock`, so concurrent instances that share one `juce::dsp::FFT` per order through `SharedFFTCache` contend on that one lock on the audio thread; on macOS the vDSP engine takes no lock and the Phase 5 proof stands. Per CLAUDE.md "Origin", `SharedFFTCache` was extracted from OpenSpatialDelay v1.0, so the migration adds no new exposure on Windows (this plan did not read OpenSpatialDelay's tree to confirm it; 06-09 should state it as the plan's premise or check it before it writes the sentence). This plan changed no source for the finding: SharedFFTCache.h lines 35-38 are still the "checked in Phase 6" text and 06-09 owns the rewrite. Not measured: the contention itself (no Windows or Linux timing was run, D-08).

## Task Commits

1. **Task 1 (tracer): Linux runs the whole suite** - `c65f79c` (ci), `1a380cf` (ci, continue-on-error removed for linux-tests)
2. **Task 2: Windows builds every target** - `8942ff7` (ci, vcpkg zs.lib), `d90b281` (fix, MSVC test-source fixes and per-target build calls), `5e17ff1` (ci, continue-on-error removed)
3. **Out-of-plan fix found by the final macOS run** - `422c308` (test, wall-clock sender case)

**Plan metadata:** the docs commit that adds this SUMMARY.

## Files Created/Modified

- `.github/workflows/ci.yml` - `linux-tests` and `windows-build` jobs, header comment updated; no `continue-on-error` anywhere.
- `tests/Support/FreeUdpPort.h` - `NOMINMAX` before `winsock2.h` (MSVC).
- `tests/Binaural/BinauralTestUtilities.h`, `tests/Binaural/PartitionedConvolverTests.cpp`, `tests/Binaural/BinauralRendererTests.cpp` - one `constexpr` local each became `static constexpr` (MSVC C3493).
- `tests/OSC/ADMOSCSenderTests.cpp` - the wall-clock rate case discards an attempt in which its own caller paused.

## Decisions Made

See `key-decisions`. The zlib source for Windows is vcpkg in CI only: SpatialCore's own CMake is unchanged, so a consumer keeps its own way of providing zlib (the Windows finding above).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Windows has no zlib for libmysofa and SpatialCore**
- **Found during:** Task 2 (runs 39065979298 and 39066785312)
- **Issue:** see triage rows 1 and 2.
- **Fix:** vcpkg install of zlib in the Windows job, include folder through `CFLAGS`/`CXXFLAGS`, `zs.lib` named explicitly.
- **Files modified:** `.github/workflows/ci.yml`
- **Committed in:** `1a380cf`, `8942ff7`

**2. [Rule 1 - Bug] `FreeUdpPort.h` breaks JUCE headers on Windows**
- **Found during:** Task 2 (run 39067025723)
- **Issue:** triage row 3. **Fix:** `NOMINMAX` before `winsock2.h`. **Files modified:** `tests/Support/FreeUdpPort.h`. **Committed in:** `d90b281`

**3. [Rule 1 - Bug] Three no-capture lambdas MSVC 19.51 rejects**
- **Found during:** Task 2 (run 39067025723)
- **Issue:** triage row 4. **Fix:** `static constexpr`, same values. **Files modified:** `BinauralTestUtilities.h`, `PartitionedConvolverTests.cpp`, `BinauralRendererTests.cpp`. **Committed in:** `d90b281`

**4. [Rule 1 - Bug] Wall-clock sender rate case is not robust to a pause of its own calling thread**
- **Found during:** final verification (run 39069330986, macos-gate, Debug SpatialCoreTests)
- **Issue:** `ADMOSCSenderTests.cpp:299 CHECK( total >= 25 )` failed with `24 messages from 60 wall-clock calls over 0.947842 s`. The sender, by design (D-08c), never catches up after a stall, so a runner pause of the thread that calls `tick()` costs the missed slots permanently. The case's deadline pacing (added in Plan 06-05 because a macos-26 16 ms sleep takes about 75 ms) then fires the late calls back to back and so hides the pause from `elapsed`, and the absolute bound of 25 (tighter than the rate window, which allowed 23.4) failed. Run 39070985243 and Plan 06-05's three green runs passed it, so it is an intermittent runner pause, not a sender defect. This is my diagnosis from the numbers and the sender's design, not a measured stall: the run did not record a gap.
- **Fix:** the case records the longest gap between two `tick()` calls; an attempt with a gap of 40 ms or more says nothing about the sender and is repeated, up to five times, and five paused attempts in a row fail the case (`REQUIRE (undisturbed)`). The first undisturbed attempt is judged by the original bounds, none widened. Proven by injecting a 200 ms pause locally (first attempt discarded, second passed), then reverted.
- **Files modified:** `tests/OSC/ADMOSCSenderTests.cpp`
- **Verification:** the case 3 of 3 passed locally; `[osc][send]` 19 cases pass in Debug; Release ctest 416 of 416; run 39070985243 macos-gate green including Debug SpatialCoreTests.
- **Committed in:** `422c308`

**5. [Plan text vs tool output] ctest wording and push-run evidence**
- **Issue:** Task 1's acceptance string `100% tests passed, 0 tests failed out of` exists on Linux (CMake 3.31.6) and was matched literally; macOS (CMake 4.4.3) prints `100% tests passed out of 416`, so the 06-05 pattern `100% tests passed(, 0 tests failed)? out of [1-9]` was used for the macOS line. The plan said the Windows job's build step names the four targets in one cmake call; it names all four in a loop of four calls (see Decisions), so a failure in one cannot hide the others.
- **Files modified:** none beyond the workflow.

---

**Total deviations:** 5 (1 blocking fix, 3 bug fixes, 1 plan-text note). **Impact on plan:** all fixes were needed for a green run and are in tests or the workflow; no library source, golden, tolerance or macOS sound changed (`git diff 599e0b0 --stat -- include src` is empty).

## Issues Encountered

- Four superseded runs were cancelled by hand before each push, as the GitHub rules for this repository ask; each had already shown what it was started to show. The first push also carried the two unpushed 06-05 docs commits.
- Linux needed no iteration. xvfb was installed and used from the first run, so whether the UI tests really need a display on Linux is not established; removing it is a possible later experiment, not a finding.

## Known Stubs

None.

## Threat Flags

| Flag | File | Description |
|------|------|-------------|
| threat_flag: package-install | .github/workflows/ci.yml | The windows-build job runs `vcpkg install zlib:x64-windows-static-md`, which downloads zlib 1.3.2 source from github.com/madler/zlib through the runner's preinstalled vcpkg (CI only, no secret in scope, read-only token). The plan's T-06-SC covered apt only. The package is the well-known zlib already named as a dependency in README.md and CLAUDE.md, not a new name. |

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Plan 06-07 (coverage) adds its job to the same file; keep the acceptance greps true (`uses:` 9 and 9 pinned, every `${{ }}` in an env, key, group or path line, `continue-on-error` 0).
- Plan 06-09: write the fallback-FFT finding into `SharedFFTCache.h` lines 35-38 and CLAUDE.md, and the Windows zlib prerequisite into README.md:100 and CLAUDE.md:139. Windows runs no test (D-08); a full Windows test run, and the unverified `%ProgramData%` shared-folder path at run time, remain deferred. `examples/demo/DemoComponent.cpp` has a second copy of the Windows UDP-port code that no CI job compiles (the demo target is off by default); it was left untouched.
- Plan 06-10 puts this workflow on main, which starts the nightly schedule.

## Self-Check: PASSED

- `.github/workflows/ci.yml` and the five test files exist and are committed; `git diff 599e0b0 --stat -- include src` is empty.
- Commits `c65f79c`, `1a380cf`, `8942ff7`, `d90b281`, `5e17ff1`, `422c308` are ancestors of HEAD; `git rev-list --count 599e0b0..422c308` prints 6.
- Acceptance greps: `continue-on-error` 0, `uses:` 9 and 9 pinned, expression-in-run lines 0, `ubuntu-24.04` present, `ctest --test-dir build-release --no-tests=error` 2, windows-build has no ctest or test-executable run step and its loop names SpatialCore, SpatialCoreUI, SpatialCoreTests, SpatialCoreUITests.
- Plan verification: the push run of 422c308 (39070985243) has macos-gate, linux-tests and windows-build all `success`; the Linux log contains `100% tests passed, 0 tests failed out of 416`.

---
*Phase: 06-consumer-readiness-ci*
*Completed: 2026-10-17*
