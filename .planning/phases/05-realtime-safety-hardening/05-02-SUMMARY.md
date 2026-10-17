---
phase: 05-realtime-safety-hardening
plan: 02
subsystem: audio-engine
tags: [realtime-safety, oversized-block, chunking, binaural, hrtf, catch2, malloc-trap, scratch-guard]

requires:
  - phase: 05-realtime-safety-hardening
    provides: "Plan 05-01: the allocation and lock trap (tests/Support/RealtimeGuard.h), MovingScene, OutputBank and armedRender"
provides:
  - "RenderEngine renders an oversized host block completely, in pieces of at most the prepared size, on the direct-binaural HRTF path, with no audio-thread growth (D-11)"
  - "RenderEngine::getOversizedBlockCount(): lock-free count of renderBlock calls longer than prepare()'s maxBlockSize (additive public API)"
  - "renderBlock returns at once for a zero-length block (nothing written, no fade step, no counter)"
  - "BinauralRenderer::scratchFits: one shared guard that skips, counts, asserts in Debug only and never resizes (D-03, D-04)"
  - "BinauralRenderer::getScratchTooSmallCount() and TEST-ONLY shrinkScratchForTesting() (additive public API)"
affects: [05-03, 05-04, RTSF-01, RTSF-05, VERIFY-02, OpenSpatialDelay migration]

actuals:
  tokens: 10337
  tasks: 2
  commits: 3

tech-stack:
  added: []
  patterns:
    - "Oversized block rendered in prepared-size pieces through a stack RenderSources copy whose per-sample pointers advance (same idea as PartitionedConvolver WR-02)"
    - "Defensive audio-thread guard = one helper that counts, asserts in Debug and lets the caller skip; never a resize"
    - "Twin-renderer bit comparison proves a skipped call moved no state"
    - "Pin scratch capacity (swap with a fresh vector) so an audio-thread resize cannot hide in spare capacity"

key-files:
  created: []
  modified:
    - include/SpatialCore/Engine/RenderEngine.h
    - src/Engine/RenderEngine.cpp
    - include/SpatialCore/Binaural/BinauralRenderer.h
    - src/Binaural/BinauralRenderer.cpp
    - tests/Engine/RenderEngineTests.cpp
    - tests/Engine/RealtimeSafetyTests.cpp
    - tests/Binaural/BinauralRendererTests.cpp

key-decisions:
  - "Chunk the HRTF path rather than skip: renderDirectBinauralHRTF keeps beginRendererFadeIfSwapped and the HRIR update once per block with the full length, then loops renderDirectBinauralHRTFChunk over pieces of preparedMaxBlock_"
  - "A chunk whose Simple-mode active renderer finds the renderer crossfade already over (it ended in an earlier piece) runs the plain Woodworth path straight into the output, exactly as the next block would"
  - "A never-prepared engine (preparedMaxBlock_ <= 0) writes silence on the HRTF path and returns"
  - "shrinkScratchForTesting swaps in a fresh vector so capacity equals size; resize() would leave spare capacity (KEMAR's 558 grows a 512 vector to 1024) and hide the old audio-thread resize from the trap"
  - "Allocation and lock checks for blocks that look HRIRs up run in Release only, because a Debug tree builds libmysofa with -DVDEBUG and it printf()s on every interpolation (interpolate.c:119)"

patterns-established:
  - "Oversized-block contract: handled, counted, never asserted"
  - "Scratch-too-small contract: skipped, counted, asserted in Debug only, never grown"

requirements-completed: [RTSF-01, RTSF-05]

