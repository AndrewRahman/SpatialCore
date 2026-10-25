---
phase: 03-binaural-defects-hrtf-packaging
plan: 03
subsystem: binaural-dsp
tags: [partitioned-convolver, crossfade, hrtf, kemar, block-size, realtime-safety, catch2, bug-02]

requires:
  - phase: 03-binaural-defects-hrtf-packaging
    provides: "Plan 03-01 test foundation (BinauralMetrics.h, dedicated convolver and renderer test files) and Plan 03-02 green Debug 221/221, Release 221/221"
provides:
  - "PartitionedConvolver warm-up and crossfade timed in samples (kMinCrossfadeSamples = 2048, stateSampleCount, crossfadeLengthSamples)"
  - "BinauralRenderer::ensureScratchCapacity(): scratch sized for the loaded IR in prepare() and setProfile(), so KEMAR no longer allocates on the audio thread"
  - "[bug02][moving], [bug02][steady], [convolver][transition], [renderer][scratch] test cases"
affects: [03-09, 03-11]

actuals:
  tokens: 6550
  tasks: 3
  commits: 3

plan_head_before: a02625cbe58b460befe069b18a53487f9911a9fa
plan_head_after: 174a2111ddf0b6f519885ef64daf9c10edd546a6
commits: 3

tech-stack:
  added: []
  patterns:
    - "Transition timing in samples with a floor, length fixed once when the fade starts and advanced by elapsed samples (never recomputed per call)"
    - "Scratch buffers sized off the audio thread by a grow-only helper called from prepare() and setProfile()"
    - "New timing proven against the pre-change code: the 512-sample section passes on both, the small-block and long-IR sections fail on the old code"

key-files:
  created: []
  modified:
    - include/SpatialCore/Binaural/PartitionedConvolver.h
    - src/Binaural/PartitionedConvolver.cpp
    - include/SpatialCore/Binaural/BinauralRenderer.h
    - src/Binaural/BinauralRenderer.cpp
    - tests/Binaural/PartitionedConvolverTests.cpp
    - tests/Binaural/BinauralRendererTests.cpp

key-decisions:
  - "Fade length is max(kCrossfadeBlocks x the size of the call that ended warm-up, 2048) samples, fixed once; progress is elapsed samples over that length, so it cannot run backwards when call sizes vary"
  - "Warm-up ends at max(kWarmupBlocks calls, irLen samples). At 512-sample calls and an IR of 512 or fewer samples this is the original single call, so existing behaviour is unchanged; KEMAR (558) now warms up for two calls"
  - "Scratch buffers are grow-only (max of block size, 512 and IR length), so a later prepare() at a smaller block cannot shrink below a loaded IR"
  - "The audio-thread resize guard and its jassertfalse in updateSourceHRIR and renderSourceBuffers are left in place (RTSF-01, Phase 5); only a comment was added saying it can no longer fire for a profile loaded through setProfile()"

patterns-established:
  - "Pin unchanged behaviour by running the new test against the BASE implementation (git checkout BASE -- <files>, build, run, git checkout HEAD -- <files>) and recording which sections pass and which fail"

requirements-completed: [BUG-02, EXTR-02]

