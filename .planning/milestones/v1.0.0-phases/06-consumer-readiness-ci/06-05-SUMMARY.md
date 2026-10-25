---
phase: 06-consumer-readiness-ci
plan: 05
subsystem: infra
tags: [github-actions, macos-26, catch2, threadsanitizer, git-lfs, ci]

requires:
  - phase: 06-consumer-readiness-ci
    provides: "06-04 consumer harness (examples/harness/scripts/build-consumer.sh and its sound check)"
provides:
  - "macos-gate CI job: Debug and Release build of every target, both test executables, Release ctest from the build root with --no-tests=error, ui: listing check, consumer harness and sound check, on macos-26"
  - "realtime-nightly CI job: README realtime-safety check steps 1-5 (schedule 03:17 UTC and workflow_dispatch input realtime, default true)"
  - "LFS handling for CI: lfs:false checkout, per-OS .git/lfs cache, portable HDF5 signature and size guard"
  - "Read-only token, no secrets, first-party actions pinned to commit SHAs"
affects: [06-06 linux-and-windows-jobs, 06-07 coverage, 06-10 branch-protection-and-nightly-on-main]

actuals:
  tokens: 4700
  tasks: 2
  commits: 6

plan_head_before: 81f85902b15de02806718bcb7f2b6bd8f6f8c527
plan_head_after: dbedbf67f0f0622fabb2545947fd9872f5a1e7a0

tech-stack:
  added: [actions/checkout v7.0.1, actions/cache v6.1.0, actions/upload-artifact v7.0.2]
  patterns:
    - "Each CI run step writes noisy test output to a file and prints only the verdict, tail and failure blocks"
    - "Workflow expressions enter scripts only through env: entries"
    - "Release and ThreadSanitizer trees reuse the Debug tree's FetchContent sources with FETCHCONTENT_FULLY_DISCONNECTED=ON"

key-files:
  created: []
  modified:
    - .github/workflows/ci.yml
    - tests/OSC/ADMOSCSenderTests.cpp

key-decisions:
  - "Debug test output goes to a file, not the runner's log pipe: libmysofa's Debug parser trace (about 830000 lines) through the pipe stalls the test process and starves wall-clock tests"
  - "A wall-clock case that needs 16 ms spacing follows absolute deadlines with a spin wait, because a 16 ms sleep takes about 75 ms on the macos-26 runner; the assertions are unchanged"
  - "Realtime job keeps SPATIALCORE_TEST_TIMEOUT_SCALE=10 per step and all five README steps; measured runner times fit in 360 minutes (job took 100 min)"

patterns-established:
  - "Bracket-class grep (WARNING: ThreadSanitize[r]) so a job log that echoes its own script never matches the pattern"
  - "Evidence for a pushed commit may come from the workflow_dispatch run of that commit when the push run was cancelled; the job is the same"

requirements-completed: [CI-01]

coverage:
  - id: D1
    description: "macos-gate runs the real suite (Debug both executables, Release ctest of 416 tests, ui: listing) and the consumer harness against JUCE 9.0.0 on macos-26 and is green on a pushed commit"
    requirement: CI-01
    verification:
      - kind: integration
        ref: "https://github.com/AndrewRahman/SpatialCore/actions/runs/39053183247 (push of 567bc52) and https://github.com/AndrewRahman/SpatialCore/actions/runs/39058330902 job macos-gate (dispatch of dbedbf6): conclusion success, 'JUCE checkout: 9.0.0', '100% tests passed out of 416', 'HarnessCheck: PASS (23 formats, 12 algorithms, profile 5 embedded)'"
        status: pass
    human_judgment: false
  - id: D2
    description: "realtime-nightly runs README realtime-safety check steps 1-5 on macos-26 and a dispatched run is green with zero ThreadSanitizer warnings"
    requirement: CI-01
    verification:
      - kind: integration
        ref: "https://github.com/AndrewRahman/SpatialCore/actions/runs/39058330902 job realtime-nightly: [rt] 2521 assertions in 47 cases, [tsan] exit=0 warnings=0, UI exit=0 warnings=0, whole suite exit=0 warnings=0"
        status: pass
    human_judgment: false
  - id: D3
    description: "Workflow security posture and stale-comment cleanup: read-only token, no secrets, no pull_request_target, 5 of 5 actions SHA-pinned first-party, no expression interpolated into a run body"
    requirement: CI-01
    verification:
      - kind: other
        ref: "Task 1 acceptance grep set on .github/workflows/ci.yml (pull_request_target 0, secrets. 0, contents: read 1, uses 5 = pinned 5, stale-text 0, expression-in-run none)"
        status: pass
    human_judgment: false
  - id: D4
    description: "The nightly schedule actually fires every night"
    verification: []
    human_judgment: true
    rationale: "GitHub fires scheduled workflows only from the default branch; it cannot be observed until this file is on main (Plan 06-10). The job itself is proven by the dispatched run."

