---
phase: 06-consumer-readiness-ci
plan: 07
subsystem: testing
tags: [llvm-cov, source-based-coverage, github-actions, appleclang, python-stdlib, ci]

requires:
  - phase: 06-consumer-readiness-ci
    provides: "06-05/06-06 CI workflow (macos-gate, linux-tests, windows-build), LFS cache and HRTF guard, SHA-pinned actions"
provides:
  - "SPATIALCORE_ENABLE_COVERAGE CMake option (default OFF, top-level Clang/AppleClang only)"
  - "scripts/coverage.sh: one command that builds the instrumented Debug tree, runs both test executables, merges, exports and reports"
  - "scripts/coverage-report.py: per-area engine table, separate UI table, completeness checks, optional target gate, --self-test"
  - "coverage CI job: job-summary table plus coverage-report artifact, inside GitHub only"
  - "Measured baseline (engine 94.19% lines on CI) and the user's target decision, OQ-4: engine 93%, UI report-only"
affects: [06-08 coverage enforcement, 06-09 docs (README Local test gate, CLAUDE.md Build System), 06-10 phase close]

actuals:
  tokens: 7500
  tasks: 3
  commits: 3

plan_head_before: edff43480434873ee120cc20828b999b101c1beb
plan_head_after: 5d78589997519cbf0090dceba92b123edefeea11

tech-stack:
  added: [llvm-profdata and llvm-cov (ship with Xcode, no install), Python 3 stdlib only (3.9 local, 3.14 runner)]
  patterns:
    - "Report script counts the repository's own sources and exits 3 when one is missing from a denominator"
    - "Percent is computed unrounded and displayed truncated; a target comparison uses the unrounded value"
    - "Library objects are passed to llvm-cov export with -object so unlinked UI sources keep a mapping"

key-files:
  created:
    - scripts/coverage.sh
    - scripts/coverage-report.py
  modified:
    - CMakeLists.txt
    - .gitignore
    - .github/workflows/ci.yml

key-decisions:
  - "User chose hold on 2026-10-24: engine line-coverage target 93%; the UI number (25.89%) is shown and is not a rule"
  - "UI linkage option 1 (-object of the ten UI object files) was used; its dedup check was identical on three files"
  - "Test output goes to a file, because a Debug instrumented run prints about 870k lines of libmysofa trace"
  - "Engine objects are not passed to llvm-cov: results are identical without them and passing them adds about 3400 warnings"

patterns-established:
  - "Coverage never leaves GitHub: job summary and a 14-day artifact, no third-party uploader, token or badge (D-15)"

requirements-completed: [TEST-01]

coverage:
  - id: D1
    description: "SPATIALCORE_ENABLE_COVERAGE option and scripts/coverage.sh measure real per-area engine coverage on this Mac, with the UI measured separately and nothing silently left out"
    requirement: TEST-01
    verification:
      - kind: unit
        ref: "python3 -I scripts/coverage-report.py --self-test (12 of 12 pass: 9300/10000 vs 93 passes, 9299/10000 fails, 92.996 displays 92.99, a report missing a source exits 3)"
        status: pass
      - kind: integration
        ref: "scripts/coverage.sh build-cov -- <FETCHCONTENT flags>: exit 0, summary.md has '## Engine' and '## UI' and the report-only line; two local runs 3838/4084 and 3839/4084 engine lines, UI 419/1618"
        status: pass
    human_judgment: false
  - id: D2
    description: "coverage CI job puts the table on the run's summary page and uploads the coverage-report artifact, with no outside service"
    requirement: TEST-01
    verification:
      - kind: integration
        ref: "https://github.com/AndrewRahman/SpatialCore/actions/runs/39074814315 (push of 310fb93): coverage job success; engine 3847/4084 = 94.19%, UI 419/1618 = 25.89%; grep -c -i -E 'codecov|coveralls|sonarcloud' prints 0 for ci.yml, coverage.sh and coverage-report.py"
        status: pass
    human_judgment: false
  - id: D3
    description: "TEST-01 dedicated-test clause has coverage evidence: PartitionedConvolver.cpp and BinauralRenderer.cpp line percentages"
    requirement: TEST-01
    verification:
      - kind: other
        ref: "summary.json per-file figures: src/Binaural/PartitionedConvolver.cpp 196/204 = 96.08%, src/Binaural/BinauralRenderer.cpp 254/254 = 100%"
        status: pass
    human_judgment: false
  - id: D4
    description: "The user picked the coverage target from the measured numbers at the blocking checkpoint (OQ-4)"
    requirement: TEST-01
    verification: []
    human_judgment: true
    rationale: "A target choice is the user's judgment by design (D-13); it is recorded here and in STATE.md, and Plan 06-08 enforces it"

duration: not measured as one run (Tasks 1-2 on 2026-10-18 00:22+02:00; checkpoint answered 2026-10-24 16:11+02:00)
completed: 2026-10-24
status: complete
---

# Phase 6 Plan 07: Coverage tooling and the coverage target Summary

