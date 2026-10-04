---
phase: 03-binaural-defects-hrtf-packaging
plan: 07
subsystem: testing
tags: [hrtf, threads, concurrency, threadsanitizer, watchdog, catch2, render-engine]

requires:
  - phase: 03-binaural-defects-hrtf-packaging
    provides: "03-06 setHRTFProfile, HRTFProfileLoader worker, readyRenderer_ mailbox, rendererFree_ flags, claimReadyRenderer, LiveRender and temp-folder test helpers"
provides:
  - "[hrtf-switch][threads]: 25 setHRTFProfile calls against a live render thread, no deadlock, finite output, ends on the latest request"
  - "[hrtf-switch][churn]: 16 complete switches (claim, crossfade end, free-flag release) under a paced render thread"
  - "[hrtf-switch][shutdown]: engine destroyed at once and 40 ms into a SADIE load"
  - "Test-local watchdog (runWithWatchdog) that aborts with a named message, and the SPATIALCORE_TEST_TIMEOUT_SCALE reader"
  - "ThreadSanitizer verdict for the whole [hrtf-switch] tag: zero reports, no Phase 3 race"
affects: [03-08, 03-09, Phase 5 TSAN gate (RTSF-02, RTSF-03)]

plan_head_before: 3e21966b0f8c52758d08d20e2381830066fbdf8f
plan_head_after: 9522c989cbbb623ae0358ad3cd1124ea561413a0

actuals:
  tokens: 5600
  tasks: 2
  commits: 2

tech-stack:
  added: []
  patterns:
    - "Concurrency tests measure on their own threads and assert on the Catch2 thread afterwards; the watchdog runs the body on std::async and abort()s on timeout because a std::async future blocks in its destructor"
    - "Engines under test live on the heap inside a std::async body (small secondary-thread stack)"
    - "SPATIALCORE_TEST_TIMEOUT_SCALE stretches watchdogs, idle waits and polls only; the 50 ms, 4.0 and 15 s bounds are never scaled"

key-files:
  created: []
  modified:
    - tests/Engine/ProfileSwitchTests.cpp

key-decisions:
  - "Added a third case, [hrtf-switch][churn], because the prescribed [threads] scenario fires requests faster than a SOFA load finishes: in Debug only the very last switch reached the audio thread (1 to 18 blocks on profile 5, the rest on profile 0), so it never exercised claim, crossfade end or the free-flag handoff more than once"
  - "The final ThreadSanitizer run used a RelWithDebInfo tree (/tmp/sc-tsan-o2), not Debug: a Debug-plus-TSAN SADIE load takes tens of seconds, which trips the engine's own 15 s stopThread and the unscaled waits of the 03-06 cases"
  - "Scaled the literal waitForHRTFProfileIdle waits of the Plan 03-06 cases with timeoutScale() (default 1, so normal runs are unchanged)"
  - "No engine code changed: the sanitizer found no Phase 3 race, so no fix commit exists"

patterns-established:
  - "Test a lock-free handoff with a paced render thread and a switch that waits for the profile to become active but not for the crossfade to end, so the next request has to wait for the faded renderer to come back free"

requirements-completed: [DATA-01]

coverage:
  - id: D1
    description: "Switching while another thread renders never deadlocks, never blocks the caller (slowest call 0.213 ms Debug, 0.027 ms Release), produces no non-finite sample, stays under 4.0 and ends on the last request"
    requirement: DATA-01
    verification:
      - kind: unit
        ref: "tests/Engine/ProfileSwitchTests.cpp#Profile switch: switching while another thread renders never deadlocks or produces non-finite audio"
        status: pass
    human_judgment: false
  - id: D2
    description: "Every switch of a 16-switch sequence (embedded profiles 1-5 and Simple) completes under a paced render thread, with finite output below 4.0 and audible output on every SOFA profile"
    requirement: DATA-01
    verification:
      - kind: unit
        ref: "tests/Engine/ProfileSwitchTests.cpp#Profile switch: every switch completes while a paced render thread runs, claims and frees included"
        status: pass
    human_judgment: false
  - id: D3
    description: "Destroying an engine while its loader parses SADIE returns well inside 15 s (2.5 ms at once, 780 ms Debug and 373 ms Release 40 ms into the load) with no crash; a hang ends in a named WATCHDOG abort"
    requirement: DATA-01
    verification:
      - kind: unit
        ref: "tests/Engine/ProfileSwitchTests.cpp#Profile switch: destroying the engine during a load returns promptly"
        status: pass
    human_judgment: false
  - id: D4
    description: "ThreadSanitizer over every [hrtf-switch] case reports nothing in the Phase 3 switching code (loader, mailbox claim, crossfade end, renderer and database writes through the loader)"
    requirement: DATA-01
    verification:
      - kind: other
        ref: "/tmp/sc-tsan-o2/tests/SpatialCoreTests \"[hrtf-switch]\" (zero ThreadSanitizer reports, exit 0); classification file ends phase3-races: 0"
        status: pass
    human_judgment: false

