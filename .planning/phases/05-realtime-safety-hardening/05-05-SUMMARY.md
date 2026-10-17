---
phase: 05-realtime-safety-hardening
plan: 05
subsystem: testing
tags: [realtime-safety, hrtf, legacy-route, misuse-detection, threadsanitizer, std-atomic, lock-free, catch2]

requires:
  - phase: 05-realtime-safety-hardening
    provides: "Plans 05-01 to 05-04: allocation and lock traps, scratch guards, build-tsan tree and the [tsan] subset, SPATIALCORE_TEST_TIMEOUT_SCALE use"
  - phase: 03-binaural-defects-hrtf-packaging
    provides: "setHRTFProfile loader and mailbox (DATA-01), [golden] binaural fingerprints, [hrtf-switch] cases"
provides:
  - "RenderEngine::getLegacyRouteWrongThreadCount(): swapActiveRenderer() / getPrepareRendererIndex() called on the render thread while another thread is the writer are counted (every build) and assert in Debug (SC-17 rule)"
  - "HRTFDatabase in-use API: setInUseByAudioThread, isInUseByAudioThread, getMutationWhileInUseCount, noteMutationIfInUse; every public mutator and BinauralRenderer::setProfile counts once when the audio thread is using the database"
  - "RenderEngine marks the active renderer and the crossfade source at the end of every block (markRendererUse), clears the mark before every rendererFree_ release store and in prepare()"
  - "Written thread contracts in RenderEngine.h (legacy section), HRTFDatabase.h and BinauralRenderer.h"
  - "Five new [hrtf-switch] cases, all [tsan]: [misuse], two [misuse][in-use], [ui-timer], [legacy][threads]; build-tsan [hrtf-switch] is 25 cases, exit 0, 0 warnings"
affects: [05-06, 05-07, 05-08, RTSF-03, VERIFY-02, OpenSpatialDelay migration]

plan_head_before: ea034e731a4e43057350c8cfb6b56d4045b353f6
plan_head_after: 00cdd76379df8bea34460e26b3cc3587616c5bc7

actuals:
  tokens: 9190
  tasks: 3
  commits: 5

tech-stack:
  added: []
  patterns:
    - "Detect, never block: a lock-free counter plus a Debug jassertfalse on a contract violation; the offending call still runs (D-09, D-04)"
    - "In-use mark per resource, set by the audio thread at the end of a block and cleared BEFORE the release store that hands the resource to another thread"
    - "Misuse tests keep the first thread alive (idle) while the second runs, because a joined thread's id can be reused and the detectors compare thread ids"

key-files:
  created: []
  modified:
    - include/SpatialCore/Engine/RenderEngine.h
    - src/Engine/RenderEngine.cpp
    - include/SpatialCore/Binaural/HRTFDatabase.h
    - src/Binaural/HRTFDatabase.cpp
    - include/SpatialCore/Binaural/BinauralRenderer.h
    - src/Binaural/BinauralRenderer.cpp
    - tests/Engine/ProfileSwitchTests.cpp

key-decisions:
  - "Two detectors, no lock: a wrong-thread count on the two message-thread-only legacy calls, and a per-database in-use mark that counts a load or setProfile on a renderer the audio thread is using (RESEARCH F5 design). getBinauralRenderer() is not counted or asserted (it has no thread contract and a consumer may call it from processBlock)"
  - "The in-use mark is cleared in endRendererFade() BEFORE rendererXfadeActive_ drops and before the rendererFree_ release store, so a consumer that waits for the fade to end, and the engine's loader that polls the free flag, can never see a stale mark"
  - "HRTFDatabase gets a private closeHandle(); the destructor and internal paths use it so loadFromBinaryData / loadFromFile count exactly once although the handle is closed twice internally, and the destructor never counts"
  - "ROADMAP criterion 3 is proven on the setHRTFProfile route by a 30 Hz timer case (storm then dwell), not on the legacy route"

patterns-established:
  - "Sequenced two-thread misuse harness (runOnTwoThreads): first thread idles alive until the second has joined"
  - "BASE red run first for both detectors, then a D-07 mutation that the test must fail"

requirements-completed: [RTSF-03]

