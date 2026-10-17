---
phase: 05-realtime-safety-hardening
plan: 03
subsystem: testing
tags: [realtime-safety, malloc-trap, pthread-trap, catch2, render-matrix, lock-inventory, ambisonics, trajectory]

requires:
  - phase: 05-realtime-safety-hardening
    provides: "Plans 05-01 and 05-02: the allocation and lock trap, MovingScene/OutputBank/armedRender, oversized-block chunking and scratch guards, SPATIALCORE_SOURCE_DIR"
provides:
  - "[rt][switch][tracer]: mid-play profile switches 0-1-0-3-5 and format switches Binaural/7.1.4/6OA/Stereo/Binaural with all three engine flags, fade on and off"
  - "23 per-format cold cases x 8 flag sets, 6 per-profile cases x 2 routes (plus 44.1 and 96 kHz), 5 sizes x 3 rates, 22 formats x 12 algorithm indices"
  - "[rt][trajectory], [rt][audit], [rt][ordering], [rt][legacy] cases"
  - "[rt][rule]: frozen lock and blocking-call inventory of non-UI src/ and include/, per-file allow-list with a thread for each entry"
  - "AmbisonicsAlgorithm::computeGains no longer takes a first-use lock on the render thread (max-rE weights are constants, bit-identical)"
affects: [05-04, RTSF-05, VERIFY-02, OpenSpatialDelay migration]

actuals:
  tokens: 11625
  tasks: 3
  commits: 4

tech-stack:
  added: []
  patterns:
    - "ArmedRun: per-block arming with counts accumulated across steps and message-side calls made disarmed"
    - "contextFor (format, flag bits, rate): mirrors consumer-supplied dispatch and gains when an engine flag is off"
    - "One TEST_CASE per cold-start value (macro), so ctest runs each in a process of its own"
    - "Source-scan rule with positive and negative controls and a two-way allow-list (additions and stale entries both fail)"
    - "Debug gate (kHRIRLookupChecked): alloc and lock counts for blocks that look an HRIR up are asserted in Release only"

key-files:
  created: []
  modified:
    - tests/Engine/RealtimeSafetyTests.cpp
    - src/Algorithms/AmbisonicsAlgorithm.cpp

key-decisions:
  - "Fix the Ambisonics first-use lock with exact float constants (0x3f6c835e, 0x3f3504f3, 0x3ec3ef15) rather than recomputing std::cos per call; proven bit-identical by a memcmp grid test against a copy of the old body and by cmp of a hidden capture in Debug and Release"
  - "HRIR-lookup blocks are alloc- and lock-checked in Release only; a Debug tree builds libmysofa with -DVDEBUG, which printf()s in every lookup (decided in Plan 05-02, applied here)"
  - "The profile case also runs 44.1 and 96 kHz on the engine-selected route, to carry the plan truth about libmysofa lookups at three rates (D-10)"
  - "Allow-list entries are exact per-file line counts, with the function and thread named for each; none is reachable from renderBlock"

patterns-established:
  - "A cold-start trap case is one TEST_CASE per value; a case that loops values shares process-global first use"
  - "A lock or blocking primitive added anywhere outside UI code fails [rt][rule] until a reviewer names its thread"

requirements-completed: [RTSF-05]

