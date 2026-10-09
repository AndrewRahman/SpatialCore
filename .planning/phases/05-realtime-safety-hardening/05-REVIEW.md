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
  warning: 0
  info: 0
  total: 0
status: clean
---

# Phase 5: Code Review Report (re-review, iteration 4)

**Reviewed:** 2026-10-09
**Depth:** standard
**Files Reviewed:** 32
**Status:** clean

## Summary

This pass checked the three fix commits from the previous review (d1d3a82, dc1f45c, 8865895) against the code, then re-checked that the rest of the phase still holds. No real defect remains.

### d1d3a82 (WR-01): unconditional crossfade reset in `prepare()`

The reset is correct on both routes.

- **Placement.** It sits after `readyRenderer_` is cleared and `rendererFree_` is recomputed from `activeIdx`. It runs before the `hrtfEverRequested_` block, which still owns `forceReload_` and the loader restart (`src/Engine/RenderEngine.cpp:603-613`).
- **Marks.** The in-use marks of both renderers are cleared at the top of `prepare()`, so the source of a running crossfade is unmarked. `rendererFree_` frees it. Nothing is left pointing at a fade that no longer exists.
- **Legacy route.** `prevActiveRendererIdx_ = activeIdx` means the first binaural block after a settled swap starts no fade. That agrees with `swapGenerationHandled_` having been settled. The legacy consumer's prepare renderer index stays the replaced renderer, as the new test asserts.
- **A fade running when `prepare()` is called.** The fade is cancelled. Both renderers are re-prepared in the same call, so their convolver state is reset. A fade from them could not have been coherent anyway.
- **Engine-owned route.** The reset is the same code that used to run inside `if (hrtfEverRequested_)`, so that route is unchanged. A route that never called `setHRTFProfile` now gets it too.
- **Loader running during `prepare()`.** The loader is stopped and destroyed first. `stopThread(15000)` joins it before any plain member is touched, and the destroyed loader's `run()` ends with `releaseHeld()`. The mailbox is discarded and `rendererFree_` recomputed. The new loader is built afterwards and reads `rendererMeta_` for the active renderer. The serial bump makes it re-serve the request. Nothing the loader could write is shared with the plain members reset here, and the audio thread is not running during `prepare()` by contract.
- **Poller view.** `swapGenerationHandled_` is stored at the top of `prepare()` and the fade flag is cleared later. A concurrent `isPrepareRendererFree()` therefore sees false in between, which is the safe direction.
- **Tests.** The two new sections hold up. In "a swap settled by prepare()", `legacyLoadSwapRender` returns the swapped-to renderer, so `wasActive` is the replaced one after the second swap, and `getPrepareRendererIndex() == wasActive` is the right assertion. In "a legacy crossfade that is running when prepare() is called", the flag drops and the next block does not pick the fade up again.

### dc1f45c (WR-02): load order in `isPrepareRendererFree()`

The order is correct.

- **Happens-before.** An acquire load of `swapGenerationHandled_` that sees the value a block stored at its end (release) makes that block's fade-start store visible. The fade-flag read after it then sees true or a later value.
- **Swap between the two reads.** If a swap lands between the two loads, the generation read is newer than the handled read, so the answer is false. That is the safe direction.
- **Header comment.** The comment states that the order is load-bearing. "True from the start and after prepare()" is now accurate.

### 8865895 (IN-01): ui-timer test

The test still proves what it claims, and it cannot hang.

- **Same-profile requests do not live-lock.**
  - The loader tests `requestSerial_ != serial` after the load. A same-profile request every 33 ms makes it return false and loop.
  - On the second pass `heldProfile == target`, so it skips the load and publishes at once.
  - The change from the old dwell is that the 30 Hz call is made on every tick of a step, but the old dwell did that too.
- **Bounds.** Each step is bounded by `40 s * scale`. The worst case is 4 steps, a storm of 1 s, `waitForHRTFProfileIdle` at 30 s and a settle of 10 s, which is about 201 s times the scale. The watchdog is `300 * timeoutScale()`, and it calls `abort()` on expiry rather than blocking. Both join paths are bounded, and the render thread stops after the timer thread.
- **Cross-thread data.** `activeSeen` and `ticks` are written by the timer thread and read after `join()`.
- **Count checks.** The distinct-profile count of at least 3 is satisfied by the initial Simple state plus 2, 3 and 5 on the full build. The count of at least 2 on the one-profile build is satisfied by Simple plus 5. A step that times out still fails the check loudly instead of passing quietly.
- **Run result.** `SpatialCoreTests "[in-use],[ui-timer]"` on the existing Debug build passes: 187 assertions in 3 test cases. The ui-timer case reports 103 ticks, 6680 blocks, 4 distinct active profiles and 0 non-finite samples. `[hrtf-switch][legacy]` also passes. The Debug "Leaked objects: FFT" line at process exit is the deliberate process-global `SharedFFTCache` (D-08). The exit code is 0, and it is not new in this phase.

### Rest of the phase

- **Audio-side bookkeeping.** The added audio-side work from the detector is still only atomic loads and stores plus one relaxed `fetch_add`, with a `static_assert` on lock-freedom. There is no allocation, lock or logging in `renderBlock`.
- **Swap ordering.** `swapActiveRenderer()` publishes the serial snapshot, then the index, then the generation, in release order. Each block reads the generation before the index.
- **Docs.** `CLAUDE.md`, `docs/integration-guide.md` and the `spatialcore-architecture` skill describe the `isPrepareRendererFree()` rule consistently with the code. They state that a legacy load after a non-HRTF block is allowed and that the first binaural block fades from the reloaded renderer. That is documented behaviour, not a defect.

---

_Reviewed: 2026-10-09_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