coverage:
  - id: D1
    description: "A swapActiveRenderer() or getPrepareRendererIndex() call on the render thread while another thread is the writer is counted; a single-thread harness is not"
    requirement: "RTSF-03"
    verification:
      - kind: unit
        ref: "tests/Engine/ProfileSwitchTests.cpp#Profile switch: a legacy swap or prepare-index call on the render thread is counted (D-09) (Debug, Release, build-tsan)"
        status: pass
    human_judgment: false
  - id: D2
    description: "A load, unload or setProfile on a renderer the audio thread is using is counted once per call; the documented legacy flow and the engine's own setHRTFProfile loader are never counted"
    requirement: "RTSF-03"
    verification:
      - kind: unit
        ref: "tests/Engine/ProfileSwitchTests.cpp#Profile switch: a load into the renderer the audio thread is using is counted, the documented flows are not (D-09)"
        status: pass
      - kind: unit
        ref: "tests/Engine/ProfileSwitchTests.cpp#Profile switch: the engine's own loader never trips the in-use detector (D-09)"
        status: pass
    human_judgment: false
  - id: D3
    description: "ROADMAP criterion 3: setHRTFProfile from a 30 Hz UI timer thread while another thread renders is race-free under ThreadSanitizer, with no non-finite sample and at least three distinct active profiles"
    requirement: "RTSF-03"
    verification:
      - kind: unit
        ref: "SPATIALCORE_TEST_TIMEOUT_SCALE=10 TSAN_OPTIONS=... ./build-tsan/tests/SpatialCoreTests \"[hrtf-switch]\" (25 cases, exit 0, 0 warnings, 955 s)"
        status: pass
    human_judgment: false
  - id: D4
    description: "The documented legacy load-and-swap flow with a render thread running is race-free under ThreadSanitizer and leaves every misuse counter at 0"
    requirement: "RTSF-03"
    verification:
      - kind: unit
        ref: "tests/Engine/ProfileSwitchTests.cpp#Profile switch: the documented legacy load-and-swap flow is race-free while another thread renders (D-09) (build-tsan, Debug, Release)"
        status: pass
    human_judgment: false
  - id: D5
    description: "No rendered sample changed: every [golden] fingerprint and every [hrtf-switch] case passes unchanged, and tests/Binaural has an empty diff against BASE"
    requirement: "RTSF-03"
    verification:
      - kind: unit
        ref: "./build/tests/SpatialCoreTests \"[golden],[hrtf-switch]\" (43 cases) and ./build-release/tests/SpatialCoreTests \"[golden]\" (18 cases); git diff BASE --stat -- tests/Binaural is empty"
        status: pass
    human_judgment: false
  - id: D6
    description: "Whether the written thread contracts in RenderEngine.h, HRTFDatabase.h and BinauralRenderer.h read clearly to a plugin author migrating OpenSpatialDelay"
    verification: []
    human_judgment: true
    rationale: "Clarity of prose is a reader judgment no test asserts; the contents (which thread may call which method, what is counted) are covered by D1 and D2."

duration: 51min
completed: 2026-10-06
status: complete
---

# Phase 5 Plan 05: HRTF handover thread contract and misuse detection Summary

**The legacy HRTF route OpenSpatialDelay calls today is unchanged but now watched: a render-thread swap is counted, and a load or setProfile on a renderer the audio thread is using is counted per call; the safe `setHRTFProfile` route is proven race-free under ThreadSanitizer from a 30 Hz timer, with no sample changed.**

## Performance

- **Duration:** 51 min
- **Started:** 2026-10-06T01:12:00Z
- **Completed:** 2026-10-06T02:03:00Z
- **Tasks:** 3
- **Files modified:** 7 (plus this SUMMARY and planning state)
- **BASE:** `ea034e731a4e43057350c8cfb6b56d4045b353f6` (HEAD when this plan started)

## Accomplishments