coverage:
  - id: D1
    description: "With engineComputesGains, engineDerivesDispatch and engineSelectsHRTF all set, mid-play profile switches (Simple and SOFA) and output-format switches, switch fade on and off, allocate, free and lock nothing on the render thread; closes the milestone-audit item 'no test sets all three engine flags on one binaural block'"
    requirement: RTSF-05
    verification:
      - kind: unit
        ref: "tests/Engine/RealtimeSafetyTests.cpp#Realtime: mid-play profile and format switches allocate and lock nothing on the render thread"
        status: pass
    human_judgment: false
  - id: D2
    description: "Every one of the 23 output formats, each in its own cold ctest process, renders 120 moving blocks with zero allocations, frees and locks under all 8 flag combinations"
    requirement: RTSF-05
    verification:
      - kind: unit
        ref: "ctest --test-dir build-release/tests -R 'renders every flag combination' (23 tests)"
        status: pass
    human_judgment: false
  - id: D3
    description: "Every HRTF profile 0-5 renders on the engine-selected and the consumer-selected route with zero allocations, frees and locks, and at 44.1 and 96 kHz as well (libmysofa lookup needed no change)"
    requirement: RTSF-05
    verification:
      - kind: unit
        ref: "ctest --test-dir build-release/tests -R 'HRTF profile . renders' (6 tests)"
        status: pass
    human_judgment: false
  - id: D4
    description: "Block sizes 32 to 1024 at 44.1, 48 and 96 kHz on Binaural (profile 5, all flags), 7.1.4 and Ambisonics HOA, and all 12 algorithm indices on all 22 non-binaural formats, render with zero allocations, frees and locks"
    requirement: RTSF-05
    verification:
      - kind: unit
        ref: "tests/Engine/RealtimeSafetyTests.cpp#Realtime: every block size and sample rate renders without allocating"
        status: pass
      - kind: unit
        ref: "tests/Engine/RealtimeSafetyTests.cpp#Realtime: all twelve algorithm indices render every non-binaural format without allocating"
        status: pass
    human_judgment: false
  - id: D5
    description: "TrajectoryEngine-driven positions (all 13 shapes at speed 4, forward and reverse) render with zero allocations, frees and locks on Binaural, 7.1.4 and Ambisonics 6OA"
    requirement: RTSF-05
    verification:
      - kind: unit
        ref: "tests/Engine/RealtimeSafetyTests.cpp#Realtime: trajectory-driven positions render without allocating"
        status: pass
    human_judgment: false
  - id: D6
    description: "A Binaural to 7.1.4 switch during an HRTF renderer crossfade fades out to exactly 0, fades in from exactly 0 on all 12 channels, allocates nothing and strands no renderer (a later profile 3 becomes active); closes the milestone-audit item 'switch during an HRTF renderer crossfade is untested'"
    requirement: RTSF-05
    verification:
      - kind: unit
        ref: "tests/Engine/RealtimeSafetyTests.cpp#Realtime: a Binaural to 7.1.4 switch during an HRTF renderer crossfade fades cleanly and strands nothing"
        status: pass
    human_judgment: false
  - id: D7
    description: "Ordering: three format requests between two blocks render only the last; two profile requests make the second active and the first never active; the legacy load-and-swap route renders its crossfade; all with zero allocations, frees and locks"
    requirement: RTSF-05
    verification:
      - kind: unit
        ref: "tests/Engine/RealtimeSafetyTests.cpp#Realtime: several requests between two blocks: the last one wins with no allocation"
        status: pass
      - kind: unit
        ref: "tests/Engine/RealtimeSafetyTests.cpp#Realtime: the legacy load-and-swap route renders its crossfade without allocating"
        status: pass
    human_judgment: false
  - id: D8
    description: "The lock and blocking-primitive inventory of non-UI src/ and include/ is frozen: an added primitive or a stale entry fails the test (mutation-checked both ways)"
    requirement: RTSF-05
    verification:
      - kind: unit
        ref: "tests/Engine/RealtimeSafetyTests.cpp#Realtime: lock and blocking primitives outside UI code match the reviewed allow-list"
        status: pass
      - kind: unit
        ref: "tests/Engine/RealtimeSafetyTests.cpp#Realtime: the comment stripper keeps code and drops prose"
        status: pass
    human_judgment: false
  - id: D9
    description: "Found and fixed: AmbisonicsAlgorithm::computeGains took a first-use mutex (function-local static guard) on the render thread; now constants, bit-identical in Debug and Release"
    requirement: RTSF-05
    verification:
      - kind: unit
        ref: "tests/Engine/RealtimeSafetyTests.cpp#Ambisonics algorithm: constant max-rE weights give the old gains bit for bit"
        status: pass
      - kind: other
        ref: "cmp /tmp/sc-p5-maxre-before-{debug,release}.bin /tmp/sc-p5-maxre-after-{debug,release}.bin (both exit 0)"
        status: pass
    human_judgment: false
  - id: D10
    description: "No regression: full Debug suite, Debug UI suite and Release ctest pass, ctest lists ui: tests"
    verification:
      - kind: integration
        ref: "./build/tests/SpatialCoreTests; ./build/tests/SpatialCoreUITests; ctest --test-dir build-release/tests"
        status: pass
    human_judgment: false