coverage:
  - id: D1
    description: "A host block longer than the prepared size renders completely on the HRTF path in pieces; one 256-sample block on a 64-sample engine equals four 64-sample blocks bit for bit (steady, during the renderer crossfade, during the Simple-to-HRTF blend)"
    requirement: RTSF-01
    verification:
      - kind: unit
        ref: "tests/Engine/RenderEngineTests.cpp#RenderEngine: an oversized HRTF block renders in prepared-size pieces, identical to prepared-size blocks (D-11, CR-03)"
        status: pass
    human_judgment: false
  - id: D2
    description: "The three engine grow sites are gone; a 1024-sample block on a 256-sample engine renders on all five paths with zero allocations, frees and locks (Release) and the oversized counter equals the number of oversized blocks"
    requirement: RTSF-05
    verification:
      - kind: unit
        ref: "tests/Engine/RealtimeSafetyTests.cpp#Realtime: an oversized block renders on every path without allocating (D-11)"
        status: pass
      - kind: other
        ref: "awk '/^bool RenderEngine::renderDirectBinauralHRTF/,/^}/' src/Engine/RenderEngine.cpp | grep -cE '\\.(assign|resize) *\\(' (prints 0)"
        status: pass
    human_judgment: false
  - id: D3
    description: "Adjacency and empty edges: exactly the prepared size is one piece and not counted; one sample over is counted once and allocates nothing; a zero-length block writes nothing, counts nothing and leaves an SC-20 fade pending; an oversized block with no live object is exact silence"
    requirement: RTSF-05
    verification:
      - kind: unit
        ref: "tests/Engine/RealtimeSafetyTests.cpp#Realtime: block-length edges, exactly prepared, one sample over, empty, no live object (D-11)"
        status: pass
    human_judgment: false
  - id: D4
    description: "BinauralRenderer's two guards share scratchFits: a too-small scratch skips just that work, keeps the last good sound (bit-identical to a twin that never saw the call), is counted once, and allocates nothing in Release; boundary and empty edges count nothing"
    requirement: RTSF-01
    verification:
      - kind: unit
        ref: "tests/Binaural/BinauralRendererTests.cpp#BinauralRenderer: renderSourceBuffers with a scratch that is too small skips, counts and keeps the last good sound"
        status: pass
      - kind: unit
        ref: "tests/Binaural/BinauralRendererTests.cpp#BinauralRenderer: updateSourceHRIR with a scratch that is too small skips, counts and keeps the previous HRIR"
        status: pass
      - kind: unit
        ref: "tests/Binaural/BinauralRendererTests.cpp#BinauralRenderer: the scratch guards count nothing at the boundary and exactly once one sample short"
        status: pass
    human_judgment: false
  - id: D5
    description: "Blocks of at most the prepared size are unchanged: full Debug suite, Debug UI suite and Release ctest pass, including [golden] and [hrtf-switch]"
    verification:
      - kind: integration
        ref: "./build/tests/SpatialCoreTests; ./build/tests/SpatialCoreUITests; ctest --test-dir build-release/tests"
        status: pass
    human_judgment: false

duration: 30min
completed: 2026-10-06
status: complete
plan_head_before: 7e6c254c30edeb8a2dbca63c71cb79998b80889d
plan_head_after: a4c3ba430386465a4d88107b160f4b472e19da78
commits: 3
---

# Phase 5 Plan 02: Oversized blocks and BinauralRenderer scratch guards Summary

**An oversized host block on the HRTF path is now rendered completely in prepared-size pieces (one 256-sample block equals four 64-sample blocks bit for bit in three crossfade states) with the three audio-thread grow sites deleted and a lock-free `getOversizedBlockCount()`, and BinauralRenderer's two `resize` guards became one `scratchFits` helper that skips, counts and never allocates.**

## Performance

- **Duration:** about 30 min
- **Started:** 2026-10-05T23:37Z (estimated from the first read; the start time was not captured by command)
- **Completed:** 2026-10-06T00:08Z
- **Tasks:** 2 (Task 1 tracer; Task 2 TDD: RED, GREEN)
- **Files modified:** 7 (0 created)

## Accomplishments

- `renderDirectBinauralHRTF` keeps its signature and does the once-per-block work (renderer fade start, HRIR updates) with the full length, then renders pieces of at most `preparedMaxBlock_` through the new `renderDirectBinauralHRTFChunk`. A block of at most the prepared size is exactly one piece with the consumer's own pointers, so its output is unchanged.
- `sourceAccumStorage_`, `wetBufL_`/`wetBufR_` and `xfadeWetL_`/`xfadeWetR_` are no longer grown on the audio thread. `renderBlock` returns at once for `numSamples <= 0` and counts every longer-than-prepared block in `getOversizedBlockCount()`, with no assert (it is handled).
- `BinauralRenderer::scratchFits` replaces both `resize` guards. `updateSourceHRIR` keeps the source's previous HRIR; `renderSourceBuffers` leaves its zeroed output and advances no convolver, ITD line or shelf state. Each miss increments `getScratchTooSmallCount()` and asserts in Debug only.
- The CR-03 test is rewritten to assert the new contract, and the new `[oversized]` and `[rt][scratch]` cases pin the five paths, the adjacency and empty edges, and the guards.

