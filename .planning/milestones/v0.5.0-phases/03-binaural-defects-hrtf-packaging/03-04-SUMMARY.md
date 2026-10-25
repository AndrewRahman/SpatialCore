---
phase: 03-binaural-defects-hrtf-packaging
plan: 04
subsystem: binaural-dsp
tags: [simple-binaural, woodworth, spectral-cues, biquad, pinna, head-shadow, catch2, bug-01]

requires:
  - phase: 03-binaural-defects-hrtf-packaging
    provides: "Plan 03-01 test foundation (BinauralMetrics.h, engineImpulseResponse, bandDifference, the [bug01][hrtf] case) and Plan 03-03 green Debug 225/225, Release 225/225"
provides:
  - "include/SpatialCore/Core/SimpleBinauralCues.h: SimpleCueWeights, computeSimpleCueWeights, SimpleCueStage, kSimpleCueRearStages / kSimpleCueUpStages / kSimpleCueDownStages, kSimpleCueWeightEpsilon"
  - "RenderEngine Simple-path cue bank: rear, up and down filter branches blended by position before the Woodworth pan gains, with cold-start state reset"
  - "[bug01][simple], [bug01][identity], [bug01][weights], [bug01][cold-start] test cases"
affects: [03-06, 03-10]

actuals:
  tokens: 6900
  tasks: 3
  commits: 3

plan_head_before: 0a749a681931f901df0a959221cfad38eb5fc683
plan_head_after: 12765913493e45154ed49c3186c71c6cd01a12f5
commits: 3

tech-stack:
  added: []
  patterns:
    - "Stateless algorithm plus engine-owned filter state: position-to-weights is a pure header function, the recursive filter state lives in RenderEngine (DR-7)"
    - "Fixed-coefficient biquad cascades designed in prepare() with juce IIR make* helpers, run as hand-written transposed direct form II (no juce::dsp::IIR::Filter on the audio thread)"
    - "Blend-weight snap (1e-6) so a float cosine residue cannot break bit-exact identity in the region that must not change"
    - "Cold-start on path resume: state zeroed and weights set to target when the previous block ran another path"

key-files:
  created:
    - include/SpatialCore/Core/SimpleBinauralCues.h
  modified:
    - include/SpatialCore/Engine/RenderEngine.h
    - src/Engine/RenderEngine.cpp
    - tests/Binaural/BinauralCueTests.cpp
    - tests/Algorithms/PanningLawTests.cpp

key-decisions:
  - "Down-branch dip tuned from -6 dB to -5.5 dB at 5.5 kHz: the research table value measured -5.78 dB at 5.5 kHz (the 1.2 kHz lift adds +0.22 dB there), outside the plan's -5.2 +- 0.5 dB design check; -5.5 dB gives -5.28 dB. Within the +-3 dB tuning allowance; no test bound was lowered"
  - "A non-finite mono input sample reaches the output exactly as before but is fed to the cue filters as 0, so one bad sample cannot poison the recursive filter state permanently"
  - "Cue filters run for every slot with a non-null mono buffer, before the tapFade early-out, so state stays current while a tap is faded; slots with a null buffer keep their state until the Simple path next cold-starts"
  - "No switch back to the flat Simple sound (D-02); OSD release note is recorded by Plan 03-10"

patterns-established:
  - "Direction-pair measurement through the engine impulse response with every value printed via WARN"
  - "Identity test with consumer-supplied constant gains (engineComputesGains false) so blocks 2 and 3 have no ramp and compare with =="

requirements-completed: [BUG-01]