duration: 22min
completed: 2026-10-06
status: complete
plan_head_before: a3593a3f07ae028b9c51a335dfe8113b41ed8c8e
plan_head_after: 085bd78905bcfc30c1258f9a1da8c167e46c6d36
commits: 4
---

# Phase 5 Plan 03: The whole audio path under the trap, and a frozen lock inventory Summary

**Every render path, format, flag set, profile, block size, rate, algorithm index, trajectory shape and switch now renders with zero allocations, frees and locks on the render thread (39 new cases), the sweep found and fixed one real first-use lock in `AmbisonicsAlgorithm` (bit-identical), and a source-scan rule freezes the lock and blocking-call inventory outside UI code.**

## Performance

- **Duration:** about 22 min
- **Started:** 2026-10-06T00:09:48Z
- **Completed:** 2026-10-06T00:32Z
- **Tasks:** 3 (Task 1 tracer; Task 2 RED then GREEN; Task 3)
- **Files modified:** 2 (0 created)

## Accomplishments

- A render thread survives every mid-play switch with all three engine flags set: profiles 0, 1, 0, 3, 5 (Simple and SOFA both ways) and formats Binaural, 7.1.4, 6OA, Stereo, Binaural, with the SC-20 fade on and off. This is the first case with `engineComputesGains`, `engineDerivesDispatch` and `engineSelectsHRTF` on one binaural block.
- The matrix: 23 per-format cases (each a cold ctest process) x 8 flag sets, 6 per-profile cases x 2 routes, plus 44.1 and 96 kHz, 5 block sizes x 3 rates x 3 configurations, and 22 non-binaural formats x 12 algorithm indices. All measure zero in Release; Debug measures zero everywhere a block does not look an HRIR up.
- Trajectories (13 shapes, speed 4, forward and reverse), the Binaural to 7.1.4 switch during an HRTF renderer crossfade (exact-zero fade endpoints, nothing stranded), request ordering, and OpenSpatialDelay's legacy load-and-swap route are all trap-proven.
- `[rt][rule]` freezes the lock inventory: 16 matching lines in 4 files, every one on the message thread, the loader thread, or at prepare time.
- Both STATE milestone-audit test gaps are closed.

## Evidence (as the plan's `<output>` requires)

- **BASE:** `a3593a3f07ae028b9c51a335dfe8113b41ed8c8e` (HEAD when the plan started).
- **Was any allocation found?** No allocation. One **lock** was found (below).
- **The finding, RED at `1f27d49` (Release):** `[rt][algorithms]` failed with `counts.locks == 0` reading `2 == 0` (`formatIndex := 1` Stereo, `index := 0` Ambisonics algorithm, `allocs := 0`, `frees := 0`), deterministic over 3 runs, only on the first engine in the process. A temporary diagnostic (a stack printed on the first lock; reverted with `git checkout`, never committed) showed: `rt_mutex_lock` <- `__cxa_guard_acquire` <- `AmbisonicsAlgorithm::computeGains` <- `RenderEngine::computeObjectGains` <- `RenderEngine::renderBlock`. Cause: the function-local `static const float maxrE[4]` built with `std::cos` is guard-initialised, and the first call in a process is on the audio thread, where the guard takes a mutex. It is a one-time lock, not a per-block one, but it is a lock on the render thread and it would block if two instances first-rendered together.
- **D-10 handling:** SpatialCore code, not libmysofa, so fixed the way 05-01 fixed the NFC temporary. The weights are now `static constexpr` with the exact floats the static held. Checked first that the folded and the runtime `cosf` values agree bit for bit at -O0 and -O3 (`0x1.d906bcp-1`, `0x1.6a09e6p-1`, `0x1.87de2ap-2`). Proof of bit-identity:
  - `[ambi][maxre][d10]`: 25 azimuths x 13 elevations (325 positions), 12 speakers, a fixed non-trivial decode matrix, `memcmp` against a test-local copy of the old body: passes at BASE (before the fix) and after, Debug and Release.
  - Hidden capture `[.][ambi-maxre-capture]` of the same grid (15,600 bytes, 2,355 of 3,900 floats non-zero, max 0.885, no NaN): `cmp before-debug after-debug` exit 0, `cmp before-release after-release` exit 0.
  - After the fix `[rt][algorithms]` passes in Release and Debug; the Phase 3 `[golden]` and Ambisonics algorithm tests are in the full runs below and unchanged.
