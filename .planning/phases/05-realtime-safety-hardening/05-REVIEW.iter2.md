---
phase: 05-realtime-safety-hardening
reviewed: 2026-10-06T00:00:00Z
depth: standard
files_reviewed: 31
files_reviewed_list:
  - CLAUDE.md
  - .gitignore
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
  warning: 6
  info: 7
  total: 13
status: issues_found
---

# Phase 5: Code Review Report

**Reviewed:** 2026-10-06
**Depth:** standard
**Files Reviewed:** 31
**Status:** issues_found

## Summary

Reviewed the Phase 5 diff (`39a764b^..HEAD`) with the surrounding code. Audio-thread allocation, lock and logging claims were checked against the code, the libmysofa and JUCE sources, and a run of the `[oversized]`, `[rt][rule]` and `[d10]` cases in the Release tree (all pass).

**What holds up.**
- The chunked HRTF render is correct:
  - It advances every per-sample pointer (`monoBuffers`, `tapFadeGainPerSample`, `distGainPerSample`).
  - It re-reads crossfade state per piece, so a fade that ends inside an oversized block is handled.
  - It resets the SC-20 snap after the first piece.
  - The Woodworth path is null-safe on `outL`/`outR`.
- `IIR::Coefficients::operator=(std::array)` does not allocate. JUCE's `assignImpl` reuses storage, and the placeholder's capacity is 8.
- The hex-float max-rE constants match `cos(k*pi/8)` as floats.
- The in-use mark ordering (relaxed clear, then release store of the free flag or crossfade flag) is correct for the engine-owned loader.
- `PartitionedConvolver::process` self-chunks, so the scratch guard cannot admit an unsafe block.
- libmysofa's `kd_nearest` in the pinned tree does not malloc.

No correctness blocker was found in the production render path. The warnings are in the new legacy-route misuse detection (false positives, and a documented flow that still has a race), doc claims that contradict the code, a vacuous pinned test, a vacuous-pass path in the isolation test, and a missing version bump for a public member type change.

## Warnings

### WR-01: In-use marks go stale when no block renders, so the documented legacy flow trips the detector (and `jassertfalse` in Debug)

**File:** `src/Engine/RenderEngine.cpp:870-883` (with `src/Binaural/HRTFDatabase.cpp:386-394`)
**Issue:** The marks are written only at the end of `renderBlock()` (`markRendererUse`). `swapActiveRenderer()` does not touch them, and nothing clears them when audio stops. Trace:
1. Blocks run, so renderer 0 is marked in use (active).
2. The consumer follows the documented flow: load into the prepare renderer (1), `setProfile`, `swapActiveRenderer()`. Active is now 1 and prepare is 0.
3. No block renders. The transport is stopped, the track is bypassed, or the host does not call `processBlock` while idle (some AU and Pro Tools paths).
4. The next profile change loads into `getPrepareRendererIndex()`, which is renderer 0. Its stale mark is still true, so `noteMutationIfInUse()` counts a "mutation while in use" and calls `jassertfalse`.

Nothing is in use at that moment, so this is a false positive. The tests miss it because `legacyLoadSwapRender` always renders 80 blocks after each swap.

The mark also stays on the active renderer for every non-binaural format. A legacy load into the active renderer while the engine renders Ambisonics or surround is flagged even though that renderer is untouched.
**Fix:** Do not cache the mark. Evaluate it at mutation time from live engine state:
```cpp
// RenderEngine: give each HRTFDatabase a cheap "in use?" probe instead of a stored bool, or
// have swapActiveRenderer() re-derive both marks itself.
void RenderEngine::swapActiveRenderer()
{
    noteLegacyRouteCall();
    const int newActive = prepareRendererIndex_;
    activeRendererIndex.store (newActive, std::memory_order_release);
    // Only a running crossfade keeps the old renderer in use; with no audio it is free.
    binauralRenderers[(size_t) newActive].hrtfDatabase.setInUseByAudioThread (true);
    binauralRenderers[(size_t) (1 - newActive)].hrtfDatabase.setInUseByAudioThread (false);
    prepareRendererIndex_ = 1 - prepareRendererIndex_;
}
```
Also gate `markRendererUse` on the block having taken the HRTF path. If a heuristic is wanted, add a `lastBlockSerial_` the check can compare.

### WR-02: The documented legacy flow can still load into the renderer the audio thread is about to crossfade from

