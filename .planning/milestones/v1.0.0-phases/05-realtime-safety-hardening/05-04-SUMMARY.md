---
phase: 05-realtime-safety-hardening
plan: 04
subsystem: testing
tags: [threadsanitizer, realtime-safety, trajectory, std-atomic, lock-free, catch2, build-tsan]

requires:
  - phase: 05-realtime-safety-hardening
    provides: "Plans 05-01 to 05-03: allocation and lock traps, shared dependency cache, SPATIALCORE_TEST_TIMEOUT_SCALE use"
provides:
  - "build-tsan/: third build tree (RelWithDebInfo, -fsanitize=thread on C, C++ and both linkers), git-ignored, no allocation trap, [rt][canary] skips with exit 4"
  - "TrajectoryEngine::finalAz_/finalEl_/finalDist_ as std::atomic<float>[12] behind an is_always_lock_free static_assert; tick() stores each finished value once; active_ release/acquire"
  - "[trajectory][threads][tsan]: concurrent tick, read and reset case; red at BASE, clean after the fix"
  - "[tsan] tag on 20 [hrtf-switch], 4 [sc16], 4 [sc17] cases and the new threads case: 29 cases, one-command sanitizer gate"
  - "[trajectory][step]: measured per-shape largest 60 Hz step table, pinned as kMaxAzStepDeg 179, kMaxElStepDeg 180, kMaxDistStep 0.51"
affects: [05-05, 05-06, 05-07, 05-08, RTSF-05, VERIFY-02, OpenSpatialDelay migration]

plan_head_before: ea7548137daaa2e3b16cdac346b293b8a302a035
plan_head_after: fb8f5ba3e10c0b2f94b019fad4575ae0ec5bd900

actuals:
  tokens: 6631
  tasks: 2
  commits: 2

tech-stack:
  added: []
  patterns:
    - "Compute into locals, store each published value once (relaxed), then publish with a release store of a flag read with acquire"
    - "Sanitizer gate = exit status 0 AND zero 'WARNING: ThreadSanitizer' lines, run with output redirected to a log (never through tee)"
    - "[tsan] tag marks the thread-interaction subset; the whole suite under TSan is reserved for the phase-end gate"

key-files:
  created: []
  modified:
    - .gitignore
    - include/SpatialCore/Trajectory/TrajectoryEngine.h
    - src/Trajectory/TrajectoryEngine.cpp
    - tests/Trajectory/TrajectoryTests.cpp
    - tests/Engine/ProfileSwitchTests.cpp
    - tests/Engine/RenderEngineTests.cpp

key-decisions:
  - "Lock-free relaxed atomic floats with compute-then-store-once, not a mutex, spinlock or seqlock; the reader stays wait-free (DR-1)"
  - "Pin the mixed-tick bound at the measured overall maxima (179 deg, 180 deg, 0.51) as the plan specified; four shapes (Circle, Figure8, Helix, Spiral) make large steps of their own, so for them a mixed reading is no worse than the shape's own jump"
  - "Tag only the TEST_CASE tag strings with [tsan]; the three runWithWatchdog label strings that repeat the [hrtf-switch] text are diagnostics and are left alone"

patterns-established:
  - "BASE red run first: the race test was built and run in build-tsan against the unmodified engine before the fix was written"

requirements-completed: [RTSF-02, RTSF-05]

coverage:
  - id: D1
    description: "TrajectoryEngine final positions are lock-free atomic floats written once per value; the concurrent tick/read/reset test is clean under ThreadSanitizer"
    requirement: "RTSF-02"
    verification:
      - kind: unit
        ref: "build-tsan/tests/SpatialCoreTests [trajectory][threads] (exit 0, 0 warnings)"
        status: pass
      - kind: unit
        ref: "build/tests/SpatialCoreTests [trajectory],[route] and build-release same (44 cases)"
        status: pass
    human_judgment: false
  - id: D2
    description: "build-tsan third tree exists, is git-ignored, has no allocation trap, and [rt][canary] skips there"
    requirement: "RTSF-05"
    verification:
      - kind: other
        ref: "./build-tsan/tests/SpatialCoreTests \"[rt][canary]\" exits 4; build-tsan/tests/libSpatialCoreRTGuard.dylib absent; git status --ignored shows build-tsan/"
        status: pass
    human_judgment: false
  - id: D3
    description: "The [tsan] subset (29 cases) is the one-command sanitizer gate and is clean"
    requirement: "RTSF-05"
    verification:
      - kind: unit
        ref: "SPATIALCORE_TEST_TIMEOUT_SCALE=10 TSAN_OPTIONS=... ./build-tsan/tests/SpatialCoreTests \"[tsan]\" (exit 0, 0 warnings, 860 s)"
        status: pass
    human_judgment: false
  - id: D4
    description: "The largest per-tick change of azimuth, elevation and distance for every shape is measured and pinned"
    requirement: "RTSF-02"
    verification:
      - kind: unit
        ref: "tests/Trajectory/TrajectoryTests.cpp#TrajectoryEngine: the largest change between two 60 Hz ticks is pinned for every shape (RTSF-02 mixed-tick bound)"
        status: pass
    human_judgment: false
  - id: D5
    description: "Whether a mixed reading (azimuth from one tick, elevation from the next) is audible for Circle, Figure8, Helix and Spiral, whose own steps reach about 180 deg or 0.5 per tick"
    verification: []
    human_judgment: true
    rationale: "Audibility of a mixed reading is a listening judgment no test asserts; the size is pinned, the audibility is not. The consumer flow in docs/integration-guide.md (copy positions on the message thread) does not read these arrays on the audio thread at all."

