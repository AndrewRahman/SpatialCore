---
phase: 05-realtime-safety-hardening
fixed_at: 2026-10-09T02:00:00Z
review_path: .planning/phases/05-realtime-safety-hardening/05-REVIEW.md
iteration: 2
findings_in_scope: 2
fixed: 2
skipped: 0
status: all_fixed
---

# Phase 5: Code Review Fix Report

**Fixed at:** 2026-10-09
**Source review:** .planning/phases/05-realtime-safety-hardening/05-REVIEW.md
**Iteration:** 2

**Summary:**
- Findings in scope: 2
- Fixed: 2
- Skipped: 0

Both findings are logic and contract changes, so both are marked "fixed: requires human verification". Tests pin the new behaviour, but they were written by the same hand as the fix.

**Where the edits and verification ran:** in the main checkout (`irvine`), not in a nested git worktree. The task required staying in this worktree, and the `build` and `build-release` trees live here; a second worktree would have had no build trees. Every gate below ran against this checkout, so the numbers are reproducible from it.

## Fixed Issues

### WR-01: `swapActiveRenderer()` clears the fade source's in-use mark while audio is running, so the detector misses the one misuse the contract text warns about

**Files modified:** `include/SpatialCore/Binaural/HRTFDatabase.h`, `src/Binaural/HRTFDatabase.cpp`, `include/SpatialCore/Engine/RenderEngine.h`, `src/Engine/RenderEngine.cpp`, `tests/Engine/ProfileSwitchTests.cpp`
**Commit:** e90df13
**Status:** fixed: requires human verification
**Applied fix:** A real code fix, not a documented blind spot. Keeping the old renderer marked until a block has run was rejected because "the audio is stopped" and "the audio is running but the next block has not started" cannot be told apart from the message thread. Keeping the mark would bring back iteration 1's stale-mark false positive for a stopped host, and the existing `[in-use]` WR-01 test pins that case. Clearing the mark (as now) hides the load. So the load is detected after the fact instead:
- `HRTFDatabase` gains an always-incremented mutation serial (`getMutationSerial()`, bumped by `noteMutationIfInUse()` whether or not the mark is set) and `countMutationBeforeFade()` (one relaxed add, no assert, no logging).
- `swapActiveRenderer()` snapshots the replaced database's serial before it publishes the new index, then bumps `swapGeneration_`. It still clears the replaced renderer's mark, so a stopped host leaves no stale mark.
- `beginRendererFadeIfSwapped()` compares the fade source's serial with the snapshot when the swap has not yet been "handled" by a completed block, and counts a difference through `countMutationBeforeFade()`.
- Every completed block stores the generation it saw at its top into `swapGenerationHandled_`, so a block on a non-HRTF path (stereo variant, Ambisonics, surround, Simple) releases the swap. A load made after such a block is not counted, which keeps iteration 1's WR-01 behaviour. `prepare()` resets the handled generation.
- The audio-side additions are acquire/relaxed loads, one release store and one relaxed add. There is no allocation, lock or logging.
- The header comment states the timing precisely: a load into the replaced renderer made before the next block is counted when that block starts the crossfade, not when the load runs. It is not counted if no block ever crossfades from that renderer (audio stopped, a non-HRTF path, a swap back).
- New `[in-use]` sections pin:
  - the counted case: a load after a swap with blocks running reads 0 at once, then 1 after the first block, and stays 1 through the fade;
  - a non-HRTF block between the swap and the load: 0;
  - swap, load, swap back with no block: no fade and 0;
  - a stopped host that resumes: counted at the first crossfade block.

Residual limits, documented in the header and integration guide:
- The count arrives one block late.
- A load that is still running when a binaural block begins is counted once, at the serial bump; the in-use mark covers any later `setProfile`.

### WR-02: The "wait until the flag is seen true, then false" rule can never be satisfied while blocks run on a path that does not crossfade

**Files modified:** `include/SpatialCore/Engine/RenderEngine.h`, `tests/Engine/ProfileSwitchTests.cpp`, `docs/integration-guide.md`, `CLAUDE.md`, `.claude/skills/spatialcore-architecture/spatialcore-architecture.md` (CRLF kept)
**Commit:** e3eae4a
**Status:** fixed: requires human verification
**Applied fix:** The rule is now backed by a queryable engine condition instead of a consumer-side guess. `RenderEngine::isPrepareRendererFree()` (any thread, two acquire loads) is true when no crossfade runs and a block has completed since the last swap. It is true from construction and after `prepare()`.
- It turns true after one block on any path that does not use the HRTF renderers, where `isRendererCrossfadeActive()` never turns true.
- On the binaural HRTF path it turns true once the crossfade has ended.
- With no blocks running it stays false. The header, integration guide and CLAUDE.md say this is the one exception: a load is then safe, but the first binaural block after the host resumes fades from the reloaded renderer and is counted (WR-01).
- All three docs now say to poll `isPrepareRendererFree()` and not to wait on `isRendererCrossfadeActive()` alone. The architecture skill file did not repeat the "seen true, then false" rule; it got a one-clause pointer.
- Tests pin: true at start and after the documented flow; false after a swap and during the first fade block; true after the fade; true after one stereo-variant block and one Simple-path block with the flag never true; false while no block renders; and true after one block following a swap back.
- The answer is a snapshot. It cannot stop a format switch to binaural from landing while a load is in flight, and the docs say so.

## Verification

All gates ran in this checkout.

- `cmake --build build -j8`: ok. `build/tests/SpatialCoreTests`: all tests passed (328996 assertions in 387 test cases). `build/tests/SpatialCoreUITests`: all tests passed (150 assertions in 16 test cases).
- `cmake --build build-release -j8`: ok. `ctest --test-dir build-release/tests -j8`: 100% tests passed out of 403.
- `build-release/tests/SpatialCoreTests "[rt]"`: all tests passed (2514 assertions in 46 test cases).
- Not run this iteration: `build-tsan`. The new audio-side code is relaxed/acquire/release atomics only, but the realtime-safety check at phase end should include the `[tsan]` subset.

---

_Fixed: 2026-10-09_
_Fixer: Claude (gsd-code-fixer)_
_Iteration: 2_