coverage:
  - id: D1
    description: "Simple path: az 180 el 0 is measurably different from az 0 el 0 (>= 2.0 dB RMS and >= 4.0 dB max third-octave difference)"
    requirement: BUG-01
    verification:
      - kind: unit
        ref: "tests/Binaural/BinauralCueTests.cpp#BUG-01: the Simple path separates front from back and overhead (front vs back)"
        status: pass
    human_judgment: false
  - id: D2
    description: "Simple path: az 0 el +90 is measurably different from az 0 el 0; up45, down45 and down90 also separate from front"
    requirement: BUG-01
    verification:
      - kind: unit
        ref: "tests/Binaural/BinauralCueTests.cpp#BUG-01: the Simple path separates front from back and overhead (up90, up45, down45, down90)"
        status: pass
    human_judgment: false
  - id: D3
    description: "Every Simple-path source at ear level in the front half (|az| <= 90 incl. exactly +-90) renders bit-identical to the pre-change formula"
    requirement: BUG-01
    verification:
      - kind: unit
        ref: "tests/Binaural/BinauralCueTests.cpp#BUG-01: ear-level front sources on the Simple path are bit-identical to the old formula"
        status: pass
    human_judgment: false
  - id: D4
    description: "Weight table, branch designs at 48 kHz, finite output on a 15 degree grid and for non-finite positions and input"
    requirement: BUG-01
    verification:
      - kind: unit
        ref: "tests/Binaural/BinauralCueTests.cpp#BUG-01: cue weights, branch designs and non-finite robustness"
        status: pass
    human_judgment: false
  - id: D5
    description: "Cue filter state is zeroed when the Simple path resumes after another path (no stale tail)"
    requirement: BUG-01
    verification:
      - kind: unit
        ref: "tests/Binaural/BinauralCueTests.cpp#BUG-01: the Simple cue state is zeroed when the Simple path resumes"
        status: pass
    human_judgment: false
  - id: D6
    description: "SpatializationAlgorithm, BinauralGains and DirectBinauralAlgorithm unchanged; the algorithm stays left/right-only and a test pins that"
    requirement: BUG-01
    verification:
      - kind: unit
        ref: "tests/Algorithms/PanningLawTests.cpp#DirectBinaural: property checks at the horizon (elevation and front/back are engine-side)"
        status: pass
    human_judgment: false
  - id: D7
    description: "The Simple-mode sound for sources behind, above or below the listener is audibly more natural on headphones"
    requirement: BUG-01
    verification: []
    human_judgment: true
    rationale: "Perceptual quality of the cue shape needs headphone listening; recorded as ROADMAP backlog 999.4 and deliberately not a gate (D-03)"

duration: 11min
completed: 2026-10-04
status: complete
---

# Phase 3 Plan 04: Simple-path spectral cues Summary

**Simple (Woodworth) binaural path now separates front, back, overhead and underfoot with a position-blended bank of three fixed biquad branches (rear head-shadow cut, up pinna peak, down dip) in RenderEngine, while ear-level front sources stay bit-identical**

## Performance

- **Duration:** 11 min
- **Started:** 2026-10-04T13:26:36Z
- **Completed:** 2026-10-04T13:37:55Z
- **Tasks:** 3
- **Files modified:** 5 (1 new header, 2 engine files, 2 test files)

## Accomplishments

- Closed the Simple half of SpatialCore#15 (BUG-01): front vs back went from 0.00 dB (measured 5.8e-7 dB RMS and max, red) to 4.49 dB RMS / 11.59 dB max.
- `SimpleBinauralCues.h` holds the pure weight function and the three filter tables with their literature source and measured responses; filter state stays in `RenderEngine` so `SpatializationAlgorithm`, `BinauralGains` and `DirectBinauralAlgorithm` are untouched (`git diff --stat` over those paths is empty).
- Ear-level front half proven bit-identical (`==`) at az 0, 30, 60, 89.9, 90, -45, -90 with a 1e-6 weight snap that removes the float cosine residue at exactly +-90.
- Cue state is cold-started when the Simple path resumes; a test that fails on the pre-fix code (peak 0.0535 instead of 0) now passes.
- `PanningLawTests` replaces the Phase 2 deferral comment with an assertion and a pointer to the engine-side tests.