## Evidence (as the plan's `<output>` requires)

- **BASE:** `7e6c254c30edeb8a2dbca63c71cb79998b80889d` (HEAD when the plan started).
- **BASE oversized allocation count (D-07 old behaviour), Release:** the new five-path case at BASE, 30 blocks of 1024 on a 256-sample engine, failed only on the HRTF configuration with **7 allocations and 7 frees, 0 locks**; first offender `std::vector<float>::assign` inside `renderDirectBinauralHRTF` (the CR-03 `sourceAccumStorage_` grow), called from `renderBlock`. The other four configurations were already at 0. After the change: 0 allocations, 0 frees, 0 locks in Release, counter equal to 30 on all five configurations.
- **Three bit-identity SECTION results** (Debug and Release, `REQUIRE std::memcmp == 0` over 256 left and 256 right floats, output finite and not all zero, `A.getOversizedBlockCount() == 1`, `B.getOversizedBlockCount() == 0`):
  - "steady" (80 prepared-size warm-up blocks, KEMAR claimed, crossfade finished): PASS.
  - "during the renderer crossfade" (no warm-up, the block under test claims KEMAR and starts the fade): PASS.
  - "Simple-to-HRTF blend" (`engineSelectsHRTF`, profile 0 fading into KEMAR): PASS.
- **D-07 mutation for the guards (scratch step, never committed), after the GREEN commit `a4c3ba4`:** in `scratchFits`' miss path both vectors were resized to `needed` and `true` returned (the old behaviour). Release `"[rt][scratch]"` then reported `test cases: 3 | 1 passed | 2 failed`, `assertions: 41 | 33 passed | 8 failed`. Failing lines (BinauralRendererTests.cpp): 529 and 530 (output not exact zero after the skipped `renderSourceBuffers`), 536 and 560 (`sameBits (fromHit, fromTwin)`, the renderer drifted from its twin), and 497 and 498 twice (`counts.allocs == 0 for: 2 == 0`, `counts.frees == 0 for: 2 == 0`, with `counts.allocs := 2`, `counts.frees := 2`, `counts.locks := 0`). Restored with `git checkout -- src/Binaural/BinauralRenderer.cpp` (no `MUTATION` text left), rebuilt, `"[rt][scratch]"` 41 assertions in 3 cases, all pass.
- **RED for Task 2 at commit `4f13459` (Release):** `3 | 0 passed | 3 failed`, `assertions: 41 | 26 passed | 15 failed`: counter 0 where 1 was expected, `counts.allocs := 2` and `counts.frees := 2` from the old resize, output differing from the twin. These are assertion failures on the target behaviour. `gsd_run check tdd-red-evidence` was not run: it parses Node-test TAP or Surefire XML only, and Catch2 output is neither; the evidence above is recorded by hand.
- **Test counts:** Debug `SpatialCoreTests` 334 cases, 325,599 assertions pass (329 before this plan: CR-03 rewritten in place, plus 2 `[oversized]` and 3 `[rt][scratch]`); Debug `SpatialCoreUITests` 14 cases, 133 assertions pass; Release `ctest` 348 of 348 pass (343 before + 5); `ctest -N` lists 14 `ui:` tests. `[engine],[hrtf-switch],[golden]` in Debug and Release: 4102 assertions in 85 cases, unchanged. Release `"[rt]"`: 8 cases, 149 assertions, none skipped.

## Task Commits

1. **Task 1 (tracer): oversized HRTF block rendered in prepared-size pieces, counter, CR-03 rewrite, `[oversized]` cases** - `2d8df43` (feat)
2. **Task 2 RED: failing `[rt][scratch]` cases, API stubs and the TEST-ONLY shrink seam** - `4f13459` (test)
3. **Task 2 GREEN: `scratchFits` behind both guards** - `a4c3ba4` (feat)

**Plan metadata:** the docs commit that follows this file (`docs(05-02): complete oversized block and scratch guard plan`).

