---
phase: "5"
slug: "realtime-safety-hardening"
# status lifecycle: draft (seeded by plan-phase) → validated (set by validate-phase §6)
# audit-milestone §5.5 distinguishes NOT-VALIDATED (draft) from PARTIAL (validated + nyquist_compliant: false) (#2117)
status: draft
nyquist_compliant: false
wave_0_complete: false
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

Filled in by the planner / validate-phase. Requirement → test map from RESEARCH.md:

| Requirement | Behavior | Test Type | Automated Command | File Exists | Status |
|-------------|----------|-----------|-------------------|-------------|--------|
| RTSF-01 | Oversized block into `renderSourceBuffers` / `updateSourceHRIR`: no allocation in Release, counter increments, output defined | unit + trap | `./build-release/tests/SpatialCoreTests "[rt][scratch]"` | ❌ W0 | ⬜ pending |
| RTSF-01 (engine sites) | Oversized block through `renderBlock` allocates nothing | trap | `./build-release/tests/SpatialCoreTests "[rt-alloc][oversized]"` | partial | ⬜ pending |
| RTSF-02 | Concurrent `tick` and `getFinal*` race-free and in range | thread + TSan | `./build-tsan/tests/SpatialCoreTests "[trajectory][threads]"` | ❌ W0 | ⬜ pending |
| RTSF-03 | Legacy-route misuse counted; `setHRTFProfile` while rendering race-free | unit + TSan | `./build-release/tests/SpatialCoreTests "[hrtf-switch][misuse]"`; `./build-tsan/tests/SpatialCoreTests "[hrtf-switch]"` | ❌ W0 / ✅ | ⬜ pending |
| RTSF-05 | Alloc/lock matrix over formats, flags, profiles, block sizes, rates, algorithms (incl. Ambisonics NFC fix) | trap | `./build-release/tests/SpatialCoreTests "[rt-alloc]"` and `"[rt-lock]"` | ❌ W0 | ⬜ pending |
| RTSF-05 | Lock primitives limited to allow-list | source scan | `./build-release/tests/SpatialCoreTests "[rt][rule]"` | ❌ W0 | ⬜ pending |
| VERIFY-02 | `SharedFFTCache` identity, retention, isolation, 16-thread first use | unit + TSan | `./build-release/tests/SpatialCoreTests "[fftcache]"` | ❌ W0 | ⬜ pending |
| VERIFY-02 | 8 engines concurrently; OSD#96 isolation bit-identical | stress + TSan | `./build-release/tests/SpatialCoreTests "[multi]"` | ❌ W0 | ⬜ pending |
| VERIFY-02 (OSD#131) | Several live maps share typefaces, paint, no crash | UI | `./build-release/tests/SpatialCoreUITests "[ui][multi]"` | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/Support/RealtimeGuard.h`, `tests/Support/rtguard/RTGuard.cpp`, `SpatialCoreRTGuard` CMake target (APPLE), `SPATIALCORE_SOURCE_DIR` on `SpatialCoreTests`
- [ ] `tests/Engine/RealtimeSafetyTests.cpp`, `tests/Engine/MultiInstanceTests.cpp`, `tests/Binaural/SharedFFTCacheTests.cpp`, `tests/UI/MultiMapTests.cpp`; additions to `TrajectoryTests.cpp`, `BinauralRendererTests.cpp`
- [ ] `[tsan]` tag on existing `[hrtf-switch]`, `[sc16]`, `[sc17]` cases
- [ ] `.gitignore` entry `build-tsan/`; README "Local test gate" extension
- [ ] Framework install: none

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Windows/Linux lock behaviour of JUCE fallback FFT | RTSF-05 | No macOS-side tool sees it; CI arrives in Phase 6 | Recorded as platform scope in README; verified in Phase 6 |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 180s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
