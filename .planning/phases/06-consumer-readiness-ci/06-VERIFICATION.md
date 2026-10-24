---
phase: 06-consumer-readiness-ci
verified: 2026-10-24T19:30:00Z
status: passed
score: 5/5 must-haves verified
covered_files:
  - .claude/skills/spatial-audio-dsp/SKILL.md
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
  - CLAUDE.md
  - CMakeLists.txt
  - README.md
  - docs/integration-guide.md
  - examples/harness/CMakeLists.txt
  - examples/harness/README.md
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
covered_digest: "v3:sha256:dde7d296993abdf2686bf56ba0ea5d762b32fb27fab3f1ecc6ff981541a46c42"
behavior_unverified: 0
overrides_applied: 0
re_verification:
  previous_status: human_needed
  previous_score: 5/5
  gaps_closed: []
  gaps_remaining: []
  regressions: []
deferred:
  - truth: "Engine line coverage is at or above the 93% rule on the merged tree"
    addressed_in: "First release-candidate CI run (tag v1.0.0-rc1 or similar), or one manual scripts/coverage.sh run before it"
    evidence: "User decisions D-19 and D-20 (STATE.md, 2026-10-24): the coverage build runs only when preparing a public release, and GitHub builds start only on a release tag or by hand. Not a later roadmap phase; deferral is the user's call. Last measured 94.19% (run 39074814315 on 310fb93, 2026-10-17); the merged tree has not been measured."
---

# Phase 6: Consumer Readiness & CI Verification Report

**Phase Goal:** OpenSpatialDelay can adopt SpatialCore as a submodule without surprises, proven by a harness and a green CI run.
**Verified:** 2026-10-24
**Status:** passed (one user-deferred measurement, listed below)
**Re-verification:** Yes. Previous report e445bbd was `human_needed`; the only open item was the unmeasured 93% rule, which the user has since deferred (D-19, D-20).

## Short answer for the owner

Nothing failed. All five success criteria and all four requirements hold. Since the last report only documentation and the CI trigger changed (six files, no source, header, test or script), and I confirmed that from the diff. The new CI trigger policy (D-20) is coherent: the workflow file parses, starts only on a `v*` tag or by hand, and every job's start condition works on both. I found and fixed one doc line that still said CI runs on every push to main. The 93% coverage number is still not measured on the merged tree; that is recorded as deferred, by your decision, not as a gap.

## What changed since the last report (e445bbd to b7a71a1)

`git diff --name-only e445bbd HEAD`: `.github/workflows/ci.yml`, `.planning/STATE.md`, `CLAUDE.md`, `README.md`, `docs/integration-guide.md`, `examples/harness/README.md`. No file under `src`, `include`, `tests`, `scripts`, `CMakeLists.txt`. The Release test binaries (18:47 today) are newer than every source, header, test and script file, so the earlier behavioral evidence still describes the current code.

## Goal Achievement

### Success criteria (ROADMAP contract)

