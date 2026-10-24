---
phase: 05-realtime-safety-hardening
fixed_at: 2026-10-08T23:05:00Z
review_path: .planning/phases/05-realtime-safety-hardening/05-REVIEW.md
iteration: 1
findings_in_scope: 13
fixed: 13
skipped: 0
status: all_fixed
---

# Phase 5: Code Review Fix Report

**Fixed at:** 2026-10-08
**Source review:** .planning/phases/05-realtime-safety-hardening/05-REVIEW.md
**Iteration:** 1

**Summary:**
- Findings in scope: 13 (6 warnings, 7 info; fix_scope `all`)
- Fixed: 13
- Skipped: 0

**Isolation:** edits and commits were made directly in this checkout, which is already an isolated git worktree (`AndrewRahman/gsd-phase-5-discuss`). No nested worktree or recovery sentinel was created, because the run was told never to leave this worktree and a nested one would have had no build trees.

**Verification ran in this worktree** (the main checkout was not used):
- Debug: `cmake --build build -j8`, `build/tests/SpatialCoreTests` passed (387 test cases, 328925 assertions), `build/tests/SpatialCoreUITests` passed (16 test cases, 150 assertions). Both print the existing "Leaked objects detected: FFT" line at exit. That is the process-global `SharedFFTCache` singleton, kept for the life of the process on purpose (D-08), and it is not a failure.
- Release: `cmake --build build-release -j8`, `ctest --test-dir build-release/tests -j8` passed 403 of 403.
- Realtime-safety: `build-release/tests/SpatialCoreTests "[rt]"` passed (46 test cases, 2514 assertions).
- `build-tsan` was not run.

## Fixed Issues

### WR-01: In-use marks go stale when no block renders, so the documented legacy flow trips the detector (and `jassertfalse` in Debug)

**Files modified:** `src/Engine/RenderEngine.cpp`, `tests/Engine/ProfileSwitchTests.cpp`
**Commit:** ba92b89
**Applied fix:** `swapActiveRenderer()` now re-derives both in-use marks itself. The new active renderer is marked. The old one is cleared unless a crossfade is still running. `markRendererUse` at the end of `renderBlock()` now marks a renderer only when the block took the binaural path (not a stereo variant, and `engineSelectsHRTF` or `useHRTF`). A block in any other format leaves both unmarked. All stores are still relaxed atomics, with no allocation, lock or logging. Two new `[in-use]` sections cover a load after a swap with no block between, and a load into the active renderer while a stereo-variant block renders. Both fail without the fix.
**Status note:** logic change, so this is flagged "fixed: requires human verification". In particular, a swap followed at once by a load into the old renderer is now deliberately not counted. The detector cannot tell "no audio is running" from "the next block is about to start", and WR-02 covers that window with the documented rule.

### WR-02: The documented legacy flow can still load into the renderer the audio thread is about to crossfade from

**Files modified:** `include/SpatialCore/Engine/RenderEngine.h`, `docs/integration-guide.md`, `CLAUDE.md`, `tests/Engine/ProfileSwitchTests.cpp`
**Commit:** 1a0fe87
**Applied fix:** I took the review's first option and corrected the contract text in all three places. After `swapActiveRenderer()`, the consumer must wait until `isRendererCrossfadeActive()` has been seen true and then false before the next load. With no blocks running the flag never turns true, but then a load is safe. The `getBinauralRenderer` thread-contract comment now says the in-use mark reflects the last rendered binaural block or the last swap. A new `[in-use]` section pins the window. Right after a swap the flag reads false. After one block it reads true, a load into the fade source is counted once, and after the fade the same renderer is free. I did not make `swapActiveRenderer()` raise `rendererXfadeActive_` itself, because the review warns it would never clear when no block runs.

### WR-03: Docs claim the scratch guard "keeps the last good sound", but the skipped `renderSourceBuffers` call outputs silence

**Files modified:** `docs/integration-guide.md`, `CLAUDE.md`, `.claude/skills/spatialcore-architecture/spatialcore-architecture.md` (CRLF kept), `include/SpatialCore/Binaural/BinauralRenderer.h`
**Commit:** f65a1b2
**Applied fix:** All three docs now say the call is skipped, `renderSourceBuffers` outputs silence for that call and advances no state, and `updateSourceHRIR` keeps the previous HRIR. The `renderSourceBuffers` header comment now says "outputs silence for that block" explicitly.

### WR-04: The `[trajectory][step]` "pin" is vacuous and not per shape, contradicting the header

**Files modified:** `tests/Trajectory/TrajectoryTests.cpp`
**Commit:** f8241ce
**Applied fix:** The three global constants are replaced by a `kBounds` table with one az/el/dist bound per shape. Each is the measured maximum from the comment table plus a margin of about 2%, rounded up to 0.5 degrees or 0.01. A `static_assert` requires 13 entries, and an axis a shape does not move is bounded at 0.01. The comment now describes per-shape bounds. The header's "pinned per shape" wording is now true.

### WR-05: The multi-instance isolation test can pass vacuously

