---
phase: 05-realtime-safety-hardening
verified: 2026-10-06T04:04:16Z
status: passed
score: 5/5 must-haves verified
covered_files:
  - ".claude/skills/spatialcore-architecture/spatialcore-architecture.md"
  - ".gitignore"
  - "CLAUDE.md"
  - "README.md"
  - "docs/integration-guide.md"
  - "examples/demo/DemoComponent.h"
  - "include/SpatialCore/Binaural/BinauralRenderer.h"
  - "include/SpatialCore/Binaural/HRTFDatabase.h"
  - "include/SpatialCore/Binaural/SharedFFTCache.h"
  - "include/SpatialCore/Engine/RenderEngine.h"
  - "include/SpatialCore/Trajectory/TrajectoryEngine.h"
  - "include/SpatialCore/UI/SpatialMapComponent.h"
  - "src/Algorithms/AmbisonicsAlgorithm.cpp"
  - "src/Binaural/BinauralRenderer.cpp"
  - "src/Binaural/HRTFDatabase.cpp"
  - "src/Binaural/PartitionedConvolver.cpp"
  - "src/Engine/RenderEngine.cpp"
  - "src/Trajectory/TrajectoryEngine.cpp"
  - "src/UI/SpatialMapComponent.cpp"
  - "tests/Binaural/BinauralRendererTests.cpp"
  - "tests/Binaural/SharedFFTCacheTests.cpp"
  - "tests/CMakeLists.txt"
  - "tests/Engine/MultiInstanceTests.cpp"
  - "tests/Engine/ProfileSwitchTests.cpp"
  - "tests/Engine/RealtimeSafetyTests.cpp"
  - "tests/Engine/RenderEngineTests.cpp"
  - "tests/Support/RealtimeGuard.h"
  - "tests/Support/Watchdog.h"
  - "tests/Support/rtguard/RTGuard.cpp"
  - "tests/Trajectory/TrajectoryTests.cpp"
  - "tests/UI/MultiMapTests.cpp"
covered_digest: "v2:sha256:971c10e0a0f2eeef571f37557a946373064b9d3f49ee1e9b3b15ea9e9c5b44f6"
behavior_unverified: 0
overrides_applied: 0
re_verification: false
coincidental_reliance_items:
  - truth: "Engine A renders bit-identically alone and beside a profile-switching engine B (OSD#96 isolation, 05-06)"
    reason: undeclared-precondition
    harden: "The test never asserts engine A's profile 2 actually became active. waitForHRTFProfileIdle() returns false on timeout and its result is ignored at MultiInstanceTests.cpp:143, 178, 189, 300. If the load timed out, A would render the Simple path in both runs and the test would pass vacuously (WR-05). Assert getHRTFProfileStatus().activeProfile == 2 on the test thread."
human_verification:
  - test: "Listen to (or plot) a jumping trajectory shape (Circle, Figure8, Helix, Spiral) read by a thread that is not the one ticking, at 60 Hz tick and speed 4"
    expected: "A mixed reading (azimuth from tick N, elevation from tick N+1) is judged inaudible or acceptable for those shapes, whose own per-tick step reaches about 180 degrees azimuth or 0.5 distance"
    why_human: "05-04 recorded this as human_judgment: true (coverage D5). The race is gone and every value is published whole, but whether a reading that combines two consecutive ticks is audible is a perceptual call no test can make. The [trajectory][step] bounds are too loose to say anything about it (WR-04)."
  - test: "Load several plugin instances that link SpatialCore in a real DAW on the same project and play audio through all of them while opening and closing their editors"
    expected: "No crash and no buzzing, in particular none of the CoreGraphics glyph crash of OpenSpatialDelay#131"
    why_human: "ROADMAP criterion 4 is proven in-process on macOS only (eight engines on eight threads, sixteen live maps sharing one face set). The phase deliberately does not claim a real-DAW run (D-05) or Windows/Linux (D-13); 05-07 does not claim the real-DAW CoreGraphics crash path. A real host can load plugins in ways an in-process test cannot."