duration: 3h 40m
completed: 2026-10-17
status: complete
---

# Phase 6 Plan 05: macOS CI gate and nightly realtime job Summary

**GitHub CI now runs the whole SpatialCore suite on macos-26 against JUCE 9.0.0 (416 Release ctest tests, both Debug executables, the consumer harness) and runs README's ThreadSanitizer and allocation-trap check on demand and nightly, both proven green on real runs.**

## Performance

- **Duration:** about 3h 40m wall clock (first CI push 16:35 UTC to the green dispatched run at 20:10 UTC, including runner waits and the pause described under Issues Encountered)
- **Tasks:** 2
- **Files modified:** 2 (`.github/workflows/ci.yml`, `tests/OSC/ADMOSCSenderTests.cpp`)

## Accomplishments

- `macos-gate` job (every push to any branch, every PR to main): Debug build of every target, `SpatialCoreTests` and `SpatialCoreUITests` run, Release build, `ctest --test-dir build-release --no-tests=error`, a check that `ui:` tests are listed (17), then `build-consumer.sh` at the commit under test.
- `realtime-nightly` job: `[rt]` in Release, a `build-tsan` tree, then the `[tsan]` subset, the UI suite and the whole suite, each failing unless exit status is 0 and the ThreadSanitizer warning count is 0; logs uploaded as artifact `tsan-logs` (14 days).
- HRTF data reaches each job from a `.git/lfs` cache with a portable guard (HDF5 signature `89484446`, size at least 100000 bytes, exactly 5 files).
- Stale comments and the old build-root `ctest` that found no tests are gone (D-12).

## CI Evidence