| # | Criterion | Status | Evidence |
|---|-----------|--------|----------|
| 1 | Harness submodules SpatialCore, links both targets, includes both umbrella headers, resolves HRTF data, builds clean in Release on macOS | VERIFIED | Unchanged since the last report (harness files untouched; only `examples/harness/README.md` wording changed). `build-consumer.sh` still submodules the GitHub remote and fetches the exact commit by SHA (line 99), exiting 3 if it is not pushed, so a tag push (where `github.sha` is the tagged commit) works. Recorded green: runs 39053183247, 39058330902, 39070985243. Caveat carried over: the harness has not run on the merged tree (no GitHub build since 310fb93, user-decided) |
| 2 | A consumer that does not set `engineComputesGains` gets computed gains or fails loudly | VERIFIED | `RenderEngine.cpp` counts the block and asserts once; `getUnsuppliedGainsBlockCount()` public. `[intg02]` cases re-run by me against the current Release binary: pass |
| 3 | Green macOS CI run builds against JUCE 9.0.0 and runs the Catch2 suite | VERIFIED | Recorded green runs 39053183247 and 39058330902 (416 of 416, JUCE 9.0.0). `macos-gate` steps are unchanged by D-20 (the ci.yml diff touches only the header comments, the `on:` triggers, the `realtime` input default (true to false) and the realtime job's `if:`; no step changed). The criterion asks for a green run, which exists; it ran on the pre-merge tree. See Deferred Items |
| 4 | Coverage wired into CMake and reports a real number; `PartitionedConvolver.cpp` and `BinauralRenderer.cpp` have dedicated tests | VERIFIED | `SPATIALCORE_ENABLE_COVERAGE`, `scripts/coverage.sh`, `coverage-report.py`; `python3 -I scripts/coverage-report.py --self-test` re-run: all checks PASS, "self-test passed". CI run 39074814315 read engine 94.19% (3847 of 4084), UI 25.89%. Dedicated test files exist and are in the 418 tests (`ctest --test-dir build-release -N`: Total Tests 418) |
| 5 | Regression tests for OSD #50, #89, #96, #131, #234 that fail against the old behaviour | VERIFIED (caveat on #234 as before) | Re-ran `[intg02],[umbrella],[osd50],[osd89],[bug02],[fftcache],[multi][isolation],[convolver][transition]` in the Release binary: all passed (216 assertions in 20 cases); UI `[ui][multi],[umbrella]`: all passed (19 assertions in 3 cases). The red-against-old-code proof is the mutation table in 06-03 (recorded, not re-run) |

**Score:** 5/5 success criteria verified.

### Requirements Coverage

| Requirement | Plans | Status | Evidence |
|-------------|-------|--------|----------|
| INTG-01 submodule, link, build | 06-01, 06-04, 06-09, 06-10 | SATISFIED | Criterion 1 |
| INTG-02 flag does not silently no-op | 06-02, 06-09, 06-10 | SATISFIED | Criterion 2 |
| CI-01 CI-verified against JUCE 9.0.0 | 06-01, 06-05, 06-06, 06-09, 06-10 | SATISFIED | Criterion 3, Linux 416 of 416 and Windows build in run 39070985243. The requirement text does not say "every push" ("a green macOS CI run building against JUCE 9.0.0 and running the Catch2 suite"), so D-20 does not contradict it; I added a dated trigger-policy note to REQUIREMENTS.md so nobody reads it as a per-push check |
| TEST-01 coverage tooling + untested modules covered | 06-03, 06-07, 06-08, 06-09, 06-10 | SATISFIED | Criteria 4 and 5; the 93% rule is enforced in code. Its measurement on the merged tree is deferred (below); I added a dated note to REQUIREMENTS.md saying so |

Orphaned requirements: none (REQUIREMENTS.md maps exactly these four IDs to Phase 6).

## CI trigger policy (D-20) check

| Check | Result |
|-------|--------|
| `ci.yml` parses (ruby YAML load) | Valid. Triggers: `push: tags: ["v*"]` and `workflow_dispatch` (boolean input `realtime`, default false). No `pull_request`, no branch `push`, no `schedule` |
| Jobs | macos-gate, linux-tests, windows-build, coverage, realtime-nightly |
| `if:` on macos-gate, linux-tests, windows-build, coverage | None, so each runs on a tag push and on every manual dispatch |
| `if:` on realtime-nightly | `startsWith(github.ref, 'refs/tags/v') || (github.event_name == 'workflow_dispatch' && inputs.realtime)`. On a tag push `github.ref` is `refs/tags/vX`, so the first operand is true; on dispatch it follows the ticked input. On a push the `inputs` context is empty and is not reached. Coherent |
| Leftover `pull_request` expressions | `concurrency.group` and `SHA_UNDER_TEST` still mention `github.event.pull_request...`; both fall back (`|| github.ref`, `|| github.sha`) and are inert. Info only |
| Edge: manual dispatch run from a tag ref | The realtime job would also run without the input ticked (first operand true). Harmless and arguably intended |
| Harness SHA on a tag push | `github.sha` is the tagged commit; `build-consumer.sh` fetches it by SHA from the remote, which exists after the push. Works |
| Docs agree with the workflow | README:135, README "Consumer harness", CLAUDE.md (Tests, Consumer Plugins), `docs/integration-guide.md` (lines 185 and 561), `examples/harness/README.md:68`, the `ci.yml` header: all say tag-or-manual, no branch push. STATE.md D-20 matches |
| Stale "every push" claims | One found and fixed: `.claude/skills/spatial-audio-dsp/SKILL.md:549` said "GitHub Actions CI on push to `main`"; now states tag or manual only (D-20). `.claude/skills/cross-platform-builds/REFERENCE.md:628` (`on: [push, pull_request]`) is a generic template for any JUCE project, not a statement about SpatialCore; left as is. `.planning/ROADMAP.md` plan lines ("each one pushes the work branch for CI") are historical records of 06-04 to 06-08, superseded in STATE.md by D-20; left as history |

## The 93% coverage rule

Enforcement chain unchanged and re-confirmed: `scripts/coverage-targets.json` `engine_line_percent: 93`, `coverage.sh` passes `--targets`, `coverage-report.py` exits 1 below target (self-test proves equal passes, one below fails), and the `coverage` job runs `scripts/coverage.sh build-cov` as a plain step. With D-20 that job now runs on a tag push or manual dispatch, so the rule still fires, just later.

Not measured: whether the merged tree meets 93%. Last real figures: 94.19% (CI, 310fb93, 2026-10-17); local 94.00% (2026-10-18 export, pre-merge). The README and CLAUDE.md date fix from the last report is confirmed: both now say "2026-10-17, run 39074814315 on `310fb93`" (310fb93 is dated 2026-10-18 00:22 +0200; the run id sits beside the 2026-10-17 runs).

### Deferred Items

| # | Item | Addressed In | Evidence |
|---|------|-------------|----------|
| 1 | Engine coverage at or above 93% on the merged tree | First release-candidate CI run (tag `v*`), or one manual `scripts/coverage.sh` run before it | User decisions D-19 and D-20 in STATE.md. The estimate from the last report still holds: about 49 uncovered lines of slack at 4084 lines, and CI-versus-local differences of 0.2 points were seen, so a result between 93.0 and 93.3 would be a coin flip. If the first release-candidate run reads below 93, that is a real finding then, not before |

## Artifacts and key links

Unchanged from the previous report (no code in the covered set changed except `ci.yml`, whose artifacts and wiring are re-read above). `verify.key-links` still fails on nine plans because those plans' link specs hold prose in `from`, not paths; that was assessed by hand last time and nothing it covers changed.

## Anti-patterns and warnings

| Severity | Item | Evidence |
|----------|------|----------|
| Warning | One unexplained test failure, still open | `[hrtf-switch][churn]` ended 15 of 16 switches once in a full Debug run (`deferred-items.md`); not reproduced in 34 later runs. Watch it on the first release-candidate run |
| Info | Stale comment in the `[osd50]` test | `kFadeFloor = 2048` comment predates PR #33 (fade is 1024 samples at 48 kHz); the test still bites |
| Info | Stale "12 per-source" counts left on purpose | Listed in 06-10-SUMMARY (three `.claude/skills` files, `docs/development-roadmap.md:74`, `.planning/codebase/*`) |
| Info | `.claude/skills/spatial-audio-dsp/SKILL.md:550` points to `scripts/download_windows_build.sh`, which does not exist in `scripts/` | Pre-existing, not part of this phase; not fixed |
| Info | `06-VALIDATION.md` frontmatter still `status: draft` | Cosmetic |
| Info | Debt-marker scan | None in the files that changed in this round |

### User-decided scope (not counted as gaps)

D-18 (realtime check release-only), D-19 (everyday gate is Release-only), D-20 (no GitHub build except a release tag or by hand), no branch protection, no GitHub evidence after 310fb93.

### Behavioral spot-checks

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| Phase regression tags pass in current Release binary | `./build-release/tests/SpatialCoreTests "[intg02],[umbrella],[osd50],[osd89],[bug02],[fftcache],[multi][isolation],[convolver][transition]"` | All tests passed (216 assertions in 20 test cases), 4 s | PASS |
| UI multi-instance and umbrella | `./build-release/tests/SpatialCoreUITests "[ui][multi],[umbrella]"` | All tests passed (19 assertions in 3 test cases) | PASS |
| Coverage rule logic | `python3 -I scripts/coverage-report.py --self-test` | self-test passed | PASS |
| UI tests are in ctest | `ctest --test-dir build-release -N` | Total Tests: 418 | PASS |

Probe execution: SKIPPED (the phase declares no `probe-*.sh` probes).

## Gaps Summary

No failed truths, no missing artifacts, no unwired links, no unmet requirements. The one open number (93% on the merged tree) is a user-deferred item, not a gap. The first tag-triggered run is the first moment GitHub exercises the merged tree, the harness on it, the 93% rule and the realtime check together, so expect that to be the real final check.

---

_Verified: 2026-10-24_
_Verifier: Claude (gsd-verifier)_
