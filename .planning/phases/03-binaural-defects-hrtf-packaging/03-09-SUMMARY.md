---
phase: 03-binaural-defects-hrtf-packaging
plan: 09
subsystem: engine
tags: [hrtf, render-engine, crossfade, profile-switching, simple-binaural, woodworth, catch2]

requires:
  - phase: 03-binaural-defects-hrtf-packaging
    provides: "03-06 setHRTFProfile, claimReadyRenderer and the renderer free flags; 03-04 Simple-path cue bank and simplePathRanLastBlock_; 03-03 sample-based convolver crossfade; 03-01 switchStepRatio / switchMinRmsRatio and the legacy-swap baseline; 03-08 libmysofa v1.3.5"
provides:
  - "Sample-based renderer crossfade: max (kRendererXfadeBlocks x block size at the start of the fade, kMinRendererXfadeSamples = 4096) samples, fixed at fade start, advanced by elapsed samples"
  - "RenderBlockContext::engineSelectsHRTF (default false): the engine derives useHRTF from its active renderer and crossfades the Woodworth and HRTF paths during a Simple <-> HRTF switch"
  - "Tests: [hrtf-switch][click] at 32/64/128/512 on both swap paths, [hrtf-switch][click][xfade-length], [hrtf-switch][simple], [hrtf-switch][simple][blend], [hrtf-switch][flag-identity]"
affects: [03-10 docs and consumer glue (OSD can drop its derive-useHRTF flow), 03-11, OpenSpatialDelay and OpenSpatialPanner consumer migration, Phase 5 RT-safety audit]

plan_head_before: d0b5fda462a52c6c60132a7c4c4c0ad65b64d92b
plan_head_after: 3c87250517eb261ed3a25ee9a90b2bb837cfdfe2

actuals:
  tokens: 10900
  tasks: 2
  commits: 3

tech-stack:
  added: []
  patterns:
    - "Fade length counted in elapsed samples and fixed when the fade starts, never recomputed per block"
    - "Opt-in context flag that overrides one consumer field from engine state (same family as engineComputesGains and engineDerivesDispatch); flag off keeps the legacy code path call-for-call"
    - "Blend verified by rebuilding it outside the engine: each side from a flag-off engine, the ramp from the published law, compared sample by sample"

key-files:
  created: []
  modified:
    - include/SpatialCore/Engine/RenderEngine.h
    - src/Engine/RenderEngine.cpp
    - tests/Engine/ProfileSwitchTests.cpp

key-decisions:
  - "Fade length is max (8 x block at the start of the fade, 4096) samples, fixed at the start. At 512-sample blocks and above it is the old 8-block fade, so those hosts are unchanged"
  - "engineSelectsHRTF claims any ready profile first, then derives useHRTF from the active renderer. A binaural block always counts as renderer-crossfade-capable, because the fade now blends Simple and HRTF too"
  - "Plain Simple, plain HRTF and HRTF <-> HRTF blocks call exactly the flag-off path; only a Simple <-> HRTF fade runs the blend, so no-switch output is bit-identical to the flag-off path"
  - "The Woodworth path in a fade writes simpleWetL_/simpleWetR_ (sized in prepare()). For an oversized block (a prepare-contract violation) it writes into the pre-sized wet buffer of its own side instead, so the fade never allocates"
  - "Two Simple-mode renderers on both sides of a fade carry the same signal: the fade is ended and the Woodworth path runs once"
  - "The Woodworth cold start in an HRTF -> Simple fade keeps the existing semantics (state zeroed, weights at target, gains from the stored previous gains); the fade-in gain is still near zero in that block"

patterns-established:
  - "A Simple-mode renderer on the HRTF path is silent; with engineSelectsHRTF it is the Woodworth path instead"
  - "Tests that need a specific SOFA profile copy the real file into a temporary shared folder, so they hold in a KEMAR-only build too"

requirements-completed: [DATA-01, BUG-02]

