---
phase: 06-consumer-readiness-ci
plan: 10
subsystem: testing
tags: [ci, merge, local-gate, validation, requirements, catch2, decision-record]

requires:
  - phase: 06-consumer-readiness-ci
    provides: plans 06-01 to 06-09 (root ctest, INTG-02 counter, regression tests, harness, five CI jobs, coverage gate at 93%, docs truth, D-18 policy commit 1704415)
provides:
  - origin/main merged into the Phase 6 branch locally (SC-1 MAX_SOURCES 32, SC-20 single-load race fix, Nyquist validation of phases 1, 2, 4)
  - the everyday local gate passing on the merged tree, with logs and durations
  - 06-VALIDATION.md statuses and a phase-end evidence section
  - INTG-01, INTG-02, CI-01, TEST-01 ticked on recorded evidence
  - the decision that Phase 6 goes onto main only after phase verification and with the user's go-ahead
affects: [phase-6-verification, push-to-main, OpenSpatialDelay-pin-bump, OpenSpatialPanner-pin-bump]

actuals:
  tokens: 40000
  tasks: 3
  commits: 4
plan_head_before: 17044157360c8b8e79a8964e28db4f87aa65c49d
plan_head_after: abcdc11056f5ff4aaad07337c7a1088522cae2c1

tech-stack:
  added: []
  patterns:
    - "A local merge of origin/main, with the gate re-run on the merged tree, before anything is pushed"

key-files:
  created:
    - .planning/phases/06-consumer-readiness-ci/deferred-items.md
    - .planning/phases/06-consumer-readiness-ci/06-10-SUMMARY.md
  modified:
    - .planning/phases/06-consumer-readiness-ci/06-VALIDATION.md
    - .planning/phases/06-consumer-readiness-ci/.continue-here.md
    - .planning/REQUIREMENTS.md
    - .planning/STATE.md
    - .planning/ROADMAP.md
    - README.md
    - CLAUDE.md
    - tests/Binaural/BinauralRendererTests.cpp
    - tests/Binaural/MitKemarLargePinnaTests.cpp
    - tests/Binaural/PartitionedConvolverTests.cpp

key-decisions:
  - "Phase 6 goes onto main only after phase verification passes, and the push to main needs the user's go-ahead (it would start a GitHub build); no branch protection for now"
  - "The 06-03 regression tests write their issue references in full (Spatial-Media-Lab/OpenSpatialDelay#N) to satisfy main's BUG-03 guard; no assertion or tolerance moved"

requirements-completed: [INTG-01, INTG-02, CI-01, TEST-01]

duration: 40min
completed: 2026-10-24
status: complete
---

# Phase 6 Plan 10: Trimmed close-out Summary

**origin/main merged into the Phase 6 branch with four conflicts resolved, the everyday gate green on the merged tree (Debug 401 + 17 cases, Release ctest 418 of 418, `[rt]` 2521 assertions), and INTG-01, INTG-02, CI-01 and TEST-01 ticked on recorded evidence; nothing pushed, no GitHub setting touched.**

This is the trimmed 06-10 the user chose on 2026-10-24: no cloud builds, no ThreadSanitizer re-run (D-18), no branch protection, no push.

## VERIFICATION evidence

Written before the requirements were ticked. Local runs are on this Mac at merge commit `37fef2e` plus fix `a15f927`; logs were in `/tmp/sc-*.log` (temporary).

### Local gate (README "Local test gate", as written)

| Step | Command | Result | Duration |
|------|---------|--------|----------|
| Debug build | `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j8` | exit 0 | 9 s (incremental) |
| Debug, DSP | `./build/tests/SpatialCoreTests` | exit 0; 401 cases, 400 passed, 1 skipped; 330129 of 330129 assertions | 531 s |
| Debug, UI | `./build/tests/SpatialCoreUITests` | exit 0; 17 cases, 152 assertions | 14 s |
| Release build | `cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release && cmake --build build-release -j8` | exit 0 | 11 s (incremental) |
| Release ctest | `ctest --test-dir build-release/tests --output-on-failure` | exit 0; `100% tests passed out of 418` | 130 s |
| ui: listing | `ctest --test-dir build-release/tests -N \| grep 'ui:'` | exit 0; 17 `ui:` tests | instant |
| Root ctest | `ctest --test-dir build-release --no-tests=error --output-on-failure` | exit 0; `100% tests passed out of 418` | 129 s |
| Allocation and lock trap | `./build-release/tests/SpatialCoreTests "[rt]"` | exit 0; 2521 assertions in 47 cases | 25 s |
| Coverage tool | `python3 -I scripts/coverage-report.py --self-test` | exit 0; `self-test passed` | instant |
| Docs-truth tags after the README/CLAUDE.md edit | `SpatialCoreTests "[counts],[umbrella],[bug03]"` | exit 0; 54 assertions in 6 cases | seconds |

