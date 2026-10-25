---
phase: 05-realtime-safety-hardening
fixed_at: 2026-10-09T00:00:00Z
review_path: .planning/phases/05-realtime-safety-hardening/05-REVIEW.md
iteration: 3
findings_in_scope: 3
fixed: 3
skipped: 0
status: all_fixed
---

# Phase 5: Code Review Fix Report

**Fixed at:** 2026-10-09
**Source review:** .planning/phases/05-realtime-safety-hardening/05-REVIEW.md
**Iteration:** 3

**Summary:**
- Findings in scope: 3
- Fixed: 3
- Skipped: 0

## Fixed Issues

### WR-01: `prepare()` settles a pending legacy swap without resetting the crossfade state, so `isPrepareRendererFree()` reports true and the detector is skipped while the next block still fades from the replaced renderer

**Files modified:** `src/Engine/RenderEngine.cpp`, `tests/Engine/ProfileSwitchTests.cpp`
**Commit:** d1d3a82
**Status:** fixed: requires human verification (state handling logic; the new tests pin it, and the reasoning below is worth a human read)
**Applied fix:**
- In `RenderEngine::prepare()` the crossfade-state reset (`rendererXfading_`, `rendererXfadeSamplesDone_`, `rendererXfadeLengthSamples_`, `rendererXfadeActive_`, `prevActiveRendererIdx_ = activeIdx`) now runs unconditionally, right after `rendererFree_` is recomputed. `forceReload_`, the re-issued request and the loader restart stay inside `if (hrtfEverRequested_)`.
- The engine-owned (`setHRTFProfile`) route is unchanged: it already ran exactly this reset, in the same position relative to the other statements, so for it the move is a no-op. Only the legacy route (`hrtfEverRequested_ == false`) gains the reset.
- No block runs during `prepare()` (it is message-thread, the audio thread is not running), so the plain members are single-threaded there. No audio-path code changed, so the audio path stays lock-free and allocation-free.
- Two new sections in the `[hrtf-switch][misuse][in-use][tsan]` test case:
  - swap, `prepare()`, then a binaural block. After `prepare()` the free query is true, a load into the replaced renderer is not counted, and the first block starts no crossfade, leaves the replaced renderer unmarked and marks the new active one.
  - a legacy crossfade that is running when `prepare()` is called ends there; the next block does not pick it up and the replaced renderer is free.
- Test-first check: with the old `prepare()` restored, both sections fail (8 assertions: the first block did start the fade, and the free query was false). With the fix they pass.

### WR-02: `isPrepareRendererFree()` reads the two conditions in the wrong order, so it can return true while a crossfade from the replaced renderer has just started

**Files modified:** `include/SpatialCore/Engine/RenderEngine.h`
**Commit:** dc1f45c
**Status:** fixed: requires human verification (memory-ordering logic; it cannot be exercised deterministically by a test)
**Applied fix:** `isPrepareRendererFree()` now loads `swapGenerationHandled_` (acquire) and `swapGeneration_` (acquire) first and returns false on a mismatch, and reads `rendererXfadeActive_` (acquire) last. A comment states that the order is load-bearing and why: seeing `handled == generation` through the acquire load makes the same block's fade-start store visible to the flag read that follows. The "two acquire loads" wording in the doc comment became "acquire loads only". Existing `isPrepareRendererFree()` sections (WR-02 of iteration 2) still pass.

### IN-01: The `[hrtf-switch][ui-timer]` distinct-profile check fails under machine load

**Files modified:** `tests/Engine/ProfileSwitchTests.cpp`
**Commit:** 8865895
**Applied fix:**
- The dwell phase no longer changes the wanted profile every `12 * scale` ticks. It now steps through the cycle, calling `setHRTFProfile (wanted)` on every 30 Hz tick, and moves to the next profile only when `getHRTFProfileStatus().activeProfile == wanted`, bounded by a per-step timeout of `40 s * SPATIALCORE_TEST_TIMEOUT_SCALE`. How long a load takes now changes how long the test runs, not how many distinct profiles it sees.
- Unchanged: the storm phase (first `1000 ms * scale`, a new profile on every tick), the 30 Hz calls, the final settle, the render thread, and the checks on non-finite audio, blocks, mutation counters and wrong-thread count.
- Watchdog raised from 120 s to 300 s (times the scale) so the bounded waits fit. It only matters on a starved machine; an idle run takes about 3.4 s.
- `CHECK (activeSeen.size() >= 3)` is kept for the full embedded set; on the one-profile build (`SPATIALCORE_EMBEDS_ALL_HRTF` off) the cycle is `{5, 0, 5, 0}`, which can only show 2 distinct profiles, so the bound there is 2. This is an extra change beyond the review text; the old check could never pass on that build.
- Load evidence (Debug, `[ui-timer]`):
  - idle: 4 distinct profiles, 3 of 3 runs (about 3.4 s each)
  - 12 busy processes on 14 cores: 4 distinct, 2 of 2 passes (8 to 11 s)
  - 20 busy processes on 14 cores, step bound at 20 s: failed 3 of 3 (1 to 2 distinct profiles; the low-priority loader was starved, nothing stuck)
  - 20 busy processes, scale 3: passed (4 distinct, 109 s)
  - 20 busy processes, final 40 s bound, scale 1: passed (3 distinct, 82 s)

## Verification

Where it ran: the main checkout of this worktree (`irvine`), in place. The directory is itself an isolated git worktree that holds the build trees (`build`, `build-release`, `build-tsan`), so no nested worktree was created. `workflow.use_worktrees` is not set to false in the config, but a nested worktree would have lacked the build trees and the launching agent told me to stay here. So there was no temp branch, no recovery sentinel and no cleanup tail. The numbers below are reproducible from this tree.

| Gate | Result |
|------|--------|
| Debug `build/tests/SpatialCoreTests` | All tests passed (329024 assertions in 387 test cases) |
| Debug `build/tests/SpatialCoreUITests` | All tests passed (150 assertions in 16 test cases) |
| Release `ctest --test-dir build-release/tests -j8` | 100% tests passed out of 403 |
| Release `build-release/tests/SpatialCoreTests "[rt]"` | All tests passed (2514 assertions in 46 test cases) |
| TSan `[hrtf-switch]`, `SPATIALCORE_TEST_TIMEOUT_SCALE=10` | exit=0, All tests passed (539 assertions in 25 test cases), ThreadSanitizer warnings: 0 |

The TSan ui-timer line was `573 ticks, 30480 blocks, 4 distinct active profiles, 0 non-finite`. The Debug runs print a "Leaked objects detected: FFT" line after the pass; it is the deliberate process-global `SharedFFTCache` (D-08) and was there before this iteration.

---

_Fixed: 2026-10-09_
_Fixer: Claude (gsd-code-fixer)_
_Iteration: 3_