duration: 36min
completed: 2026-10-06
status: complete
---

# Phase 5 Plan 04: ThreadSanitizer tree and the TrajectoryEngine race fix Summary

**build-tsan (RelWithDebInfo, -fsanitize=thread) reported the SpatialCore#18 trajectory race on unmodified code (exit 134), and reports nothing after lock-free `std::atomic<float>` finals written once per value; a 29-case `[tsan]` subset makes the sanitizer gate one clean command.**

## Performance

- **Duration:** 36 min
- **Started:** 2026-10-06T00:33:57Z
- **Completed:** 2026-10-06T01:10:52Z
- **Tasks:** 2
- **Files modified:** 6 (plus this SUMMARY and planning state)

## Accomplishments

- Third build tree `build-tsan/`: configured with `-DCMAKE_BUILD_TYPE=RelWithDebInfo`, `-fsanitize=thread -fno-omit-frame-pointer` on C and C++ flags, `-fsanitize=thread` on the exe and shared linker flags, and the same `FETCHCONTENT_FULLY_DISCONNECTED` plus three `FETCHCONTENT_SOURCE_DIR_*` flags from `/private/tmp/sc-kemar-only-validate/_deps`. `SpatialCoreTests` and `SpatialCoreUITests` built in 1 min 39 s. No `-ffast-math`. `build-tsan/tests/libSpatialCoreRTGuard.dylib` does not exist and `./build-tsan/tests/SpatialCoreTests "[rt][canary]"` exits 4 (skipped: "allocation/lock trap available on macOS only, and not under a sanitizer"). `.gitignore` gained one CRLF line (`git ls-files --eol` still `i/crlf w/crlf`, diff is one insertion).
- Race fixed: `finalAz_`, `finalEl_`, `finalDist_` are `std::atomic<float>[12]` under `static_assert (std::atomic<float>::is_always_lock_free, ...)`. `tick()` computes each value into a local (the Random azimuth wrapped once, elevation and distance clamped) and stores it exactly once with relaxed order; `active_` is then stored with release and `isActive` loads with acquire. `reset()` stores `active_` false (release) then the three values. Getter signatures are unchanged, and `examples/` and `tests/Engine/ControlRouteTests.cpp` are untouched. The class comment now states the precise thread contract and no longer says the audio thread reads final positions.
- One-command sanitizer gate: `[tsan]` marks 29 cases (20 `[hrtf-switch]`, 4 `[sc16]`, 4 `[sc17]`, 1 `[trajectory][threads]`).
- Mixed-tick size is a number: `[trajectory][step]` measured and pinned (table below).

## D-07 old-behaviour evidence (BASE `ea75481`)

New case `[trajectory][threads][tsan]` run in `build-tsan` against the unmodified engine
(`SPATIALCORE_TEST_TIMEOUT_SCALE=10 TSAN_OPTIONS="halt_on_error=0 second_deadlock_stack=1" ./build-tsan/tests/SpatialCoreTests "[trajectory][threads]" > /tmp/sc-p5-traj-base.log 2>&1`):

- **Exit status 134**, 1 `WARNING: ThreadSanitizer: data race` line (`ThreadSanitizer: reported 1 warnings`).
- Stack top 1 (write, thread T2): `spatialcore::TrajectoryEngine::tick(int, ObjectInput const&, float)` at `TrajectoryEngine.cpp:88`.
- Stack top 2 (previous read, thread T1): `spatialcore::TrajectoryEngine::getFinalAz(int) const` at `TrajectoryEngine.h:95`.
- The test's own range check also failed: `violations := 892` out of `passesDuringTicking := 2918251`, from the reader seeing values outside [-180, 180] (the Random pre-wrap azimuth) while the timer thread ticked.

After the fix, the same command: **exit 0, 0 warnings**, "All tests passed (2 assertions in 1 test case)" (`/tmp/sc-p5-traj.log`).

## Pinned per-tick step table (`[trajectory][step]`)

