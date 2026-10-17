---
phase: "06"
slug: "consumer-readiness-ci"
# status lifecycle: draft (seeded by plan-phase) → validated (set by validate-phase §6)
# audit-milestone §5.5 distinguishes NOT-VALIDATED (draft) from PARTIAL (validated + nyquist_compliant: false) (#2117)
status: draft
nyquist_compliant: false
wave_0_complete: false
created: "2026-10-17"
---

# Phase 06 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Catch2 v3.7.1 (FetchContent); `SpatialCoreTests` (387 cases) + `SpatialCoreUITests` (16 cases, ctest names prefixed `ui:`) |
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
| (filled by planner) | — | — | INTG-01 | — | Consumer build of the SHA under test, both targets, Release | integration script | `examples/harness/scripts/build-consumer.sh "$(git rev-parse HEAD)"` | ❌ W0 | ⬜ pending |
| (filled by planner) | — | — | INTG-01 | — | Umbrella headers include every public header | unit | `./build-release/tests/SpatialCoreTests "[umbrella]"` | ❌ W0 | ⬜ pending |
| (filled by planner) | — | — | INTG-02 | — | Unsupplied gains detected and counted, no false positive on HRTF/Ambisonics | unit | `./build-release/tests/SpatialCoreTests "[intg02]"` | ❌ W0 | ⬜ pending |
| (filled by planner) | — | — | INTG-02 | — | Detector allocates and locks nothing | trap | `./build-release/tests/SpatialCoreTests "[rt]"` | ✅ | ⬜ pending |
| (filled by planner) | — | — | CI-01 | — | Root ctest finds 403 tests; macOS CI green | local + CI | `ctest --test-dir build-x -N`; `gh run list -R AndrewRahman/SpatialCore --workflow ci.yml --limit 1` | ❌ W0 | ⬜ pending |
| (filled by planner) | — | — | TEST-01 | — | Coverage report with per-area numbers | script | `python3 -I scripts/coverage-report.py core-summary.json "$PWD"` | ❌ W0 | ⬜ pending |
| (filled by planner) | — | — | TEST-01 | — | OSD#50 / #89 regressions red against old behaviour | unit + mutation | `./build-release/tests/SpatialCoreTests "[osd50],[osd89]"` | ❌ W0 | ⬜ pending |
| (filled by planner) | — | — | TEST-01 | — | OSD#96, #131, #234 tests exist with recorded red evidence | existing | `./build-release/tests/SpatialCoreTests "[multi][isolation],[bug02]"` | ✅ | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `enable_testing()` at the top level of `CMakeLists.txt` (and the `SPATIALCORE_BUILD_TESTS` default decision)
- [ ] `tests/Engine/UnsuppliedGainsTests.cpp`, `tests/Core/UmbrellaTests.cpp`, `[osd50]` / `[osd89]` cases — added to `tests/CMakeLists.txt`
- [ ] `examples/harness/**` and `scripts/coverage-report.py`
- [ ] `.gitignore`: `build-cov/`

No new framework install.

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Coverage target chosen | TEST-01 (OQ-4) | User judgment on the measured number | Planned checkpoint after the first coverage run |
| Branch protection mode | CI-01 (D-11) | GitHub repo setting, owner decision | Planned checkpoint |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 200s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
