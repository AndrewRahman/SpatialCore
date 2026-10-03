---
phase: 02-algorithm-format-verification
plan: 10
subsystem: testing
tags: [catch2, float-tolerance, ambisonics, release-build, gap-closure, fma]
gap_closure: true
gap_ids: [G-02-10]

requires:
  - phase: 02-algorithm-format-verification
    provides: "[ambi-pin] test and referenceAmbiDecode (plan 02-06, commit 317cc51)"
provides:
  - "[ambi-pin] passes in Release (macOS arm64) and Debug with a derived bound, kAmbiPinTolerance = 2.5e-5"
  - "Double-precision anchor (kAmbiFloatVsDoubleTolerance = 4.0e-5) that re-measures the float noise floor on every run"
  - "Release suite gate in 02-VALIDATION.md and a Build Types rule in codebase/TESTING.md"
  - "build-release/ gitignored"
affects: [phase-02-verify-work, phase-03-binaural-goldens, phase-06-ci]

requirements-completed: [VERIFY-01, EXTR-03]

actuals:
  tokens: 3129
  tasks: 2
  commits: 2
plan_head_before: 101fe6f929f79bf157e17f3c2fa312a7c4b92e2e
plan_head_after: 264fd8d1ad8019c836b01907855d7e923a65d4b9

tech-stack:
  added: []
  patterns:
    - "A test comparing two separately compiled float computations gets a bound derived from conditioning, checked in both Debug and Release"
    - "Mutation proof in a disposable detached worktree, with a reverted control run"

key-files:
  created: []
  modified:
    - tests/Engine/RenderEngineTests.cpp
    - .gitignore
    - .planning/phases/02-algorithm-format-verification/02-VALIDATION.md
    - .planning/codebase/TESTING.md
    - .planning/phases/02-algorithm-format-verification/deferred-items.md

key-decisions:
  - "kAmbiPinTolerance = 2.5e-5 (about 4.7x above the real Release spread, 2.3x below the smallest +0.1% epsilon failure), derived not guessed"
  - "Release gate uses ctest on build-release/tests, because ctest from the build root finds no tests and exits 0"
  - "No floating-point compiler flag added; library decoder and verbatim reference untouched"

coverage:
  - id: D1
    description: "[ambi-pin] passes in Release (arm64) and Debug with the derived bound"
    requirement: VERIFY-01
    verification:
      - kind: unit
        ref: "./build-release/tests/SpatialCoreTests [ambi-pin] and ./build/tests/SpatialCoreTests [ambi-pin]"
        status: pass
    human_judgment: false
  - id: D2
    description: "A +0.1% Tikhonov epsilon change fails the pin in both build types (7.0, 7.1, 7.1.2, 7.1.4, 7.1.6 plus the all-layout check) and passes again once reverted"
    requirement: EXTR-03
    verification:
      - kind: other
        ref: "disposable worktree mutation, logs /tmp/sc-g0210-logs/{rel,dbg}-{mut,ctl}.log"
        status: pass
    human_judgment: false
  - id: D3
    description: "Release suite is a phase gate (VALIDATION, TESTING.md, gitignore); out-of-scope risks recorded"
    verification:
      - kind: other
        ref: "02-10 Task 2 automated verify (grep gates)"
        status: pass
    human_judgment: false
  - id: D4
    description: "Accepting a 25-parts-per-million limit in place of one part per million for the decode pin"
    verification: []
    human_judgment: true
    rationale: "Loosening a test's limit is a trust judgment the user makes with the provenance in front of them; the numbers are checked, the acceptance is theirs."

duration: "wall clock 2026-10-03T17:03:58Z to 21:54Z, including a pause while a usage limit reset"
completed: 2026-10-03
status: complete
---

# Phase 02 Plan 10: Honest [ambi-pin] bound (G-02-10) Summary

**The Ambisonics decode pin now allows 2.5e-5 (derived from the matrix conditioning and re-checked against a double-precision solve on every run), passes in Release and Debug, and fails for a deliberate +0.1% Tikhonov change; the Release suite is now part of the phase gate.**

## Where the old number came from

RESEARCH F10 measured the two decoders bit-identical ("worst 0") in a scratch harness whose build type was not recorded. It recommended one part per million "to survive compiler flag changes", without measuring that. Plan 02-06 copied the number (T-02-16) and commit 317cc51 added the test. Phase 2's gates ran Debug only, where the two copies round identically, so the Release failure first showed in UAT test 10. The decoder was never wrong: each float copy sits about 1e-5 from a double-precision solve, and the two copies differ from each other by at most 5.3e-6 in Release. Plan 02-06's "within 1e-6" truth is superseded by this plan.

## What changed

The decoder that makes speaker feeds from Ambisonics is unchanged, so nothing you hear changes. One test that compares it with the original copy now allows 25 parts in a million instead of one. Ordinary computer rounding in the faster (Release) build already makes the two copies differ by about five parts in a million. The new limit is worked out from the maths, and Claude proved it still catches a deliberate 0.1% change to the decoder. The suite now has to pass in both build types before a phase is signed off.