coverage:
  - id: D1
    description: "A source moving 180 degrees through the engine's KEMAR HRTF path has the same largest sample-to-sample step at 32, 64 and 128-sample blocks as at 512 (within 1.2x; measured 1.00x)"
    requirement: "BUG-02"
    verification:
      - kind: unit
        ref: "tests/Binaural/PartitionedConvolverTests.cpp#BUG-02: a moving source is as smooth at 32, 64 and 128-sample blocks as at 512 ([bug02][moving], Debug and Release)"
        status: pass
    human_judgment: false
  - id: D2
    description: "Convolver warm-up lasts at least irLen samples and the crossfade lasts max(4 x call size, 2048) samples; at 512-sample calls with IR 256 the transition is the original one warm-up call and four cos/sin quarter-step fade calls"
    requirement: "BUG-02"
    verification:
      - kind: unit
        ref: "tests/Binaural/PartitionedConvolverTests.cpp#PartitionedConvolver: transition timing is sample-based and unchanged at 512 ([convolver][transition], 3 sections)"
        status: pass
    human_judgment: false
  - id: D3
    description: "Engine steady-state output at block plans 32, 64, 128, 37 and two irregular plans matches the 512-sample reference within 1e-4 on the Simple path (0) and the KEMAR HRTF path (worst 1.9e-5), in Debug and Release"
    requirement: "BUG-02"
    verification:
      - kind: unit
        ref: "tests/Binaural/PartitionedConvolverTests.cpp#BUG-02: engine output at small and irregular block plans matches the 512-sample reference ([bug02][steady])"
        status: pass
    human_judgment: false
  - id: D4
    description: "BinauralRenderer sizes its scratch buffers for the loaded IR in prepare() and setProfile(); KEMAR (IR 558) at maximum blocks 64, 256, 512 and after a re-prepare at 64 renders finite non-silent output and prints no BinauralRenderer.cpp assertion in Debug"
    requirement: "EXTR-02"
    verification:
      - kind: unit
        ref: "tests/Binaural/BinauralRendererTests.cpp#BinauralRenderer: KEMAR's 558-sample IR is prepared off the audio thread at every block size ([renderer][scratch]); Debug log of [renderer] and [bug02] and of the full suite: 0 assertion lines"
        status: pass
    human_judgment: false
  - id: D5
    description: "Oracle and pending-IR deferral still pass; full suites green"
    verification:
      - kind: integration
        ref: "./build/tests/SpatialCoreTests: 225/225 Debug; ctest --test-dir build-release/tests: 225/225 Release"
        status: pass
    human_judgment: false

duration: 8min
completed: 2026-10-04
status: complete
---

# Phase 3 Plan 03: Sample-timed convolver and KEMAR scratch sizing Summary

**PartitionedConvolver warm-up and crossfade now run in samples (warm-up at least irLen, fade max(4 x call, 2048)), so a moving source is as smooth at 32/64/128-sample blocks as at 512 (step ratio 4.6x/1.25x down to 1.00x), and BinauralRenderer sizes its scratch buffers for KEMAR's 558-sample IR off the audio thread**

## Performance

- **Duration:** 8 min
- **Started:** 2026-10-04T13:17:13Z
- **Completed:** 2026-10-04T13:25:00Z
- **Tasks:** 3
- **Files modified:** 6 (4 source and header, 2 test)
- **Diff base (BASE):** `a02625cbe58b460befe069b18a53487f9911a9fa`

## Accomplishments

- BUG-02 is finished on SpatialCore's side. The per-call overlap-add was already correct; the remaining small-block artifact was the call-counted warm-up and crossfade, so a new HRIR faded in after 160 samples at 32-sample blocks while the IRs run 128-558 samples. Both timings are now in samples with a floor.
- The tracer test went red then green through the engine: moving-source largest step against the 512-sample step was 1.05x at 128, 1.25x at 64 and 3.49x at 32 before the fix, and 0.9993x, 1.0000x and 0.9996x after.
- Behaviour at 512-sample calls with an IR that fits a call is unchanged. This was proven, not argued: the new `[convolver][transition]` section for 512/IR 256 passes on both the original and the new convolver, while the 32-sample and the 558-sample sections fail on the original.
- The KEMAR audio-thread allocation is gone for profiles loaded through `setProfile()`: Debug log assertion lines from `BinauralRenderer.cpp` went from 4 (before the fix, same test) to 0 in `[renderer]`, `[bug02]` and the whole Debug suite.

## Task Commits

1. **Task 1: Tracer, sample-based warm-up and crossfade** - `cfeece7` (fix)
2. **Task 2: Pin transition timing and engine block-size independence** - `f42d991` (test)
3. **Task 3: Scratch buffers sized for the loaded IR** - `174a211` (fix)

**Plan metadata:** the docs commit that follows this summary.

## Task 1 tracer evidence (red then green)

Test: KEMAR in the active renderer, `BinauralPath::HRTF`, 0.5-amplitude 440 Hz sine, 2.25 s, azimuth -90 to +90 degrees over 2 s, largest step over both ears from 0.25 s.