The first Debug `SpatialCoreTests` run (510 s) did not pass: `[bug03]` failed (fixed, see Deviations) and `[hrtf-switch][churn]` failed once (15 of 16 switches; not reproduced, see Issues Encountered). The table row above is the second full run, after the fix.

### ROADMAP Phase 6 criteria and requirements

| # | Criterion / requirement | Proof |
|---|--------------------------|-------|
| 1 | Harness: both targets, both umbrellas, HRTF data, clean Release build on macOS | `examples/harness/scripts/build-consumer.sh` PASS in CI run **39058330902** (`HarnessCheck: PASS (23 formats, 12 algorithms, profile 5 embedded)`) and in 06-04 and 06-09. Not re-run after the merge: it builds from a pushed commit on GitHub. Locally: `[umbrella]` (both umbrella headers held complete) inside the 418 |
| 2 | `engineComputesGains` unset does not silently no-op | `[intg02]` and `[rt][intg02]` inside Release ctest 418 of 418 and `[rt]` 2521 assertions; counter `getUnsuppliedGainsBlockCount()` (06-02) |
| 3 | Green macOS CI on JUCE 9.0.0 running the suite | CI runs **39053183247** (567bc52) and **39058330902** (dbedbf6): macos-gate success, `JUCE checkout: 9.0.0`, `100% tests passed out of 416`; run **39074814315** (310fb93) green on all four jobs |
| 4 | Coverage wired into CMake with a real number; dedicated tests for `PartitionedConvolver.cpp` and `BinauralRenderer.cpp` | Engine 94.19% (3847/4084 lines), UI 25.89% in CI run **39074814315**; dedicated files 96.08% and 100%; `--self-test` passes after the merge; target 93% in `scripts/coverage-targets.json` (local 94.00%) |
| 5 | Regression tests for OSD #50, #89, #96, #131, #234 that fail against the old behaviour | Seven-row mutation table below (06-03), all tests green again in the 418 after the merge |
| INTG-01 | Consumer can submodule, link and build | Rows 1 and 3; harness PASS in CI |
| INTG-02 | `engineComputesGains` does not silently no-op | Row 2 |
| CI-01 | CI-verified against JUCE 9.0.0 | Row 3; Linux run 39070985243 (416 of 416, 44 skipped by design); Windows builds all targets in the same run |
| TEST-01 | Coverage tooling plus dedicated tests | Row 4 |

### CI evidence, stated plainly

- Last fully green CI run: **39074814315** on `310fb93` (macos-gate, linux-tests, windows-build, coverage at 94.19%).
- `realtime-nightly` last passed in run **39058330902** on 2026-10-17 (`[rt]` 2521 assertions in 47 cases; `[tsan]` subset, UI suite and whole suite each exit 0, 0 ThreadSanitizer warnings).
- Run **40102582138** on `86280a5` was **cancelled** under the user's no-cloud-builds rule. The `coverage` job's 93% enforcement therefore has not run on GitHub. It is confirmed locally only.
- Nothing on this branch after `310fb93` has run on GitHub, the merge with `origin/main` included.

### D-17 mutation table (copied from 06-03-SUMMARY)

