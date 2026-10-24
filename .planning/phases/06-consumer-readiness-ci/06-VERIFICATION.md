---
phase: 06-consumer-readiness-ci
verified: 2026-10-24T17:30:00Z
status: human_needed
score: 5/5 must-haves verified
covered_files:
  - .github/workflows/ci.yml
  - .planning/phases/06-consumer-readiness-ci/06-01-PLAN.md
  - .planning/phases/06-consumer-readiness-ci/06-01-SUMMARY.md
  - .planning/phases/06-consumer-readiness-ci/06-02-PLAN.md
  - .planning/phases/06-consumer-readiness-ci/06-02-SUMMARY.md
  - .planning/phases/06-consumer-readiness-ci/06-03-PLAN.md
  - .planning/phases/06-consumer-readiness-ci/06-03-SUMMARY.md
  - .planning/phases/06-consumer-readiness-ci/06-04-PLAN.md
  - .planning/phases/06-consumer-readiness-ci/06-04-SUMMARY.md
  - .planning/phases/06-consumer-readiness-ci/06-05-PLAN.md
  - .planning/phases/06-consumer-readiness-ci/06-05-SUMMARY.md
  - .planning/phases/06-consumer-readiness-ci/06-06-PLAN.md
  - .planning/phases/06-consumer-readiness-ci/06-06-SUMMARY.md
  - .planning/phases/06-consumer-readiness-ci/06-07-PLAN.md
  - .planning/phases/06-consumer-readiness-ci/06-07-SUMMARY.md
  - .planning/phases/06-consumer-readiness-ci/06-08-PLAN.md
  - .planning/phases/06-consumer-readiness-ci/06-08-SUMMARY.md
  - .planning/phases/06-consumer-readiness-ci/06-09-PLAN.md
  - .planning/phases/06-consumer-readiness-ci/06-09-SUMMARY.md
  - .planning/phases/06-consumer-readiness-ci/06-10-PLAN.md
  - .planning/phases/06-consumer-readiness-ci/06-10-SUMMARY.md
  - CMakeLists.txt
  - examples/harness/CMakeLists.txt
  - examples/harness/scripts/build-consumer.sh
  - examples/harness/src/HarnessPlugin.cpp
  - examples/harness/src/HarnessPlugin.h
  - examples/harness/src/SoundCheck.cpp
  - include/SpatialCore/Engine/RenderEngine.h
  - include/SpatialCore/SpatialCore.h
  - include/SpatialCore/SpatialCoreUI.h
  - scripts/coverage-report.py
  - scripts/coverage-targets.json
  - scripts/coverage.sh
  - src/Engine/RenderEngine.cpp
  - tests/CMakeLists.txt
  - tests/Core/UmbrellaTests.cpp
  - tests/Engine/UnsuppliedGainsTests.cpp
  - tests/UI/UmbrellaUITests.cpp
covered_digest: "v3:sha256:10993e20e1b19e072287a209b39acdd406688359ca4591a8d836ee976f35e644"
behavior_unverified: 0
overrides_applied: 0
human_verification:
  - test: "Measure engine line coverage at HEAD 0f4d1f7 (run scripts/coverage.sh, about 10 minutes, or read the coverage job of the first CI run after the push)"
    expected: "The report prints 'Target: 93% - PASS (Engine, now NN.NN%)' with NN.NN at or above 93.00"
    why_human: "The only local figure (94.00%) is an export from 2026-10-18, made before the 93% target existed and before two merges of origin/main changed about 330 lines of engine source. The 93% rule has never been evaluated against the code that is about to be pushed, and a build is outside this verification's rules."
---

# Phase 6: Consumer Readiness & CI Verification Report

**Phase Goal:** OpenSpatialDelay can adopt SpatialCore as a submodule without surprises, proven by a harness and a green CI run.
**Verified:** 2026-10-24
**Status:** human_needed (nothing failed; one number the phase promised has not been measured on the final code)
**Re-verification:** No, initial verification

## Short answer for the owner

Every success criterion holds in the code. Nothing is broken and no requirement is unmet. One open item remains, and it is small: the "engine coverage must stay at 93% or more" rule is wired correctly, but nobody has measured the coverage of the code as it stands now. The 94.00% "local proof" quoted in the notes is from 18 October, before two merges of main added about 330 lines of engine code. The rule has about 49 lines of slack (94.19% on the last CI run against 93%), so it is probably fine, but it is a guess until the first measurement.

## Goal Achievement

### Success criteria (ROADMAP contract)