| Block size | Before (red) step, ratio to 512 | After (green) step, ratio to 512 |
|---|---|---|
| 512 | 0.022375 | 0.02235 |
| 128 | 0.0234465 (x1.048) | 0.0223352 (x0.99934) |
| 64 | 0.027906 (x1.247) | 0.0223499 (x0.999997) |
| 32 | 0.0780641 (x3.489) | 0.0223412 (x0.999605) |

The red run failed `CHECK (step64 <= 1.2 * step512)` and `CHECK (step32 <= 1.2 * step512)`. The ratios differ from the plan's research figures (4.6x / 1.29x / 1.12x) because this test's absolute step at 512 is 0.0224 rather than 0.0149 (different sweep and level); the direction and the fix agree, and the green run reaches 1.00x at every size, as the prototype did. Release gives the same figures (x0.999342 / x1 / x0.99961).

## Task 2 measurements

**`[convolver][transition]` against the pre-change convolver (BASE files checked out, built, run, then restored with `git checkout HEAD --`):** the 512-sample / IR 256 section passed (the fade is the original one warm-up call plus four cos/sin quarter-step calls); the 32-sample section failed (first 256 samples already differed from conv(A) by 0.4997); the 558-sample section failed (two warm-up calls differed from conv(A) by 0.198). On the new code all three sections pass.

**`[bug02][steady]` worst |diff| against the 512-sample reference after sample 16384 (bound 1e-4), identical in Debug and Release:**

| Plan | Simple path | KEMAR HRTF path |
|---|---|---|
| {32} | 0 | 1.50e-5 |
| {64} | 0 | 1.41e-5 |
| {128} | 0 | 1.64e-5 |
| {37} | 0 | 1.90e-5 |
| {1,7,300,512,3,64} | 0 | 1.76e-5 |
| {32,512,32,64,512} | 0 | 1.49e-5 |

Non-finite count 0 everywhere. The research measured 3.3e-5 for KEMAR; this tree reads lower.

## Verification Results

| Gate | Result |
|---|---|
| `[bug02][moving]` and `[convolver]` (Task 1 verify) | pass; `[convolver]` lists 3 test cases |
| `[bug02]` | lists 2 test cases; pass in Debug and Release |
| `[renderer]` and `[bug02]` Debug log, `grep -c 'Assertion failure in BinauralRenderer.cpp'` | 0 (and 0 "No test cases matched") |
| `[renderer][scratch]` before the fix | 4 assertion lines, test passed (the allocation is only visible in the log) |
| Full Debug suite | 225 test cases, all passed, 308,026 assertions; 0 `BinauralRenderer.cpp` assertion lines |
| Full Release `ctest --test-dir build-release/tests` | 100% passed, 225/225 |
| Baseline before this plan | Debug 221/221, Release 221/221; this plan adds 4 test cases |
| `processSlot` body | unchanged: the only hunk inside it (`@@ -126,0 +128,8`) is 8 added comment lines; code hunks lie in `setIR`, `process` and `reset` |
| Engine and ITD | `git diff a02625c..HEAD --stat -- src/Engine` is empty; `kITDBufferSize = 64` untouched |

Expected Debug noise seen and not counted: libmysofa parse traces and the `juce_LeakedObjectDetector.h` assertion at exit.

## Files Created/Modified

- `include/SpatialCore/Binaural/PartitionedConvolver.h` - `kMinCrossfadeSamples = 2048`, `stateSampleCount`, `crossfadeLengthSamples`; comments describe the sample-based timing and name both halves of the OSD#234 fix
- `src/Binaural/PartitionedConvolver.cpp` - sample-based Warmup and Crossfading in `process`, counters in `setIR` and `reset`; `processSlot` code untouched, comment extended
- `include/SpatialCore/Binaural/BinauralRenderer.h` - `ensureScratchCapacity()` declaration; `prepare`/`setProfile` docs say they size scratch and must not run on the audio thread
- `src/Binaural/BinauralRenderer.cpp` - `ensureScratchCapacity()` (grow-only, max of block size, 512, IR length), called from `prepare()` and `setProfile()`; comment at the `updateSourceHRIR` guard
- `tests/Binaural/PartitionedConvolverTests.cpp` - `[bug02][moving]`, `[convolver][transition]` (3 sections), `[bug02][steady]`
- `tests/Binaural/BinauralRendererTests.cpp` - `[renderer][scratch]` (2 sections)