| Run | Commit | Event | Result |
|---|---|---|---|
| [39050058846](https://github.com/AndrewRahman/SpatialCore/actions/runs/39050058846) | bede5d4 | push | failure (red, see below) |
| [39051694753](https://github.com/AndrewRahman/SpatialCore/actions/runs/39051694753) | 500be34 | push | failure (red, see below) |
| [39053075549](https://github.com/AndrewRahman/SpatialCore/actions/runs/39053075549) | 790f6a2 | push | success (temporary timing diagnostic commit) |
| [39053183247](https://github.com/AndrewRahman/SpatialCore/actions/runs/39053183247) | 567bc52 | push | **success - the tracer's green run** |
| [39055582384](https://github.com/AndrewRahman/SpatialCore/actions/runs/39055582384) | dbedbf6 | push | cancelled by the orchestrator (GitHub-minutes concern) |
| [39055587059](https://github.com/AndrewRahman/SpatialCore/actions/runs/39055587059) | dbedbf6 | workflow_dispatch | cancelled by the orchestrator (same reason) |
| [39058330902](https://github.com/AndrewRahman/SpatialCore/actions/runs/39058330902) | dbedbf6 | workflow_dispatch (`-f realtime=true`) | **success, both jobs** |

Run 39058330902: `macos-gate` 33 min (18:29:55 to 19:03:24 UTC), `realtime-nightly` 100 min (18:29:53 to 20:10:47 UTC). Saved logs: `/tmp/sc-p6-05-gate.log` (run 39053183247), `/tmp/sc-p6-05-gate-dbedbf6.log`, `/tmp/sc-p6-05-realtime.log` (run 39058330902).

### macos-gate, per step (run 39058330902, dbedbf6, the final workflow)

| Step | Time |
|---|---|
| Checkout, LFS cache restore, git lfs pull, HRTF guard, toolchain | 3 s, 2 s, 1 s, 0 s, 2 s |
| Configure Debug (JUCE checkout 9.0.0 verified) | 67 s |
| Build Debug, every target | 301 s |
| Debug SpatialCoreTests (exit status 0, 828893 log lines) | 758 s |
| Debug SpatialCoreUITests (exit status 0) | 21 s |
| Configure Release (reusing downloaded sources) | 24 s |
| Build Release, every target | 356 s |
| Release ctest from the build root | 85 s |
| ui: listing check (17 tests) | 0 s |
| Consumer harness and sound check | 369 s |

ctest total N = **416**, "100% tests passed out of 416". Harness line: `HarnessCheck: PASS (23 formats, 12 algorithms, profile 5 embedded)`. Guard line: `All 5 SOFA files resolved to real LFS content.` The tracer's run 39053183247 shows the same: 416 tests, 89.83 s of test time, Debug SpatialCoreTests 726 s.

### realtime-nightly, per step (run 39058330902) beside the local times

| README step | Runner | Local (README) | Exit status | ThreadSanitizer warnings |
|---|---|---|---|---|
| Configure and build Release | 456 s | n/a | 0 | n/a |
| 1: `[rt]` allocation and lock trap, Release | 46 s (2521 assertions in 47 cases) | n/a | 0 | n/a |
| 2: configure and build `build-tsan` | 406 s | n/a | 0 | n/a |
| 3: `[tsan]` subset | 1887 s (628 assertions in 40 cases) | 992 s | 0 | 0 |
| 4: UI suite under ThreadSanitizer | 58 s (152 assertions in 17 cases) | 45 s | 0 | 0 |
| 5: whole SpatialCoreTests suite | 3183 s (399 cases, 355 passed, 44 skipped) | 1938 s | 0 | 0 |

The `realtime` workflow input was accepted by GitHub (`gh workflow run ... -f realtime=true` returned the run URL); no fallback dispatch was needed. The `tsan-logs` artifact (7048 bytes) was uploaded. The 44 skipped cases in the whole-suite log are skips reported by Catch2 itself; CI skips nothing.

### Action pins

| Action | Version | Commit SHA |
|---|---|---|
| actions/checkout | v7.0.1 | 3d3c42e5aac5ba805825da76410c181273ba90b1 |
| actions/cache | v6.1.0 | 55cc8345863c7cc4c66a329aec7e433d2d1c52a9 |
| actions/upload-artifact | v7.0.2 | cf430e030ddbb5b0abf93d22962f4752f3646cd9 |

## Task Commits

1. **Task 1 (tracer): macOS gate runs the real suite and the harness** - `bede5d4` (ci); two follow-up fixes found by red runs: `500be34` (ci, Debug output to a file) and `567bc52` (test, deadline pacing of the sender rate case); a temporary diagnostic `790f6a2` and its revert `4e539c4`
2. **Task 2: nightly realtime-safety job** - `dbedbf6` (ci)

**Plan metadata:** the docs commit that adds this SUMMARY.

## Files Created/Modified

- `.github/workflows/ci.yml` - rewritten: `macos-gate` and `realtime-nightly`, LFS cache and guard, read-only token, SHA-pinned actions, concurrency group, `SPATIALCORE_TEST_TIMEOUT_SCALE`.
- `tests/OSC/ADMOSCSenderTests.cpp` - the wall-clock rate case follows absolute 16 ms deadlines with a spin wait (assertions unchanged).

## Decisions Made

See `key-decisions` above. The Debug output redirect and the deadline pacing are both fixes for the runner being slower or coarser, as the plan's ground rules prescribe, and no test was weakened.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Debug test output starved wall-clock tests through the runner's log pipe**
- **Found during:** Task 1, run 39050058846 (bede5d4): `ADMOSCSender` rate case failed (`tests/OSC/ADMOSCSenderTests.cpp:288`, 60 calls took 5.4 s, not 1 s) while libmysofa streamed its verbose Debug trace into the log pipe.
- **Fix:** the Debug test steps write to `$RUNNER_TEMP/debug-*.log` and print only exit status, the Catch2 summary tail and failure blocks.
- **Files modified:** `.github/workflows/ci.yml`
- **Committed in:** `500be34`

**2. [Rule 1 - Bug] Sender rate test assumed a precise 16 ms sleep**
- **Found during:** Task 1, run 39051694753 (500be34): the same case still failed with output no longer piped. A temporary diagnostic (`790f6a2`, reverted by `4e539c4`) measured that on macos-26 a 16 ms sleep takes about 75 ms (60 sleeps took 4.3 to 4.6 s while 60 loopback UDP sends took 1 ms), so `tick()` was called below 30 Hz and the test could not see the sender's 30 Hz clock.
- **Fix:** the calls follow absolute 16 ms deadlines with a spin wait; the assertions are unchanged. The case passes on the runner (runs 39053183247 and 39058330902).
- **Files modified:** `tests/OSC/ADMOSCSenderTests.cpp`
- **Committed in:** `567bc52`

**3. [Plan text vs tool output] The plan's literal ctest string does not exist on this runner**
- **Found during:** Task 1 verification. The plan's check greps `100% tests passed, 0 tests failed out of`. CMake 4.4.3 on macos-26 prints `100% tests passed out of 416` (no failure count when there are none).
- **Handling:** the intent (ctest ran N above 0 tests and all passed) is verified with `grep -E "100% tests passed(, 0 tests failed)? out of [1-9]"`, which passes on both saved gate logs. The workflow itself needs no change: `--no-tests=error` and ctest's exit status are the gate. 06-06 and later plans that grep for this line should use the same pattern.
- **Files modified:** none

**4. [Plan wording] Push-event run for the final HEAD**
- **Found during:** Task 1 and 2 verification. Task 1's automated check looks for a successful push run for HEAD, and Task 2's for a dispatched run. The push run of dbedbf6 (39055582384) was cancelled by the orchestrator. The dispatched run of dbedbf6 (39058330902) ran `macos-gate` on that same commit and is green, and the push run of 567bc52 (39053183247, the previous commit; dbedbf6 changed only the workflow's realtime job and the Debug steps' summary tail) is green. The push event was not re-run, to avoid a duplicate run answering the same question.

**Total deviations:** 4 (1 blocking fix, 1 bug fix, 2 plan-text mismatches). **Impact on plan:** the two fixes were needed for a green run and weaken nothing; the plan-text items change evidence wording only.

## Issues Encountered

- Red runs 39050058846 and 39051694753: cause and fixes above (deviations 1 and 2).
- Cancelled runs 39055582384 and 39055587059: the orchestrator cancelled both at about 25 minutes over a GitHub-minutes concern. That concern was resolved (SpatialCore is a public repository, its standard runners are free, and the user approved using its CI). Execution paused, then continued by dispatching the realtime job once more (run 39058330902), which is the green evidence for dbedbf6. No code changed during the pause.
- The temporary diagnostic run 39053075549 (790f6a2) is a green run of a commit that was reverted next; it is not evidence for anything.

## Known Stubs

None.

## Threat Flags

None. The new surface (a workflow with a read-only token, no secrets, only `pull_request` for outside code, first-party SHA-pinned actions) is the plan's own threat model (T-06-12 to T-06-17), mitigated as planned.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Plans 06-06 (Linux and Windows jobs) and 06-07 (coverage) add jobs to the same file; keep the acceptance greps true (every `${{ }}` in an env, key, group or path line; first-party SHA-pinned actions).
- The nightly schedule fires only from the default branch, so it starts after Plan 06-10 puts this file on main. The dispatched run proves the job itself.
- Full-suite ThreadSanitizer time on the runner is about 53 minutes and the whole realtime job 100 minutes, well inside the 360-minute limit.

## Self-Check: PASSED

- `.github/workflows/ci.yml` and `tests/OSC/ADMOSCSenderTests.cpp` exist and are committed.
- Commits `bede5d4`, `500be34`, `790f6a2`, `4e539c4`, `567bc52`, `dbedbf6` are ancestors of HEAD.
- Acceptance greps: pull_request_target 0, `secrets.` 0, `contents: read` 1, `uses:` 5 and 5 pinned, stale text 0, no expression in a run body, `realtime-nightly` 3, `fsanitize=thread` 4.
- Plan verification results: macos-gate success, 416-test ctest line, JUCE 9.0.0, harness PASS and HRTF guard lines present; realtime-nightly success, zero ThreadSanitizer warnings, `[rt]` and `sc-tsan-full.log` present.

---
*Phase: 06-consumer-readiness-ci*
*Completed: 2026-10-17*
