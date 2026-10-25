---
phase: 07-process-docs-consumer-closeout
plan: 03
subsystem: docs
tags: [preset-browser, integration-guide, ci-wording, light-tier, "#13", "#42", D-01, D-03, D-04, D-11]
status: complete

requires:
  - phase: 07-01
    provides: README "Local test gate" tiers and decision P7-D01, which the guide now points at
provides:
  - PresetBrowser.h contract comments (menu chrome only, Entry rules, threading, async lifetime, no state or versioning)
  - docs/integration-guide.md "### PresetBrowser contract" section with a wiring example
  - Guide Testing paragraph and harness sentence that say CI runs only by hand
affects: [07-04, phase-11]

actuals:
  tokens: 4390   # chars/4 over `git diff 354667d HEAD`; the plan's estimate was 45000
  tasks: 3
  commits: 3
commits: 3
plan_head_before: 354667d0e6d007b675ac5be5d5acfec3615a0513
plan_head_after: 0636bcb0dede511d35360b9ac60de7cfa09df9ea

tech-stack:
  added: []
  patterns: ["Contract stated in the header next to the declarations; the guide summarises it and names the header as the full statement"]

key-files:
  created: []
  modified:
    - include/SpatialCore/UI/PresetBrowser.h
    - docs/integration-guide.md

key-decisions:
  - "PresetBrowser is documented as it is (menu chrome only, no state, state format, save/load or versioning); it is not recorded as unsuitable (D-11)"
  - "The guide names the tiers once, by pointing at README 'Local test gate' (P7-D01), and says CI runs only on workflow_dispatch (D-03, D-04)"

requirements-completed: ["#13", "#42"]
duration: ~15 min
completed: 2026-10-25
---

# Phase 7 Plan 03: PresetBrowser contract and integration-guide CI wording Summary

PresetBrowser's contract is now written in its header and in a short integration-guide section: consumer-supplied `Entry` list in, `originalIndex` out, a save overlay that hands back a trimmed non-empty name, message thread only, async menu lifetime, and no state format, save/load or versioning in SpatialCore.

## What changed

- **Task 1 (tracer), commit a477492.** `PresetBrowser.h`: class comments for `PresetSaveOverlay` and `PresetBrowser` rewritten to the D-11 contract (cites SpatialCore#13 twice); `Entry` and its four fields documented; `showMenu` params rewritten; `getSaveOverlay` and `activeMenu` comments updated; the old "always saves to User/" note replaced by a comment that SpatialCore saves nothing. The overlay comment tells the consumer to strip path separators and characters the file system rejects (T-07-05). The `onSelect` docs tell the consumer to capture a `juce::Component::SafePointer` (T-07-06).
- **Task 3, commit 8a5fdee.** Guide harness sentence says the harness builds in CI in a manual run (`workflow_dispatch` since PR #46). The Testing paragraph points at README "Local test gate" (P7-D01) for the tiers and lists the CI jobs as manual-only; the final `getUnsuppliedGainsBlockCount() == 0` sentence is kept.
- **Task 2, commit 0636bcb.** `### PresetBrowser contract` appended under "## Preset System" (existing four bullets kept): lead paragraph, supply / get-back table, entry-list rules, threading and lifetime bullets, and a `cpp` wiring example using `SafePointer`.

Tasks 2 and 3 edit the same file; Task 3 was committed first so each commit holds only its own task's change.

## Light-tier run (D-01)

Release tree `build-release/` was already configured and built, so times are incremental.

| Step | Wall clock | Result |
|------|-----------|--------|
| cmake configure + build `SpatialCoreUITests` | 6 s | ok |
| `./build-release/tests/SpatialCoreUITests` | under 1 s | All tests passed (152 assertions in 17 test cases) |

## Verification

- Code-only comparison against 91c0f30 printed `COMMENT-ONLY`: the header differs only in comments; `git diff 91c0f30 -- src/UI/PresetBrowser.cpp` is empty.
- Header greps: `SpatialCore#13` 2, `state format` 1, `message thread` 5, `originalIndex + 1` 1, `SafePointer` 1, `path separators` 1, `always saves to User` 0.
- Guide section contains `state format`, `SafePointer`, `message thread`, `originalIndex + 1`, `Phase 11`, `getSaveOverlay`, `dismissAllActiveMenus`; the existing "Follow the OpenSpatialDelay pattern" bullets remain.
- Task 3 verify printed `GUIDE-CI-OK`; the stale tag-trigger sweep finds nothing in the guide (baseline 2); `workflow_dispatch` appears twice.
- Each documented rule was checked against `src/UI/PresetBrowser.cpp` (grouping L212-218, separator L220-222, ID `originalIndex + 1` L225, "User" fallback L236-247, callback `result > 0` L259-260).
- No GitHub run started, nothing pushed, no GitHub issue touched, no file outside this repository edited. Full ctest not run: comment-only header change and doc edits (P7-D01 light tier).

## Deviations from Plan

None - plan executed exactly as written. (Tasks 3 and 2 were committed in that order to keep one-task-per-commit on a shared file.)

## Known Stubs

None.

## Threat Flags

None.

## Self-Check: PASSED

- `include/SpatialCore/UI/PresetBrowser.h`, `docs/integration-guide.md` exist; commits a477492, 8a5fdee, 0636bcb are ancestors of HEAD.