coverage:
  - id: D1
    description: "Switching HRTF profiles through setHRTFProfile and through the legacy escape-hatch swap is click-free and dropout-free at 32, 64, 128 and 512-sample blocks (sine step ratio at most 1.5, pink-noise RMS ratio at least 0.7)"
    requirement: DATA-01
    verification:
      - kind: unit
        ref: "tests/Engine/ProfileSwitchTests.cpp#Profile switch: setHRTFProfile KEMAR to SADIE is click-free and dropout-free at every block size"
        status: pass
      - kind: unit
        ref: "tests/Engine/ProfileSwitchTests.cpp#Profile switch: legacy escape-hatch swap KEMAR to SADIE is click-free and dropout-free at every block size"
        status: pass
    human_judgment: false
  - id: D2
    description: "The renderer crossfade lasts max (8 blocks, 4096 samples) on both swap paths, fixed when the fade starts"
    requirement: BUG-02
    verification:
      - kind: unit
        ref: "tests/Engine/ProfileSwitchTests.cpp#Profile switch: the renderer crossfade lasts max (8 blocks, 4096 samples) on both swap paths"
        status: pass
    human_judgment: false
  - id: D3
    description: "With engineSelectsHRTF set, a switch between Simple (profile 0) and any of profiles 1-5 is click-free and dropout-free at 32, 64, 128 and 512 samples (0 <-> 5 and 0 <-> 3 at all four sizes, 0 <-> 1, 2, 4 at 32)"
    requirement: DATA-01
    verification:
      - kind: unit
        ref: "tests/Engine/ProfileSwitchTests.cpp#Profile switch: with engineSelectsHRTF a switch between Simple and an HRTF profile is click-free and dropout-free"
        status: pass
      - kind: unit
        ref: "tests/Engine/ProfileSwitchTests.cpp#Profile switch: with engineSelectsHRTF Simple to and from each of profiles 1, 2 and 4 is click-free at 32 samples"
        status: pass
    human_judgment: false
  - id: D4
    description: "With the flag set and no switch the output is bit-identical to the flag-off path the active renderer implies; an HRTF <-> HRTF switch equals the flag-off switch; the consumer's useHRTF is ignored on every block through a switch; a consumer that leaves the flag false is unchanged"
    requirement: DATA-01
    verification:
      - kind: unit
        ref: "tests/Engine/ProfileSwitchTests.cpp#Profile switch: engineSelectsHRTF with no switch is bit-identical to the path the active renderer implies"
        status: pass
      - kind: unit
        ref: "tests/Engine/ProfileSwitchTests.cpp#Profile switch: engineSelectsHRTF ignores the consumer's useHRTF on every block, through a switch"
        status: pass
      - kind: unit
        ref: "build /tmp/osd-dr3-check: cmake --build build --target OpenSpatialDelay OpenSpatialDelayTests"
        status: pass
    human_judgment: false
  - id: D5
    description: "During a Simple <-> HRTF fade the Woodworth path runs exactly once per block and the two paths are blended on the equal-power ramp"
    requirement: DATA-01
    verification:
      - kind: unit
        ref: "tests/Engine/ProfileSwitchTests.cpp#Profile switch: during a Simple <-> HRTF fade the Woodworth path runs once per block and the two are blended equal-power"
        status: pass
    human_judgment: false
  - id: D6
    description: "The dual-path fade allocates, locks and logs nothing on the audio thread (DR-1)"
    requirement: DATA-01
    verification: []
    human_judgment: true
    rationale: "Established by design review: simpleWetL_/simpleWetR_ are sized in prepare(), the fade code takes no lock and logs nothing, and an oversized block reuses pre-sized buffers. An allocation-trap run over the new path belongs to the Phase 5 RT-safety audit"

duration: 31 min
completed: 2026-10-04
status: complete
---

# Phase 3 Plan 09: Click-free profile switching Summary

**The renderer crossfade is now counted in elapsed samples (at least 4096), and a new default-off `engineSelectsHRTF` flag lets the engine pick the render path itself and blend the Woodworth and HRTF outputs, so every switch in the profile list, Simple included, is click-free and dropout-free at 32, 64, 128 and 512 samples.**

## What you will hear