**Files modified:** `tests/Engine/MultiInstanceTests.cpp`
**Commit:** c9d3472
**Applied fix:** `IsolationState` gained `aActiveAlone` and `aActiveBeside`. The watchdog lambda stores `getHRTFProfileStatus().activeProfile` after each 100-block render, with no Catch assertions inside the lambda. The test thread then does `REQUIRE (... == 2)` on both, so a timed-out load can no longer let two Simple-path renders compare equal. The idle result in `makeProfile2Engine` is explicitly discarded, with a comment saying the caller checks. The `waitForHRTFProfileIdle` calls in `runSwitchingEngine` and the stress thread are cleanup or pacing waits for engine B, so I left them unchanged.

### WR-06: Public API changes with no version bump (`TrajectoryEngine` member types, new `RenderEngine` surface)

**Files modified:** `CMakeLists.txt`, `docs/integration-guide.md`
**Commit:** 6bad816
**Applied fix:** I bumped the CMake project version from 1.1.0 to 1.2.0, a minor bump for the new public API. This follows the 10001af precedent, which bumped to 1.1.0 for `setAlgorithmIndex`. No tag was created. I also added a "Source-compatibility notes (SpatialCore 1.2.0)" paragraph to the integration guide. It lists the new API, says the `TrajectoryEngine` final-position members are now `std::atomic<float>` arrays (use the getters, not addresses, references or `memcpy`), and says `getPrepareRendererIndex()` and `swapActiveRenderer()` are now out of line, so consumers must rebuild together with SpatialCore. I did not make the members private, to avoid code churn. Whether a major bump is warranted for the member-type change is a maintainer call. I treated it as a documented source-compat note.

### IN-01: Stale line-number references in the lock-inventory comment

**Files modified:** `tests/Engine/RealtimeSafetyTests.cpp`
**Commit:** 70270a2
**Applied fix:** The comment now names the 13 RenderEngine.cpp sites by function and thread, with no line numbers. The per-function counts add up to 13, and the enforced count is unchanged.

### IN-02: Test-only seams are public members of production classes

**Files modified:** `include/SpatialCore/Binaural/BinauralRenderer.h`, `include/SpatialCore/UI/SpatialMapComponent.h`, `docs/integration-guide.md`
**Commit:** f076349
**Applied fix:** I took the review's "keep them and note that consumers must not call them" option. The `ForTesting` pattern is already used across the codebase (`setSharedHRTFFolderForTesting`, `setLoaderFailureForTesting`), and moving one method to a `friend` would be inconsistent. The three seams now say "not part of the consumer API: a plugin must never call it". The `shrinkScratchForTesting` comment also says it makes the audio path skip calls. The integration guide has a new "Test-only methods" paragraph.

### IN-03: The guard threshold is not "the prepared size", so the guard docs are inaccurate

**Files modified:** `include/SpatialCore/Binaural/BinauralRenderer.h`, `src/Binaural/BinauralRenderer.cpp`
**Commit:** 54c9552
**Applied fix:** The header and source comments now describe the threshold as the scratch size, `max (prepared block size, 512, loaded IR length)`. They also say a block between the prepared size and that threshold is accepted and the convolver chunks it itself.

### IN-04: CLAUDE.md and README say the allocation trap is Release-only, but CMake enables it in any macOS non-sanitizer tree

**Files modified:** `CLAUDE.md`, `README.md` (CRLF kept)
**Commit:** 18bbd8d
**Applied fix:** I took the review's reword option. The docs now say the trap is armed in any macOS build without a sanitizer, and that the HRIR-lookup cases assert allocations in Release only. The robustness note about `CMAKE_CXX_FLAGS_<CONFIG>` is not acted on. The review itself says the canary fails the case loudly rather than giving a false zero.

### IN-05: README describes the `[tsan]` subset more narrowly than the tags select

**Files modified:** `README.md` (CRLF kept)
**Commit:** 532d75e
**Applied fix:** The step 3 comment now lists every tag group the `[tsan]` subset selects: all `[hrtf-switch]` cases (including misuse and in-use), `[sc16]`, `[sc17]`, `[fftcache]`, `[multi]` and `[trajectory][threads]`. I checked the list against the `[tsan]` tags in `tests/`.

### IN-06: `TrajectoryEngine::reset` stores `active_` before the values, against its own comment

**Files modified:** `src/Trajectory/TrajectoryEngine.cpp`
**Commit:** 8c76987
**Applied fix:** `reset()` now stores the three values first and then `active_ = false` with release, the same order as `tick()` and the header comment. A reader that acquires `active_` now sees the reset position. The `[trajectory]` cases pass.
**Status note:** memory-ordering change, so flagged "fixed: requires human verification".

### IN-07: NFC bit-identity test checks a test-local copy of the formula, not the engine's stored coefficients

**Files modified:** `include/SpatialCore/Engine/RenderEngine.h`, `tests/Engine/RealtimeSafetyTests.cpp`
**Commit:** 6847a03
**Applied fix:** I added a test-access seam: `friend struct RenderEngineTestAccess;` in `RenderEngine`. It adds no public method and no layout change, and the tests define the struct. A new `[ambi][nfc][d10][stored]` case renders 40 blocks in each of the six Ambisonics formats. It then reads every object's stored NFC filter coefficients and the engine's last-update distance, recomputes the formula, and compares bit for bit for orders 1 to 6 across 12 objects (252 comparisons). A change to the engine's formula or clamps now fails it.

---

_Fixed: 2026-10-08_
_Fixer: Claude (gsd-code-fixer)_
_Iteration: 1_