| # | Criterion | Status | Evidence |
|---|-----------|--------|----------|
| 1 | Harness submodules SpatialCore, links both targets, includes both umbrella headers, resolves HRTF data, builds clean in Release on macOS | VERIFIED | `examples/harness/CMakeLists.txt` runs `add_subdirectory(SpatialCore)` on a real submodule and links `SpatialCore` and `SpatialCoreUI`; `HarnessPlugin.h:7-8` includes `<SpatialCore/SpatialCore.h>` and `<SpatialCore/SpatialCoreUI.h>`; `SoundCheck.cpp` checks profile 5 loads from the embedded data and prints `HarnessCheck: PASS`; `build-consumer.sh` fails (exit 4) on any compiler warning in the harness or SpatialCore. Recorded green: CI runs 39053183247, 39058330902, 39070985243. `[umbrella]` cases re-run by me in the existing Release binary: pass. Caveat: the harness builds from a pushed commit, so it has not run on the two merges of main (nothing after 310fb93 has run on GitHub; user-decided). The harness files did not change since 310fb93 |
| 2 | A consumer that does not set `engineComputesGains` gets computed gains or fails loudly | VERIFIED | `src/Engine/RenderEngine.cpp:869-873` counts the block in `unsuppliedGainsBlockCount_` and runs `jassertfalse` once per engine; `blockHasUnsuppliedGains` (line 955) mirrors the dispatch chain; `getUnsuppliedGainsBlockCount()` is public (`RenderEngine.h:457`). By locked decision D-05/D-06 the flag stays default false, and Release counts only (no silent gain fill). `docs/integration-guide.md:161-183` states the two modes and the counter. 6 `[intg02]` cases plus the `[rt][intg02]` case, re-run by me: pass |
| 3 | Green macOS CI run builds against JUCE 9.0.0 and runs the Catch2 suite | VERIFIED | `ci.yml` `macos-gate` configures Debug, aborts unless `git describe` of JUCE is `9.0.0`, runs both Debug executables, Release ctest from the build root with `--no-tests=error`, and the UI-listing check. Recorded green runs 39053183247 and 39058330902 (416 of 416, JUCE 9.0.0). The `macos-gate`, `linux-tests` and `windows-build` jobs are unchanged since 310fb93 except removal of `if: github.event_name != 'schedule'` lines. YAML parses; jobs: macos-gate, linux-tests, windows-build, coverage, realtime-nightly. Run ids are quoted from the summaries, I could not read GitHub (no `gh`) |
| 4 | Coverage wired into CMake and reports a real number; `PartitionedConvolver.cpp` and `BinauralRenderer.cpp` have dedicated tests | VERIFIED | `CMakeLists.txt:62-75` option `SPATIALCORE_ENABLE_COVERAGE` (Clang only, off by default); `scripts/coverage.sh` and `coverage-report.py` produce a per-area table (`python3 -I scripts/coverage-report.py --self-test`: all 12 checks PASS, exit 0, run by me). CI run 39074814315: engine 94.19% (3847/4084), UI 25.89%. Dedicated test files `tests/Binaural/PartitionedConvolverTests.cpp` and `BinauralRendererTests.cpp` exist and run in the 418 |
| 5 | Regression tests for OSD #50, #89, #96, #131, #234 that fail against the old behaviour | VERIFIED (caveat on #234) | Tags present: `[osd50]` (PartitionedConvolverTests.cpp:149), `[osd89]` (BinauralRendererTests.cpp:647, MitKemarLargePinnaTests.cpp:157), `[multi][isolation]` (#96), `[fftcache]` and `[ui][multi]` (#131), `[bug02]` and `[convolver][transition]` (#234). I ran all of them against the current Release binary (built after the last source change, no source newer than it): 260 assertions in 24 cases pass, UI `[ui][multi]` and `[umbrella]` 19 assertions pass. The red-against-old-code proof is the seven-row mutation table in 06-03 (recorded, not re-run: no builds allowed). Caveat: the #234 row mutated the convolver as it was before PR #33 rewrote it (time-based crossfade). The #234 tests were then rewritten by main (`10d79c7`) and the red proof was not repeated. Reading them, `[convolver][transition]` asserts sample-exact timing at 32, 512 and irregular call sizes, which a call-counted fade cannot meet, so I judge they still bite |

**Score:** 5/5 success criteria verified.

### Requirements Coverage