Switching HRTF profiles while audio plays no longer depends on how small the host's block size is. Below 512-sample blocks the old fade lasted only a few milliseconds; it now lasts at least 85 ms at 48 kHz at every block size, and at 512 and above it is exactly what it was. A plugin that opts in with the new flag also gets Simple (profile 0) in the same list: switching to or from Simple used to be a hard jump of the whole render path (a loud step going to HRTF, a level dip coming back), and now fades like any other switch. A plugin that does not set the flag hears no change except the longer fade at small block sizes.

One thing to know: on a pure 440 Hz tone, the level in the middle of a Simple <-> KEMAR fade drops to about 0.46 of the quieter side, because the two renderings of one tone partly cancel when their phases differ (the HRTF path delays by up to 1-2 ms, Simple does not). On broadband material the dip does not exist (pink noise stays at 0.92 or above). It is recorded, not a bound.

## Performance

- **Duration:** 31 min
- **Started:** 2026-10-04T15:43:22Z
- **Completed:** 2026-10-04T16:14:13Z
- **Tasks:** 2 (Task 1 tracer, Task 2 TDD)
- **Files modified:** 3

## Accomplishments

- `kMinRendererXfadeSamples = 4096` beside `kRendererXfadeBlocks`; `rendererXfadeSamplesDone_` and `rendererXfadeLengthSamples_` replace `rendererXfadeBlockCount_`. The length is fixed when the fade starts, so a host that varies its block size mid-fade cannot move the end. The Plan 03-06 abandon path and `prepare()` reset the new counters.
- `RenderBlockContext::engineSelectsHRTF` (default false, additive, minor bump). `renderBlock` claims a ready profile first, then sets `useHRTF = ! activeRenderer.isSimpleMode()` in the scratch context. One function (`renderBinauralWithProfileFade`) routes the block: plain Simple, plain HRTF and HRTF <-> HRTF call exactly what the flag-off dispatch calls; a Simple <-> HRTF fade runs the Woodworth path once into `simpleWetL_`/`simpleWetR_` and blends it with the convolution output on the same ramp.
- Fade start and end are shared helpers (`beginRendererFadeIfSwapped`, `endRendererFade`), and `renderDirectBinauralHRTF` takes the block's single `activeRendererIndex` load.
- 7 new test cases; the legacy click case now covers all four block sizes with the dropout bound too.

## Measured results

**Task 1, KEMAR (5) to SADIE (1), flag off, same numbers on `setHRTFProfile` and on the legacy escape-hatch swap** (Debug and Release agree):

| Block | Sine step ratio (bound 1.5) | Pink-noise min RMS ratio (bound 0.7) | Sine min RMS ratio (recorded) |
|---|---|---|---|
| 32 | 1.000 | 0.925 | 0.991 |
| 64 | 1.000 | 0.925 | 0.991 |
| 128 | 1.000 | 0.905 | 0.982 |
| 512 | 1.000 | 0.902 | 0.980 |

At the plan base the legacy swap read: noise RMS 0.855 / 0.855 / 0.855 / 0.902 and sine RMS 2.409 / 2.256 / 1.841 / 0.980 at 32 / 64 / 128 / 512 (sine step 1.000 at all four). The sine RMS above 1 at small blocks was the metric window being wider than the 256-sample fade (mostly the louder SADIE side), not a real dip.

**Task 2, engineSelectsHRTF (Debug and Release identical):**

| Switch | Block | Sine step (<= 1.5) | Noise RMS (>= 0.7) | Sine RMS (recorded) |
|---|---|---|---|---|
| 0 -> 5 | 32 / 64 / 128 / 512 | 1.009 / 1.009 / 1.009 / 1.030 | 1.135 / 1.135 / 1.189 / 1.184 | 0.464 / 0.464 / 0.464 / 0.463 |
| 5 -> 0 | 32 / 64 / 128 / 512 | 1.000 at all four | 1.142 / 1.142 / 1.147 / 1.143 | 0.458 / 0.458 / 0.459 / 0.458 |
| 0 -> 3 | 32 / 64 / 128 / 512 | 1.226 at all four | 0.916 / 0.916 / 0.911 / 0.913 | 1.106 / 1.106 / 1.113 / 1.144 |
| 3 -> 0 | 32 / 64 / 128 / 512 | 1.228 / 1.228 / 1.228 / 1.226 | 1.064 / 1.064 / 1.088 / 1.085 | 1.103 / 1.103 / 1.102 / 1.099 |
| 0 -> 1 | 32 | 1.045 | 1.081 | 1.474 |
| 1 -> 0 | 32 | 1.042 | 1.188 | 1.475 |
| 0 -> 2 | 32 | 1.000 | 0.992 | 0.485 |
| 2 -> 0 | 32 | 1.000 | 1.003 | 0.484 |
| 0 -> 4 | 32 | 1.000 | 1.220 | 0.552 |
| 4 -> 0 | 32 | 1.002 | 1.243 | 0.544 |