**One command (`scripts/coverage.sh`) now measures real per-area line and branch coverage with Clang source-based instrumentation, a CI job publishes it on every run, and the user set the engine target at 93% (measured 94.19% on CI), with the UI figure (25.89%) shown but not a rule.**

## Performance

- **Tasks:** 3 (Task 1 tracer, Task 2 CI job, Task 3 blocking decision checkpoint, answered by the user)
- **Files modified:** 5 (`CMakeLists.txt`, `.gitignore`, `.github/workflows/ci.yml`, plus new `scripts/coverage.sh` and `scripts/coverage-report.py`); no library source
- **Run times:** local build 92 s; local `SpatialCoreTests` 479 s; local `SpatialCoreUITests` 16 s. CI: the coverage step took about 19.5 min.
- **Wall clock:** Tasks 1-2 were committed 2026-10-18 00:22+02:00; the checkpoint waited until the user's answer on 2026-10-24. No single duration is meaningful, so none is recorded.

## Accomplishments

- `SPATIALCORE_ENABLE_COVERAGE` (default OFF) adds `-fprofile-instr-generate -fcoverage-mapping` for a top-level Clang or AppleClang build, warns and ignores itself in a consumer build, and stops configure on a non-Clang compiler.
- `scripts/coverage.sh` builds the git-ignored `build-cov/` tree in Debug, runs `SpatialCoreTests` and `SpatialCoreUITests` directly with one profile each, merges, exports, writes an HTML report per executable and runs the report script, whose exit status becomes its own.
- `scripts/coverage-report.py` (stdlib only, Python 3.9 and 3.14 compatible) gives each engine area, an engine total and a separate UI total, exits 3 when a repository source is missing from a denominator, and `--self-test` proves the boundary (exactly at target passes, 0.01 below fails) and the truncating display.
- The `coverage` CI job writes the table to the run summary and uploads `coverage-report` (14 days), inside GitHub only (D-15).
- The user answered the target question: `hold`, engine 93%.

## Measurements

### CI run 39074814315 (push of `310fb93`, coverage job green)

| Area | Line % |
|---|---|
| Algorithms | 99.68 |
| Binaural | 86.76 |
| Core | 92.83 |
| Engine | 97.31 |
| IO | 98.60 |
| OSC | 98.00 |
| Trajectory | 91.21 |
| **Engine total** | **3847 / 4084 = 94.19%** (branches 85.92%) |
| **UI (separate)** | **419 / 1618 = 25.89%** (branches 18.82%) |

### Local runs (this Mac, AppleClang)

- Engine total: 3838/4084 = 93.97% (branches 86.23%) and 3839/4084 = 94.00% (branches 86.08%).
- Engine area alone: 1298/1343 = 96.64% and 1299/1343 = 96.72%.
- CI vs local: 0.19 to 0.22 points on the engine total, inside the plan's 0.5 limit. The cause is about 9 Engine lines that timing-dependent tests hit on some runs only.

### Weakest files

- `src/Binaural/HRTFDatabase.cpp`: 68.0% (221/325), mostly malformed-file paths.
- `src/Core/DopplerVelocity.cpp`: 81.9% (77/94).
- `src/IO/OutputFormatRegistry.cpp`: 8 of 9 lines.

### TEST-01 dedicated files

- `src/Binaural/PartitionedConvolver.cpp`: 196/204 = 96.08%.
- `src/Binaural/BinauralRenderer.cpp`: 254/254 = 100%.

### Against the research baseline (93.9%, 3792/4037 at HEAD `206f766`)

Phase 6 added about 47 source lines, covered at the same rate, so the total moved from 93.9% to 94.0-94.2%. Algorithms reads 99.68% (baseline 100.0) because `SpatializationAlgorithm.h` is now folded into that area. Binaural (86.7 to 86.76), IO (98.6) and Trajectory (90.9 to 91.21) are within about 0.3 points of the baseline; Engine moved from 96.6 to 97.31, OSC from 98.9 to 98.0 and Core from 93.8 to 92.83, none traced to a specific change in this plan (Phase 6 added code and tests in those areas). UI is 419/1618 = 25.89%, the same figure as the baseline's 25.9%.

### UI linkage and the dedup check

Option 1 of the plan's list was used: the ten UI object files are passed to `llvm-cov export` with `-object`. Without it only 11 UI entries show, at 419/1058, because the sources of a static archive with no referenced symbol carry no mapping. Dedup check: `PresetBrowser.cpp` 62/200, `SMLLookAndFeel.cpp` 17/312 and `SpatialMapComponent.cpp` 321/457 are identical with and without `-object`, so the option did not change the numbers of already-linked files. llvm-cov prints a harmless warning, "3869 functions have mismatched data".

## The user's Task 3 decision (OQ-4)