- **libmysofa (D-10 truth):** the per-block HRIR lookup allocated nothing for profiles 1 to 5 at 44.1, 48 and 96 kHz in Release (profile cases at 3 rates, sizes case at profile 5, all three rates). No lookup change was needed and no golden was touched.
- **Run times.** Release `[rt]`: 46 cases, 2,514 assertions, 20.1 s. Debug `[rt]`: 46 cases, 2,373 assertions, 103 s. `[matrix]` alone: Release 14.4 s, Debug 76.7 s (well under the 5 minute threshold, so no block counts were reduced anywhere). Debug full suite 6 min 34 s. Release ctest 105 s.
- **Test counts.** Debug `SpatialCoreTests` 373 cases, 328,164 assertions pass (334 before + 39). Debug `SpatialCoreUITests` 14 cases, 133 assertions pass. Release `ctest` 387 of 387 pass (348 before + 39); `ctest -N` lists 14 `ui:` tests. The 39 new cases: 1 tracer, 23 format, 6 profile, sizes, algorithms, trajectory, audit, ordering, legacy, 2 rule, `[d10]`. No case skipped in any log.
- **Cold-process proof:** `ctest -R 'renders every flag combination|HRTF profile . renders|every block size|twelve algorithm'` (34 tests, `-j4`) passed in Release, and `ctest -N | grep -c "renders every flag combination"` prints 23.
- **Final lock allow-list** (exact matching lines per file; the scan strips comments; none is reachable from `renderBlock`):

  | File | Lines | Function | Thread |
  |------|-------|----------|--------|
  | `include/SpatialCore/Binaural/SharedFFTCache.h` | 1 | `SharedFFTCache::lock`, a `juce::SpinLock` (the pthread trap cannot see it) | prepare-time (see next row) |
  | `src/Binaural/PartitionedConvolver.cpp` | 1 | `SharedFFTCache::getOrCreate`, reached from `PartitionedConvolver::prepare` and `HRTFDatabase::convertToMinPhase` | prepare-time, message or loader thread |
  | `include/SpatialCore/Engine/RenderEngine.h` | 1 | `sharedFolderLock_`, a `juce::CriticalSection` | message and loader threads |
  | `src/Engine/RenderEngine.cpp` | 13 | `:89` loader `run` `wait (200)`; `:238` loader backoff `wait (pollMs)`; `:347-348` destructor `notify`/`stopThread`; `:357` `getSharedFolderForLoader` `ScopedLock`; `:363` `setSharedHRTFFolderForTesting` `ScopedLock`; `:394`/`:396` `setHRTFProfile` `startThread`/`notify`; `:441` `waitForHRTFProfileIdle` `Thread::sleep (2)`; `:499-500` `prepare` `notify`/`stopThread`; `:612-613` `prepare` `startThread`/`notify` | loader thread (`:89`, `:238`, `:357`); message thread (all others) |

- **Rule mutation checks (scratch, restored):** adding `src/Core/zz_scratch_mutation.h` with a `std::mutex` and a `juce::SpinLock` made `[rt][rule]` fail naming `src/Core/zz_scratch_mutation.h:3` (a `std::mutex` in a comment on line 4 was correctly ignored); replacing the `Thread::sleep (2)` line made it fail with `stale allow-list entry: src/Engine/RenderEngine.cpp lists 13 lines but the scan finds 12`. Both restored (`git status` clean for `src/`), rule green again.

