---
phase: "5"
slug: "realtime-safety-hardening"
# status lifecycle: draft (seeded by plan-phase) → validated (set by validate-phase §6)
# audit-milestone §5.5 distinguishes NOT-VALIDATED (draft) from PARTIAL (validated + nyquist_compliant: true) (#2117)
status: validated
nyquist_compliant: true
wave_0_complete: true
created: "2026-10-06"
---

# Phase 5 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Catch2 v3.7.1 (FetchContent); executables `SpatialCoreTests` and `SpatialCoreUITests` |
| **Config file** | `tests/CMakeLists.txt` (explicit source lists; CRLF line endings — preserve them) |
| **Quick run command** | `./build-release/tests/SpatialCoreTests "[rt],[fftcache],[multi],[trajectory][threads]"` |
| **Full suite command** | README "Local test gate": Debug build + both executables, then Release build + `ctest --test-dir build-release/tests --output-on-failure` |
| **Sanitizer gate (D-01)** | `build-tsan` tree running `"[tsan]"`; whole suite under TSan once at the phase gate |
| **Estimated runtime** | quick ~10 s; full gate ~3 min; `[tsan]` subset a few minutes; whole suite under TSan ~30 min |

---

## Sampling Rate

- **After every task commit:** Run the quick run command plus the touched area's existing tags
- **After every plan wave:** Run the full suite command
- **Before `/gsd-verify-work`:** Full suite green, `[tsan]` clean (exit 0, zero `WARNING: ThreadSanitizer`), whole suite under TSan once, mutation evidence recorded in SUMMARY (D-07)
- **Max feedback latency:** 180 seconds (excluding the TSan phase gate)

---

## Per-Task Verification Map

Requirement → test map from RESEARCH.md. Statuses from the 2026-10-17 validate-phase run (each command run against a fresh rebuild of `build-release` and `build-tsan` at `2534f4f`):