Tracer feedback gate: auto mode not active, `human_verify_mode` is the default end-of-phase, and the tracer `<verify>` carries only `<automated>`, so the gate was a re-run of the tracer verify end to end (Debug build, `[oversized]`, `[engine],[hrtf-switch],[golden]`, Release build, `[oversized]`). It passed, and expansion (Task 2) went ahead.

## Files Created/Modified

- `include/SpatialCore/Engine/RenderEngine.h` - public `getOversizedBlockCount()`, private `oversizedBlockCount_` and `renderDirectBinauralHRTFChunk`
- `src/Engine/RenderEngine.cpp` - `renderDirectBinauralHRTF` loops over pieces; `renderDirectBinauralHRTFChunk` holds the old body without any grow; `renderBlock` early return and counter
- `include/SpatialCore/Binaural/BinauralRenderer.h` - `getScratchTooSmallCount()`, TEST-ONLY `shrinkScratchForTesting()`, private `scratchFits` and `scratchTooSmallCount_`; doc comments state the skip behaviour
- `src/Binaural/BinauralRenderer.cpp` - `scratchFits` used by `updateSourceHRIR` and `renderSourceBuffers`; no resize outside `ensureScratchCapacity` and the test seam
- `tests/Engine/RenderEngineTests.cpp` - CR-03 test rewritten (three SECTIONs)
- `tests/Engine/RealtimeSafetyTests.cpp` - five-path `[oversized]` case and the block-length edge case (with the SC-20 pending-fade check)
- `tests/Binaural/BinauralRendererTests.cpp` - three `[rt][scratch]` cases; the 03-03 comment that said the guards "stay as they are" now says they skip, count and never allocate

## Decisions Made

- Kept the plan's design: `beginRendererFadeIfSwapped` and the HRIR update loop run once per block with the full length. The renderer crossfade of an oversized block is therefore `max (8 x full block, 4096)` samples long, which equals the prepared-size-blocks value only while 8 x block is at most 4096 (true for the 256-versus-64 test). For a much larger oversized block the fade is longer, which is harmless for a prepare-contract violation; the test comment records why 64 is used.
- A new chunk-level branch handles a Simple-mode active renderer after the renderer crossfade ended in an earlier piece: the old body assumed a fade was still running and would have copied from `wetBufL_` instead of the Woodworth output.
- `shrinkScratchForTesting` swaps in fresh vectors (capacity equals size). With `resize`, KEMAR's 558 grows a 512-float vector to 1024 capacity, so the old guard's `resize (600)` allocated nothing and the trap could not see the old behaviour. The tests pin the scratch to exactly 558 for that reason.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Chunk of a Simple-active blend after the fade ended read the wrong buffer**
- **Found during:** Task 1 (design of `renderDirectBinauralHRTFChunk`, before the first build)
- **Issue:** the pre-D-11 body assumed a renderer fade is always running when `simpleCtx` is set (true for one block, false once pieces split a block and the fade ends mid-block). With the Simple renderer active and no fade it would have written the Woodworth output to `simpleWetL_/R_` and then copied the stale `wetBufL_/R_` to the output.
- **Fix:** at the top of each piece, when `simpleCtx` is set, the active renderer is Simple and no fade is running, the piece runs `renderSimpleBinauralWoodworth` straight into the output pointers and returns true, exactly what the following block would do through `renderBinauralWithProfileFade`.
- **Files modified:** `src/Engine/RenderEngine.cpp`
- **Verification:** blocks of at most the prepared size never reach this branch (a fade is guaranteed there), so `[golden]`, `[hrtf-switch]` and `[engine]` are unchanged; the three bit-identity SECTIONs pass.
- **Committed in:** `2d8df43`

**2. [Rule 1 - Test seam] `shrinkScratchForTesting` made capacity equal size**
- **Found during:** Task 2 RED (the first red run showed `allocs == 0` for the old guard)
- **Issue:** `resize` left spare capacity, so the old audio-thread resize did not allocate and the trap could not prove the old behaviour.
- **Fix:** swap with a freshly built vector; the tests pin the scratch to exactly 558 floats.
- **Files modified:** `src/Binaural/BinauralRenderer.cpp`, `include/SpatialCore/Binaural/BinauralRenderer.h` (comment), `tests/Binaural/BinauralRendererTests.cpp`
- **Verification:** RED now shows 2 allocations and 2 frees from the old guard; the D-07 mutation shows the same.
- **Committed in:** `4f13459`

