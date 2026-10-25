---
phase: 06-consumer-readiness-ci
plan: 08
subsystem: testing
tags: [coverage, ci, llvm-cov, targets-json]

requires:
  - phase: 06-consumer-readiness-ci
    provides: "06-07 coverage tooling (scripts/coverage-report.py --targets, scripts/coverage.sh) and the user's target decision (OQ-4)"
provides:
  - "scripts/coverage-targets.json: engine_line_percent 93 with a _decision note; no UI rule"
  - "Proof on the real local exports that the rule bites at the boundary (E + 0.01 fails, E passes)"
affects: [06-09 docs, 06-10 phase close (CI run of the coverage job, TEST-01 tick)]

actuals:
  tokens: 400
  tasks: 2
  commits: 1

plan_head_before: bdf94bfb0651ab66faa3e3f61030d35fe9aeb14a
plan_head_after: 674614cbead8bb684f8c28273ff87007f4541fc7

tech-stack:
  added: []
  patterns:
    - "The targets file is the single place the coverage rule lives; scripts/coverage.sh passes it to the report script whenever it exists"

key-files:
  created:
    - scripts/coverage-targets.json
  modified: []

key-decisions:
  - "Target written as the user gave it: engine 93, not recomputed as floor(94.19)=94 (06-07-SUMMARY note); no ui_line_percent because the user kept the UI report-only"

requirements-completed: [TEST-01]

coverage:
  - id: D1
    description: "scripts/coverage-targets.json holds the user's chosen engine target (93) with a _decision note, and the report script prints a PASS line against it"
    requirement: TEST-01
    verification:
      - kind: integration
        ref: "python3 -I scripts/coverage-report.py --core build-cov/coverage/core-summary.json --ui build-cov/coverage/ui-summary.json --root $PWD --markdown /tmp/sc-p6-08-t1.md --json /tmp/sc-p6-08-t1.json --targets scripts/coverage-targets.json: exit 0, 'Target: 93% - PASS (Engine, now 94.00%)'"
        status: pass
    human_judgment: false
  - id: D2
    description: "The rule fails one hundredth of a point above the unrounded engine figure and passes exactly at it, on the real exports"
    requirement: TEST-01
    verification:
      - kind: integration
        ref: "coverage-report.py --targets /tmp/sc-p6-08-above.json exits 1 with BELOW TARGET; --targets /tmp/sc-p6-08-equal.json exits 0 with the PASS line"
        status: pass
    human_judgment: false
  - id: D3
    description: "The CI coverage job reads the file on a pushed commit and its summary shows 'Target: 93% - PASS'"
    requirement: TEST-01
    verification: []
    human_judgment: true
    rationale: "Deferred by the orchestrator: no push and no GitHub Actions run in this plan; the orchestrator pushes once after 06-09 and checks the coverage job then"

duration: 1 min
completed: 2026-10-24
status: complete
---

# Phase 6 Plan 08: Coverage target enforcement Summary

**The user's engine coverage target of 93% is committed in `scripts/coverage-targets.json` (UI stays report-only), the report script passes it locally at 94.00%, and it is shown to fail at E + 0.01 and pass at exactly E on the real exports; the CI confirmation waits for the orchestrator's single push.**

## Performance

- **Duration:** about 1 min of execution (started 2026-10-24T14:15:21Z)
- **Tasks:** 2 (Task 1 tracer, Task 2 boundary proof)
- **Files modified:** 1 created (`scripts/coverage-targets.json`); no library source

## Accomplishments

- `scripts/coverage-targets.json` written (LF, two-space indent) and committed in `674614c`.
- The local report on the 06-07 exports prints `Target: 93% - PASS (Engine, now 94.00%)` and exits 0.
- The boundary is proved on real data (below).

## Target file content

```json
{
  "_decision": "2026-10-24, OQ-4, user decision at Plan 06-07: option hold with the engine line-coverage target set to 93% by the user (CI measured 94.19%, local runs 93.97% and 94.00%); the UI figure (25.89% on CI) is shown and is not a rule, so there is no ui_line_percent.",
  "engine_line_percent": 93
}
```