duration: 31 min
completed: 2026-10-04
status: complete
---

# Phase 3 Plan 07: Thread proof for engine-owned profile switching Summary

**Engine-owned HRTF switching holds up under real threads: a render thread and a switching thread together, 16 complete claim-and-free cycles, and shutdown in the middle of a SADIE load all pass in Debug and Release, and ThreadSanitizer reports nothing in the Phase 3 switching code.**

## What you will hear

Nothing changes in the sound. This plan only proves, with real threads running together, that the profile switch from Plan 03-06 does what it promised: asking for a profile never makes the caller wait (the slowest call took 0.2 thousandths of a second in a debug build), the audio never produces garbage or a runaway level while profiles change underneath it, the last profile asked for is the one you end up on, and closing a plugin while a large file is still loading is quick and safe.

## Performance

- **Duration:** 31 min
- **Started:** 2026-10-04T14:31:17Z
- **Completed:** 2026-10-04T15:02:00Z
- **Tasks:** 2 (Task 1 tracer, Task 2 sanitizer run)
- **Files modified:** 1 (`tests/Engine/ProfileSwitchTests.cpp`, 348 insertions, 16 deletions)

## Accomplishments

- `runWithWatchdog (name, seconds, body)` runs a body on `std::async` and, on timeout, prints `WATCHDOG: <name> exceeded <n> s` and calls `std::abort()`; `timeoutScale()` reads `SPATIALCORE_TEST_TIMEOUT_SCALE`.
- `[hrtf-switch][threads]`: a flat-out render thread (64-sample blocks, 440 Hz sine, azimuth stepping 1 degree per block) against 25 `setHRTFProfile` calls (24 drawn from {5,3,0,4,2,5,3,0} with profile 1 at position 12, seeded 20-120 ms gaps, then a final 5).
- `[hrtf-switch][churn]` (added, see Deviations): 16 switches over {5,2,3,4,5,0,2,5,3,0,4,5,2,3,5,1}, each waited until the profile is active but not until the crossfade ends, with the render thread paced at 250 microseconds a block.
- `[hrtf-switch][shutdown]`: SADIE requested, then the engine destroyed at once and 40 ms later.
- ThreadSanitizer over the whole `[hrtf-switch]` tag: zero reports.

## Measurements

| Measure | Debug | Release | Bound |
|---|---|---|---|
| Slowest `setHRTFProfile` call, `[threads]` (runs) | 0.032 to 0.213 ms over 8 runs | 0.027 ms | 50 ms |
| Slowest `setHRTFProfile` call, `[churn]` | 0.035 to 0.064 ms | 0.013 ms | 50 ms |
| Largest sample, `[threads]` | 0.0005 to 0.343 (varies with how many blocks ran on a SOFA profile) | 0.559 | below 4.0 |
| Largest sample, `[churn]` | 1.308 to 1.344 (profile 4 loudest) | 1.344 | below 4.0 |
| Non-finite samples | 0 (every run) | 0 | 0 |
| Blocks rendered, `[threads]` | 0.8 to 1.0 million | 3.1 million | n/a |
| Switches completed, `[churn]` | 16 of 16 (every run) | 16 of 16 | all |
| Engine destruction, at once | 2.5 ms | 2.5 ms | below 15 s |
| Engine destruction, 40 ms into a SADIE load | 746 to 780 ms | 373.5 ms | below 15 s |

Under ThreadSanitizer (`-O2`): slowest call 0.213 ms and 0.047 ms, no non-finite sample, largest sample 1.311, destruction 5850 ms 40 ms into the load, all within bounds.

## ThreadSanitizer result

