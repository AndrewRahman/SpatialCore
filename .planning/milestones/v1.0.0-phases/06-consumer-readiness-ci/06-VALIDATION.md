---
phase: "06"
slug: "consumer-readiness-ci"
# status lifecycle: draft (seeded by plan-phase) → validated (set by validate-phase §6)
# audit-milestone §5.5 distinguishes NOT-VALIDATED (draft) from PARTIAL (validated + nyquist_compliant: false) (#2117)
status: validated
nyquist_compliant: true
wave_0_complete: true
created: "2026-10-17"
validated: "2026-10-25"
---

# Phase 06 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Catch2 v3.7.1 (FetchContent); `SpatialCoreTests` (404 cases at `17224af`, 2026-10-25) + `SpatialCoreUITests` (17 cases, ctest names prefixed `ui:`); 421 ctest entries in all |
| **Config file** | `tests/CMakeLists.txt` (explicit source lists, CRLF line endings) |
| **Quick run command** | `./build-release/tests/SpatialCoreTests "[intg02],[osd50],[osd89]"` |
| **Full suite command** | README "Local test gate": Debug build + both executables; Release build + `ctest --test-dir build-release/tests --output-on-failure` |
| **Estimated runtime** | ~200 seconds (70 s Release build + 126 s Release ctest, measured) |

---

## Sampling Rate