Shapes 1..13, speeds 1 and 4, forward and reverse, origin 20/5/0.5, 600 ticks at 1/60 s, Random seeded 1234. Largest change between two consecutive ticks:

| Shape | az (deg) | el (deg) | dist |
|---|---|---|---|
| 1 Bounce | 24.000 | 8.000 | 0.000 |
| 2 Circle | 178.565 | 0.000 | 0.204 |
| 3 Cross | 48.000 | 32.000 | 0.000 |
| 4 Figure8 | 174.308 | 0.000 | 0.203 |
| 5 Heart | 26.170 | 0.000 | 0.277 |
| 6 Helix | 24.000 | 179.877 | 0.000 |
| 7 Infinity | 22.132 | 0.000 | 0.188 |
| 8 Line | 23.139 | 0.000 | 0.084 |
| 9 Orbit | 24.000 | 0.000 | 0.000 |
| 10 Random | 14.633 | 16.387 | 0.100 |
| 11 Spiral | 86.538 | 0.000 | 0.506 |
| 12 Square | 50.890 | 0.000 | 0.122 |
| 13 Triangle | 56.314 | 0.000 | 0.173 |
| **Overall** | **178.565** | **179.877** | **0.50621** |

Pinned constants (overall maxima rounded up): `kMaxAzStepDeg = 179`, `kMaxElStepDeg = 180`, `kMaxDistStep = 0.51`.

Research assumption A4 ("a mixed reading is inaudible") is only partly borne out by the measurement. For the nine smooth shapes (1, 3, 5, 7, 8, 9, 10, 12, 13) the largest step is 56.3 deg, 32 deg and 0.28. Circle, Figure8, Helix and Spiral make steps of their own that reach about 180 deg, 180 deg or 0.5 per tick, so a mixed reading of those is no worse than the jump the shape already makes. The pin is therefore a "no worse than the shape's own motion" ceiling rather than a small number. If a mixed reading is ever judged audible, the escape named in RESEARCH F8 is a per-object seqlock (lock-free, not wait-free). The documented consumer flow never reads these arrays on the audio thread.

## `[tsan]` subset gate

`SPATIALCORE_TEST_TIMEOUT_SCALE=10 TSAN_OPTIONS="halt_on_error=0 second_deadlock_stack=1" ./build-tsan/tests/SpatialCoreTests "[tsan]" > /tmp/sc-p5-tsan.log 2>&1`

- **29 test cases, 386 assertions, all passed.**
- **Exit status 0, 0 `WARNING: ThreadSanitizer` lines.**
- **Run time 860 s (14 min 20 s)**, run alone with no other build in parallel.

## Test results

| Tree | Executable | Result |
|---|---|---|
| build-tsan (RelWithDebInfo + TSan) | `SpatialCoreTests "[trajectory][threads]"` | 1 case, exit 0, 0 warnings |
| build-tsan | `SpatialCoreTests "[tsan]"` | 29 cases, 386 assertions, exit 0, 0 warnings |
| build (Debug) | `SpatialCoreTests` | 375 cases, 328,205 assertions, all passed (expected "Leaked objects detected: 4 FFT" exit noise, D-08) |
| build (Debug) | `SpatialCoreUITests` | 14 cases, 133 assertions, all passed |
| build (Debug) | `SpatialCoreTests "[trajectory],[route]"` | 44 cases, 14,061 assertions, passed (after Task 1) |
| build-release | `ctest --test-dir build-release/tests` | 389 of 389 passed; 14 `ui:` tests listed |
| build-release | `SpatialCoreTests "[trajectory],[route]"` | 44 cases, 14,067 assertions, passed (after Task 1) |

BASE Release ctest count was 343 after 05-01; this plan adds 2 cases (`[trajectory][threads][tsan]` and `[trajectory][step]`) on top of what earlier plans left, giving 389 now.

## Task Commits

1. **Task 1: Tracer, sanitizer tree reports the race on today's code and nothing after the atomic fix** - `52b089a` (fix)
2. **Task 2: One-command `[tsan]` subset and the pinned per-tick step bound** - `fb8f5ba` (test)

**Plan metadata:** the docs commit that carries this SUMMARY, STATE.md and ROADMAP.md.

Tracer feedback gate: auto mode off, `human_verify_mode` default `end-of-phase`, tracer `<verify>` automated-only, so the verify was re-run end to end (TSan clean, Debug and Release trajectory and route runs green) and the plan expanded.

## Files Created/Modified