- Final run: `/tmp/sc-tsan-o2` (RelWithDebInfo, `-fsanitize=thread` on C and C++), `SPATIALCORE_TEST_TIMEOUT_SCALE=10 TSAN_OPTIONS="halt_on_error=0 second_deadlock_stack=1" ... "[hrtf-switch]"` into `/tmp/sc-0307-tsan.log`: 12 test cases, 143 assertions, exit 0, **0 ThreadSanitizer reports**. The binary links the TSAN runtime (`libclang_rt.tsan_osx_dynamic.dylib`) and a deliberate-race sanity program reports as expected, so the empty result is a real result.
- Debug tree `/tmp/sc-tsan` (`-O0`), cases `[threads]`, `[churn]`, `[tracer]`, `[non-binaural]`, `[abandon]`: 5 cases, 39 assertions, exit 0, 0 reports (`/tmp/sc-0307-tsan-debug.log`).
- Classification file `/tmp/sc-0307-tsan-classification.txt` (copied here; it has no `report` lines because the log has no reports):

```
note: final run = RelWithDebInfo ThreadSanitizer tree /tmp/sc-tsan-o2, whole [hrtf-switch] tag (12 cases), SPATIALCORE_TEST_TIMEOUT_SCALE=10, exit 0, zero ThreadSanitizer reports
note: Debug ThreadSanitizer tree /tmp/sc-tsan, [threads],[churn],[tracer],[non-binaural],[abandon] cases, zero reports; the cases that destroy an engine mid-load abort there on the engine's own 15 s stopThread because an -O0 SADIE load takes tens of seconds under instrumentation
phase3-races: 0
```