| # | Issue | Mutation (scratch, never committed) | Test run | Red result at the 06-03 HEAD |
|---|-------|--------------------------------------|----------|-------------------------------|
| 1 | OSD#50 | `PartitionedConvolver::setIR` Idle branch loads the new IR into the active slot (old hard swap) | `"[osd50]"` | step ratio 9.61 at 64 and 512 samples; 2 of 12 assertions failed |
| 2 | OSD#89 onset | deleted the zero-delay block of `HRTFDatabase::getAlignedHRIR` | `"[mitkemar][osd89]"` | at az +90, -90, +45 the delay and onset checks failed; 6 of 16 assertions |
| 3 | OSD#89 pole | `BinauralRenderer::updateSourceHRIR` great-circle early return replaced by independent az and el checks | `"[renderer][osd89]"` | both memcmp checks failed; max abs diff 0.1322; ear-level control stayed green |
| 4 | OSD#96 | `BinauralRenderer::hrtfDatabase` bound to one function-local static `HRTFDatabase` | `"[multi][isolation]"` | crash (SIGTRAP or SIGABRT), 3 of 3 runs |
| 5 | OSD#131 map faces | `SpatialMapComponent::monoFaces_` per instance | `SpatialCoreUITests "[ui][multi]"` | 4 of 17 assertions failed; construction delta 16 == 1; 15 of 16 identities differ |
| 6 | OSD#131 FFT cache | `SharedFFTCache::getOrCreate` returned a fresh FFT every call | `"[fftcache]"` | 3 of 4 cases failed, 6 of 34 assertions; 15 distinct FFTs per order |
| 7 | OSD#234 | `PartitionedConvolver::process` warm-up and fade without the sample floors | `"[bug02],[convolver][transition]"` | 5 of 55 assertions failed (step 0.07806 vs 0.02685; warm-up 0.4997 vs 2e-6) |

## Merge of origin/main (step A)

`git fetch origin main` then `git merge origin/main` (merge commit `37fef2e`). Four conflicts, each resolved to keep both sides' intent:

| File | Resolution |
|------|-----------|
| `.planning/STATE.md` | kept this branch's Phase 6 position and decisions (including D-18); added one line for main's activity (SC-1, SC-20 fix, Nyquist validation of phases 1, 2, 4) |
| `CLAUDE.md` | main's Binaural row (`MAX_SOURCES` = 32 convolver pairs) joined with this branch's Engine row (`getUnsuppliedGainsBlockCount()`) |
| `include/SpatialCore/Engine/RenderEngine.h` | kept both: `getUnsuppliedGainsBlockCount()` and main's test-only `setLayoutPeekHookForTesting` block |
| `tests/CMakeLists.txt` (CRLF) | kept both new sources, `UmbrellaTests.cpp` and `IssueReferenceTests.cpp`; CRLF endings preserved (checked with `file`) |

Contradiction sweep: `git grep` for `MAX_SOURCES`, "12 per-source", "12 independent" and similar across tracked docs. One live contradiction fixed in the merge commit: CLAUDE.md's design principle "12 independent PartitionedConvolvers" now says one pair per object slot, `MAX_SOURCES` = 32. README and `docs/integration-guide.md` already said 32 (main's text). No doc count `static_assert` moved (algorithms 8, formats 23, layouts 15, profiles 5 are unchanged by main).

Stale "12" left alone on purpose (not introduced by the merge, outside this plan): `.claude/skills/spatial-audio-dsp/SKILL.md` lines 166, 426 and 581, `.claude/skills/spatial-synth-architecture/spatial-synth-architecture.md` lines 12, 27 and 161, `.claude/skills/spatialcore-architecture/spatialcore-architecture.md` line 43, `docs/development-roadmap.md` line 74, and the historical `.planning/codebase/*` map files and `PROJECT.md` line 161. Worth one docs pass.

## Decision (Task 2, recorded in STATE.md)

"Phase 6 goes onto main only after phase verification passes, and the push to main needs the user's go-ahead (it would start a GitHub build); no branch protection for now." This replaces the plan's four-option checkpoint: the user decided it on 2026-10-24 when trimming the plan. The user's answer in the plan's own terms is closest to `wait`, with the merge and the push deferred to after verification.

## Outward actions taken

None. No `git push`, no `gh` command, no GitHub setting, no branch protection, no workflow dispatch. `origin/main` was fetched (read-only) and merged locally.

## Task Commits

1. **Task 1 (merge and evidence):**
   - `37fef2e` merge of origin/main with the four resolutions
   - `a15f927` fix(06-10): qualified the 7 short-form OSD references
   - `83206a2` docs(06-10): `06-VALIDATION.md` statuses and phase-end evidence, `deferred-items.md`
2. **Task 3 (non-push parts):** `abcdc11` docs(06-10): README and CLAUDE.md say `main` has no branch protection yet
3. **Plan metadata:** the final docs commit holding this SUMMARY, REQUIREMENTS.md ticks, STATE.md, ROADMAP.md and `.continue-here.md` (not counted in `commits:`; the raw `git rev-list --count` from the plan base is 25 because it includes the 21 commits merged in from `origin/main`, the first-parent count is 4)