## Task Commits

1. **Task 1 (tracer): mid-play switches with all three flags** - `a8910a5` (test)
2. **Task 2 RED: the matrix (finds the Ambisonics first-use lock)** - `1f27d49` (test)
3. **Task 2 GREEN: max-rE weights as constants, `[d10]` proof and hidden capture** - `9fe1829` (fix)
4. **Task 3: trajectories, audit switch, ordering, legacy route, `[rt][rule]`** - `085bd78` (test)

**Plan metadata:** the docs commit that follows this file (`docs(05-03): complete whole-audio-path trap plan`).

Tracer feedback gate: auto mode not active, `human_verify_mode` is the default end-of-phase and the tracer `<verify>` carries only `<automated>`, so the gate was a re-run of the tracer verify end to end (Debug build and run, Release build and run, `[rt][switch][tracer]` 1 case, 48 assertions, 0 skipped). It passed, "Tracer verified end-to-end, expanding".

## Files Created/Modified

- `tests/Engine/RealtimeSafetyTests.cpp` - `ArmedRun`, `contextFor`, `runFormatMatrix`, `runProfileMatrix`, macros `SC_RT_FORMAT_CASE` and `SC_RT_PROFILE_CASE`, and every new case above; `MovingScene::externalPositions`; a local `timeoutScale`, a local copy of `stripComments`, the lock allow-list
- `src/Algorithms/AmbisonicsAlgorithm.cpp` - `maxrE` is `static constexpr` with exact float constants (10 lines added, 5 removed, comment explains why)

## Decisions Made

