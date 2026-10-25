---
phase: 07-process-docs-consumer-closeout
plan: 01
subsystem: docs
tags: [test-tiers, ci-wording, landing-order, handoffs, P7-D01, P7-D02]

requires:
  - phase: 06
    provides: PR #46 (CI is workflow_dispatch only), D-18/D-19/D-20 decision rows
provides:
  - README "Local test gate" as the single written source of the four test moments and their commands
  - PROJECT.md decision rows P7-D01 (tiers) and P7-D02 (landing order and handoffs); D-19 and D-20 amended
  - CI-by-hand-only wording in README, CLAUDE.md and PROJECT.md
  - Handoff Done-when lists that point at the tiers
affects: [07-02, 07-03, 07-04, consumer handoffs]

actuals:
  tokens: 10963   # chars/4 over `git diff 007a6e1 HEAD` (43852 chars); the plan's estimate was 55000
  tasks: 3
  commits: 3
plan_head_before: 007a6e19ea6e7e6c7dc08bf7751e6f159dc48cc0
plan_head_after: 60544383f727bec00398fbbcbb8c804395c5d748
commits: 3

tech-stack:
  added: []
  patterns: ["Tiers written once in README; other docs point at it (D-03)"]

key-files:
  created: []
  modified:
    - README.md
    - CLAUDE.md
    - .planning/PROJECT.md
    - .planning/HANDOFF-ambisonics-fix.md

key-decisions:
  - "P7-D01: tests run in tiers chosen by the moment (no test run for docs, light tier while iterating, full Release ctest before main, full tier only for a release); supersedes D-19's everyday-gate wording"
  - "P7-D02: SpatialCore commit and push first, then each consumer bumps its pin; handoffs are consumer-repo issues whose Done-when uses the light tier unless release prep"

requirements-completed: ["#42"]

coverage:
  - id: D1
    description: "README Local test gate holds the four test moments and commands once, with the Catch2 file-tag light tier"
    requirement: "#42"
    verification:
      - kind: other
        ref: "grep -c '^#### Light tier' README.md (and No test run, Before merging, Full tier headings) each print 1"
        status: pass
    human_judgment: true
    rationale: "The light-tier commands are exercised for real by plan 07-02 Task 1; this plan is doc-only (D-02) and ran no tests"
  - id: D2
    description: "No sentence in README, CLAUDE.md or PROJECT.md says a tag push starts CI; CI is described as manual-only"
    requirement: "#42"
    verification:
      - kind: other
        ref: "tag-trigger and everyday-gate sweep from Task 2 verify prints CI-WORDING-OK"
        status: pass
    human_judgment: false
  - id: D3
    description: "Landing order and handoff rule written in README, CLAUDE.md, PROJECT.md (P7-D02); handoff Done-when lists use the tiers"
    requirement: "#42"
    verification:
      - kind: other
        ref: "Task 3 verify prints T3-OK"
        status: pass
    human_judgment: false

duration: 15min
completed: 2026-10-25
status: complete
---

# Phase 7 Plan 01: Test tiers, CI wording and landing order Summary

**The #42 two-tier test policy is written once in README "Local test gate" (no test run for docs, Catch2 file-tag light tier, full Release ctest before main, full tier for releases), with CLAUDE.md, PROJECT.md (P7-D01, P7-D02) and the handoff pointing at it and every stale "tag push starts CI" sentence corrected.**

## Performance

- **Duration:** about 15 min
- **Completed:** 2026-10-25
- **Tasks:** 3
- **Files modified:** 4

## Accomplishments

- README "Local test gate" rewritten into four moments: `#### No test run`, `#### Light tier: while iterating` (single-file and per-folder `--filenames-as-tags "[#File]~[.]"` forms, per-area table, UI via `SpatialCoreUITests`), `#### Before merging or pushing to \`main\``, `#### Full tier: only when preparing a release`.
- CLAUDE.md "Tests:" bullet points at README instead of repeating the gate; PROJECT.md has P7-D01 and P7-D02, D-19's everyday wording is marked superseded, D-18 untouched, D-20 and CI-01 amended.
- CI wording corrected in all three files: `workflow_dispatch` is the only trigger since PR #46, and a manual run waits for the user's go-ahead. The realtime-safety check is now "full tier, before a release".
- Landing order and handoff rule added to README "Updating SpatialCore in a consumer" and CLAUDE.md "Consumer Plugins"; the handoff Done-when lists use the light tier.

## Task Commits

