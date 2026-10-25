---
phase: 02-algorithm-format-verification
plan: 03
subsystem: spatial-audio-dsp
tags: [vbap, triplet-selection, coplanar-ties, github-issue, d-18]
status: complete

requires:
  - phase: 02-algorithm-format-verification
    provides: RESEARCH Critical Finding F5 measurements (coplanar-quad exact ties, 0.1-degree sweep jumps)
provides:
  - "Open GitHub issue AndrewRahman/SpatialCore#22 recording the coplanar-quad tie-break defect with the F5 evidence (D-18)"
  - "02-03-ISSUE-BODY.md: the exact body text that was filed"
affects: [02-05 scoped continuity tests cite #22, 02-07 spatial-audio-dsp skill text cites #22]

plan_head_before: db6547b3cdf4f192a9cd52c8124c2332ff22e873
plan_head_after: d033ae18826043c182b7fcc937427c1c88b8ee24

actuals:
  tokens: 750
  tasks: 2
  commits: 1

tech-stack:
  added: []
  patterns:
    - "Outward-facing text is extracted mechanically from the approved plan draft and kept on disk as the record of what was filed"

key-files:
  created:
    - .planning/phases/02-algorithm-format-verification/02-03-ISSUE-BODY.md
  modified: []

key-decisions:
  - "Task 1 resolved as file-as-drafted: the user delegated the choice to the orchestrator, which chose file-as-drafted because every figure in the draft is quoted from RESEARCH F5"
  - "Coplanar-quad tie-break defect filed as AndrewRahman/SpatialCore#22 (label bug); Phase 2 does not fix the triangulation (D-18)"

patterns-established:
  - "Duplicate check (gh issue list --state all) before any gh issue create"

requirements-completed: [EXTR-01]

coverage:
  - id: D1
    description: "Open bug issue in AndrewRahman/SpatialCore with the F5 tie-break evidence, number recorded for 02-05/02-07"
    requirement: "EXTR-01"
    verification:
      - kind: other
        ref: "gh issue list --repo AndrewRahman/SpatialCore --state open --search 'coplanar-quad triangulation in:title' --json number --jq 'length'  (returned 1)"
        status: pass
      - kind: other
        ref: "gh issue view 22 --repo AndrewRahman/SpatialCore --json labels  (lists bug)"
        status: pass
    human_judgment: false
  - id: D2
    description: "Issue body file on disk containing the approved text (1.325751162 and 0.851 present), byte-identical to the filed body apart from a trailing newline"
    requirement: "EXTR-01"
    verification:
      - kind: other
        ref: "grep -c '1.325751162' / grep -c '0.851' 02-03-ISSUE-BODY.md  (2 / 1)"
        status: pass
      - kind: other
        ref: "diff of gh issue view 22 --json body against 02-03-ISSUE-BODY.md  (only trailing blank line differs)"
        status: pass
    human_judgment: false

duration: 2min
completed: 2026-10-01
---

**Filed issue: AndrewRahman/SpatialCore#22 — https://github.com/AndrewRahman/SpatialCore/issues/22**

# Phase 2 Plan 03: Coplanar-Quad Tie-Break Issue Summary

**The 3D VBAP coplanar-quad tie-break defect (exact gain-sum ties resolved by float rounding, 0.45-0.85 gain jumps on 0.1-degree sweeps) is open as AndrewRahman/SpatialCore#22, labelled `bug`, carrying the RESEARCH F5 measurements. The body was filed verbatim from the approved draft.**

## Performance

- **Duration:** ~2 min for the continuation (Task 1 was resolved earlier at a blocking-human checkpoint)
- **Started:** 2026-10-01T06:35:06Z (continuation)
- **Completed:** 2026-10-01T06:36:30Z
- **Tasks:** 2 (Task 1 decision, Task 2 file)
- **Files modified:** 1 created

## Accomplishments
- Issue **AndrewRahman/SpatialCore#22** is open with title "3D VBAP: coplanar-quad triangulation ties are decided by float rounding (0.45–0.85 gain jumps on 0.1° azimuth sweeps)" and label `bug`.
- `02-03-ISSUE-BODY.md` holds the exact body filed. It was extracted mechanically from the plan's "Issue draft" blockquote with the `> ` markers removed, and it has not been edited since filing.
- Plans 02-05 (scoped `[panning-law][continuity]` tests) and 02-07 (spatial-audio-dsp skill text) can now cite `#22`.

## Task 1 Decision (checkpoint:decision, gate=blocking-human)

- **Chosen option:** `file-as-drafted`. No edits were made.
- **Provenance, recorded verbatim:** the user delegated the decision to the orchestrator ("You decide about 1 and 2, these are technical questions and they are not my expertise. Pick your recommendations."). The orchestrator chose "file-as-drafted" because every figure in the draft is quoted from RESEARCH F5 and the forward-referenced test files are created later in this phase (02-01 already created tests/Core/VBAPTripletSelectionTests.cpp; 02-05 creates tests/Algorithms/PanningLawTests.cpp).
- **Scope of authorization:** exactly one outward-facing action, `gh issue create` in AndrewRahman/SpatialCore. Nothing else was pushed, filed, or commented.
- **Executor provenance check before filing:** the executor traced every figure in the body to its source:
  - RESEARCH F5 (line 262): `1.325751162` tie, 0.607 / 0.751 / 0.851 / 0.716 sweep maxima, 0.0053 ear-level bound, 129 of 3600 flips.
  - RESEARCH line 409: the ~0.10 trapezoid-band steps, ~20x the smooth slope.
  - CONTEXT D-07 (line 111): 38-111 per 1M directions, jumps up to 0.96.

## Task Commits

1. **Task 1: Approve the exact issue text** — no commit (decision only, no files)
2. **Task 2: Write the approved body to disk and file the issue** — `d033ae1` (docs)

**Plan metadata:** recorded in the docs(02-03) completion commit that adds this SUMMARY

## Files Created/Modified
- `.planning/phases/02-algorithm-format-verification/02-03-ISSUE-BODY.md`: the exact body passed to `gh issue create --body-file`

## Decisions Made
- File as drafted (see Task 1 Decision above).
- Before filing, the executor ran a duplicate check (`gh issue list --search "coplanar-quad triangulation" --state all`, plus a broader `coplanar` search). Both returned no issues, so a new issue was created rather than an existing one reused.

## Deviations from Plan

None. The plan was executed exactly as written.

Note: this checkout is a Conductor-managed git worktree on branch `gsd-remap`, not a GSD per-agent `agent-*` worktree. The orchestrator dispatched a sequential executor onto it, and plan 02-01 committed on the same branch. The supplied root pin passed before every write and commit, and `gsd-remap` is not a protected branch.

## Issues Encountered
None

## User Setup Required
None. No external service configuration is required.

## Next Phase Readiness
- `AndrewRahman/SpatialCore#22` is ready to be cited from the excluded-range comments in `tests/Algorithms/PanningLawTests.cpp` (02-05) and from the skill text (02-07).
- The fix itself stays undecided; the issue lists three options. Option 1 (a deterministic epsilon) changes above-horizon output bits for OpenSpatialDelay sessions.

## Self-Check: PASSED
- FOUND: .planning/phases/02-algorithm-format-verification/02-03-ISSUE-BODY.md
- FOUND: commit d033ae1
- FOUND: issue #22 open, label `bug`

---
*Phase: 02-algorithm-format-verification*
*Completed: 2026-10-01*