## Files Created/Modified

- `.planning/phases/06-consumer-readiness-ci/06-VALIDATION.md` - all 25 per-task rows set from the runs; evidence section added
- `.planning/phases/06-consumer-readiness-ci/deferred-items.md` - the one-off churn-test failure
- `README.md`, `CLAUDE.md` - one sentence each: no branch protection, direct pushes as before, a push to main waits for the user
- `tests/Binaural/BinauralRendererTests.cpp`, `MitKemarLargePinnaTests.cpp`, `PartitionedConvolverTests.cpp` - issue references written in full (test names and WARN text only)
- `.planning/REQUIREMENTS.md`, `.planning/STATE.md`, `.planning/ROADMAP.md`, `.continue-here.md` - ticks, decision, progress, hand-off

## Decisions Made

- No push and no protection until phase verification passes and the user agrees (see above).
- Test names containing `OSD#50` and `OSD#89` changed to the full tracker form, so any external filter on those exact names must change; none exists in this repo (`git grep` over tests, scripts and CI files).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Short-form OSD issue references tripped main's new BUG-03 guard**
- **Found during:** Step B (first Debug run of `SpatialCoreTests` on the merged tree)
- **Issue:** `tests/Core/IssueReferenceTests.cpp` (from main) rejected `OSD#50` and `OSD#89` in 7 places in tests written on this branch by 06-03. Neither side was wrong alone; the merge combined them.
- **Fix:** wrote the references as `Spatial-Media-Lab/OpenSpatialDelay#N`. No assertion, tolerance or tag changed.
- **Files modified:** `tests/Binaural/BinauralRendererTests.cpp`, `tests/Binaural/MitKemarLargePinnaTests.cpp`, `tests/Binaural/PartitionedConvolverTests.cpp`
- **Verification:** `[bug03]` passes; full Debug rerun 401 cases, 0 failed
- **Committed in:** `a15f927`

---

**Total deviations:** 1 auto-fixed (1 bug). The plan itself was run in its trimmed form by the user's decision: Task 1 without the ThreadSanitizer steps, Task 2 answered by a recorded decision, Task 3 without push, nightly or protection.
**Impact on plan:** none beyond the trim.

## Issues Encountered

- **One failure that did not reproduce.** In the first full Debug run, `[hrtf-switch][churn]` ("every switch completes while a paced render thread runs") finished 15 of 16 switches: the last request, profile 1 (SADIE), never became active within the test's 20 s wait. It passed 16 of 16 in 3 + 10 single Debug runs, 10 Release runs, 2 runs of the 25-case `[hrtf-switch]` group, a second full Debug run and the Release ctest. The cause is not known and the merge is not shown to be clear of it (the SC-20 change touches `renderBlock`; the profile loader was not changed by main). Nothing was loosened. Logged in `deferred-items.md`; if it shows again, capture the status after the timeout first.
- Incremental Debug and Release builds took 9 s and 11 s because the build trees were reused (as the task said), so those durations do not measure a clean build.

## Authentication Gates

None.

## Known Stubs

None (this plan only touched tests' strings, docs and planning files).

## Threat Flags

None. No new endpoint, auth path, file access or schema.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Remaining for Phase 6: phase verification (`/gsd-verify-work 6`), then the user's OK to push `main` (fast-forward only, which starts one GitHub build). Until then the merged branch has not run on GitHub.
- When pushed, watch that run: it is the first for the merge with `origin/main`, and the first to exercise the `coverage` job's 93% enforcement.
- OpenSpatialDelay (pin `3da7d89`) and OpenSpatialPanner (pin `ca8c88b`) can bump their SpatialCore pins once `main` has this branch.

---
*Phase: 06-consumer-readiness-ci*
*Completed: 2026-10-24*

## Self-Check: PASSED

- Files: `06-VALIDATION.md`, `deferred-items.md`, `06-10-SUMMARY.md` exist; README.md, CLAUDE.md and the three test files carry the edits.
- Commits `37fef2e`, `a15f927`, `83206a2`, `abcdc11` are ancestors of HEAD.
- Local gate commands above all exited 0 on the final tree; no conflict markers remain (`grep` of the four resolved files returns 0).