| Requirement | Plan-Task | Behavior | Test Type | Automated Command | File Exists | Status |
|-------------|-----------|----------|-----------|-------------------|-------------|--------|
| RTSF-05 (tooling) | 05-01 T1 | Trap canary sees malloc, new, CriticalSection, std::mutex; cold-start 7.1.4 render counts zero | trap | `./build-release/tests/SpatialCoreTests "[rt][canary],[rt][tracer]"` | ✅ | ✅ green |
| RTSF-05 (NFC) | 05-01 T2 | Six Ambisonics formats render moving distance with zero allocations; before/after capture byte-identical (D-10) | trap + cmp | `./build-release/tests/SpatialCoreTests "[rt],[nfc]"`; `cmp` of `[.][ambi-nfc-capture]` output | ✅ | ✅ green |
| RTSF-01 (engine sites, D-11) | 05-02 T1 | Oversized block renders in prepared-size pieces, bit-identical to prepared-size blocks; no allocation on any path; adjacency and empty edges | unit + trap | `./build-release/tests/SpatialCoreTests "[oversized]"` | ✅ | ✅ green |
| RTSF-01 (renderer guards) | 05-02 T2 | Too-small scratch skips, keeps the last good sound, counts, no allocation in Release | unit + trap | `./build-release/tests/SpatialCoreTests "[rt][scratch]"` | ✅ | ✅ green |
| RTSF-05 (switches) | 05-03 T1 | Mid-play profile and format switches with all three flags: zero allocations and locks | trap | `./build-release/tests/SpatialCoreTests "[rt][switch]"` | ✅ | ✅ green |
| RTSF-05 (matrix) | 05-03 T2 | 23 formats x 8 flag sets, 6 profiles x 2 routes, sizes x rates, 12 algorithms | trap | `./build-release/tests/SpatialCoreTests "[rt][matrix]"` | ✅ | ✅ green |
| RTSF-05 (rest + rule) | 05-03 T3 | Trajectories, audit switch, ordering, legacy route; lock inventory allow-list | trap + source scan | `./build-release/tests/SpatialCoreTests "[rt]"` | ✅ | ✅ green |
| RTSF-02 | 05-04 T1 | Concurrent `tick` and getters race-free and in range (red at BASE under TSan) | thread + TSan | `./build-tsan/tests/SpatialCoreTests "[trajectory][threads]"` | ✅ | ✅ green |
| RTSF-02 / D-01 | 05-04 T2 | `[tsan]` subset clean; per-tick step pinned | TSan + unit | `./build-tsan/tests/SpatialCoreTests "[tsan]"`; `"[trajectory][step]"` | ✅ | ✅ green |
| RTSF-03 (wrong thread) | 05-05 T1 | Legacy call on the render thread counted; single-thread harness not | unit | `./build-release/tests/SpatialCoreTests "[hrtf-switch][misuse]"` | ✅ | ✅ green |
| RTSF-03 (in use) | 05-05 T2 | Load into an in-use renderer counted; documented flows and engine loader not | unit + thread | `./build-release/tests/SpatialCoreTests "[hrtf-switch][in-use]"` | ✅ | ✅ green |
| RTSF-03 (criterion 3) | 05-05 T3 | setHRTFProfile from a 30 Hz UI timer while rendering; legacy flow with a render thread | thread + TSan | `./build-tsan/tests/SpatialCoreTests "[hrtf-switch]"`; `"[golden]"` unchanged | ✅ | ✅ green |
| VERIFY-02 (OSD#96) | 05-06 T1 | Engine A bit-identical alone vs beside a switching engine B | stress | `./build-release/tests/SpatialCoreTests "[multi][isolation]"` | ✅ | ✅ green |
| VERIFY-02 (cache, stress) | 05-06 T2 | Cache identity, retention, cross-order, 16-thread first use; 8 engines on 8 threads | unit + stress + TSan | `./build-tsan/tests/SpatialCoreTests "[fftcache],[multi]"` | ✅ | ✅ green |
| VERIFY-02 (OSD#131) | 05-07 T1 | Sixteen live maps share one face set and paint identically | UI | `./build-release/tests/SpatialCoreUITests "[ui][multi]"` | ✅ | ✅ green |
| VERIFY-02 (UI churn) | 05-07 T2 | Churn beside live maps; UI suite clean under TSan | UI + TSan | `./build-tsan/tests/SpatialCoreUITests` | ✅ | ✅ green |
| All (gate) | 05-08 T1 | README check run as written, whole suite under TSan | gate | README "Realtime-safety check" commands | ✅ | ✅ green |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [x] `tests/Support/RealtimeGuard.h`, `tests/Support/rtguard/RTGuard.cpp`, `SpatialCoreRTGuard` CMake target (APPLE), `SPATIALCORE_SOURCE_DIR` on `SpatialCoreTests`
- [x] `tests/Engine/RealtimeSafetyTests.cpp`, `tests/Engine/MultiInstanceTests.cpp`, `tests/Binaural/SharedFFTCacheTests.cpp`, `tests/UI/MultiMapTests.cpp`; additions to `TrajectoryTests.cpp`, `BinauralRendererTests.cpp`
- [x] `[tsan]` tag on existing `[hrtf-switch]`, `[sc16]`, `[sc17]` cases
- [x] `.gitignore` entry `build-tsan/`; README "Local test gate" extension
- [x] Framework install: none

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Windows/Linux lock behaviour of JUCE fallback FFT | RTSF-05 | No macOS-side tool sees it; CI arrives in Phase 6 | Recorded as platform scope in README; verified in Phase 6 |

---

## Validation Sign-Off

- [x] All tasks have `<automated>` verify or Wave 0 dependencies
- [x] Sampling continuity: no 3 consecutive tasks without automated verify
- [x] Wave 0 covers all MISSING references
- [x] No watch-mode flags
- [x] Feedback latency < 180s
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** validated 2026-10-17

---

## Validation Audit 2026-10-17

| Metric | Count |
|--------|-------|
| Gaps found | 0 |
| Resolved | 0 |
| Escalated | 0 |

Evidence (all run in this audit, Apple Silicon macOS):
- Release, each mapped command: `[rt][canary],[rt][tracer]` 3 cases, `[rt],[nfc]` 48, `[oversized]` 3, `[rt][scratch]` 3, `[rt][switch]` 3, `[rt][matrix]` 31, `[rt]` 46 (2,514 assertions), `[trajectory][step]` 1, `[hrtf-switch][misuse]` 3, `[hrtf-switch][in-use]` 2, `[golden]` 18, `[multi][isolation]` 1, UI `[ui][multi]` 2; all exit 0.
- Release `ctest --test-dir build-release/tests`: 403/403 passed.
- `build-tsan` whole `SpatialCoreTests` suite (includes every `[tsan]`, `[trajectory][threads]`, `[fftcache]`, `[multi]` and `[hrtf-switch]` case): exit 0, 0 `WARNING: ThreadSanitizer`, 387 cases (344 passed, 43 trap cases skipped by design), 326,697 assertions.
- `build-tsan` `SpatialCoreUITests`: exit 0, 0 warnings, 16 cases, 150 assertions.
- D-10 NFC byte identity: permanent `[nfc][d10]` case green in the `[rt],[nfc]` run; the one-time before/after `cmp` is recorded in 05-01-SUMMARY.

The Debug half of the README gate was not rebuilt in this audit. The only manual-only item (Windows/Linux fallback FFT) remains assigned to Phase 6 CI-01.
