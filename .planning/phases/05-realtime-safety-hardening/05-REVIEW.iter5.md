---
phase: 05-realtime-safety-hardening
reviewed: 2026-10-09T00:00:00Z
depth: standard
files_reviewed: 32
files_reviewed_list:
  - CLAUDE.md
  - .gitignore
  - CMakeLists.txt
  - examples/demo/DemoComponent.h
  - include/SpatialCore/Binaural/BinauralRenderer.h
  - include/SpatialCore/Binaural/HRTFDatabase.h
  - include/SpatialCore/Binaural/SharedFFTCache.h
  - include/SpatialCore/Engine/RenderEngine.h
  - include/SpatialCore/Trajectory/TrajectoryEngine.h
  - include/SpatialCore/UI/SpatialMapComponent.h
  - src/Algorithms/AmbisonicsAlgorithm.cpp
  - src/Binaural/BinauralRenderer.cpp
  - src/Binaural/HRTFDatabase.cpp
  - src/Binaural/PartitionedConvolver.cpp
  - src/Engine/RenderEngine.cpp
  - src/Trajectory/TrajectoryEngine.cpp
  - src/UI/SpatialMapComponent.cpp
  - tests/Binaural/BinauralRendererTests.cpp
  - tests/Binaural/SharedFFTCacheTests.cpp
  - tests/CMakeLists.txt
  - tests/Engine/MultiInstanceTests.cpp
  - tests/Engine/ProfileSwitchTests.cpp
  - tests/Engine/RealtimeSafetyTests.cpp
  - tests/Engine/RenderEngineTests.cpp
  - tests/Support/RealtimeGuard.h
  - tests/Support/Watchdog.h
  - tests/Support/rtguard/RTGuard.cpp
  - tests/Trajectory/TrajectoryTests.cpp
  - tests/UI/MultiMapTests.cpp
  - README.md
  - docs/integration-guide.md
  - .claude/skills/spatialcore-architecture/spatialcore-architecture.md
findings:
  critical: 0
  warning: 2
  info: 1
  total: 3
status: issues_found
---

# Phase 5: Code Review Report (re-review, iteration 3)

**Reviewed:** 2026-10-09
**Depth:** standard
**Files Reviewed:** 32
**Status:** issues_found

## Summary

This pass checked the two iteration-2 fix commits (e90df13, e3eae4a) against the code. The rest of the phase is unchanged since the last pass, and I did not re-raise anything that is actually fixed.

Verified as correct:

- **Audio-path realtime safety of the new detector.** The audio side adds one acquire load, one release store, two relaxed loads, a serial compare and one `fetch_add`. All are `std::atomic<unsigned>` or `<int>`, with a `static_assert` on lock-freedom. There is no allocation, lock or logging.
- **Swap ordering.** `swapActiveRenderer()` stores the serial snapshot, then the active index (release), then bumps the generation (release). A block that acts on the new index through its acquire load also sees the snapshot. A block that sees the new index but an old generation can only have started before the swap returned.
- **Swap back.** Two swaps before any block leave `prevActiveRendererIdx_` equal to the active index, so no fade starts and nothing is counted.
- **Wrap-around.** Unsigned wrap-around of the serial and generation is harmless, since both are compared for inequality only.
- **Early return.** The zero-length-block early return does not touch the generation. Every other block stores `handled` at its end.

Two problems remain in the new code.

## Warnings

### WR-01: `prepare()` settles a pending legacy swap without resetting the crossfade state, so `isPrepareRendererFree()` reports true and the detector is skipped while the next block still fades from the replaced renderer

**File:** `src/Engine/RenderEngine.cpp:512-514` (the settle) against `src/Engine/RenderEngine.cpp:603-610` (the reset)

**Issue:**
- The new line in `prepare()` sets `swapGenerationHandled_ = swapGeneration_`. Its comment says "prepare() resets the crossfade state below".
- That reset (`rendererXfading_ = false`, `rendererXfadeActive_ = false`, `prevActiveRendererIdx_ = activeIdx`) sits inside `if (hrtfEverRequested_)`. A legacy-route consumer never calls `setHRTFProfile`, so `hrtfEverRequested_` is false and none of it runs.
- Sequence on the legacy route:
  1. A binaural block runs with renderer A active. `prevActiveRendererIdx_` is A.
  2. The consumer calls `swapActiveRenderer()`. B is now active and A is the replaced renderer.
  3. The host calls `prepareToPlay()` again. It does this often: on a sample-rate or block-size change, and at the start of an offline render.
  4. `isPrepareRendererFree()` now returns true. `rendererXfadeActive_` is false and `handled == generation`.
  5. The consumer loads into A, which is the prepare renderer.
  6. The first binaural block has `activeIdx (B) != prevActiveRendererIdx_ (A)`, so it starts a crossfade from A.
  7. The new WR-01 check is skipped because `handled == generation`. The load is not counted.
  8. A was never marked in use. The loader is therefore writing a renderer the audio thread starts reading, with no count and no Debug assert. If the load is still in flight, it is a data race.