- **Wrong-thread detector.** `getPrepareRendererIndex()` and `swapActiveRenderer()` moved out of line with identical signatures and behaviour. Both call `noteLegacyRouteCall()`: when the caller is the known render thread and a different thread is the writer (`setOutputFormat()` caller, or a prior legacy caller), `getLegacyRouteWrongThreadCount()` goes up by one and Debug asserts. A thread that is not the render thread becomes the legacy writer. A single-thread harness is never counted.
- **In-use detector.** `HRTFDatabase` carries a relaxed in-use mark and a counter. `renderBlock()` ends with `markRendererUse`, which marks the active renderer and the crossfade source and clears the other (two relaxed stores per block). `unload`, `loadFromMemory`, `loadFromFile`, `loadFromBinaryData` and `BinauralRenderer::setProfile` call `noteMutationIfInUse()` once; the mutation still runs. Destructor and internal paths use the new `closeHandle()`, so a load counts once and destruction never counts.
- **Clear-before-release.** The mark is cleared before every `rendererFree_` release store (`endRendererFade`, the immediate-switch branch of `claimReadyRenderer`) and in `prepare()` with the worker stopped, so the engine's own loader is never counted.
- **Contracts in the headers.** `RenderEngine.h` states per legacy method which thread may call it and what is counted; `HRTFDatabase.h` states one database is used by one thread at a time (lookups are `const` but write `easy->fir`); `BinauralRenderer.h` states its thread contract.
- **ROADMAP criterion 3 on the safe route.** `[hrtf-switch][ui-timer]` runs a 30 Hz timer thread calling `setHRTFProfile` while another thread renders four moving objects with all three engine flags set. Under TSan: exit 0, zero warnings, 910 ticks, 51,921 blocks, 4 distinct active profiles (0, 2, 3, 5), 0 non-finite samples, both renderers' mutation counters 0.
- **No sound change (D-10).** No lookup, parse or render path changed. `[golden],[hrtf-switch]` passes in Debug (43 cases) and `[golden]` in Release (18 cases); `git diff BASE -- tests/Binaural` is empty.

## Task Commits

1. **Task 1: Tracer, a legacy swap on the render thread is counted** - `270b40a` (feat). RED run before it (below), D-07 mutation after it.
2. **Task 2: In-use detector (TDD)** - RED `a8aba4d` (test), GREEN `0c5f1ea` (feat)
3. **Task 3: ROADMAP criterion 3 and legacy contract under TSan** - `107b052` (fix, in-use mark ordering found while writing the legacy-threads case), `00cdd76` (test)

**Plan metadata:** the `docs(05-05)` SUMMARY, STATE and ROADMAP commit follows this file.

## Evidence

### Task 1 RED run (getter returning 0, count not implemented)

`./build/tests/SpatialCoreTests "[hrtf-switch][misuse]"`: 1 test case failed, assertions 4, 2 passed, 2 failed. SECTION (1) `CHECK( engine->getLegacyRouteWrongThreadCount() == 2 )` expanded `0 == 2`; SECTION (3) `== 1` expanded `0 == 1`. SECTIONs (2) and (4) passed (their expected count is 0). The target test failed on the planned count assertions, so this is a valid RED.

### Task 1 D-07 mutation (after `270b40a`)

`fetch_add` in `noteLegacyRouteCall` commented out, rebuilt: `[hrtf-switch][misuse]` fails SECTION (1) (`0 == 2`) and SECTION (3) (`0 == 1`), 2 of 4 assertions failed. Restored with `git checkout -- src/Engine/RenderEngine.cpp`, rebuilt, 4 of 4 pass. Debug logs 3 JUCE assertion lines (RenderEngine.cpp, `noteLegacyRouteCall`), one per deliberate misuse.

### Task 2 RED run (`a8aba4d`: API stubs, nothing counts)

`./build/tests/SpatialCoreTests "[hrtf-switch][in-use]"`: 2 cases, 43 assertions, 37 passed, 6 failed: `0 == 1` and `0 == 2` on the active-renderer load and `setProfile`, `0 == 1` and `0 == 2` on the `loadFromBinaryData`, `loadFromMemory` and `unload` sections, and `isInUseByAudioThread()` false. The engine-loader case passes vacuously at RED (it asserts the absence of counts), which is why it needs the D-07 mutation below.

### Task 2 D-07 mutation (after `0c5f1ea`)

`markRendererUse()` made a no-op, rebuilt: the first case fails with the same 6 assertions as the RED run (`0 == 1`, `0 == 2` and so on). Restored with `git checkout`, rebuilt, 43 of 43 pass.

**Second mutation, not caught.** Removing both early `setInUseByAudioThread (false)` calls (in `endRendererFade` and the immediate switch) was run 3 times: `[hrtf-switch][in-use]` still passed 3 of 3. The end-of-block `markRendererUse` clears the same mark a few microseconds later, and the loader needs far longer than that to reach `loadFromFile`, so the early clear is a belt-and-braces ordering guarantee that no deterministic test can pin. It stays in the code (it is what makes the ordering correct for a consumer that polls `isRendererCrossfadeActive()`, see the deviation below); the claim here is "implemented and reasoned", not "mutation-proven".

### Debug-log assertion check (full Debug suite, 380 cases, output to a log)