- Phase 3 races found: none. Fix commits: none. Pre-existing reports handed to Phase 5: none (nothing in TrajectoryEngine, SharedFFTCache or the RTSF-03 `easyHandle` path appeared in this tag's run, so the exposure is not widened and not narrowed either).

## Task Commits

1. **Task 1: Tracer, render thread plus switching thread, then shutdown mid-load** - `6bf0b8d` (test)
2. **Task 2: ThreadSanitizer run and classification** - `9522c98` (test; the only repository change from this task is the timeout-scale change to the Plan 03-06 waits. The sanitizer run itself lives outside the repository and found nothing to fix.)

**Plan metadata:** the docs commit following this SUMMARY.

Tracer gate: interactive run, `end-of-phase` mode, tracer `<verify>` automated-only; the verify (Debug build, `[threads]`, `[shutdown]`, Release build, whole `[hrtf-switch]`) was re-run end to end and passed, so execution continued to Task 2.

## Verification results

| Check | Result |
|---|---|
| `[hrtf-switch][threads] --list-tests` and `[hrtf-switch][shutdown] --list-tests` | 1 test case each |
| Debug `[threads]`, `[shutdown]` (several runs) | pass |
| Release `[hrtf-switch]` | 12 cases, 143 assertions, pass |
| Debug `[hrtf-switch]` | 12 cases, 143 assertions, pass |
| Full Debug suite | 253 of 253 test cases pass (309,062 assertions) |
| Release ctest (`build-release`) | 100% passed, 253 of 253 |
| `SPATIALCORE_TEST_TIMEOUT_SCALE` greps | 2 (need 1); `std::abort` 1 (need 1) |
| `git diff BASE..HEAD -- include/SpatialCore/Engine/RenderEngine.h` | empty: no public declaration removed, `RenderBlockContext` untouched |
| Classification vs log | 0 reports in the log, 0 `report` lines, last line `phase3-races: 0` |

The known exit-time "Leaked objects detected ... class FFT" message appears as before and is not from this plan.

## Files Created/Modified

- `tests/Engine/ProfileSwitchTests.cpp` - watchdog and scale helpers, the three concurrency cases, and `timeoutScale()` applied to the idle waits of the Plan 03-06 cases

## Decisions Made

See `key-decisions` in the frontmatter. The two that matter for later plans: the prescribed `[threads]` scenario is a good deadlock and finite-audio check but a weak handoff check, so `[churn]` carries the handoff proof; and the Phase 5 TSAN gate should build with optimization (RelWithDebInfo), because an `-O0` instrumented SADIE load is slow enough to trip the engine's 15 s shutdown wait.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] ThreadSanitizer tree built as RelWithDebInfo, not Debug**
- **Found during:** Task 2
- **Issue:** The Debug `-O0` TSAN tree ran the whole tag to a JUCE assertion (exit 133) after about 5 minutes: with instrumentation a SADIE load takes tens of seconds (measured: about 170 s for the 4 loads of the `[prepare]` case), so the engine's own 15 s `stopThread` in `prepare()` gave up and force-killed the loader, and the Plan 03-06 waits (20 to 60 s, unscaled) timed out. No ThreadSanitizer report was involved; the log was just cut short. The plan treats timing failures under TSAN as instrumentation effects, but an abort cannot be classified.
- **Fix:** Built `/tmp/sc-tsan-o2` as RelWithDebInfo with the same flags and ran the whole tag there (exit 0, 0 reports). Also ran the cases that never destroy an engine mid-load on the Debug TSAN tree (0 reports). The 15 s bound in the engine was not touched.
- **Files modified:** none (verification build outside the repository)
- **Verification:** both logs above
- **Committed in:** n/a

**2. [Rule 3 - Blocking] Idle waits of the Plan 03-06 cases scaled**
- **Found during:** Task 2
- **Issue:** Same cause as above; the fixed waits could not be stretched for a sanitizer run.
- **Fix:** Moved `timeoutScale()` ahead of the 03-06 cases and multiplied their literal `waitForHRTFProfileIdle` timeouts by it. At the default scale of 1 nothing changes.
- **Files modified:** `tests/Engine/ProfileSwitchTests.cpp`
- **Verification:** Debug and Release `[hrtf-switch]` and full suites pass at scale 1; the tag passes under TSAN at scale 10
- **Committed in:** `9522c98`

**3. [Rule 2 - Missing critical] Added `[hrtf-switch][churn]`**
- **Found during:** Task 1
- **Issue:** With the prescribed 20-120 ms gaps, requests arrive faster than a SOFA load completes, so nearly every result is superseded before it is published. Per-profile counters showed the render thread on profile 0 for all but 1 to 18 blocks, with one claim at the very end. That proves no deadlock and finite audio, but exercises the claim, crossfade-end and free-flag handoff only once, which is the part T-03-26 is about.
- **Fix:** A fourth concurrency scenario (third new test case) where every switch completes under a paced render thread and the next request must wait for the faded-out renderer to come back free. The prescribed `[threads]` case is kept exactly as specified.
- **Files modified:** `tests/Engine/ProfileSwitchTests.cpp`
- **Verification:** 16 of 16 switches in every Debug, Release and TSAN run, audio above 1e-4 on every SOFA profile, no non-finite sample
- **Committed in:** `6bf0b8d`

**4. [Rule 3 - Blocking] `[shutdown]` is generated over two delays**
- **Found during:** Task 1
- **Issue:** "Destroy the engine at once" can finish before the worker thread has started the parse (2.5 ms), which does not test shutdown mid-load.
- **Fix:** The case runs twice via `GENERATE (0, 40)`: at once, and after 40 ms when the SADIE parse is running. It is still one test case in `--list-tests`.
- **Committed in:** `6bf0b8d`

---

**Total deviations:** 4 auto-fixed (3 blocking, 1 missing critical). **Impact on plan:** no engine code changed and no bound was loosened; the additions make the proof stronger and let the sanitizer run complete.

## Issues Encountered

- A Debug `-O0` instrumented load is slow enough to hit the engine's 15 s `stopThread` limit, which on a JUCE debug build force-kills the thread and trips an assertion. This is not reachable in a normal Debug or Release run (SADIE loads in under a second), but it is worth knowing for the Phase 5 TSAN gate: build it with optimization.
- Largest sample in `[threads]` varies from 0.0005 to 0.34 between runs because the flat-out render thread spends only a handful of blocks on the final SOFA profile; this is expected and the bound that matters (below 4.0) holds with wide margin.

## Known Stubs

None.

## Threat Flags

None. No new network endpoint, auth path, file access or schema surface; tests only.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Plan 03-08 can proceed. The switching loader, mailbox and free-flag protocol is proven under real threads and under ThreadSanitizer, so Plan 03-09 can build the Simple <-> HRTF and sample-based crossfade on it.
- Phase 5 owns the TSAN gate; no pre-existing report was observed in `[hrtf-switch]`, so nothing is handed over from this plan beyond the build advice above.

## Self-Check: PASSED

- `tests/Engine/ProfileSwitchTests.cpp` exists; commits `6bf0b8d` and `9522c98` exist; both files named in `key-files` exist.
- Re-run of the Task 1 and Task 2 acceptance criteria after the last commit: all pass (see Verification results).

---
*Phase: 03-binaural-defects-hrtf-packaging*
*Completed: 2026-10-04*