1. **Task 1 (tracer): test policy end to end** - `7076ea2` (docs)
2. **Task 2: CI runs only by hand** - `d5f8036` (docs)
3. **Task 3: landing order and handoffs (P7-D02)** - `6054438` (docs)

**Plan metadata:** committed after this file (docs: complete plan).

## Files Created/Modified

- `README.md` - tiers, CI paragraph, realtime-safety heading and wording, landing-order paragraph
- `CLAUDE.md` - Tests bullet, realtime bullet, Consumer Plugins sentences
- `.planning/PROJECT.md` - P7-D01, P7-D02 rows; D-19, D-20, CI-01, P6-DR9 amended; footer date
- `.planning/HANDOFF-ambisonics-fix.md` - note line and two Done-when bullets

## Consumer pins read (Task 3, 2026-10-25)

- openspatialdelay main pins `2f16bae` (full `2f16baee0900ce1486ddd9e1364679bfe1b4d1bb`)
- OpenSpatialPanner main pins `ca8c88b` (full `ca8c88b7acc2c91e42a4de0e2433e99715ff7eeb`)

Same as at planning time.

## Acceptance greps (after; baselines from the plan)

- Task 1: `### Local test gate` 1; `#### No test run` 1, `#### Light tier` 1, `#### Before merging or pushing to` 1, `#### Full tier` 1 (baseline 0 each); `Which part to run when` 0 (baseline 1); `--filenames-as-tags "` 2 (baseline 0); `area=Binaural` 2 (baseline 0); `--target SpatialCoreUITests` 1 (baseline 0); `During development the everyday gate` in CLAUDE.md 0 (baseline 1); `P7-D01` in CLAUDE.md 1 and README 1 (baseline 0); `^| P7-D01 |` 1; D-19 row mentions P7-D01 1; D-18 row 1 and no `-| D-18 |` line in the diff vs 91c0f30 (0).
- Task 2: tag-trigger sweep prints nothing (baseline 12 lines) and everyday-gate sweep prints nothing (baseline 4); `workflow_dispatch` README 1, CLAUDE.md 1, PROJECT.md 2 (baseline 0 each); renamed Realtime heading 1; D-20 row mentions `amended 2026-10-25` 1 (baseline 0); `user's go-ahead` in README 4 (baseline 0).
- Task 3: `^| P7-D02 |` 1 with two 7-char pins (1); `openspatialdelay#23` in README 1 (baseline 0); `P7-D02` in CLAUDE.md 1 (baseline 0); `light tier` in handoff 2 (baseline 0); `Full local gate passes` in handoff 0 (baseline 1); `Last updated: 2026-10-25` in PROJECT.md 1.
- Prohibitions: `gh run list -R AndrewRahman/SpatialCore --limit 1` still shows the 2026-10-24 run (none created during execution); `git diff --name-only 007a6e1 HEAD` lists only the four plan files; nothing pushed.

## Decisions Made

P7-D01 and P7-D02 as planned (recorded in PROJECT.md).

## Deviations from Plan

**1. [Rule 1 - Stale fact] README Phase 5 history quote reworded**
- **Found during:** Task 2
- **Issue:** The Task 2 sweep regex (`before a release tag`) also matched the historical description of the Phase 5 rule: `with the rule "every phase end and before a release tag"`, which the plan's edit list did not name. Left as is, the verify command could not print CI-WORDING-OK.
- **Fix:** Changed the quoted rule to `"every phase end and before a release"`; the meaning is unchanged and it states no CI trigger.
- **Files modified:** README.md
- **Commit:** `d5f8036`

**2. [Rule 3 - Blocking] README line endings**
- **Found during:** Task 1
- **Issue:** README.md uses CRLF line endings; a first scripted edit normalised them to LF and turned the diff into a whole-file rewrite.
- **Fix:** Discarded that edit and redid it preserving CRLF; the final README is all CRLF (286 lines, 286 CRs) and the diffs show only the intended lines.
- **Commit:** `7076ea2`

**Total deviations:** 2 auto-fixed (1 stale wording, 1 blocking). **Impact:** none on scope.

## Issues Encountered

None. No tests were run: the plan is documentation and planning files only (D-02). The light-tier command forms are executed for real by plan 07-02 Task 1.

## Known Stubs

None.

## Threat Flags

None.

## Next

Ready for 07-02.

## Self-Check: PASSED

- README.md, CLAUDE.md, .planning/PROJECT.md, .planning/HANDOFF-ambisonics-fix.md exist; commits `7076ea2`, `d5f8036`, `6054438` are ancestors of HEAD; three plan verify commands printed TRACER-OK, CI-WORDING-OK, T3-OK.