Assertion lines by source line, after Task 3: `BinauralRenderer.cpp:47` x4 (scratch guard cases from 05-02), `HRTFDatabase.cpp:396` x5 (exactly the five deliberate in-use misuses), `RenderEngine.cpp:1788` x1 (the SC-17 writer-view case), `RenderEngine.cpp:1830` x3 (exactly the three deliberate legacy wrong-thread calls), `juce_LeakedObjectDetector.h:116` x1 ("Leaked objects detected: 4 instance(s) of class FFT", D-08), `juce_MessageListener.cpp:50` x75 (existing). No other test tripped either new detector. The same count held after Task 2 (378 cases) and after Task 3 (380 cases).

### ThreadSanitizer, `[hrtf-switch]` in `build-tsan`

`SPATIALCORE_TEST_TIMEOUT_SCALE=10 TSAN_OPTIONS="halt_on_error=0 second_deadlock_stack=1" ./build-tsan/tests/SpatialCoreTests "[hrtf-switch]"` to `/tmp/sc-p5-hrtf-tsan.log`: **exit 0, 0 `WARNING: ThreadSanitizer` lines, 25 test cases, 401 assertions, 955 s.** The 25 are the 20 existing `[hrtf-switch]` cases plus the five new ones. No defect found in any documented flow, so no atomic fix (for `loaded` or `easyHandle`) was needed; the engine and `HRTFDatabase` carry no new data race.

### Final gates

- Debug: `SpatialCoreTests` 380 cases, 328,263 assertions, all passed (exit 0); `SpatialCoreUITests` 14 cases, 133 assertions, all passed.
- Release: `ctest --test-dir build-release/tests` 394 of 394 passed (14 listed with a `ui:` prefix).
- build-tsan: `[hrtf-switch]` as above.

## Files Created/Modified

- `include/SpatialCore/Engine/RenderEngine.h` - `getLegacyRouteWrongThreadCount()`, legacy methods out of line, per-method thread contract, private `legacyRouteWrongThreadCount_`, `legacyWriterThreadHash_`, `noteLegacyRouteCall()`
- `src/Engine/RenderEngine.cpp` - `noteLegacyRouteCall` (SC-17 rule), `markRendererUse` at the end of `renderBlock`, mark cleared before every free store and in `prepare()`
- `include/SpatialCore/Binaural/HRTFDatabase.h` / `src/Binaural/HRTFDatabase.cpp` - in-use API, `closeHandle()`, class thread contract, non-copyable through its atomics (no copy existed in the tree)
- `include/SpatialCore/Binaural/BinauralRenderer.h` / `src/Binaural/BinauralRenderer.cpp` - thread-contract comment; `setProfile` calls `noteMutationIfInUse()` first
- `tests/Engine/ProfileSwitchTests.cpp` - five new cases and the `runOnTwoThreads` / `FourObjectRender` helpers

## Decisions Made

- The wrong-thread rule is the SC-17 rule, so a harness that renders and swaps on one thread is never counted; `getBinauralRenderer()` is deliberately neither counted nor asserted (RESEARCH A6).
- Detection only, never blocking: the counted call still runs exactly as before, and no lock or wait was added to the audio path or to HRTF lookups (DR-1).
- The `[ui-timer]` case runs a storm phase (every tick a different profile) and then a dwell phase (every tick calls `setHRTFProfile`, the wanted profile changes every 12 x scale ticks), see the deviation.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] In-use mark cleared after the crossfade flag dropped**
- **Found during:** Task 3 (designing the legacy-threads case, which waits on `isRendererCrossfadeActive()` exactly as the documented flow says)
- **Issue:** The Task 2 `endRendererFade()` stored `rendererXfadeActive_ = false` and only then cleared the faded-out renderer's mark. A consumer that sees the fade over and loads into the prepare renderer in that window would have been counted as a misuse although it followed the documented flow (a false positive, T-05-13).
- **Fix:** `endRendererFade()` clears the mark first, then drops the flag and releases the renderer. The legacy-threads case and the single-thread documented-flow case both rely on it.
- **Files modified:** `src/Engine/RenderEngine.cpp`
- **Verification:** `[hrtf-switch]` Debug 43 cases, build-tsan 25 cases, full Debug suite and Release ctest all pass.
- **Committed in:** `107b052`

