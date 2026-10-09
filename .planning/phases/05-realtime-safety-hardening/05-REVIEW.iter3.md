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
  info: 0
  total: 2
status: issues_found
---

# Phase 5: Code Review Report (re-review, iteration 2)

**Reviewed:** 2026-10-09
**Depth:** standard
**Files Reviewed:** 32
**Status:** issues_found

## Summary

This pass checked the 13 fix commits (`06256ca..HEAD`) against the code. I did not re-raise any earlier finding that is actually fixed.

Verified as correct:

- **IN-06 (`TrajectoryEngine::reset`).** The three values are stored relaxed, then `active_` is stored with release. This matches `tick()` and the header comment. `tick()`'s `shape == 0` early-out stores no values, so it is unaffected.
- **IN-07 (NFC stored-coefficient test).** The friend seam adds no public member and no layout change. The test formula matches `RenderEngine.cpp` at lines 1606-1620, and `prevNfcDistance` is initialised to -1, so the `REQUIRE (distance >= 0)` check is meaningful.
- **WR-04 (per-shape step bounds).** Every entry in `kBounds` is at or above the measured maximum in the comment table, and the table has 13 entries.
- **WR-05 (profile 2 assertion).** The profile is read on the worker thread, with no Catch assertions there, and asserted on the test thread.
- **WR-06, IN-04, IN-05, IN-03, IN-02, IN-01 and WR-03.** The doc and comment changes match the code. This covers the `max (block, 512, IR length)` threshold, the `[tsan]` tag list, the "trap armed in any non-sanitizer macOS build" wording, and the `ForTesting` list.

Two problems remain, both in the WR-01 / WR-02 legacy-route work.

## Warnings

### WR-01: `swapActiveRenderer()` clears the fade source's in-use mark while audio is running, so the detector misses the one misuse the contract text warns about

**File:** `src/Engine/RenderEngine.cpp:1853-1865`
**Issue:**
- The fix clears the old renderer's mark inside `swapActiveRenderer()` whenever `rendererXfadeActive_` is false. That is always the case in the window between the swap and the next block, which is exactly the window the new docs describe as dangerous. The next block reads that renderer as the crossfade source.
- With blocks running, the flow "swap, then load straight away into the old renderer" is now silently uncounted. `getMutationWhileInUseCount()` stays 0 while the loader writes into a renderer the audio thread is about to convolve. Before WR-01, the stale mark counted this.
- The new `[in-use]` WR-02 test pins that the flag reads false after the swap. It does not pin that a load in that window is counted. The WR-01 test covers the opposite case, with no block running, and asserts 0.
- The header comment (`RenderEngine.h` ~444-452) and `docs/integration-guide.md` still say a mutation of a renderer "the audio thread is using" is counted, "as of the last rendered binaural block or the last swap". A consumer who reads `getMutationWhileInUseCount() == 0` as proof of correct use gets a false negative here.
- The trade-off is real: a stale mark gives a false positive when audio is stopped, and clearing gives a false negative when audio runs. But the false negative is the more dangerous failure, because it hides a data race.

**Fix:** Do one of the following.
1. Have the audio thread publish a cheap relaxed `blocksRendered_` counter. Keep the old mark set at swap and clear it only if no block has rendered since the previous swap. That needs a second swap-side counter, or a timestamp of the last block compared against a threshold.
2. If the best-effort detector is accepted as is, state in the header comment, `CLAUDE.md` and the integration guide that a load made between `swapActiveRenderer()` and the next block is not detected. Add a `[in-use]` section that pins it, so the behaviour is intentional rather than accidental.

### WR-02: The "wait until the flag is seen true, then false" rule can never be satisfied while blocks run on a path that does not crossfade

**File:** `docs/integration-guide.md:359-365`, `CLAUDE.md` (thread-contract sentence in the DATA-01 paragraph), `include/SpatialCore/Engine/RenderEngine.h:444-474`
**Issue:**
- The rule is written as "wait until `isRendererCrossfadeActive()` has been seen true and then false". The only exception given is "the host is not calling `processBlock`".
- In the code, `rendererXfadeActive_` turns true only from `beginRendererFadeIfSwapped`, which is reached only on the binaural HRTF path (`renderDirectBinauralHRTF` and `renderBinauralWithProfileFade`). A legacy consumer that swaps while the engine renders a stereo variant, Ambisonics or surround format, or the Simple path (`useHRTF == false`), has blocks running but never sees the flag go true.
- A consumer that follows the sentence literally will stall its next load indefinitely in that state. In that state a load is in fact safe: after WR-01, those blocks leave both marks cleared.
- The `[in-use]` test comment says a swap is "the pinned reason for the documented rule", but only the binaural case is tested.

**Fix:** Reword the rule to cover this case. For example: "wait until the flag has been seen true and then false, or, if the engine is rendering a format or path that does not use the HRTF renderers (stereo variants, Ambisonics, surround, Simple), the next load is safe at once and the flag will not turn true until a binaural HRTF block runs." Alternatively, expose a small `isRendererPending()` style query that covers the swap-to-first-fade window, instead of asking consumers to infer it from the flag.

---

_Reviewed: 2026-10-09_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
