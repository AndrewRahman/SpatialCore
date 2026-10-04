---
phase: 03-binaural-defects-hrtf-packaging
plan: 11
subsystem: testing
tags: [github-issues, itd, loudness, d-03, d-13, d-14, d-16, phase-gate]

requires:
  - phase: 03-binaural-defects-hrtf-packaging
    provides: "03-01 convolver/ITD/loudness measurements, 03-03 moving-source numbers, 03-04 Simple cue numbers, 03-08 libmysofa v1.3.5, 03-10 phase gate"
provides:
  - "03-11-OUTWARD.md: the four approved texts, the dated per-item decision, the Pending-after-push commands and the Done record"
  - "AndrewRahman/SpatialCore#25 filed (64-sample ITD wrap, label bug) and cited from the [renderer][itd-characterisation] table comment"
  - "[loudness] pass/fail tolerance: profiles 1-5 pinned to measured means within 0.5 LU, spread at most 2.98 LU"
  - "Debug 260/260 and Release ctest 260/260 after the last test edit"
affects: [phase-3-push-to-main, OpenSpatialDelay#234, SpatialCore#15, verify-work for phase 3]

plan_head_before: 3d9cb7354acc0b628d1d5ed7d0f98eb46e4b891e
plan_head_after: bb8b3d1ae453213d187e967d29f6b4e771b3b7fc
commits: 2

actuals:
  tokens: 4500
  tasks: 3
  commits: 2

tech-stack:
  added: []
  patterns:
    - "Outward text is drafted from measured SUMMARY numbers, approved per item, and posted from the fenced block of one note, so the approved text and the posted text cannot diverge"
    - "Public actions that say 'fixed' are deferred until the code is on main"

key-files:
  created:
    - .planning/phases/03-binaural-defects-hrtf-packaging/03-11-OUTWARD.md
  modified:
    - tests/Binaural/BinauralRendererTests.cpp
    - tests/Binaural/ProfileLoudnessTests.cpp

key-decisions:
  - "A (OSD#234 comment) and C (close SpatialCore#15) are approved as written but their posting is deferred until Phase 3 is pushed to main; D-13 and D-03 closure are 'approved, posting deferred to push'"
  - "B filed now as AndrewRahman/SpatialCore#25 with the exact approved title and body"
  - "D approved as written: profiles 1-5 within 0.5 LU of -20.83, -19.80, -20.16, -19.34, -18.34 LKFS; spread at most 2.98 LU; Simple excluded; WARN table kept"

patterns-established:
  - "Bound constants for a measured-and-approved tolerance live beside the table in the test file, with the approval date in the comment"

requirements-completed: [BUG-01, BUG-02, EXTR-02]

coverage:
  - id: D1
    description: "ITD wrap issue filed on AndrewRahman/SpatialCore with the approved title and body, and cited from the ITD characterisation test"
    requirement: "BUG-01"
    verification:
      - kind: other
        ref: "gh issue create -> https://github.com/AndrewRahman/SpatialCore/issues/25; grep -c 'AndrewRahman/SpatialCore#' tests/Binaural/BinauralRendererTests.cpp"
        status: pass
    human_judgment: false
  - id: D2
    description: "Loudness tolerance pinned in [loudness] to the user-approved bounds, no built-in level changed"
    requirement: "EXTR-02"
    verification:
      - kind: unit
        ref: "tests/Binaural/ProfileLoudnessTests.cpp#Loudness: perceived level of each built-in profile, recorded for the future standard"
        status: pass
    human_judgment: false
  - id: D3
    description: "Full suite still green after the last test edit, in Debug and Release"
    requirement: "BUG-02"
    verification:
      - kind: unit
        ref: "./build/tests/SpatialCoreTests (260 test cases, 309251 assertions) and ctest --test-dir build-release/tests (260/260)"
        status: pass
    human_judgment: false
  - id: D4
    description: "OSD#234 comment and SpatialCore#15 close are approved text, not yet posted; they run after Phase 3 is on main"
    requirement: "BUG-02"
    verification: []
    human_judgment: true
    rationale: "External GitHub actions the user deferred on purpose; they cannot be proven by a test and must be run by the orchestrator after the push (see Pending after push)"

duration: ~1h (two sessions; Task 1 drafted in the first, Tasks 2-3 here)
completed: 2026-10-04
status: complete
---

# Phase 3 Plan 11: Outward items Summary

**Filed the ITD-wrap issue as SpatialCore#25 and cited it from its pinning test, pinned the five built-in HRTF profiles' loudness to the user-approved bounds, and parked the OSD#234 comment and the #15 closure as approved text to post after the push; Debug and Release both pass 260/260.**

## Performance

- **Duration:** about 1h across two sessions
- **Completed:** 2026-10-04
- **Tasks:** 3 (1 auto, 1 decision checkpoint, 1 auto)
- **Files modified:** 3 (one created, two test files edited)

## Accomplishments

- **B (D-16 issue half): done.** https://github.com/AndrewRahman/SpatialCore/issues/25, label `bug`, exact title and body from `03-11-OUTWARD.md` section B. The `[renderer][itd-characterisation]` table comment now ends "Tracked by AndrewRahman/SpatialCore#25".
- **D (D-14 tolerance half): done.** `[loudness]` now CHECKs each of profiles 1-5 within 0.5 LU of its measured mean (-20.83, -19.80, -20.16, -19.34, -18.34 LKFS) and the spread among them at most 2.98 LU (it was `< 20.0`). Simple stays excluded, the WARN table is kept, and the comment reads "tolerance approved by the user on 2026-10-04 from the Phase 3 measurement (D-14)". No built-in level changed. The re-run reads exactly the approved means and a 2.48 LU spread.
- **A and C (D-13, D-03 closure): approved, posting deferred to push.** Nothing was posted to Spatial-Media-Lab/OpenSpatialDelay#234 and SpatialCore#15 is not closed. The commands are below.
- **Gate evidence holds after the last test edit:** full Debug suite 260 test cases, 309251 assertions, all passed; Release `ctest` 100% passed out of 260; `[renderer]` (6 cases, 280 assertions) and `[loudness]` (2 cases, 26 assertions) pass in both builds.

## Decision recorded (user, 2026-10-04, per item)

| Item | Decision |
|---|---|
| A: comment on Spatial-Media-Lab/OpenSpatialDelay#234 | Approved as written; **deferred**, post only after Phase 3 is pushed to `main` |
| B: new SpatialCore issue (ITD wrap) | Approved as written; filed now as #25 |
| C: close AndrewRahman/SpatialCore#15 with comment | Approved as written; **deferred**, post and close only after Phase 3 is pushed to `main` |
| D: loudness tolerance | Approved as written; applied |

## Pending after push

The orchestrator runs these after Phase 3 is pushed to `main`, from the repository root. The texts are
the fenced blocks of sections A and C of `.planning/phases/03-binaural-defects-hrtf-packaging/03-11-OUTWARD.md`
(the same commands, with the extraction step, are in that file's "Pending after push" section).

```bash
F=.planning/phases/03-binaural-defects-hrtf-packaging/03-11-OUTWARD.md
python3 - "$F" <<'PY'
import re, sys
t = open(sys.argv[1], encoding='utf-8').read()
a = t.split("## A. Comment")[1].split("## B. New issue")[0]
c = t.split("## C. Close")[1].split("## D. Loudness")[0]
open('/tmp/03-11-A-body.md', 'w').write(re.findall(r"````text\n(.*?)\n````", a, re.S)[0] + "\n")
open('/tmp/03-11-C-body.md', 'w').write(re.findall(r"````text\n(.*?)\n````", c, re.S)[0] + "\n")
PY

# A: comment only, never close (D-13)
gh issue comment 234 --repo Spatial-Media-Lab/OpenSpatialDelay --body-file /tmp/03-11-A-body.md
gh issue view 234 --repo Spatial-Media-Lab/OpenSpatialDelay --json state    # expect OPEN

# C: comment and close (D-03)
gh issue close 15 --repo AndrewRahman/SpatialCore --comment "$(cat /tmp/03-11-C-body.md)"
gh issue view 15 --repo AndrewRahman/SpatialCore --json state               # expect CLOSED
```

Then append the comment URL and the closed state to the "Done" section of `03-11-OUTWARD.md`. Until
then, Phase 3's D-13 and D-03 are open on the GitHub side only.

## Task Commits

1. **Task 1: Draft the four items with the measured numbers** - `d108a8a` (docs)
2. **Task 2: The user approves, edits or skips each item** - no commit (decision checkpoint; recorded in `03-11-OUTWARD.md` and this SUMMARY)
3. **Task 3: Carry out the approved items, then re-run the full suite** - `bb8b3d1` (test)

**Plan metadata:** the docs commit that carries this SUMMARY, STATE.md and ROADMAP.md.

## Files Created/Modified

- `.planning/phases/03-binaural-defects-hrtf-packaging/03-11-OUTWARD.md` - drafted texts (Task 1), dated Decision, Pending after push, Done
- `tests/Binaural/BinauralRendererTests.cpp` - ITD table comment cites AndrewRahman/SpatialCore#25
- `tests/Binaural/ProfileLoudnessTests.cpp` - per-profile and spread bounds as constants and CHECKs, header comment updated to describe them

## Decisions Made

- A and C ride along as approved text because the user does not want "fixed" said publicly before the code is on `main` (this was difference 4 in the drafted note).
- The loudness bound uses `WithinAbs (measured, 0.5)` per profile and `spread <= 2.98`, exactly the text approved.

## Deviations from Plan

None - plan executed exactly as written. The one structural difference is the user's decision itself: A and C were deferred rather than posted in Task 3, which the plan allows ("only for each approved item") and which the continuation instructions made explicit.

## Issues Encountered

- The first Read of the plan and note returned only their first line (a hook summarised them); both were re-read in full with `cat`.
- Both test binaries print "Leaked objects detected: ... FFT" and a JUCE assertion line at process exit, with exit code 0. This is the process-global `SharedFFTCache` singleton outliving JUCE's leak detector and is already recorded in the 03-01 to 03-07 SUMMARY files; not caused by this plan.

## Authentication Gates

None. `gh auth status` showed a logged-in account (AndrewRahman) with the needed scope.

## Known Stubs

None.

## Threat Flags

None. No new network endpoints, auth paths, file access or schema changes. Public text (issue #25) was checked for `/Users/` and local paths before posting (0 matches); only `AndrewRahman/SpatialCore` was written to (T-03-24, T-03-25).

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Phase 3 has no plans left. The orchestrator pushes Phase 3 to `main`, then runs the "Pending after push" commands above.
- OpenSpatialDelay and OpenSpatialPanner follow-ups are already recorded in PROJECT.md (03-10).

## Self-Check: PASSED

- `03-11-OUTWARD.md`, `03-11-SUMMARY.md`, both edited test files exist.
- Commits `d108a8a` and `bb8b3d1` exist; `git rev-list --count 3d9cb73..HEAD` = 2 at SUMMARY write (the SUMMARY's own docs commit follows).
- Issue #25 exists in AndrewRahman/SpatialCore; OSD#234 and SpatialCore#15 were not touched.
- `grep -c '/Users/' 03-11-OUTWARD.md` = 0; `grep -c "D-14" tests/Binaural/ProfileLoudnessTests.cpp` >= 2; `grep -c "AndrewRahman/SpatialCore#" tests/Binaural/BinauralRendererTests.cpp` >= 1.

---
*Phase: 03-binaural-defects-hrtf-packaging*
*Completed: 2026-10-04*