## Measured results (Debug and Release identical)

Simple path, engine impulse response, third-octave band difference against front (az 0, el 0):

| Pair | RMS dB | Max dB | Bound |
|---|---|---|---|
| front / back (180, 0) | 4.4899 | 11.5946 | >= 2.0 RMS, >= 4.0 max |
| front / up90 (0, 90) | 2.5908 | 7.4136 | >= 2.0 RMS, >= 4.0 max |
| front / up45 (0, 45) | 1.8808 | 5.7335 | max >= 2.0 |
| front / down45 (0, -45) | 1.3020 | 3.0558 | max >= 2.0 |
| front / down90 (0, -90) | 1.8769 | 4.5818 | max >= 4.0 |

Designed branch responses at 48 kHz (product of stage magnitudes): DC at 1 Hz within 3e-5 dB for all three; Rear -3.95 dB at 5 kHz (target -4.0); Up +8.38 dB at 8 kHz (target +8.4); Down -5.28 dB at 5.5 kHz (target -5.2, each within 0.5 dB).

Final filter table (48 kHz design, recomputed in `prepare()`, frequencies capped at 0.45 x sample rate):

| Branch | Stages | Status |
|---|---|---|
| Rear | high-shelf -4 dB 2.5 kHz Q 0.7; high-shelf -8 dB 10 kHz Q 0.7; peak +2 dB 1.2 kHz Q 0.8 | unchanged from research |
| Up | peak +10 dB 8 kHz Q 2.0; peak -4 dB 3 kHz Q 0.8; high-shelf -5 dB 11 kHz Q 0.7 | unchanged from research |
| Down | peak -5.5 dB 5.5 kHz Q 1.5; peak +2.5 dB 1.2 kHz Q 0.7 | tuned: dip -6 -> -5.5 dB (see Deviations) |

Test totals: Debug 229/229 test cases (308,085 assertions), Release ctest 229/229 100% passed (previously 225; four new `[bug01]` Simple cases).

## Task Commits

1. **Task 1: Tracer, front vs back on the Simple path with the rear branch** - `79c8ab5` (feat). Red before Step 2 (5.8e-7 dB), green after Step 3 (4.49 / 11.59). Tracer re-verified end-to-end before expansion (interactive, end-of-phase, automated-only verify).
2. **Task 2: Up and down branches, identity, design checks, robustness** - `ca15425` (feat). Tests written first; the direction-pair, down-design and cold-start checks failed, then passed.
3. **Task 3: PanningLawTests and header documentation** - `1276591` (test)

**Plan metadata:** committed separately (docs: complete plan)

## Files Created/Modified

- `include/SpatialCore/Core/SimpleBinauralCues.h` - pure weights, epsilon snap and the three filter stage tables
- `include/SpatialCore/Engine/RenderEngine.h` - private `CueBiquad`, branch coefficients, per-source state, `prevCueWeights_`, `simplePathRanLastBlock_`, `designSimpleCueBank`, `resetSimpleCueState`; class and method documentation
- `src/Engine/RenderEngine.cpp` - bank design in `prepare()`, per-sample cue blend in `renderSimpleBinauralWoodworth`, path-ran flag in `renderBlock`
- `tests/Binaural/BinauralCueTests.cpp` - `[bug01][simple]`, `[identity]`, `[weights]`, `[cold-start]`
- `tests/Algorithms/PanningLawTests.cpp` - "elevation and front/back are engine-side" section and updated comment

## Decisions Made