**3. [Rule 3 - Blocking] HRTF-lookup allocation and lock checks gated to Release**
- **Found during:** Task 1 (first Debug run of `[oversized]`)
- **Issue:** in a Debug tree libmysofa is compiled with `-DVDEBUG` and `printf()`s on every HRIR interpolation (`interpolate.c:119`), which allocates and takes stdio locks on the rendering thread (3 allocations, 1285 locks in the HRTF configuration). This is third-party debug output, not a SpatialCore allocation, and the plan's Debug run of `[oversized]` could not pass otherwise.
- **Fix:** blocks that look HRIRs up (the HRTF configuration and the prepared-size and one-over edge blocks) are allocation- and lock-checked only when `JUCE_DEBUG` is off; their output, counter and finite checks run in both builds, and blocks that look nothing up (no live object, empty, SC-20) are trap-checked in both. The Release ctest remains the RTSF-01 proof, as the plan states.
- **Files modified:** `tests/Engine/RealtimeSafetyTests.cpp`
- **Verification:** Debug `[oversized]` passes (92 assertions, 3 cases); Release passes with every check on (101 assertions).
- **Committed in:** `2d8df43`

---

**Total deviations:** 3 auto-fixed (2 Rule 1, 1 Rule 3)
**Impact on plan:** all three were needed for correctness or for a verifiable test; no scope creep and no change to any block of at most the prepared size.

## Issues Encountered

- None beyond the deviations above. Expected Debug noise appeared: "Leaked objects detected: 1 FFT" at exit (D-08), libmysofa trace lines, and one JUCE assertion line per deliberate guard trip in `[rt][scratch]`.

## Known Stubs

None.

## Threat Flags

None. T-05-04 (oversized block versus fixed buffers), T-05-05 (audio-thread resize) and T-05-06 (skipped guard leaving stale state) are mitigated as planned and proven by the bit-identity, trap and twin-renderer cases. The new public surface (`getOversizedBlockCount`, `getScratchTooSmallCount`, the TEST-ONLY `shrinkScratchForTesting`) adds no network, auth or file access.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Plan 05-03 can reuse the five-path `[oversized]` scaffolding (`OversizedConfig`, `makeOversizedEngine`, `armedOnce`) which lives in the anonymous namespace of `RealtimeSafetyTests.cpp`.
- Public API added (all additive, minor bump): `RenderEngine::getOversizedBlockCount()`, `BinauralRenderer::getScratchTooSmallCount()` and the TEST-ONLY `BinauralRenderer::shrinkScratchForTesting (size_t)`. `BinauralRenderer` is now non-copyable through its atomic counter (no copy exists in the tree; the build is clean).
- Not done here, by design: the CLAUDE.md "Design Principles" and README text about the oversized-block contract belong with the Phase 5 documentation plan.

## Self-Check: PASSED

- Modified files: all 7 FOUND. Commits `2d8df43`, `4f13459`, `a4c3ba4`: FOUND; `git log --all --grep="05-02"` returns 3 commits; `git rev-list --count 7e6c254..HEAD` is 3.
- Task 1 acceptance: Release `[oversized]` 3 cases pass, none skipped; the `renderDirectBinauralHRTF` body grep for `.assign (`/`.resize (` prints 0; `renderDirectBinauralHRTFChunk` appears 3 times in the .cpp; `getOversizedBlockCount` appears once in the header; `[engine],[hrtf-switch],[golden]` passes.
- Task 2 acceptance: `resize` count in `updateSourceHRIR` and `renderSourceBuffers` bodies is 0 and 0; `scratchFits` appears 3 times in the .cpp; Release `[rt][scratch]` 3 cases pass, none skipped; the D-07 mutation run and its restore are recorded above; the full Debug suite, UI suite and Release ctest pass (348 of 348, 14 `ui:` tests).
- Plan-level verification re-run after the last source change: Debug build of every target, full Debug suite (334 cases), UI suite (14 cases), Release build of every target, Release `[rt]` and full ctest. `git grep -l -e malloc_logger -e __interpose -- src include CMakeLists.txt` prints nothing.

---
*Phase: 05-realtime-safety-hardening*
*Completed: 2026-10-06*