- The `RenderEngine.h` comment on `isPrepareRendererFree()` says "True from the start and after prepare()". That is wrong for a pending swap, and for a legacy fade that is running when `prepare()` is called. `rendererXfadeActive_` is left true in that case.
- If `handled` were left alone instead, the first block after the prepare would count a false positive. `prepare()` calls `setProfile` on both renderers, which bumps their mutation serials, and the serial snapshot is older than that.
- `ProfileSwitchTests.cpp` has no test that does swap, then `prepare()`, then a binaural block.

**Fix:** Make the claim in the `prepare()` comment true on every path. Move the reset out of the `hrtfEverRequested_` branch, or duplicate it, so it runs unconditionally:
```cpp
// prepare() settles every legacy swap, so the first block must not fade from the replaced renderer.
rendererXfading_ = false;
rendererXfadeSamplesDone_ = 0;
rendererXfadeLengthSamples_ = 0;
rendererXfadeActive_.store (false, std::memory_order_release);
prevActiveRendererIdx_ = activeIdx;
```
Keep the `forceReload_` and loader restart inside `if (hrtfEverRequested_)`. Add a test: legacy swap, `prepare()`, then a binaural block. It should assert that no crossfade starts, or alternatively that a load made after `prepare()` is counted.

### WR-02: `isPrepareRendererFree()` reads the two conditions in the wrong order, so it can return true while a crossfade from the replaced renderer has just started

**File:** `include/SpatialCore/Engine/RenderEngine.h:505-509`

**Issue:**
- The function evaluates `!rendererXfadeActive_` first and `handled == generation` second.
- A poller on the message thread, or any other thread, can read the fade flag as false before block 1 after the swap. Block 1 then starts the crossfade from the replaced renderer. It stores `rendererXfadeActive_ = true`, and at its end stores `handled = generation`. The poller then reads `handled == generation` as true. The result is true while the crossfade is reading the replaced renderer for at least 4096 samples.
- The window is narrow. It needs the poller to be preempted between the two loads for the length of a block. But the answer is the one the contract rests on, and the fix costs nothing.
- The reverse order is correct. Once the poller sees `handled == generation` (an acquire load that pairs with the release store at the end of the block), the fade-start store in the same block happens-before it. A later read of the fade flag then sees true or a later value.

**Fix:** Read the generation pair first:
```cpp
bool isPrepareRendererFree() const noexcept
{
    if (swapGenerationHandled_.load (std::memory_order_acquire) != swapGeneration_.load (std::memory_order_acquire))
        return false;
    return ! rendererXfadeActive_.load (std::memory_order_acquire);
}
```
Add a comment that the order is load-bearing.

## Info

### IN-01: The `[hrtf-switch][ui-timer]` distinct-profile check fails under machine load

**File:** `tests/Engine/ProfileSwitchTests.cpp:2289`
**Issue:** Added by the orchestrator from a verified test run. The whole `[tsan]` subset, run while other builds and an agent loaded the machine, failed `CHECK( result.activeSeen.size() >= 3 )` with `2 >= 3` (zero ThreadSanitizer warnings, 0 non-finite). Run alone three times it passed (3, 4, 3 distinct profiles). The dwell phase changes the wanted profile on a fixed tick count (`12 * timeoutScale()` ticks), so when a load takes longer than the dwell the profile is superseded before the audio thread claims it, and the count of distinct active profiles depends on machine speed. The local gate is the only gate, so a load-dependent failure in it is a false alarm the user cannot judge.
**Fix:** Make the dwell phase wait for each wanted profile to become active (or for the loader to go idle, bounded by a scaled timeout) before moving to the next one, instead of a fixed tick count, while keeping the 30 Hz `setHRTFProfile` calls during the dwell and the storm phase unchanged.

---

_Reviewed: 2026-10-09_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