**File:** `include/SpatialCore/Engine/RenderEngine.h:445-466`, `docs/integration-guide.md` (older-route paragraph), `CLAUDE.md` (HRTF profile switching paragraph)
**Issue:** The contract says to load only "once `isRendererCrossfadeActive()` is false". `swapActiveRenderer()` flips the active index at once, but `rendererXfadeActive_` is set only by `beginRendererFadeIfSwapped()` at the next block. Between the swap and that block, `isRendererCrossfadeActive()` is false while the old renderer is still the fade source (or is mid-render in the current block). A consumer that swaps and then immediately starts the next load (a 30 Hz timer easily does this) follows the documented rule and mutates a database the audio thread is reading. libmysofa lookups write `easy->fir`, which is the exact race the new HRTFDatabase comment describes. The detector may flag it, but the contract text is what consumers will follow, and it is insufficient.
**Fix:** State the real condition: after `swapActiveRenderer()`, wait until the crossfade has started and then ended (or until a block has rendered since the swap), not just until the flag reads false. Alternatively have `swapActiveRenderer()` raise `rendererXfadeActive_` itself, with the caveat that it then never clears when no block runs. Add a test that swaps and immediately re-loads with no block in between and pins the intended outcome.

### WR-03: Docs claim the scratch guard "keeps the last good sound", but the skipped `renderSourceBuffers` call outputs silence

**File:** `docs/integration-guide.md:409`, `CLAUDE.md` (Profile loader and mailbox bullet), `.claude/skills/spatialcore-architecture/spatialcore-architecture.md:140`, `include/SpatialCore/Binaural/BinauralRenderer.h:56-62`
**Issue:** The header says `renderSourceBuffers` "leaves that zeroed output", so a skipped call is a silent block (a dropout, and a click if a crossfade is running). `updateSourceHRIR` does keep the previous HRIR. "The last good sound is kept" is true only for the HRIR update and for filter state. A consumer reading the integration guide would expect continuity.
**Fix:** Reword to "the call is skipped: `renderSourceBuffers` outputs silence for that call and advances no state, `updateSourceHRIR` keeps the previous HRIR". Same edit in all four places.

### WR-04: The `[trajectory][step]` "pin" is vacuous and not per shape, contradicting the header

**File:** `tests/Trajectory/TrajectoryTests.cpp:647-693`, `include/SpatialCore/Trajectory/TrajectoryEngine.h:51`
**Issue:** The header says the mixed-read size is "pinned per shape". The test pins three global constants: `kMaxAzStepDeg = 179` (the largest value `unwrapAzimuthDelta` can return is 180), `kMaxElStepDeg = 180` (the full elevation range, so unfailable), and `kMaxDistStep = 0.51` (Spiral measures 0.506). Only the dist bound can catch a regression, and only by about 0.004. The smooth-shape claim in the comment block (at most 56.4 degrees, 32 degrees and 0.28) is not enforced. A change that doubles the per-tick step of Bounce or Heart would pass.
**Fix:** Pin per shape, using the measured table with a small margin:
```cpp
struct StepBound { int shape; float az, el, dist; };
constexpr StepBound kBounds[] = { { TrajShape::Bounce, 25.0f, 9.0f, 0.01f }, /* ... all 13 ... */ };
```
Alternatively, drop the "pinned per shape" wording.

### WR-05: The multi-instance isolation test can pass vacuously

**File:** `tests/Engine/MultiInstanceTests.cpp:143, 178, 189, 300`
**Issue:** `waitForHRTFProfileIdle()` returns `false` on timeout, and its result is ignored in `makeProfile2Engine`, `runSwitchingEngine` and `runStressThread`. If the load times out, for example under a slow sanitizer build, engine A renders the Simple path in both runs, so `alone == beside` and the test passes. It never checks that profile 2 became active. The `silent` check does not catch this, because Simple output is not silent.
**Fix:** Return the idle result, or the final `getHRTFProfileStatus().activeProfile`, out of the watchdog lambda (no Catch assertions inside it) and assert afterwards:
```cpp
state->aActive = a2->getHRTFProfileStatus().activeProfile;   // in the lambda
...
REQUIRE (state->aActive == 2);                               // on the test thread
```

### WR-06: Public API changes with no version bump (`TrajectoryEngine` member types, new `RenderEngine` surface)

**File:** `include/SpatialCore/Trajectory/TrajectoryEngine.h:127-134`, `CMakeLists.txt:4` (`VERSION 1.1.0`)
**Issue:** `finalAz_`, `finalEl_` and `finalDist_` are public members. They changed from `float[]` to `std::atomic<float>[]`. Reads and assignments still compile. Anything that takes the address as `float*`, binds a `float&`, `memcpy`s the array, or uses it in a `constexpr` context breaks. The release also adds public API (`getLegacyRouteWrongThreadCount`, `getOversizedBlockCount`, `getScratchTooSmallCount`, the `HRTFDatabase` counters) and makes `getPrepareRendererIndex`/`swapActiveRenderer` out-of-line, which changes the class layout and ABI. The project version stays at 1.1.0, and CLAUDE.md says breaking API changes need a major bump and features a minor one.
**Fix:** Decide the bump before consumers move their submodule pin. At least a minor bump for the new surface. Call out the `TrajectoryEngine` member type change in the release notes, or make the members private and keep the getters as the only access path.