Both numbers quoted as asked: 06-07-SUMMARY records "Engine line-coverage target: 93%. UI: shown, not a rule." and this file holds `engine_line_percent: 93` with no UI key. They are equal.

## Boundary results (Task 2)

Unrounded engine figure from `build-cov/coverage/summary.json`: **E = 94.00097943192948** (3839 of 4084 lines). Scratch files were outside the repo.

| Target | File | Exit | Printed line |
|---|---|---|---|
| E + 0.01 = 94.01097943192948 | `/tmp/sc-p6-08-above.json` | **1** | `BELOW TARGET (Engine): target 94.01097943192948%, now 94.00% (3839 of 4084 lines)` |
| E = 94.00097943192948 | `/tmp/sc-p6-08-equal.json` | **0** | `Target: 94.00097943192948% - PASS (Engine, now 94.00%)` |
| 93 (the committed rule) | `scripts/coverage-targets.json` | **0** | `Target: 93% - PASS (Engine, now 94.00%)` |

No UI rule was set, so there is no UI pair. The plan's `<automated>` boundary command passes (`VERIFY-PASS`). `git diff bdf94bf -- scripts/coverage-targets.json` is Task 1's change only (4 added lines).

## Task Commits

1. **Task 1 (tracer): the chosen target is committed** - `674614c` (ci)
2. **Task 2: boundary proof** - no commit; it writes only scratch files and records results here, as the plan says ("The committed file is not changed by this task").

**Plan metadata:** the docs commit that adds this SUMMARY.

## Decisions Made

- Wrote 93 exactly as the user gave it. The plan's rule `hold -> floor(CI engine figure)` would read 94; the orchestrator and 06-07-SUMMARY both say the user's number wins. 93 leaves 0.19 points of headroom on the CI figure (94.19) and 0.97 on the lowest local run (93.97) for the timing-dependent lines.

## Deviations from Plan

### Orchestrator overrides (not auto-fixes)

**1. [Orchestrator override] Target is 93, not floor(94.19) = 94**
- **Found during:** Task 1
- **Issue:** the plan's `hold` rule computes 94; the user explicitly chose 93.
- **Fix:** wrote 93; the `_decision` note names it as the user's number.
- **Files modified:** `scripts/coverage-targets.json`
- **Committed in:** `674614c`

**2. [Orchestrator override] No push, no CI watch in Task 1**
- **Found during:** Task 1
- **Issue:** the plan pushes, finds the run for HEAD, watches the `coverage` job and saves `/tmp/sc-p6-08-cov.log`. The user wants CI round trips minimised.
- **Fix:** skipped the second `<automated>` check (the gh/CI one). The first (file shape) passed. The orchestrator pushes once after 06-09 and verifies the `coverage` job's `Target: 93% - PASS` line then.
- **Files modified:** none

---

**Total deviations:** 2 (both orchestrator instructions). **Impact on plan:** the CI half of truth 2 is unproven until the orchestrator's push; the local report proves the file is read and the rule evaluates.

## Issues Encountered

None. No CI run exists for this commit, so no CI run id is recorded.

## Known Stubs

None.

## Threat Flags

None. The only new file is a JSON config read by the existing report script. T-06-22 is mitigated by the boundary runs above; T-06-23 by writing only the user's recorded number with a `_decision` note.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- After the orchestrator's push, check that the CI `coverage` job is green and its summary shows `Target: 93% - PASS`. If CI measures below 93 (CI was 94.19 on run 39074814315), the job turns red by design; the number must not be lowered.
- TEST-01 is declared by 06-08 and 06-10; it is left to the shared-ID rule and 06-10 to tick.

## Self-Check: PASSED

- `scripts/coverage-targets.json` exists and is committed.
- Commit `674614c` is an ancestor of HEAD; `git rev-list --count bdf94bf..674614c` prints 1.
- File shape check passes (numeric `engine_line_percent` in (0, 100], `_decision` present); the boundary command prints `VERIFY-PASS`.
- Not done by instruction: push, CI run, artifact download.

---
*Phase: 06-consumer-readiness-ci*
*Completed: 2026-10-24*