**Question for you (one yes/no, also queued for end-of-phase UAT):** is it OK to accept the new 25-parts-per-million limit? A "no" means keeping the one-in-a-million limit and forcing identical rounding with a compiler setting on the whole library, which slows the audio code and means re-checking OpenSpatialDelay's sound; that needs a new decision before any change.

## Performance

- **Duration:** wall clock 17:03:58Z to 21:54Z (includes a pause while a usage limit reset)
- **Tasks:** 2
- **Files modified:** 5 (plus this summary)

## Task Commits

1. **Task 1: honest [ambi-pin] bound, double-precision anchor, gitignore** - `d3ab2c5` (test)
2. **Task 2: Release gate in VALIDATION and TESTING.md, deferred notes** - `264fd8d` (docs)

The mutation lived only in a disposable worktree and appears in no commit (`git log -p 1ac244a..HEAD -- src` is empty).

## Red run (Task 1 Step 2, unmodified test, Release build of this tree)

`./build-release/tests/SpatialCoreTests "[ambi-pin]"` exited non-zero: 11 failed assertions, 10 per-layout plus the all-layout check, all at `RenderEngineTests.cpp:700` / `:706` against `1e-6f`:

| Layout | worst abs(new - old) |
|---|---|
| Quadraphonic | 3.23355e-06 |
| 5.0 and 5.1 | 2.96533e-06 |
| 7.0 and 7.1 | 5.27501e-06 |
| 5.1.2 | 3.8147e-06 |
| 7.1.6 | 5.02169e-06 |
| 9.1.4 | 3.94881e-06 |
| 9.1.6 | 4.85778e-06 |
| SpatialMediaLab 13.1 | 3.30433e-06 |

Overall: 5.27501e-06. Test cases: 2 total, 1 failed. Assertions: 80 total, 11 failed. The numbers match the UAT exactly.

## Per-layout distances after the fix (green in both builds)

lib-ref is abs(library - reference), lib-exact and ref-exact are distances from the test-local double-precision decode of the same E. Stop rules: Release worst lib-ref 5.28e-6 (limit 1.25e-5), worst float-vs-exact 1.68e-5 (Debug 7.1.6, limit 2.0e-5). Neither broken.

| Layout | Debug lib-ref | Debug lib-exact | Debug ref-exact | Release lib-ref | Release lib-exact | Release ref-exact |
|---|---|---|---|---|---|---|
| Quadraphonic | 0 | 3.26e-06 | 3.26e-06 | 3.23e-06 | 3.26e-06 | 4.85e-06 |
| 5.0 Surround | 0 | 5.53e-06 | 5.53e-06 | 2.97e-06 | 5.53e-06 | 6.00e-06 |
| 5.1 Surround | 0 | 5.53e-06 | 5.53e-06 | 2.97e-06 | 5.53e-06 | 6.00e-06 |
| 7.0 Surround | 0 | 7.38e-06 | 7.38e-06 | 5.28e-06 | 7.38e-06 | 7.60e-06 |
| 7.1 Surround | 0 | 7.38e-06 | 7.38e-06 | 5.28e-06 | 7.38e-06 | 7.60e-06 |
| 9.1 Surround | 0 | 6.86e-06 | 6.86e-06 | 0 | 5.73e-06 | 5.73e-06 |
| Octaphonic | 0 | 2.80e-06 | 2.80e-06 | 0 | 3.36e-06 | 3.36e-06 |
| 5.1.2 Atmos | 0 | 9.73e-06 | 9.73e-06 | 3.81e-06 | 9.73e-06 | 7.82e-06 |
| 5.1.4 Atmos | 0 | 7.62e-06 | 7.62e-06 | 0 | 9.17e-06 | 9.17e-06 |
| 7.1.2 Atmos | 0 | 4.90e-06 | 4.90e-06 | 0 | 6.99e-06 | 6.99e-06 |
| 7.1.4 Atmos | 0 | 8.58e-06 | 8.58e-06 | 0 | 8.87e-06 | 8.87e-06 |
| 7.1.6 Atmos | 0 | 1.68e-05 | 1.68e-05 | 5.02e-06 | 1.40e-05 | 1.35e-05 |
| 9.1.4 Atmos | 0 | 6.74e-06 | 6.74e-06 | 3.95e-06 | 8.69e-06 | 8.04e-06 |
| 9.1.6 Atmos | 0 | 9.79e-06 | 9.79e-06 | 4.86e-06 | 9.85e-06 | 9.85e-06 |
| SpatialMediaLab 13.1 | 0 | 6.86e-06 | 6.86e-06 | 3.30e-06 | 6.86e-06 | 6.86e-06 |

`[ambi-pin]` in both builds: All tests passed (110 assertions in 2 test cases).

## Mutation and control (Task 2 Step 1)

Disposable detached worktree `/tmp/sc-g0210-mut` (removed and pruned), library epsilon `0.01f` to `0.01001f` (+0.1%), reference epsilon unchanged, Release and Debug trees.