**2. [Rule 3 - Blocking, test design] Misuse harness keeps the first thread alive instead of joining it**
- **Found during:** Task 1 (writing the four SECTIONs)
- **Issue:** The plan sequences each step with `join()`. On macOS the id of a joined thread can be handed to the next thread; the detectors compare thread ids, so a render thread that reused the writer thread's id would hide the misuse and make SECTIONs (1) and (3) flaky.
- **Fix:** `runOnTwoThreads` runs the first body, then lets that thread idle (waiting on a future) while the second thread runs and joins, then releases and joins the first. No two threads ever touch the engine at once, which is what the plan wanted from the joins.
- **Files modified:** `tests/Engine/ProfileSwitchTests.cpp`
- **Verification:** `[hrtf-switch][misuse]` passes in Debug, Release and build-tsan; the D-07 mutation fails SECTIONs (1) and (3).
- **Committed in:** `270b40a`

**3. [Rule 1 - Bug in the plan's test shape] The UI-timer case would sit at the minimum of three distinct profiles**
- **Found during:** Task 3 (first run of `[ui-timer]`)
- **Issue:** Requesting a different profile every 33 ms supersedes every load that takes longer than 33 ms (latest request wins), so only the start state (profile 0), one lucky mid-run load and the final settle appeared: exactly 3 in five Release runs, a flake risk at the plan's own threshold of 3.
- **Fix:** A storm phase (the first third, the plan's literal every-tick cycling) followed by a dwell phase in which every 33 ms tick still calls `setHRTFProfile` but the wanted profile changes only every 12 x `timeoutScale()` ticks. The race exposure is the same (the timer thread calls `setHRTFProfile` every tick throughout); loads can now finish. Results: 4 distinct profiles in five Release runs and in the TSan run, 3 in Debug (0, 2, 3).
- **Files modified:** `tests/Engine/ProfileSwitchTests.cpp`
- **Verification:** `[hrtf-switch][ui-timer]` Debug, Release x5, build-tsan.
- **Committed in:** `00cdd76`

**4. [Process note] Task 2 RED commit carries the API stubs**
- The test cannot compile without `getMutationWhileInUseCount()` and `isInUseByAudioThread()`, so the RED commit `a8aba4d` includes the header and a stub `noteMutationIfInUse` that never counts; the failing assertions are the planned count checks (valid RED, not a compile error).

---

**Total deviations:** 3 auto-fixed (2 Rule 1, 1 Rule 3) plus 1 process note
**Impact on plan:** All necessary for correctness of the detector or reliability of its tests; no scope creep, no change to any rendered sample or public signature.

## Issues Encountered

- A BSD `sed -i` substitution used for the first D-07 attempt failed on a multi-line pattern (no mutation was applied, tests passed, which was noticed rather than recorded); redone with a Python replacement. Recorded here so the recorded mutation result is the second, real one.
- The second Task 2 mutation (removing the early clears) is not caught by any deterministic test, see Evidence.

## Known Stubs

None.

## Threat Flags

None. No new network endpoint, auth path, file access or schema change; the new surface is three relaxed atomics and a counter, all within the plan's `<threat_model>` (T-05-11, T-05-12, T-05-13).

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- RTSF-03 is done: the safe route is proven under TSan and the legacy route is documented and watched. OpenSpatialDelay's migration can read the contracts in `RenderEngine.h` (legacy section) and replace the legacy flow with `setHRTFProfile`.
- Public API added (additive, minor bump): `RenderEngine::getLegacyRouteWrongThreadCount()` and four `HRTFDatabase` methods. `HRTFDatabase` is now non-copyable (no copy existed in the tree).
- Ready for 05-06.

## Self-Check: PASSED

- All 7 modified source files exist; commits `270b40a`, `a8aba4d`, `0c5f1ea`, `107b052`, `00cdd76` all found; `git log --grep=05-05` returns 5 commits and `git rev-list --count BASE..HEAD` is 5.
- Acceptance criteria: `getLegacyRouteWrongThreadCount` x2 in `RenderEngine.h`, the two out-of-line signatures x2, `noteLegacyRouteCall` x3 in `RenderEngine.cpp`; `noteMutationIfInUse` x5 in `HRTFDatabase.cpp` and x1 in `BinauralRenderer.cpp`; destructor uses `closeHandle` x1; `endRendererFade` has `setInUseByAudioThread (false)` x1; `[hrtf-switch][in-use]` passes 2 cases in Release; `[ui-timer],[legacy][threads]` passes 2 cases in Release; `git diff BASE -- tests/Binaural` is empty.

---
*Phase: 05-realtime-safety-hardening*
*Completed: 2026-10-06*