## Decisions Made

- Fade length from the call that ends warm-up, fixed once, advanced by elapsed samples (the research's two corrections to its own prototype).
- The fade expectation in `[convolver][transition]` repeats the convolver's float gain accumulation instead of an exact double ramp, because the existing per-sample accumulation drifts about 4e-6 from an exact ramp over 512 samples. The test pins timing and gain law; float rounding of the unchanged ramp loop is not its subject.
- The RTSF-01 guard pattern stays; Phase 5 owns it.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fade-gain expectation in the new test used an exact ramp**
- **Found during:** Task 2
- **Issue:** The first version of the `[convolver][transition]` 512/IR 256 section compared the fade against an exact double-precision linear ramp and read 3.97e-6 against the plan's 1e-6. The convolver's unchanged ramp loop accumulates the gain in float one increment per sample, which drifts by that much; this was a wrong expectation in my test, not a convolver defect.
- **Fix:** The test now computes the expected gains with the same float accumulation (the plan's "same formula"). It then reads within 1e-6, and the section also passes on the pre-change convolver, which confirms the behaviour at 512 is unchanged.
- **Files modified:** `tests/Binaural/PartitionedConvolverTests.cpp`
- **Verification:** `[convolver]` 27 assertions pass in Debug; the same section passes against the BASE convolver
- **Committed in:** `f42d991`

---

**Total deviations:** 1 auto-fixed (1 test expectation)
**Impact on plan:** None on scope or on the code under test. The bound stayed at 1e-6.

## Issues Encountered

- The research's absolute step values (0.0149 at 512) do not reproduce because the plan's sweep is described loosely; the measured ratios are the figures that matter and meet the plan's bound with wide margin.
- The Task 3 allocation is not observable from a Catch2 assertion (the scratch vectors are private), so `[renderer][scratch]` passes both before and after the fix; the red-to-green evidence is the Debug log count (4 to 0), as the plan specifies.

## Known Stubs

None.

## Threat Flags

None. No new network endpoint, auth path, file access or schema surface. T-03-04 (audio-thread allocation) and T-03-05 (transition timing) are mitigated as planned.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Ready for Plan 03-04. Plan 03-09 applies the same sample-floor rule to the engine's renderer crossfade (`kRendererXfadeBlocks = 8`), which this plan deliberately did not touch.
- Behaviour change to carry into the SUMMARY of later plans and the item-D listening notes: a KEMAR HRIR change at 512-sample blocks now warms up for two calls instead of one, and every convolver transition below 512-sample calls lasts at least irLen + 2048 samples.
- `Spatial-Media-Lab/OpenSpatialDelay#234` stays open for OSD's delay line and pitch shifter (D-13); the comments in the convolver say so.

## Self-Check: PASSED

- Files checked: `include/SpatialCore/Binaural/PartitionedConvolver.h`, `src/Binaural/PartitionedConvolver.cpp`, `include/SpatialCore/Binaural/BinauralRenderer.h`, `src/Binaural/BinauralRenderer.cpp`, `tests/Binaural/PartitionedConvolverTests.cpp`, `tests/Binaural/BinauralRendererTests.cpp` exist and carry the changes.
- Commits `cfeece7`, `f42d991`, `174a211` are in `git log`; `git rev-list --count a02625c..174a211` is 3.
- Acceptance greps: `kMinCrossfadeSamples` in the header (3), `stateSampleCount` in the cpp (9), `ensureScratchCapacity` in `BinauralRenderer.cpp` (5), `OpenSpatialDelay#234` in `PartitionedConvolver.cpp` (3), `[bug02][moving]` in the test file.

---
*Phase: 03-binaural-defects-hrtf-packaging*
*Completed: 2026-10-04*