---

# Phase 5: Realtime Safety Hardening Verification Report

**Phase Goal:** Nothing reachable from `processBlock` allocates, locks, or races, proven in a Release build with tools.
**Verified:** 2026-10-06T04:04:16Z
**Status:** human_needed
**Re-verification:** No, initial verification

No ROADMAP success criterion fails. All five hold against the code and against test runs I did myself, not against the SUMMARY claims. The status is `human_needed` because two items cannot be settled by a test: the audibility of a mixed trajectory reading, and a real-DAW multi-instance run, which the phase deliberately did not claim. Six open review warnings are recorded below as WARNING-level; none defeats a must-have.

## Goal Achievement

### Observable Truths (ROADMAP Success Criteria)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | `renderSourceBuffers` and `updateSourceHRIR` cannot allocate in Release (SpatialCore#19) | ✓ VERIFIED | `src/Binaural/BinauralRenderer.cpp`: the only `resize` calls are in `ensureScratchCapacity()` (lines 28/30), reached from `prepare()` and `setProfile()`. The audio-side guards (`:204`, `:257`) call `scratchFits()`, which never resizes: it counts, `jassertfalse`s in Debug only, and returns false so the caller returns early. In `RenderEngine.cpp` the only `resize` calls (566-578) are in `prepare()`. An oversized host block is rendered in pieces of at most `preparedMaxBlock_` through `renderDirectBinauralHRTFChunk` (`:1168`, `:1188`) and counted at `:746-747`. I ran `./build-release/tests/SpatialCoreTests "[rt]"` myself: 46 cases, 2,514 assertions, all passed. The `[canary]` case passed in the same run, so the zero counts are meaningful. |
| 2 | `finalAz_`/`finalEl_`/`finalDist_` are no longer plain floats read across threads; clean under TSan (SpatialCore#18) | ✓ VERIFIED | `TrajectoryEngine.h:130-135`: `static_assert (std::atomic<float>::is_always_lock_free)`, then `std::atomic<float> finalAz_/finalEl_/finalDist_[kMaxObjects]` and `std::atomic<bool> active_`. `TrajectoryEngine.cpp:60-98`: each value is computed into a local (Random azimuth wrapped, elevation and distance clamped), stored once relaxed, then `active_` is published with a release store. `isActive` reads with acquire. No mutex, spinlock or seqlock. I ran `build-tsan/tests/SpatialCoreTests "[trajectory][threads]"`: exit 0, 0 `WARNING: ThreadSanitizer` lines. |
| 3 | `setProfile()` from a UI timer during audio processing does not race on the libmysofa handle | ✓ VERIFIED (on the engine-owned `setHRTFProfile` route; legacy route carries WARNINGs WR-01/WR-02) | Engine-owned loader plus a one-slot atomic mailbox; the audio thread claims the renderer at a block boundary. `[hrtf-switch][ui-timer][tsan]` drives a 30 Hz timer thread against a rendering thread with all three engine flags set. 05-08 records it as clean under TSan with 910 ticks, 51,921 blocks and 4 distinct active profiles. The full `[tsan]` subset and whole suite are recorded at 0 warnings. The legacy route (`swapActiveRenderer`, `getBinauralRenderer`) is kept, documented, and watched by two misuse counters (D-09). It is detection only, not prevention, and WR-01/WR-02 show weaknesses in that detection and its documented contract. See Warnings. |
| 4 | Several plugin instances render concurrently without crashing or buzzing (SpatialCore#12; OSD#96 and OSD#131) | ✓ VERIFIED (in-process, macOS) | `MultiInstanceTests.cpp`: `[multi][isolation]` (engine A bit-identical alone and beside a profile-switching engine B on another thread) and `[multi][stress]` (eight engines on eight threads, twice). `SharedFFTCacheTests.cpp`: 4 cases, including 16 threads released together on orders 14 and 15. `MultiMapTests.cpp`: sixteen live `SpatialMapComponent`s share one face set and paint identically; 200-round create/paint/destroy churn beside four live maps. I ran in `build-tsan`: `"[fftcache],[multi][isolation]"` (5 cases, 40 assertions, exit 0, 0 warnings) and `SpatialCoreUITests "[ui][multi]"` (2 cases, exit 0, 0 warnings). Caveat: the isolation test has an undeclared precondition (WR-05). The real-DAW path is routed to human verification. |
| 5 | A TSan plus allocation-detector pass over the `processBlock` path reports zero allocations, zero locks, zero races | ✓ VERIFIED (macOS only) | Release `[rt]`: 46 cases, 2,514 assertions, passed (my run). The trap library (`tests/Support/rtguard/RTGuard.cpp`, an `__interpose` pthread tuple set plus a malloc logger) is canary-checked against a deliberate malloc, new, `CriticalSection` and `std::mutex` in the same build. It is not linked into `SpatialCore` or `SpatialCoreUI`. The 23-format matrix, 6 profiles, block sizes, rates, all 12 algorithm indices, 13 trajectory shapes and format and profile switches each have their own cold-process `TEST_CASE`. The `[rt][rule]` source-scan freezes the lock inventory. The whole-suite TSan run is recorded in 05-08 at exit 0, 0 warnings (386 cases, 1,938 s). I did not re-run that 32 minute run; I spot-checked the subsets above. The orchestrator independently ran Release ctest at 402/402. |

**Score:** 5/5 roadmap truths verified (0 present-but-behavior-unverified).

The plan-level truths (05-01 to 05-08 frontmatter) restate or refine the five above. Checked beyond the roadmap contract: the NFC fix is in place (05-01); the chunked HRTF render and the `getOversizedBlockCount()` counter exist (05-02); `[rt][rule]` and the matrix cases exist (05-03); `build-tsan/` is git-ignored and builds both executables (05-04); the legacy-route counters exist (05-05); `SharedFFTCache.h` documents the D-08 contract (05-06); the read-only UI test accessors exist and no pixel changed (05-07).

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `tests/Support/rtguard/RTGuard.cpp` | malloc logger plus pthread interposers, test-only | ✓ VERIFIED | Present, in the file list and linked only into `SpatialCoreTests` |
| `tests/Engine/RealtimeSafetyTests.cpp` | canary, matrix, rule | ✓ VERIFIED | 46 `[rt]` cases pass in Release |
| `src/Binaural/BinauralRenderer.cpp` | `scratchFits`, no audio-thread resize | ✓ VERIFIED | See truth 1 |
| `src/Engine/RenderEngine.cpp` | chunked HRTF render, oversized counter, in-place NFC | ✓ VERIFIED | See truth 1 |
| `include/SpatialCore/Trajectory/TrajectoryEngine.h` | atomic finals, lock-free `static_assert` | ✓ VERIFIED | See truth 2 |
| `tests/Engine/MultiInstanceTests.cpp`, `tests/Binaural/SharedFFTCacheTests.cpp`, `tests/UI/MultiMapTests.cpp` | VERIFY-02 proof | ✓ VERIFIED | Exist, wired into `tests/CMakeLists.txt`, pass under TSan |
| `build-tsan/` tree and `.gitignore` entry | third build tree | ✓ VERIFIED | Present, both executables built and current |
| `README.md`, `CLAUDE.md`, `docs/integration-guide.md`, skill, `PROJECT.md` | docs describing what shipped | ⚠️ PARTIAL | Present and updated. WR-03 and IN-03 show the guard docs overstate behavior (see Warnings). |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `renderDirectBinauralHRTF` | `renderDirectBinauralHRTFChunk` | loop over pieces of at most `preparedMaxBlock_` | WIRED | `RenderEngine.cpp:1168-1188` |
| `updateSourceHRIR` / `renderSourceBuffers` | `scratchFits` | early return on miss | WIRED | `BinauralRenderer.cpp:204`, `:257` |
| `TrajectoryEngine::tick` | `getFinal*` / `isActive` | relaxed stores, then release store paired with acquire load | WIRED | `TrajectoryEngine.cpp:83-98`, `TrajectoryEngine.h:110-113` |
| `ScopedRealtimeTrap` | `renderBlock` | armed around each call, inputs filled disarmed | WIRED | canary and matrix cases pass |
| `[tsan]` tag | TSan gate | tag on hrtf-switch, sc16, sc17, trajectory-threads, fftcache, multi | WIRED | 40 cases per 05-08. README describes the set more narrowly than the tags select (IN-05). |

### Data-Flow Trace (Level 4)

Not applicable. The phase delivers realtime-safety invariants and tests, not rendered data sources.

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| Allocation and lock trap across the whole audio path | `./build-release/tests/SpatialCoreTests "[rt]"` | 46 cases, 2,514 assertions, all passed | ✓ PASS |
| Trap is not blind | `... "[rt][canary]" -s` | 3 assertions passed | ✓ PASS |
| Trajectory finals race-free | `build-tsan/.../SpatialCoreTests "[trajectory][threads]"` | exit 0, 0 TSan warnings | ✓ PASS |
| SharedFFTCache and OSD#96 isolation under TSan | `... "[fftcache],[multi][isolation]"` | 5 cases, exit 0, 0 warnings | ✓ PASS |
| Sixteen maps share faces under TSan | `build-tsan/.../SpatialCoreUITests "[ui][multi]"` | 2 cases, exit 0, 0 warnings | ✓ PASS |
| Binaries current | `cmake --build build-release`, `cmake --build build-tsan` | nothing to rebuild | ✓ PASS |

Not re-run by me: the 17 minute `[tsan]` subset and the 32 minute whole-suite TSan run (taken from 05-08's logged results, exit 0 and 0 warnings), and Debug suites. The orchestrator ran Release ctest at 402/402.

### Probe Execution

No `probe-*.sh` scripts are declared by the phase or present in the repository. SKIPPED.

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|-------------|-------------|--------|----------|
| RTSF-01 | 05-02, 05-08 | `BinauralRenderer` cannot allocate on the audio thread in Release | ✓ SATISFIED | truth 1 |
| RTSF-02 | 05-04, 05-08 | TrajectoryEngine finals safe to read from the audio thread | ✓ SATISFIED | truth 2 |
| RTSF-03 | 05-05, 05-08 | HRTFDatabase access to the libmysofa handle is thread-safe | ✓ SATISFIED on the `setHRTFProfile` route. The legacy route is contract-plus-detection only (WR-01/WR-02). | truth 3 |
| RTSF-05 | 05-01, 05-02, 05-03, 05-04, 05-08 | The whole audio path is lock-free and realtime-safe | ✓ SATISFIED (macOS scope) | truth 5 |
| VERIFY-02 | 05-06, 05-07, 05-08 | SharedFFTCache safe across concurrent plugin instances | ✓ SATISFIED in-process, macOS | truth 4 |

All five IDs from PLAN frontmatter (`requirements:` in 05-01 through 05-08) appear in REQUIREMENTS.md, are ticked `[x]`, and are marked Complete in the traceability table. RTSF-04 is closed as a non-finding. No orphaned requirements: the traceability table maps no other ID to Phase 5.

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| (31 changed non-planning files) | n/a | `TBD`, `FIXME`, `XXX` markers | none found | Debt-marker gate clear |
| `src/`, `include/` changed files | n/a | `TODO`, `HACK`, `PLACEHOLDER` | none found | n/a |

### Code Review Findings, weighed against the must-haves

0 critical, 6 warnings, 7 info, all disposition `open`. None of them falsifies a ROADMAP criterion. They are recorded here for the developer's decision.

| ID | Effect on a must-have | Severity here |
|----|-----------------------|---------------|
| WR-01 (stale in-use marks give a false positive in the legacy flow when no block renders) | Affects only the misuse detector on the legacy route. Does not touch the engine-owned route or any audio-path allocation, lock or race. | ⚠️ WARNING |
| WR-02 (documented legacy flow can still load into the renderer about to crossfade from; `isRendererCrossfadeActive()` is false between the swap and the next block) | Real residual race window on the legacy route, and the documented contract is insufficient. OpenSpatialDelay uses this route today. The roadmap's criterion 3 is proven on `setHRTFProfile`, which D-09 chose, so I do not call it FAILED. It should be closed before OSD adopts this Core version. | ⚠️ WARNING |
| WR-03 (docs say the scratch guard "keeps the last good sound", but a skipped `renderSourceBuffers` outputs silence) | Docs contradict the code in four places. The safety property (no allocation) holds. | ⚠️ WARNING |
| WR-04 (`[trajectory][step]` bounds are 179 / 180 / 0.51, not per shape; the header says "pinned per shape") | RTSF-02's race-freedom is unaffected. The 05-04 plan truth that mixed-read size is "measured and pinned" is only weakly met, and the header overstates it. It also leaves the human audibility item with no usable bound. | ⚠️ WARNING |
| WR-05 (isolation test can pass vacuously; I confirmed `waitForHRTFProfileIdle` results are ignored and `activeProfile` is never asserted in `MultiInstanceTests.cpp`) | Weakens the OSD#96 regression truth to "passes if the load happens". 05-06 recorded a mutation run (37,886 of 51,200 samples differ) showing the test does discriminate when the load works. Recorded as a coincidental-reliance item. | ⚠️ WARNING |
| WR-06 (public API change with no version bump; `CMakeLists.txt` is still `VERSION 1.1.0`, and `TrajectoryEngine`'s public members changed type) | Not a roadmap criterion. CLAUDE.md requires a minor or major bump before consumers move their pin. Decide before the OSD submodule bump. | ⚠️ WARNING |
| IN-01 to IN-07 | Doc rot, test-only seams public, guard-threshold wording, trap build-type wording, `[tsan]` description, `reset()` store order, NFC test checks a test-local formula. None affects a must-have. | ℹ️ Info |

### Human Verification Required

#### 1. Audibility of a mixed trajectory reading

**Test:** Run a jumping shape (Circle, Figure8, Helix or Spiral) at the 60 Hz tick, read from a thread other than the one ticking, and listen to or plot the resulting positions.
**Expected:** A reading that combines azimuth from one tick with elevation from the next is acceptable.
**Why human:** 05-04 flagged this as `human_judgment: true`. The per-tick step of these shapes reaches about 180 degrees azimuth or 0.5 distance, and no existing test bound is tight enough to judge it. The documented escape, if it is judged audible, is a per-object seqlock, not a mutex.

#### 2. Real-DAW multi-instance run

**Test:** Load several plugin instances in a real host, play audio through all, and open and close editors.
**Expected:** No crash and no buzzing, including none of the OSD#131 CoreGraphics glyph crash.
**Why human:** The proof is in-process, macOS (Apple Silicon) only. The phase states this scope (D-05, D-13) and 05-07 does not claim the real-DAW crash path. Windows and Linux fall to Phase 6.

### Gaps Summary

No gaps. Every ROADMAP success criterion is backed by code I read and test runs I executed. The Release allocation and lock pass is canary-checked, and the TSan spot-checks I ran are clean. Two human items remain (trajectory audibility, real-DAW run), which move the status to `human_needed`.

Recommended before Phase 6 or the OSD migration, none blocking:
1. Fix WR-02 (state the real legacy-flow condition, or have `swapActiveRenderer()` raise the crossfade flag) and WR-01 (do not cache in-use marks).
2. Fix the WR-03 wording in four places.
3. Tighten or reword WR-04's per-shape pin and add the `activeProfile == 2` assertion for WR-05.
4. Decide the WR-06 version bump.

---

_Verified: 2026-10-06T04:04:16Z_
_Verifier: Claude (gsd-verifier)_