- `.gitignore` - `build-tsan/` (CRLF kept)
- `include/SpatialCore/Trajectory/TrajectoryEngine.h` - atomic finals, lock-free `static_assert`, acquire `isActive`, thread-contract comment
- `src/Trajectory/TrajectoryEngine.cpp` - compute-then-store-once `tick()` and `reset()`, release store of `active_`
- `tests/Trajectory/TrajectoryTests.cpp` - `[trajectory][threads][tsan]` and `[trajectory][step]` (only additions)
- `tests/Engine/ProfileSwitchTests.cpp` - `[tsan]` on 20 `[hrtf-switch]` TEST_CASE tag strings, one comment
- `tests/Engine/RenderEngineTests.cpp` - `[tsan]` on the 4 `[sc16]` and 4 `[sc17]` cases, one comment

## Decisions Made

- Relaxed atomic floats plus a release/acquire flag, no lock of any kind: the audio-side read stays wait-free (DR-1). No stored position value changed; the existing `[trajectory]` expectations are untouched and pass.
- Pinned the step bound at the measured overall maxima as the plan specified, and recorded in the test comment and here that four shapes already jump by that much on their own.
- Left the three `runWithWatchdog ("[hrtf-switch]...", ...)` label strings unchanged (see deviation 1).

## Deviations from Plan

### Plan wording that did not match the tree

**1. [Acceptance wording] `[hrtf-switch]` count is 20 TEST_CASE tag strings, not 19, and the literal grep equality cannot hold**
- **Found during:** Task 2
- **Issue:** The plan says 19 `[hrtf-switch]` cases and requires `grep -c "\[hrtf-switch\]"` to equal `grep -c "\[hrtf-switch\].*\[tsan\]"` in `tests/Engine/ProfileSwitchTests.cpp`. The file has 20 TEST_CASE tag strings with `[hrtf-switch]` (all 20 now carry `[tsan]`) and 3 further lines (`runWithWatchdog ("[hrtf-switch][threads]" ...`, `[churn]`, `[shutdown]`) that repeat the text inside a watchdog diagnostic label. The two raw greps print 23 and 20.
- **Fix:** Tagged every TEST_CASE tag string (`grep -c '^           "\[hrtf-switch\].*\[tsan\]")$'` prints 20 of 20) and left the labels alone, per the plan's "change nothing else in either file". The intent of the criterion (every hrtf-switch tag string carries tsan) holds.
- **Effect on the count criterion:** the `[tsan]` subset lists 29 cases, above the plan's "at least 28" (the extra one is the 20th `[hrtf-switch]` case).
- **Files modified:** none beyond the planned tags
- **Committed in:** `fb8f5ba`

---

**Total deviations:** 1 (acceptance wording only, no code deviation)
**Impact on plan:** None on behaviour. No Rule 1-4 fixes were needed.

## Issues Encountered

- Claude Code's file-read hook returned only line 1 for several reads; re-reading with an explicit `offset` and `limit` returned the full files. No effect on the work.
- Debug runs print "Leaked objects detected: N instance(s) of class FFT" and a JUCE assertion line at exit (D-08, known and expected); both Debug executables still exit 0.

## Known Stubs

None. No placeholder values, empty data sources or TODO text were added.

## Threat Flags

None. The plan's own register covers the new surface: T-05-09 (torn or unwrapped position) is mitigated by the lock-free atomic stores and the TSan-clean concurrent test; T-05-10 (false clean gate) is mitigated because the gate requires both exit status 0 and zero `WARNING: ThreadSanitizer` lines, and the BASE run proved the tree reports.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- `build-tsan/` and the `[tsan]` gate command are ready for the later Phase 5 plans and the phase-end gate. A full-suite TSan run (about 30 minutes) is still reserved for `/gsd-verify-work` and release tagging; the README does not yet record the `[tsan]` tag list and run time, and no plan in this wave owns that edit (RESEARCH Pitfall 5 asks for it).
- The `TrajectoryEngine` public array type change (`float[]` to `std::atomic<float>[]`) reaches a consumer only after its submodule pin bump; OpenSpatialDelay keeps its own copy until migration.

## Self-Check: PASSED

- All six modified source and test files exist; `build-tsan/tests/SpatialCoreTests` exists and `build-tsan/` is git-ignored.
- Commits `52b089a` and `fb8f5ba` exist and are found by `git log --all --grep="05-04"`.
- Task 1 acceptance: 3 `std::atomic<float> final` lines, 1 `is_always_lock_free`, 2 `finalAz_[t]` lines in `tick()`, no removed lines in the test diff, `examples/` and `ControlRouteTests.cpp` diff empty, `.gitignore` still CRLF with one `build-tsan/` line. All PASS.
- Task 2 acceptance: `[tsan]` lists 29 cases (at least 28), `[trajectory][step]` exits 0, `kMaxAzStepDeg` appears twice, subset clean with run time recorded. The literal `[hrtf-switch]` grep equality is recorded as deviation 1 (20 of 20 tag strings carry `[tsan]`).

---
*Phase: 05-realtime-safety-hardening*
*Completed: 2026-10-06*