## Info

### IN-01: Stale line-number references in the lock-inventory comment

**File:** `tests/Engine/RealtimeSafetyTests.cpp:1620-1621`
**Issue:** The comment cites `:499 :500` and `:612 :613`. The actual lines in `RenderEngine.cpp` are 502/503 and 621/622. Line-number comments in an allow-list rot on the next edit, and the count (13) is what the test enforces.
**Fix:** Cite function names only, or regenerate the numbers.

### IN-02: Test-only seams are public members of production classes

**File:** `include/SpatialCore/Binaural/BinauralRenderer.h:76-81`, `include/SpatialCore/UI/SpatialMapComponent.h:96-103`
**Issue:** `shrinkScratchForTesting`, `getSharedFaceIdentityForTesting` and `getSharedFaceConstructionCountForTesting` are callable by any consumer. `shrinkScratchForTesting` can make the audio path skip every block if misused. The headers justify keeping the class layout independent of build flags, and that is valid, but the methods themselves could sit behind a test-access hook.
**Fix:** Make the test a `friend` (a forward-declared `struct BinauralRendererTestAccess`) and drop the public methods, or keep them and note in the docs that consumers must not call them.

### IN-03: The guard threshold is not "the prepared size", so the guard docs are inaccurate

**File:** `include/SpatialCore/Binaural/BinauralRenderer.h:56, 67`, `src/Binaural/BinauralRenderer.cpp:21-31`
**Issue:** Scratch is `max(blockSize, 512, IR length)`, so `scratchFits` rejects only blocks longer than that. A block between the prepared size and 512 (or the IR length) passes, and the convolver self-chunks. The comments "a block longer than prepare() was given" and "only happens when the caller renders more samples than prepare() was given" are wrong. The engine is unaffected, since it chunks at `preparedMaxBlock_`.
**Fix:** Reword to "longer than the scratch (`max(prepared block, 512, loaded IR length)`)".

### IN-04: CLAUDE.md and README say the allocation trap is Release-only, but CMake enables it in any macOS non-sanitizer tree

**File:** `tests/CMakeLists.txt:72-77`, `CLAUDE.md` (Realtime-safety proof bullet), `README.md` ("Allocation and lock trap, Release")
**Issue:** The gate is `APPLE AND NOT CMAKE_CXX_FLAGS MATCHES "-fsanitize"`, with no build-type condition. Debug trees on macOS also arm the trap. The test code works around Debug (`kHRIRLookupTraces` skips the allocation assertions on HRIR-lookup paths), which confirms Debug runs these cases.
**Fix:** Either add the build-type condition the docs describe, or reword the docs to "macOS, any build type without a sanitizer; HRIR-lookup cases are asserted in Release only". The sanitizer check also reads only `CMAKE_CXX_FLAGS`, so a sanitizer set through `CMAKE_CXX_FLAGS_<CONFIG>` or `add_compile_options` would keep the trap. The canary then fails the case rather than giving a false zero, so this is a robustness note.

### IN-05: README describes the `[tsan]` subset more narrowly than the tags select

**File:** `README.md:150`
**Issue:** The README says the subset is "every `[hrtf-switch]`, `[sc16]` and `[sc17]` case and `[trajectory][threads]`". `[tsan]` is also on `[fftcache]`, `[multi]` and the misuse cases. The measured figures (40 cases, 992 s) presumably include them. The description and the tag set disagree.
**Fix:** List all the tagged groups, or make `[tsan]` match the sentence.

### IN-06: `TrajectoryEngine::reset` stores `active_` before the values, against its own comment

**File:** `src/Trajectory/TrajectoryEngine.cpp:147-150`, `include/SpatialCore/Trajectory/TrajectoryEngine.h:129`
**Issue:** The header says "active_ is stored with release after the three values". `tick()` follows that. `reset()` stores `active_ = false` first, then the three values. A reader can see `active == false` with either old or new values, which is harmless because the values are valid either way, but the comment is wrong for `reset()`.
**Fix:** Store the values first, then `active_` (release), or qualify the comment to say it describes `tick()`.

### IN-07: NFC bit-identity test checks a test-local copy of the formula, not the engine's stored coefficients

**File:** `tests/Engine/RealtimeSafetyTests.cpp:556-596`
**Issue:** `[ambi][nfc][d10]` recomputes the formula in the test and compares `Coefficients(...)` against `Coefficients = std::array{...}`. That proves JUCE's two construction paths agree. It does not prove `RenderEngine::renderAmbisonicsOutput` stores those values: if the engine's formula or clamps change, this test still passes. The hidden `SC_CAPTURE_FILE` capture covers a before/after comparison manually only.
**Fix:** Add a test seam, or a render-level golden comparison of the six Ambisonics formats' output against a stored hash. The existing capture tool can generate it.

---

_Reviewed: 2026-10-06_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