- Tuned the Down dip to -5.5 dB (see Deviations); every other value is as researched.
- One bad input sample is fed to the filters as 0 while the output is unchanged from before, so recursive state cannot be poisoned.
- No switch back to the flat sound (D-02, Phase 2 VBIP precedent).
- The Woodworth delays are not applied on the Simple path (that would be a second audible change D-01 did not ask for).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Down-branch design value missed its own design check**
- **Found during:** Task 2 (design-magnitude test)
- **Issue:** The research table value (peak -6 dB at 5.5 kHz plus peak +2.5 dB at 1.2 kHz) measured -5.78 dB at 5.5 kHz, 0.58 dB from the required -5.2 +- 0.5 dB.
- **Fix:** Down dip gain -6 dB to -5.5 dB (within the plan's +-3 dB tuning allowance), now -5.28 dB at 5.5 kHz. The test bound was not changed. The reason is recorded next to the table in the header.
- **Files modified:** include/SpatialCore/Core/SimpleBinauralCues.h
- **Verification:** `[bug01][weights]` passes; down90 max band difference 4.58 dB (>= 4.0), down45 3.06 dB (>= 2.0).
- **Committed in:** ca15425

**2. [Rule 2 - Missing Critical] Non-finite input sample guard for the recursive cue state**
- **Found during:** Task 1 (design of the per-sample blend)
- **Issue:** Feeding a NaN mono sample into the biquad cascade would latch NaN in the filter state and silence the engine for good on every path position, including the front half that must stay unchanged. Plan T-03-06 covers non-finite positions, not input.
- **Fix:** The cascade receives 0 for a non-finite sample; the output sample itself is unchanged from the old behaviour. `[bug01][weights]` feeds one NaN sample and requires a finite tail.
- **Files modified:** src/Engine/RenderEngine.cpp, tests/Binaural/BinauralCueTests.cpp
- **Committed in:** 79c8ab5 (guard), ca15425 (test)

---

**Total deviations:** 2 auto-fixed (1 bug-level tuning, 1 missing critical)
**Impact on plan:** Both necessary for the plan's own acceptance checks and robustness truths. No scope creep.

## Issues Encountered

- A `sed -i '1a ...'` line insert failed on macOS BSD sed; the missing Catch2 `catch_approx.hpp` include was added with a short script instead. No effect on the result.
- Debug runs print the known `juce_LeakedObjectDetector.h` FFT assertion at exit (tracked in STATE.md as an open false positive); exit status is 0 and the run reports all passed.

## Known Stubs

None. No stub patterns in the files created or modified.

## Threat Flags

None. No new network, auth, file or schema surface. T-03-06 (non-finite audio) mitigated by position sanitising plus the weight bounds and the input guard; T-03-07 (unannounced audible change) mitigated by the bit-identical front half; the OSD release-note item stays with Plan 03-10.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Criterion 1 now holds on both binaural paths (Simple here, HRTF in Plan 03-01).
- Plan 03-05 (CMake, HRTF data) can proceed; this plan was the only editor of `RenderEngine.h` / `RenderEngine.cpp` and has committed.
- Plan 03-06 adds `SimpleBinauralCues.h` to the `SpatialCore.h` umbrella (single owner of the umbrella).
- Plan 03-10 records the OSD release note for the audible Simple-mode change.
- Headphone listening check remains ROADMAP backlog 999.4 (not a gate).

## Self-Check: PASSED

- Created file present: `include/SpatialCore/Core/SimpleBinauralCues.h`; modified files present.
- Commits found: 79c8ab5, ca15425, 1276591 (`git log --grep="03-04"` returns 3).
- Task acceptance criteria re-run: `grep -c computeSimpleCueWeights src/Engine/RenderEngine.cpp` = 1; empty diff stat over Algorithms, Types.h, BinauralGains.h; `[bug01]` lists 5 cases, all pass; `[engine]` passes; `BinauralCueTests.cpp` and `SpatialCore#15` each found once in PanningLawTests.cpp.
- Plan verification: `[bug01]` green Debug and Release; `[engine]`, `[panning-law]` green; full Debug and Release ctest green; no change under `src/Algorithms` or `include/SpatialCore/Algorithms`.

---
*Phase: 03-binaural-defects-hrtf-packaging*
*Completed: 2026-10-04*