| Build | Mutant | Failing layouts (abs(new - old)) | Control after revert |
|---|---|---|---|
| Release | exit 42, 11 failed assertions | 7.0 5.84e-05, 7.1 5.84e-05, 7.1.2 5.83e-05, 7.1.4 6.13e-05, 7.1.6 6.28e-05, plus all-layout check | exit 0, 110 assertions pass |
| Debug | exit 42, 11 failed assertions | 7.0 5.87e-05, 7.1 5.87e-05, 7.1.2 5.85e-05, 7.1.4 6.09e-05, 7.1.6 6.28e-05, plus all-layout check | exit 0, 110 assertions pass |

Stop rule (worst mutant at least 5.0e-5): met in both builds. The double-precision anchor also tripped on the same five layouts (library vs double solve 5.8e-05 to 6.3e-05 against 4.0e-05), so the mutant was caught by two independent checks. Logs: `/tmp/sc-g0210-logs/{rel,dbg}-{mut,ctl}.log`.

## Phase gate in both build types (Task 2 Step 2)

| Run | Result |
|---|---|
| Debug, `build/`, HUTUBS PP2 excluded | exit 0: all passed (304865 assertions in 192 test cases) |
| Release, `./build-release/tests/SpatialCoreTests`, no exclusion | exit 0: all passed (304868 assertions in 193 test cases); HUTUBS PP2 passes in Release |
| Release, `ctest --test-dir build-release/tests --output-on-failure` | 100% passed, 193 of 193 |
| 17 Phase 2 tags, each alone, `build` and `build-release` | all 34 runs exit 0, identical assertion totals in both builds (e.g. `[tracer]` 114/3, `[vbap3d-identity]` 262104/1, `[ear]` 19215/11, `[panning-law]` 7191/10, `[ambi-pin]` 110/2, `[g02-2]` 203/1) |

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] The plan's Release gate command ran zero tests**
- **Found during:** Task 2 Step 2
- **Issue:** `ctest --test-dir build-release --output-on-failure` printed "No tests were found!!!" and exited 0. `include(CTest)` and `catch_discover_tests` are only in `tests/CMakeLists.txt`, so the build root's `CTestTestfile.cmake` has no tests. The same is true in `build/`. The gate as written would have passed vacuously.
- **Fix:** gate command is now `ctest --test-dir build-release/tests --output-on-failure` (193 tests, 100% pass), also confirmed by running the Release binary directly. VALIDATION.md and TESTING.md say why. The plan's own grep (`ctest --test-dir build-release`) still matches.
- **Files modified:** `02-VALIDATION.md`, `.planning/codebase/TESTING.md`
- **Committed in:** `264fd8d`

**2. [Scope boundary] CI's own test step has the same blind spot**
- `.github/workflows/ci.yml` runs `ctest --output-on-failure` with `working-directory: build`, so CI runs zero tests. Not fixed (the plan prohibits touching `.github/` and the CMake files); recorded as a fourth open entry in `deferred-items.md`, owner Phase 6.

---

**Total deviations:** 1 auto-fixed (Rule 1), 1 deferred
**Impact on plan:** The fix is documentation of the correct command; no library or build-file change. The finding strengthens the recurrence guard.

## Deferred items (all in `deferred-items.md`, status open)

1. Other tight test-local-reference tolerances at risk on another compiler or build type (`VBAPTripletSelectionTests.cpp:787, :86, :152, :861, :1054, :1268`, binaural golden hashes; low risk `SpatialMathTests.cpp`, `PanningLawTests.cpp`). None fails today.
2. CI has no arm64 / FMA leg (Phase 6 criterion 3, SpatialCore#9); this branch is unpushed so CI has not run Phase 2.
3. HUTUBS PP2 golden checksum fails in Debug only (Phase 3).
4. (Found during this plan) CI's ctest step runs zero tests.

## Issues Encountered

A usage limit interrupted the run between configuring and building the mutation trees; the orchestrator resumed it. The disposable worktree was intact, so nothing was redone.

## Known Stubs

None.

## Threat Flags

None. The plan's register is mitigated: T-02-27 (bound derived, mutation fails in both builds, anchor, byte-identical reference, empty src diff), T-02-28 (mutation only in `/tmp` worktree, removed, absent from history), T-02-29 (Release gate in VALIDATION and TESTING.md).

## Verification of the prohibitions

- `git diff 1ac244a -- src include CMakeLists.txt tests/CMakeLists.txt .github` is empty; the 85-line `referenceAmbiDecode` is byte-identical to 1ac244a; no floating-point flag was added.
- `/tmp/sc-backstop` was not touched and is still listed.

## Next Phase Readiness

Run the Release gate (above) together with the Debug gate before `/gsd-verify-work` for Phase 02. UAT test 10 can be re-run; the one open user decision is the yes/no above.

## Self-Check: PASSED

- `tests/Engine/RenderEngineTests.cpp`, `.gitignore`, `02-VALIDATION.md`, `TESTING.md`, `deferred-items.md`: FOUND and modified.
- Commits `d3ab2c5` and `264fd8d`: FOUND.
- Task 1 and Task 2 automated checks: pass.