Worst margins: sine step 1.228 against 1.5; noise RMS 0.911 against 0.7. No bound was loosened and none came close to failing.

**Identity and blend:** flag on with no switch equals the flag-off path with `==` for Simple (20 blocks, moving source) and for KEMAR (20 blocks); a 5 -> 1 swap with the flag on equals the flag-off swap with `==` over the whole fade; a run with the consumer's `useHRTF` flipping pseudo-randomly every block equals a run with it fixed, through Simple -> HRTF -> Simple, with `==`. The rebuilt blend (Simple side from a flag-off Simple engine, HRTF side from a flag-off HRTF engine, ramp from the published cos/sin law) matches the engine output to a worst deviation of 0 in both directions. Two mutation checks confirmed the test has teeth: running the Woodworth path twice on the active side or twice on the fading side each failed it (worst deviation 0.236 and 0.24).

**Suites:** Debug full 260 of 260 pass; Release ctest 260 of 260 pass; KEMAR-only build (`/tmp/sc-kemar-only`, `SPATIALCORE_EMBED_ALL_HRTF=OFF`) `[hrtf-switch],[hrtf-resolve][embedded]` 21 of 21; consumer build (`/tmp/osd-dr3-check`) `OpenSpatialDelay` and `OpenSpatialDelayTests` both build. The known exit-time "Leaked objects detected ... class FFT" line appears as before and is not from this plan.

## TDD record for Task 2