- **Option chosen:** `hold`, given 2026-10-24 (the user's words are not preserved in the handoff, only the option and the number).
- **Engine line-coverage target: 93%.** Plan 06-08 enforces it in CI.
- **UI: shown, not a rule** (25.89% on CI).
- Recorded in STATE.md Decisions and OQ-4 (commit `5d78589`).
- Note for 06-08: the `hold` option text says "today's engine figure rounded down to a whole percent". Applied to the CI figure (94.19%) that reads 94; the chosen 93 is the whole percent below the lower local runs (93.97-94.00%), which leaves 0.19 points on CI and 0.97 on the lowest local run for the timing-dependent lines. 06-08 should write 93 as the user gave it and not recompute it.

## Task Commits

1. **Task 1 (tracer): option, scripts, `.gitignore` entry** - `558be4c` (feat)
2. **Task 2: coverage CI job** - `310fb93` (ci)
3. **Task 3: decision recorded in STATE.md, pause handoff** - `5d78589` (docs)

**Plan metadata:** the docs commit that adds this SUMMARY.

## Files Created/Modified

- `CMakeLists.txt` - `SPATIALCORE_ENABLE_COVERAGE` block after `find_package(ZLIB REQUIRED)` (17 lines).
- `.gitignore` - `build-cov/` (still CRLF).
- `scripts/coverage.sh` - build, run, merge, export, report (mode 100755).
- `scripts/coverage-report.py` - aggregation, completeness exit 3, markdown and JSON, targets gate, `--self-test`.
- `.github/workflows/ci.yml` - `coverage` job (`macos-26`, `if: github.event_name != 'schedule'`, 150 min job and 120 min step timeouts, run summary, `coverage-report` artifact).

## Decisions Made

See `key-decisions`. The `scripts/coverage-targets.json` file is deliberately not created here: 06-08 writes it with the user's number, and until then every run prints "No coverage target set yet (OQ-4); this run reports only".

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Debug test output redirected to a file**
- **Found during:** Task 1 (local run)
- **Issue:** a Debug instrumented run prints about 870k lines of libmysofa trace, which swamps a terminal or a CI log.
- **Fix:** `scripts/coverage.sh` sends each executable's output to a file under `<dir>/coverage/`.
- **Files modified:** `scripts/coverage.sh`
- **Committed in:** `558be4c`

**2. [Plan text vs tool output] Engine objects are not passed to llvm-cov**
- **Issue:** the plan said to add `-object` for library objects. For the engine the result is identical without them, and passing them only adds about 3400 warnings, so only the ten UI objects are passed.
- **Files modified:** `scripts/coverage.sh`
- **Committed in:** `558be4c`

**3. [Plan alignment] Target wording matches Plan 06-08**
- **Issue:** the report script prints "Target: <t>% - PASS (...)" and "BELOW TARGET (...)" so the 06-08 acceptance greps match.
- **Files modified:** `scripts/coverage-report.py`
- **Committed in:** `558be4c`

---

**Total deviations:** 3 (1 blocking fix, 2 plan-text notes). **Impact on plan:** none on the numbers; no library source changed.

## Issues Encountered

- The execute-phase session paused at the blocking checkpoint on 2026-10-17 and resumed on 2026-10-24 after the user's answer. This continuation did not rebuild or rerun any CI; every figure above is from the recorded measurements.
- The CI `macos-gate` job of run 39074814315 was not read, so this plan makes no claim about it. Coverage, linux-tests and windows-build were green on that run.
- The plan's Task 2 `<verify>` also downloads the artifact. This continuation did not repeat that download; the figures above come from the job summary figures recorded at the pause.

## Known Stubs

None.

## Threat Flags

None. No new network endpoint, auth path or file access outside the scripts' own `build-cov/` folder; no third-party uploader (`grep -c -i -E "codecov|coveralls|sonarcloud"` prints 0 for `ci.yml`, `scripts/coverage.sh`, `scripts/coverage-report.py`).

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Plan 06-08 writes `scripts/coverage-targets.json` with `engine_line_percent: 93`, no `ui_line_percent`, plus a `_decision` note, and proves the boundary in CI; the target strings it greps already match the script.
- The `coverage` job takes about 19.5 min, so the whole workflow stays near the macos-gate's length (about 29 min) when jobs run in parallel.
- Open and not decided: whether to trim 06-08 to 06-10 (see `.continue-here.md`). The user has not confirmed it; this plan assumes nothing about it.
- INTG-01, INTG-02 and CI-01 stay unticked until their sibling plans finish; TEST-01 is declared by 06-08 as well, so it ticks at 06-08 or 06-10 per the shared-ID rule.

## Self-Check: PASSED

- `scripts/coverage.sh`, `scripts/coverage-report.py`, `CMakeLists.txt`, `.gitignore` and `.github/workflows/ci.yml` exist and are committed.
- Commits `558be4c`, `310fb93`, `5d78589` are ancestors of HEAD; `git rev-list --count edff434..5d78589` prints 3.
- Acceptance greps: `grep -c SPATIALCORE_ENABLE_COVERAGE CMakeLists.txt` prints 4, `git ls-files -s scripts/coverage.sh` starts with `100755`, `git ls-files --eol .gitignore` shows `w/crlf`, `continue-on-error` count in `ci.yml` is 0, outside-service grep prints 0 for all three files.
- Not re-run here by instruction: the coverage build, the self-test and the CI job (figures come from the recorded run 39074814315 and the local runs listed above).

---
*Phase: 06-consumer-readiness-ci*
*Completed: 2026-10-24*