- **After every task commit:** Run the touched area's tags (quick run) plus a Release build of the changed targets
- **After every plan wave:** Run the full local gate; `build-consumer.sh` once the harness lands
- **Before `/gsd-verify-work`:** Full suite must be green, macOS CI job green on a pushed commit, README realtime-safety check once at phase end
- **Max feedback latency:** 200 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 06-01-T1 | 06-01 | 1 | CI-01, INTG-01 | T-06-01, T-06-02 | Root ctest finds every test (BASE found 0); a consumer gets no test targets by default | local build | `ctest --test-dir build-release --no-tests=error --output-on-failure` and the `/tmp/sc-p6-consumed` target listing | ✅ | ✅ green (2026-10-25 at `17224af`: Release ctest 421/421; root ctest `-N` lists 421) |
| 06-01-T2 | 06-01 | 1 | INTG-01 | — | Both umbrellas complete, each compiles first in its own TU | unit (source scan + TU) | `./build-release/tests/SpatialCoreTests "[umbrella]"` and `./build-release/tests/SpatialCoreUITests "[umbrella]"` | ✅ | ✅ green (2026-10-25: `[umbrella]` 2 DSP cases, 1 UI case) |
| 06-02-T1 | 06-02 | 2 | INTG-02 | T-06-04, T-06-05 | Unsupplied gains counted; nothing fills them | unit (red first) | `./build-release/tests/SpatialCoreTests "[intg02]"` | ✅ | ✅ green (2026-10-25: `[intg02]` 45 assertions in 7 cases) |
| 06-02-T2 | 06-02 | 2 | INTG-02 | T-06-03 | No false positive on HRTF, Ambisonics, the engineSelectsHRTF fill; misuse path allocation- and lock-free | unit + trap | `./build-release/tests/SpatialCoreTests "[intg02]"`, `"[rt][intg02]"`, `"[rt]"` | ✅ | ✅ green (2026-10-25: `[intg02]` 7 cases, `[rt][intg02]` 1 case, `[rt]` 2521 assertions in 47 cases) |
| 06-02-T3 | 06-02 | 2 | INTG-02 | — | Guide states the flag set and the gains mode (D-07) | doc grep | `grep -c '^### Set these flags' docs/integration-guide.md` | ✅ | ✅ green (2026-10-25: count 1) |
| 06-03-T1 | 06-03 | 3 | TEST-01 | T-06-06, T-06-07 | OSD#50 test red against the hard swap | unit + mutation | `./build-release/tests/SpatialCoreTests "[osd50]"` | ✅ | ✅ green (2026-10-25: `[osd50]` 12 assertions; red run recorded in 06-03) |
| 06-03-T2 | 06-03 | 3 | TEST-01 | T-06-06, T-06-07 | OSD#89 onset and pole tests red against each old behaviour | unit + mutation | `./build-release/tests/SpatialCoreTests "[osd89]"` | ✅ | ✅ green (2026-10-25: `[osd89]` 28 assertions in 2 cases; red runs recorded in 06-03) |
| 06-03-T3 | 06-03 | 3 | TEST-01 | T-06-06, T-06-07 | OSD#96, #131, #234 tests red again at HEAD; no mutation left | mutation + full gate | `"[multi][isolation]"`, `SpatialCoreUITests "[ui][multi]"`, `"[fftcache]"`, `"[bug02],[convolver][transition]"`; `git diff --quiet -- src include` | ✅ | ✅ green (2026-10-25: `[multi][isolation]`, UI `[ui][multi]`, `[fftcache]`, `[bug02],[convolver][transition]` all pass; seven red runs recorded in 06-03) |
| 06-04-T1 | 06-04 | 4 | INTG-01 | T-06-08, T-06-09, T-06-11 | Consumer built from a GitHub submodule at the pushed commit; bad inputs refused | integration script | `examples/harness/scripts/build-consumer.sh "$(git rev-parse HEAD)"`; refusals exit 2, 2, 3 | ✅ | ✅ green (2026-10-25, local run against pushed main `17224af`: exit 0, `HarnessCheck: PASS (23 formats, 12 algorithms, profile 5 embedded)`, 0 warnings in harness and SpatialCore; refusals from CI run 39058330902) |
| 06-04-T2 | 06-04 | 4 | INTG-01 | — | Every format and algorithm sounds; editor paints; OSD-like build passes | integration script | same script; `HARNESS_OSD_LIKE=1` run | ✅ | ✅ green (same local run: every format and the algorithm sweep PASS, editor paints; OSD-like build recorded in 06-04) |
| 06-05-T1 | 06-05 | 5 | CI-01 | T-06-12 to T-06-17 | macOS gate green on GitHub with JUCE 9.0.0, ctest 100%, harness PASS | CI run | `gh run view <id> -R AndrewRahman/SpatialCore --json jobs` (macos-gate success) | ✅ | ✅ green, CI record (runs 39053183247 on `567bc52`, 39058330902 on `dbedbf6`: macos-gate success, re-read 2026-10-25); local equivalent: the 2026-10-25 gate and harness above |
| 06-05-T2 | 06-05 | 5 | CI-01 | — | realtime-nightly green on dispatch, 0 ThreadSanitizer warnings | CI run | `gh run view <id> ... realtime-nightly` success | ✅ | ✅ green, CI record (run 39058330902, realtime-nightly success, 0 warnings); `[rt]` re-run locally 2026-10-25; ThreadSanitizer not re-run (release-only, D-18) |
| 06-06-T1 | 06-06 | 6 | CI-01 | T-06-18, T-06-19 | Linux runs the suite, green, no continue-on-error | CI run | `gh run view <id> ... linux-tests` success | ✅ | ✅ green, CI record only (run 39070985243 on `422c308` and 39074814315 on `310fb93`: linux-tests success); no local Linux equivalent |
| 06-06-T2 | 06-06 | 6 | CI-01 | T-06-18 | Windows builds every target, green, no test run | CI run | `gh run view <id> ... windows-build` success | ✅ | ✅ green, CI record only (runs 39070985243, 39074814315: windows-build success); no local Windows equivalent |
| 06-07-T1 | 06-07 | 7 | TEST-01 | T-06-21 | Per-area engine number and separate UI number; no source missing | script | `python3 -I scripts/coverage-report.py --self-test`; `scripts/coverage.sh build-cov` | ✅ | ✅ green (2026-10-25: self-test passed; `scripts/coverage.sh build-cov` exit 0, engine 93.84%, UI 25.89%) |
| 06-07-T2 | 06-07 | 7 | TEST-01 | T-06-20 | Coverage on the job summary and as an artifact, inside GitHub only | CI run | `gh run download <id> -n coverage-report` | ✅ | ✅ green, CI record (run 39074814315 on `310fb93`: coverage job success, engine 94.19%); local report in `build-cov/coverage/summary.md` |
| 06-07-T3 | 06-07 | 7 | TEST-01 | — | User picks the target (OQ-4) | checkpoint | manual (see below) | — | ✅ green (user chose `hold`, 2026-10-24) |
| 06-08-T1 | 06-08 | 8 | TEST-01 | T-06-23 | CI enforces the chosen target | CI run | coverage job success with "Target: ... - PASS" | ✅ | ✅ green, local equivalent (2026-10-25 at `17224af`: `Target: 93% - PASS (Engine, now 93.84%)`, exit 0); not yet run on GitHub (run 40102582138 cancelled; first `v*` CI run will, #35) |
| 06-08-T2 | 06-08 | 8 | TEST-01 | T-06-22 | Gate fails 0.01 point below, passes exactly at the target | script | `python3 -I scripts/coverage-report.py ... --targets /tmp/sc-p6-08-above.json` exits 1 | ✅ | ✅ green (self-test 2026-10-25: "just below the target exits 1", "exactly at the target exits 0") |
| 06-09-T1 | 06-09 | 9 | INTG-01, CI-01, TEST-01 | T-06-24 | README matches the CI and its harness command runs as written | script + doc grep | `examples/harness/scripts/build-consumer.sh "$(git rev-parse HEAD)"`; stale-phrase grep | ✅ | ✅ green (harness command run as written, 2026-10-25) |
| 06-09-T2 | 06-09 | 9 | INTG-01, INTG-02, CI-01 | T-06-24 | Context docs state what shipped; header comment only | doc grep + unit | `./build-release/tests/SpatialCoreTests "[fftcache],[umbrella]"` | ✅ | ✅ green (2026-10-25: `[fftcache]`, `[umbrella]` pass) |
| 06-09-T3 | 06-09 | 9 | all four | — | Requirement amendments (D-02, D-08, D-16), OQ-4 closed | doc grep | `grep -q "SpatialCoreUI.h" .planning/REQUIREMENTS.md` | ✅ | ✅ green (2026-10-25, against the archived `.planning/milestones/v1.0.0-REQUIREMENTS.md`) |
| 06-10-T1 | 06-10 | 10 | all four | — | Phase-end gate and realtime-safety check pass as written | full gate + TSan | README "Local test gate" and "Realtime-safety check" steps 1-5 | ✅ | ✅ green, trimmed (D-18/D-19): everyday gate passes 2026-10-25; ThreadSanitizer steps skipped by decision (run before a release tag) |
| 06-10-T2 | 06-10 | 10 | CI-01 | T-06-25 | User decides merge and protection (D-11) | checkpoint | manual (see below) | — | ✅ green (decision recorded in STATE.md: no push to main without the user, no branch protection) |
| 06-10-T3 | 06-10 | 10 | all four | T-06-25, T-06-26, T-06-27 | Choice applied exactly; protection read back; ticks after evidence | API read-back + doc grep | `gh api repos/AndrewRahman/SpatialCore/branches/main/protection` | ✅ | ✅ green (2026-10-25 read-back: `Branch not protected (HTTP 404)`, matching the recorded no-protection decision) |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky or not confirmed (the row says which)*

---

## Wave 0 Requirements

- [x] `enable_testing()` at the top level of `CMakeLists.txt` and the `SPATIALCORE_BUILD_TESTS` default (06-01-T1)
- [x] `tests/Core/UmbrellaTests.cpp`, `tests/UI/UmbrellaUITests.cpp` (06-01-T2); `tests/Engine/UnsuppliedGainsTests.cpp` (06-02-T1); `[osd50]` / `[osd89]` cases (06-03-T1, T2) — each new file added to `tests/CMakeLists.txt`
- [x] `examples/harness/**` (06-04-T1) and `scripts/coverage.sh`, `scripts/coverage-report.py` (06-07-T1)
- [x] `.gitignore`: `build-cov/` (06-07-T1)

No new framework install.

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Coverage target chosen | TEST-01 (OQ-4) | User judgment on the measured number | 06-07-T3 checkpoint:decision (blocking-human), after the first CI coverage run |
| Merge to main and branch protection mode | CI-01 (D-10, D-11) | GitHub repo setting and publishing to main, owner decision | 06-10-T2 checkpoint:decision (blocking-human), after the phase-end evidence |

---

## Phase-end evidence (06-10, trimmed by the user, 2026-10-24)

Local gate on this Mac at the merge of `origin/main` (merge commit `37fef2e`, plus fix `a15f927`), run exactly as README "Local test gate" writes it:

| Step | Command | Result | Duration |
|------|---------|--------|----------|
| Debug build | `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j8` | exit 0 | 9 s (incremental) |
| Debug, DSP | `./build/tests/SpatialCoreTests` | exit 0: 401 test cases, 400 passed, 1 skipped; 330129 of 330129 assertions | 531 s |
| Debug, UI | `./build/tests/SpatialCoreUITests` | exit 0: 17 test cases, 152 assertions | 14 s |
| Release build | `cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release && cmake --build build-release -j8` | exit 0 | 11 s (incremental) |
| Release ctest | `ctest --test-dir build-release/tests --output-on-failure` | exit 0: `100% tests passed out of 418` | 130 s |
| ui: listing | `ctest --test-dir build-release/tests -N \| grep 'ui:'` | exit 0: 17 ui: tests listed | instant |
| Release ctest from the root | `ctest --test-dir build-release --no-tests=error --output-on-failure` | exit 0: `100% tests passed out of 418` | 129 s |
| Allocation and lock trap | `./build-release/tests/SpatialCoreTests "[rt]"` | exit 0: 2521 assertions in 47 test cases | 25 s |
| Coverage report | `python3 -I scripts/coverage-report.py --self-test` | exit 0: `self-test passed` | instant |

Two things the merge exposed, both fixed or recorded:

1. The first Debug `SpatialCoreTests` run failed `[bug03]` (main's new issue-reference guard): the 06-03 regression tests used the short OSD form in 7 places. Fixed in `a15f927` (references written in full; no assertion or tolerance changed); `[bug03]` then passed.
2. The same first run failed `Profile switch: every switch completes while a paced render thread runs` (`[hrtf-switch][churn]`): 15 of 16 switches done, the last one (profile 1) never became active within the 20 s wait. It did not reproduce: 3 + 10 single runs in Debug, 10 in Release, 2 runs of `[hrtf-switch]` (25 cases), one full Debug rerun and the Release ctest all passed 16 of 16. Cause not found. Logged in `deferred-items.md`; nothing was loosened.

CI evidence (no new GitHub run was started for this plan; the user forbids cloud builds):

- Last fully green CI run: **39074814315** on `310fb93` (macos-gate, linux-tests, windows-build and coverage; engine line coverage 94.19%, UI 25.89% report-only).
- `realtime-nightly` last passed in run **39058330902** on 2026-10-17 (`[rt]` 2521 assertions in 47 cases, `[tsan]` subset, UI suite and whole suite each exit 0 with 0 ThreadSanitizer warnings).
- Run **40102582138** on `86280a5` was **cancelled** under the user's no-cloud-builds rule. The `coverage` job's enforcement of the 93% target has therefore not run on GitHub; it is confirmed locally only (94.00%, and the 0.01-point-below failure case in 06-08).
- Nothing on this branch after `310fb93` has been through GitHub CI, including the merge with `origin/main`.

Skipped by decision, not by accident:

- The local ThreadSanitizer tree and the 30 to 100 minute realtime-safety check (README steps 2-5): D-18, user decision 2026-10-24. They run before a release tag.
- Branch protection on `main`: no protection for now.
- The consumer harness (`build-consumer.sh`) against the merge: it builds from a pushed commit on GitHub, so it can only run after the user allows the push.

### Second merge of main (b35bd3b), 2026-10-24

`origin/main` gained PR #33 (the #234 binaural HRIR crossfade and ITD fixes); merged locally as `be827e0` (conflict in `CLAUDE.md` only, both sides kept). Gate per decision D-19:

| Step | Command | Result | Duration |
|------|---------|--------|----------|
| Release build | `cmake --build build-release -j8` | exit 0 | 11 s |
| Release ctest | `ctest --test-dir build-release/tests --output-on-failure` | exit 0: `100% tests passed out of 418` | 147 s |
| ui: listing | `ctest --test-dir build-release/tests -N \| grep -c 'ui:'` | 17 | instant |
| Allocation and lock trap | `./build-release/tests/SpatialCoreTests "[rt]"` | exit 0: 2521 assertions in 47 test cases | 42 s |
| Coverage tool | `python3 -I scripts/coverage-report.py --self-test` | exit 0: `self-test passed` | instant |

Extra, run before D-19 was issued: full Debug `SpatialCoreTests` 401 cases (400 passed, 1 skipped), 330234 of 330234 assertions, exit 0, 488 s; Debug `SpatialCoreUITests` 17 cases, 152 assertions, 14 s; `[hrtf-switch][churn]` 5 Debug and 5 Release runs, all passed. Still nothing on GitHub for this tree.

## Validation Sign-Off

- [x] All tasks have `<automated>` verify or Wave 0 dependencies (06-07-T3 and 06-10-T2 are user decisions, listed under Manual-Only)
- [x] Sampling continuity: no 3 consecutive tasks without automated verify
- [x] Wave 0 covers all MISSING references
- [x] No watch-mode flags
- [x] Feedback latency < 200s (Release ctest 153 s)
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** validated 2026-10-25 (`/gsd-validate-phase 6`)

### Validation run 2026-10-25 (main `17224af`, tag `v0.5.0`)

Local, this Mac; no GitHub run started:

| Check | Command | Result |
|-------|---------|--------|
| Release build + ctest | `cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release`, `cmake --build build-release -j10`, `ctest --test-dir build-release/tests --output-on-failure` | exit 0: `100% tests passed out of 421`, 153 s |
| Phase 6 tags | `[umbrella]`, `[intg02]`, `[rt][intg02]`, `[osd50]`, `[osd89]`, `[multi][isolation]`, `[fftcache]`, `[bug02],[convolver][transition]`, `[rt]`; UI `[umbrella]`, `[ui][multi]` | all pass (`[rt]` 2521 assertions in 47 cases) |
| Coverage tool | `python3 -I scripts/coverage-report.py --self-test` | `self-test passed` |
| Coverage build | `scripts/coverage.sh build-cov` | exit 0, engine 93.84% (target 93%, PASS), UI 25.89%, 747 s |
| Consumer harness | `examples/harness/scripts/build-consumer.sh 17224aff553a64d2e53b242d08bb8c071daae724` | exit 0, `HarnessCheck: PASS (23 formats, 12 algorithms, profile 5 embedded)`, 243 s |
| Branch protection | `gh api repos/AndrewRahman/SpatialCore/branches/main/protection` | 404 `Branch not protected` (as decided) |

CI-only rows (Linux tests, Windows build, ThreadSanitizer in realtime-nightly) are green from the recorded CI runs named in each row, re-read with `gh run view` on 2026-10-25. Those runs predate the later merges; the Phase 6 `ci.yml` has not run on GitHub since `310fb93` and runs on the first `v*` tag whose commit does not carry `[skip ci]` (#35).

Re-check on main `f13ee59` (after PR #44, the plain AmbiX Ambisonics output, SpatialCore#32), same day: Release build exit 0; `ctest --test-dir build-release/tests --output-on-failure` exit 0, `100% tests passed out of 426`, 125 s; `[rt]` 2521 assertions in 47 cases, exit 0; coverage self-test exit 0. Coverage build and harness not re-run.

## Validation Audit 2026-10-25

| Metric | Count |
|---|---|
| Gaps found | 2 |
| Resolved | 2 |
| Escalated | 0 |