- **RED (`7a0609c`):** flag declared and documented but not read by the engine, so the tests compile and fail on the behaviour they name. Matrix: `CHECK( toHRTF.noiseRmsRatio >= 0.7 )` read `0.0 >= 0.7` for every 0 <-> 5 and 0 <-> 3 case (with the flag ignored the consumer's `useHRTF = true` runs a silent Simple-mode renderer on the HRTF path). Identity cases failed with 1279, 1280 and 5248 mismatching samples; the useHRTF-ignored case with 5950.
- **GREEN (`3c87250`):** the engine reads the flag; all seven new cases pass.
- **REFACTOR:** none.

## Task Commits

1. **Task 1: Tracer, sample-based renderer crossfade** - `56bc2b0` (feat)
2. **Task 2 RED: failing tests for engineSelectsHRTF** - `7a0609c` (test)
3. **Task 2 GREEN: engineSelectsHRTF** - `3c87250` (feat)

**Plan metadata:** the docs commits following this SUMMARY.

Tracer gate: interactive run, `end-of-phase` mode, tracer `<verify>` automated-only. The verify (configure, build, `[hrtf-switch][click]`) was re-run end to end and passed, and `[hrtf-switch]` (every 03-01, 03-06 and 03-07 case, including `[threads]` and `[shutdown]`) and `[engine]` still pass, so execution continued to Task 2.

## Decisions Made

See `key-decisions` in the frontmatter.

## Deviations from Plan

### Findings that differ from the plan's expectation

**1. The Task 1 step and dip bounds were not red on the plan-base tree.**
- **Plan said:** Step 1 fails at 32 and 64 (research 8.19 and 3.23).
- **Measured:** at the plan base the sine step ratio was 1.000 and the noise RMS ratio 0.855 or better at all four sizes, on the legacy swap. What is red at the base is the crossfade length: 256, 512 and 1024 samples against the promised 4096 at 32, 64 and 128 blocks, on both swap paths. I made that the Task 1 red (`[hrtf-switch][click][xfade-length]`) and kept the step and dip bounds as the criterion-4 guard.
- **Likely cause (not verified):** Plan 03-03's sample-based convolver warm-up and crossfade removed the per-block-count click the research measured; I did not build a pre-03-03 tree to confirm.

### Auto-fixed Issues

**2. [Rule 1 - Bug] Existing switching cases assumed the crossfade ends within about 50 blocks of 64 samples**
- **Found during:** Task 1 (first `[hrtf-switch]` run after the engine change)
- **Issue:** The fade is now 4096 samples, 64 blocks at 64 samples. `switchAndSettle` and two other 03-06 cases checked `isRendererCrossfadeActive()` false after 50 blocks and failed (`[hrtf-resolve][embedded][engine]` among them).
- **Fix:** Added `LiveRender::renderUntilFadeEnds` and used it before those checks. The stated behaviour change, not an engine fault.
- **Files modified:** `tests/Engine/ProfileSwitchTests.cpp`
- **Verification:** full `[hrtf-switch]` and `[engine]` pass
- **Committed in:** `56bc2b0`

**3. [Rule 1 - Bug] New click and crossfade-length cases needed SADIE, which a KEMAR-only build does not embed**
- **Found during:** Task 2 verification (KEMAR-only build run)
- **Issue:** Both cases switched to profile 1 with a non-existent shared folder, so in the `OFF` build the load failed.
- **Fix:** Each case copies the real SADIE file into a temporary shared folder first (`copyProfilesInto`).
- **Files modified:** `tests/Engine/ProfileSwitchTests.cpp`
- **Verification:** KEMAR-only build 21 of 21, default builds still pass
- **Committed in:** `3c87250`

---

**Total deviations:** 2 auto-fixed (2 Rule 1, both in tests) plus 1 finding that the plan's red expectation did not hold
**Impact on plan:** No scope creep. No engine behaviour differs from the plan's specification.

## Issues Encountered

- While running a mutation check I reverted `src/Engine/RenderEngine.cpp` with `git checkout`, which also discarded my uncommitted Task 2 engine edits. I re-applied them from the same edit scripts, re-ran the blend and identity cases (and the second mutation check) before the full runs, and committed only after the full suites passed. Nothing was lost and no committed work was touched.
- The profile-switch cases make the whole `[hrtf-switch]` tag take about 80 seconds in Debug (about 30 s of that is the new `[simple]` matrix, which loads SADIE, HUTUBS and KEMAR many times).

## Known Stubs

None.

## Threat Flags

None. The new surface is the opt-in flag already covered by the plan's threat model (T-03-20 accepted: one extra Woodworth pass for about 85 ms per Simple <-> HRTF switch; T-03-21 mitigated: `simpleWetL_`/`simpleWetR_` are sized in `prepare()` and an oversized block reuses already-sized buffers instead of allocating).

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Plan 03-10 can document `engineSelectsHRTF` and the longer small-block fade as the consumer-facing surface; OpenSpatialDelay can set `engineComputesGains` and `engineSelectsHRTF` together and delete its `useHRTF = ! isSimpleMode()` derivation.
- The flag needs valid `objGains` because the Woodworth path can run during an HRTF fade; the header says so.
- Phase 5's RT-safety audit should include the new fade path (D6 above).

## Self-Check: PASSED

- `include/SpatialCore/Engine/RenderEngine.h`, `src/Engine/RenderEngine.cpp`, `tests/Engine/ProfileSwitchTests.cpp` exist.
- Commits `56bc2b0`, `7a0609c`, `3c87250` exist.
- `grep -c "kMinRendererXfadeSamples"` on the header returns 2; `rendererXfadeBlockCount_` outside comments, combined over header and source: 0; `engineSelectsHRTF` in the header: 4 lines, one `bool engineSelectsHRTF = false`.
- Plan verification: `[hrtf-switch]` green in Debug and Release; full Debug 260 of 260; Release ctest 100% (260 of 260).

---
*Phase: 03-binaural-defects-hrtf-packaging*
*Completed: 2026-10-04*
