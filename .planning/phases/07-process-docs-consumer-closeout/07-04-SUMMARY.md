---
phase: 07-process-docs-consumer-closeout
plan: 04
subsystem: docs
tags: [ci-wording, closeout, "#42", "#24", "#9", D-04, D-06, D-12, D-13, D-14]
status: complete

requires:
  - phase: 07-01
    provides: README "Local test gate" tiers, decisions P7-D01 and P7-D02
  - phase: 07-02
    provides: stale-fact fixes and the renamed scratch-guard test
  - phase: 07-03
    provides: PresetBrowser contract and guide wording
provides:
  - ci.yml comments that describe the manual-only trigger and the v*-tag-ref case of realtime-nightly (comment lines only)
  - Harness README, MILESTONES, cross-platform-builds reference and architecture skill corrected
  - Repo-wide sweeps clean outside the dated history
  - 07-CLOSEOUT.md with the post-merge commands for /gsd-ship
affects: [phase-13]

actuals:
  tokens: 3259   # chars/4 over `git diff 400d38c HEAD` (code-less docs plan); the plan's estimate was 32000
  tasks: 2
  commits: 2
commits: 2
plan_head_before: 400d38c6405e478cb9703f46e574fe14e92a2bb7
plan_head_after: 3bbb72dbbb3454d5590ba54c6c0b83a385d7a044

tech-stack:
  added: []
  patterns: ["Outward GitHub actions are written into a CLOSEOUT file and run post-merge, never during execution"]

key-files:
  created:
    - .planning/phases/07-process-docs-consumer-closeout/07-CLOSEOUT.md
  modified:
    - .github/workflows/ci.yml
    - examples/harness/README.md
    - .planning/MILESTONES.md
    - .claude/skills/cross-platform-builds/REFERENCE.md
    - .claude/skills/spatialcore-architecture/spatialcore-architecture.md

key-decisions:
  - "ci.yml changed in comment lines only; the on: block and the realtime-nightly if: line stay byte-identical (T-07-07)"
  - "#42, #37 and #13 stay open until the merge to main; their close commands and the consumer notes wait in 07-CLOSEOUT.md (D-14, D-06)"

requirements-completed: ["#42", "#24", "#9"]
duration: ~10 min
completed: 2026-10-25
---

# Phase 7 Plan 04: Last stale CI surfaces and the post-merge close-out Summary

The last five surfaces that said a tag push starts CI, or spoke of an everyday gate, are corrected, repo-wide sweeps print nothing outside the dated history, and `07-CLOSEOUT.md` holds the exact commands that close #42, #37 and #13 and post the consumer notes after the merge.

## What changed

- **Task 1 (tracer), commit 7a7f9ac.**
  - `ci.yml`: the header comment and the comment above `realtime-nightly` now say the job runs on a manual run with the realtime input on, or a manual run started from a `v*` tag ref, belongs to the full tier, and has no schedule or push trigger. Cites P7-D01 and README "Local test gate".
  - `examples/harness/README.md`: CI runs the script in a manual run, `workflow_dispatch` only since PR #46.
  - `.planning/MILESTONES.md`: the v0.5.0 What's-next sentence amended (dated 2026-10-25): start CI by hand.
  - `cross-platform-builds/REFERENCE.md`: the template comment says SpatialCore CI runs only when started by hand; the `on: [push, pull_request]` template line is kept.
  - `spatialcore-architecture.md`: the ThreadSanitizer sentence points at the full tier and README "Local test gate". The file has CRLF endings; they were preserved.
- **Task 2, commit 3bbb72d.** `07-CLOSEOUT.md` created: close commands for #42, #37, #13, both consumer notes (OpenSpatialPanner#4, openspatialdelay#27), the already-closed record for #24 and #9, and what is not part of the close-out. Nothing in it was run.

## Verification results

| Check | Result |
|-------|--------|
| ci.yml diff vs 91c0f30 changes only comment lines, realtime-nightly `if:` verbatim | CI-COMMENTS-ONLY |
| Repo-wide tag-trigger sweep outside dated history (baseline 20 lines) | TAG-SWEEP-CLEAN (0) |
| Repo-wide everyday-gate sweep outside `.planning/` (baseline 5 lines) | EVERYDAY-SWEEP-CLEAN (0); CLAUDE.md, integration guide and the handoff all name "Local test gate" |
| CLOSEOUT structure check (3 close commands, 2 comment commands, at least 5 merge-sha, P7-D01/P7-D02, no forbidden command, no private string) | CLOSEOUT-OK; each `gh` line also passes `bash -n` |
| ROADMAP #24 and #9 ticked (read-only) | 1 and 1 |

Issue states read 2026-10-25 at 08:46 UTC with `gh issue view` (read-only):

| Issue | State |
|-------|-------|
| #24 | CLOSED |
| #9 | CLOSED |
| #42 | OPEN |
| #37 | OPEN |
| #13 | OPEN |

No test run (documentation, planning files and YAML comments only, D-02). No GitHub Actions run started; nothing pushed.

## Post-merge close-out

After the merge to main, `/gsd-ship` runs `.planning/phases/07-process-docs-consumer-closeout/07-CLOSEOUT.md`: replace `<merge-sha>` with the short merge commit, close #42, #37 and #13 with the comments, and post the two consumer notes. It needs the go-ahead of the user.

## Deviations from Plan

None - plan executed exactly as written. The wave-1 plans had left nothing for the sweeps to fix, so no wave-1 file was touched.

## Known Stubs

None.

## Threat Flags

None. ci.yml gained no trigger, permission or condition change (T-07-07); CLOSEOUT bodies hold repository facts only (T-07-09).

## Self-Check: PASSED

- 07-CLOSEOUT.md exists; the five modified files exist.
- Commits 7a7f9ac and 3bbb72d are ancestors of HEAD.
