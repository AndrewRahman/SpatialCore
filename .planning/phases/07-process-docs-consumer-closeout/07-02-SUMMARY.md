---
phase: 07-process-docs-consumer-closeout
plan: 02
subsystem: docs
tags: [stale-facts, shim-headers, test-rename, light-tier, "#37", "#42", D-04, D-07, D-08, D-09, D-10]
status: complete

requires:
  - phase: 07-01
    provides: README "Local test gate" light-tier command forms exercised here
provides:
  - Shim headers citing commit 0b35e55 instead of a missing .planning path
  - Scratch-guard test title and comments that say renderSourceBuffers outputs silence
  - Roadmap doc with the development-remote clone URL, 23 formats, 15 layouts, dated banner line
  - spatial-audio-dsp skill with a dated 12-source processBlock row and a manual-only CI line
affects: [07-03, 07-04]

actuals:
  tokens: 2331   # chars/4 over `git diff e29497c HEAD`; the plan's estimate was 40000
  tasks: 2
  commits: 2
commits: 2
plan_head_before: e29497c790786c9df405545b5b7ad13c7bbf6a61
plan_head_after: 4868201a0540b468a1571fd90ad993f42b1289d9

tech-stack:
  added: []
  patterns: ["Dated figure labelled as such when no measurement record exists (D-09)"]

key-files:
  created: []
  modified:
    - include/SpatialCore/Core/BinauralGains.h
    - include/SpatialCore/Core/SourcePosition.h
    - tests/Binaural/BinauralRendererTests.cpp
    - docs/development-roadmap.md
    - .claude/skills/spatial-audio-dsp/SKILL.md

key-decisions:
  - "The processBlock row is called a dated 2026-04-12 12-source figure, not a measurement, because no measurement record exists in the repo (D-09)"

requirements-completed: ["#37", "#42"]
duration: ~10 min
completed: 2026-10-25
---

# Phase 7 Plan 02: Stale facts in comments, test title, roadmap and skill Summary

No doc or comment in the listed files now states a stale fact: the two shim headers cite 0b35e55, the scratch-guard test says it outputs silence, the roadmap clones the development remote with 23 formats and 15 layouts, and the skill row and CI line are dated and manual-only.

## What changed

- **Task 1 (tracer), commit 0535338.** `BinauralGains.h` and `SourcePosition.h` now carry a three-line shim comment citing commit 0b35e55 (2026-07-05). `BinauralRendererTests.cpp`: the RTSF-01 section comment says `renderSourceBuffers` outputs silence and `updateSourceHRIR` keeps the previous HRIR; the TEST_CASE is now "BinauralRenderer: renderSourceBuffers with a scratch that is too small skips, counts and outputs silence" (tags `[renderer][rt][scratch]` unchanged); the twin-comparison comment reads "Nothing moved: ...".
- **Task 2, commit 4868201.** `docs/development-roadmap.md`: submodule example uses `https://github.com/AndrewRahman/SpatialCore.git`, both format counts read 23, layouts 15, both banner paragraphs kept and one dated 2026-10-25 line added; the org-repo row is untouched. `SKILL.md`: processBlock row reads "12 sources; dated 2026-04-12 figure, not re-measured for 32 sources"; section 9.3 says CI runs only when started by hand (`workflow_dispatch`).

## Light-tier exercise (D-01)

Release tree `build-release/` was already configured and built, so times are incremental, not a first build.

| Step | Wall clock | Result |
|------|-----------|--------|
| cmake configure | 2 s | ok |
| build `SpatialCoreTests` (rebuilt the edited test file and relinked) | 7 s | ok |
| `--filenames-as-tags "[#BinauralRendererTests]~[.],[#UmbrellaTests]~[.]"` | 17 s | 12 test cases, 424 assertions, passed |
| `"*and outputs silence"` (renamed test by name) | under 1 s | 1 test case, 15 assertions, passed |
| area loop over `tests/Binaural/*.cpp` | 39 s | 69 test cases, 1902 assertions, passed |

The README command forms (file tags with `~[.]`, per-folder loop) select and pass real tests; 69 matches the expected default-Release count for `tests/Binaural/`.

## Verification

- Code-only comparison against 91c0f30: no output for all three C++ files (only comments and the one title string differ).
- No `.planning/...` path cited under include/, src/, tests/ or examples/ is missing.
- `grep -c 'last good sound'` in the test file is 0; `outputs silence` appears 2 times.
- Task 2 verify printed `T2-OK`; the second docs grep for stray organisation URLs printed nothing; banner kept, dated line within lines 1-8, org-repo row kept.
- `gh run list` newest run is 2026-10-24T16:09:53Z, before this plan; no run was started. No push, no GitHub issue touched.
- Full ctest was not run: this plan changes comments and one test title only (P7-D01 light tier).

## Deviations from Plan

None - plan executed exactly as written.

## Known Stubs

None.

## Threat Flags

None.

## Self-Check: PASSED

- Files modified (five) exist; commits 0535338 and 4868201 are ancestors of HEAD.