| Requirement | Plans | Status | Evidence |
|-------------|-------|--------|----------|
| INTG-01 submodule, link, build | 06-01, 06-04, 06-09, 06-10 | SATISFIED | Criterion 1; `SPATIALCORE_BUILD_TESTS` defaults to the top-level flag (`CMakeLists.txt:385`), so a consumer gets no test targets; both umbrellas held complete by `[umbrella]` source-scan cases |
| INTG-02 flag does not silently no-op | 06-02, 06-09, 06-10 | SATISFIED | Criterion 2 |
| CI-01 CI-verified against JUCE 9.0.0 | 06-01, 06-05, 06-06, 06-09, 06-10 | SATISFIED | Criterion 3, plus Linux (416 of 416) and Windows build-only in run 39070985243. Scope per D-08 and D-18 |
| TEST-01 coverage tooling + untested modules covered | 06-03, 06-07, 06-08, 06-09, 06-10 | SATISFIED, with the open item below | Criteria 4 and 5. The 93% rule is enforced in code (next section) |

Orphaned requirements: none. REQUIREMENTS.md maps exactly these four IDs to Phase 6 and every one is claimed by a plan.

### The 93% coverage rule: what I could and could not confirm

Enforcement chain, read in the code (not the summaries):

1. `scripts/coverage-targets.json` holds `"engine_line_percent": 93` and no UI key (UI report-only, as decided).
2. `scripts/coverage.sh` appends `--targets scripts/coverage-targets.json` whenever the file exists, runs the report script, and `exit "$status"` passes its status up; the script runs under `set -euo pipefail`.
3. `coverage-report.py` `evaluate_targets` returns failed when covered/total*100 is below the target, using an exact fraction (no rounding), and `main` returns exit 1 (`EXIT_BELOW_TARGET`). A missing source returns exit 3.
4. `ci.yml` `coverage` job runs `scripts/coverage.sh build-cov` as a plain step, so exit 1 fails the job. The summary and artifact steps use `if: always()`.
5. `--self-test` (run by me): "9300 of 10000 against a target of 93 passes (equal)", "9299 of 10000 ... fails", end-to-end exit 0 at the target and exit 1 just below. All PASS.

So a drop below 93% turns the CI coverage job red, by construction.

What is not proven: that HEAD meets 93%.

- `build-cov/coverage/summary.md` on disk is dated 18 October (00:22) and still says "No coverage target set yet (OQ-4)". It is the export 06-08 used for "94.00%".
- Since that export, `git diff 310fb93 HEAD -- src include` shows 16 files, +529/-192 lines, including `PartitionedConvolver.cpp` (+208 changes), `BinauralRenderer.cpp` (+162), `RenderEngine.cpp`, `HRTFDatabase.cpp`. The 4084-line denominator has moved.
- Slack: at 4084 lines, 93% allows 286 uncovered lines; CI had 237. The rule tolerates about 49 more uncovered lines, plus 7% of any new lines. Main's PR #33 changed tests too (+467/-198), so it is probably fine, but this is an estimate.
- CI-versus-local differences of 0.19 to 0.22 points were seen on the same commit (timing-dependent lines), so a result between 93.0 and 93.3 would be a coin flip.

This is the human item in the frontmatter. It resolves by either path: one local `scripts/coverage.sh` run (about 10 minutes, outside the D-19 everyday gate, so your call), or reading the `coverage` job of the first CI run after you allow the push.

### Artifacts and key links

`verify.artifacts` on all 10 plans: every declared artifact exists and is substantive (5/5, 4/4, 3/3, 6/6, 1/1, 1/1, 5/5, 1/1, 6/6, 2/2). `verify.key-links` reported failures on 9 plans, but every one is a malformed link spec in the plan itself ("Source file not found (from: must be a relative file path...)"; the `from` field holds prose, not a path), not a broken link. I checked the load-bearing links by hand:

| From | To | Status |
|------|----|--------|
| `RenderEngine::renderBlock` | `unsuppliedGainsBlockCount_` / `blockHasUnsuppliedGains` | WIRED (`RenderEngine.cpp:869`) |
| `docs/integration-guide.md` "Set these flags" | `getUnsuppliedGainsBlockCount()` | WIRED (lines 161-183) |
| `ci.yml` macos-gate | `examples/harness/scripts/build-consumer.sh` | WIRED |
| `ci.yml` coverage | `scripts/coverage.sh` -> `coverage-report.py --targets` | WIRED |
| `scripts/coverage.sh` | `llvm-cov export` on both test executables (the plan's pattern named a different string) | WIRED |
| top-level `CMakeLists.txt` `enable_testing()` | `build-release/CTestTestfile.cmake` | WIRED (file present; ctest from the build root lists 418) |
| `HarnessPlugin.cpp` | all five opt-in flags, `setHRTFProfile`, `setAlgorithmIndex` | WIRED (lines 35-180) |

### Local evidence I re-ran (quick, no builds)

| Check | Result |
|-------|--------|
| `python3 -I scripts/coverage-report.py --self-test` | exit 0, 12 PASS |
| `ctest --test-dir build-release/tests -N` | 418 tests, 17 `ui:` |
| Release binary newer than every file under `src`, `include`, `tests`, `CMakeLists.txt` | yes |
| `SpatialCoreTests "[intg02],[umbrella],[osd50],[osd89],[counts],[bug03],[fftcache],[bug02],[convolver][transition],[multi][isolation]"` | all passed, 260 assertions in 24 cases |
| `SpatialCoreUITests "[ui][multi],[umbrella]"` | all passed, 19 assertions in 3 cases |
| `ci.yml` YAML parse (ruby) | valid; 5 jobs; triggers push, pull_request (main), workflow_dispatch; no `schedule` |
| Debt-marker scan (`TBD`, `FIXME`, `XXX`, `TODO`, `HACK`, `PLACEHOLDER`) over the phase's scripts, harness, ci.yml, engine and new tests | none |

The full Release ctest (418 of 418, 130-147 s) and the Debug runs are quoted from 06-10-SUMMARY; I did not re-run the whole suite (over my time limit, and not needed once the criteria tags pass).

### Anti-patterns and warnings

| Severity | Item | Evidence |
|----------|------|----------|
| Warning | Measurement date is wrong in the docs | README.md:201 and CLAUDE.md (Build System, Coverage) say the 94.19% figures were measured "2026-10-24 (CI run on 310fb93)". Commit 310fb93 is dated 2026-10-18 00:22 (+02:00) and the run id sits beside the 2026-10-17 runs. 2026-10-24 is the day the target was chosen. Provenance trail: 06-07-SUMMARY "Measurements" names run 39074814315 and the push of 310fb93, then README was written in 06-09 with the decision date. Fix the date to 2026-10-17/18 |
| Warning | REQUIREMENTS.md TEST-01 cites "local 94.00%" as proof the target holds | That figure is the pre-merge export (see above). Same root cause as the human item |
| Warning | One unexplained test failure | `[hrtf-switch][churn]` ended 15 of 16 switches once in a full Debug run after the first merge (`deferred-items.md`). Not reproduced in 34 later runs. Cause unknown; it could be a lost last request in the engine's profile loader (Phase 3 code), or load timing. Not a Phase 6 deliverable, but it is open and worth a watch on the first CI run |
| Info | Stale comment in the `[osd50]` test | `PartitionedConvolverTests.cpp:~160` `kFadeFloor = 2048 // the convolver's crossfade floor`. After PR #33 the fade is 1024 samples at 48 kHz (`hrirTransitionMsToSamples`). The test passes because 2048 only makes the settle point later; the comment and the header note about "sample floors" are out of date |
| Info | Stale "12 per-source" counts left on purpose | Listed in 06-10-SUMMARY: three `.claude/skills` files, `docs/development-roadmap.md:74`, `.planning/codebase/*`. CLAUDE.md itself is fixed |
| Info | `06-VALIDATION.md` frontmatter still says `status: draft`, `nyquist_compliant: false`, `wave_0_complete: false`, and the Wave 0 checklist is unticked | The per-task table below it is filled in. Cosmetic; `/gsd-validate-phase` would settle it |

### User-decided scope (not counted as gaps)

- ThreadSanitizer / realtime-safety re-run skipped (D-18); last full pass run 39058330902, 2026-10-17. The 20-second `[rt]` trap does run in the everyday gate, and I re-ran `[intg02]` with its `[rt]` sibling case.
- Everyday gate is Release-only (D-19); no branch protection; no push to main yet; no cloud builds, so no CI evidence after 310fb93 (run 40102582138 cancelled).

### Gaps Summary

No failed truths, no missing or stub artifacts, no unwired links, no unmet requirements. Status is `human_needed` only because the 93% coverage rule has never been evaluated on the final code. Closing that item, correcting the 2026-10-24 date in README.md and CLAUDE.md, and watching the first CI run for the churn test are the follow-ups. None of them blocks the push to main on the grounds of the phase goal, but the push is the first moment GitHub will exercise the merged tree, the harness on it, and the 93% rule together, so expect that run to be the real final check.

---

_Verified: 2026-10-24_
_Verifier: Claude (gsd-verifier)_