- Constants over per-call `std::cos`: no libm call on the audio thread and no change to the sound. The folded and runtime cosines were verified equal before the constants were written, and the `[d10]` test stays in the suite so the constants can never drift from the `std::cos` form unseen.
- Allocation and lock checks for blocks that look an HRIR up run in Release only (Plan 05-02's decision; libmysofa's `-DVDEBUG` `printf` in a Debug tree). Output, profile-activation and ordering checks run in both builds, and the Debug run still catches everything that does not look an HRIR up.
- The profile cases run 44.1 and 96 kHz in addition to the cold 48 kHz run so the plan truth about libmysofa at three rates is covered by the profile dimension and not only by profile 5 in the sizes case.
- Task 2 was committed as RED (matrix, with the algorithm case failing on the lock) then GREEN (fix plus proof), so the history shows the finding.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] First-use lock on the render thread in `AmbisonicsAlgorithm::computeGains`**
- **Found during:** Task 2 (`[rt][algorithms]`, Release)
- **Issue:** the function-local `static const float maxrE[4]` built with `std::cos` was guard-initialised; its first call in a process ran on the audio thread and took `__cxa_guard_acquire`'s mutex (2 locks counted). Reached whenever algorithm index 0 (Ambisonics) computes gains on a speaker or stereo format.
- **Fix:** `static constexpr float maxrE[4]` with the exact float constants; bit-identity proven (memcmp grid test, cmp of hidden capture in Debug and Release).
- **Files modified:** `src/Algorithms/AmbisonicsAlgorithm.cpp` (outside the plan's `files_modified`, as D-10 and Task 2 allow for a SpatialCore-code finding)
- **Verification:** `[ambi][maxre][d10]` (326 assertions) and the cmp runs above; `[rt][algorithms]` green in both builds; full suites green.
- **Committed in:** `9fe1829`

**2. [Rule 3 - Blocking] HRIR-lookup allocation and lock checks gated to Release**
- **Found during:** Task 1 (planning the Debug run of the tracer; same cause measured in Plan 05-02)
- **Issue:** a Debug tree builds libmysofa with `-DVDEBUG`, which `printf()`s in every HRIR lookup and allocates and locks on the render thread. The plan's Debug runs of the HRTF cases could not pass otherwise.
- **Fix:** `checkNothingAllocatedOrLocked (counts, lookupsHappened)` skips the three count checks when the case looks HRIRs up and the build is Debug; every non-lookup case (all formats, sizes for 7.1.4 and HOA, all algorithm indices, ordering section (a)) is checked in both builds.
- **Files modified:** `tests/Engine/RealtimeSafetyTests.cpp`
- **Verification:** Debug `[rt]` passes (2,373 assertions); Release asserts every count (2,514 assertions).
- **Committed in:** `a8910a5`, used by `1f27d49` and `085bd78`

---

**Total deviations:** 2 auto-fixed (1 Rule 1, 1 Rule 3)
**Impact on plan:** the fix is the single source change of the plan, is bit-identical, and is the kind of finding the sweep exists to make; the gate is test-only and was already decided in 05-02. No scope creep.

## Issues Encountered

- While splitting Task 2 into a RED and a GREEN commit I ran one stray `git stash push --keep-index` (a mistake; the plan and the environment say not to use stash). It stashed only the unstaged source fix. I recovered at once: confirmed the stash list held exactly my one entry, applied it by SHA (`git stash apply 01b0c5e...`), verified the diff was intact, and dropped that entry by SHA; the stash list is empty again, as it was before. Nothing was lost and no other session's stash was touched.
- Expected Debug noise: "Leaked objects detected: N FFT" at exit (D-08), libmysofa trace lines.
- `sed -i` with a `|` delimiter failed once on BSD sed; redone with the Edit tool. No effect on the tree.

## Known Stubs

None.

## Threat Flags

None. T-05-07 (a format-, profile-, size- or algorithm-specific allocation slipping through) is mitigated as planned and, in doing so, caught the Ambisonics first-use lock. T-05-08 (a lock reaching the audio path unseen by the pthread interposer) is mitigated by `[rt][rule]`, mutation-checked. The rule reads source files at test time through `SPATIALCORE_SOURCE_DIR` (already defined by 05-01) and adds no network, auth or file-write surface.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- The trap now covers the whole audio path on macOS. The remaining Phase 5 plans (documentation, README and CLAUDE.md text for the oversized-block contract, thread contracts) can cite these cases by tag: `[rt][matrix]`, `[rt][switch]`, `[rt][rule]`.
- Not proven by this plan, by design: Windows and Linux (the trap is macOS-only; the cases SKIP elsewhere, never pass green) and the Debug allocation count for HRIR-lookup blocks (Release is the proof).
- A future maintainer who adds a lock or a blocking call anywhere outside UI code gets a failing `[rt][rule]` that names the file and line, and must record its thread in `reviewedLockInventory()`.

## Self-Check: PASSED

- Modified files: `tests/Engine/RealtimeSafetyTests.cpp` and `src/Algorithms/AmbisonicsAlgorithm.cpp` FOUND. Commits `a8910a5`, `1f27d49`, `9fe1829`, `085bd78` FOUND; `git rev-list --count a3593a3..HEAD` is 4; `git log --all --grep="05-03"` returns the 4 commits.
- Task 1 acceptance: Release `[rt][switch][tracer]` 1 case passes, 0 skipped; `grep -c "engineSelectsHRTF = true"` prints 1.
- Task 2 acceptance: `--list-tests` lists 31 `[rt][matrix]` cases, run exits 0, none skipped; `SC_RT_FORMAT_CASE (` count 23, `SC_RT_PROFILE_CASE (` count 6; `ctest -N | grep -c "renders every flag combination"` prints 23; the SUMMARY states the finding and its D-10 handling.
- Task 3 acceptance: `[rt][rule]` 2 cases pass; `[rt][audit],[rt][ordering],[rt][legacy],[rt][trajectory]` 4 cases pass (82 assertions), none skipped; `SPATIALCORE_SOURCE_DIR` count 2; the map has an entry for each of the four named files; full Debug suite, UI suite and Release ctest pass.
- Plan-level verification re-run after the last source change: Debug build of every target, `[rt]`, full Debug suite (373 cases), UI suite (14), Release build of every target, Release `[rt]`, full ctest (387 of 387), 14 `ui:` tests. `git grep -l -e malloc_logger -e __interpose -- src include CMakeLists.txt` prints nothing.

---
*Phase: 05-realtime-safety-hardening*
*Completed: 2026-10-06*
