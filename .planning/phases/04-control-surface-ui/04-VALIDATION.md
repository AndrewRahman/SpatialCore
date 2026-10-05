---
phase: "4"
slug: "control-surface-ui"
# status lifecycle: draft (seeded by plan-phase) → validated (set by validate-phase §6)
# audit-milestone §5.5 distinguishes NOT-VALIDATED (draft) from PARTIAL (validated + nyquist_compliant: false) (#2117)
status: draft
nyquist_compliant: false
wave_0_complete: false
created: "2026-10-05"
---

# Phase 4 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Catch2 v3.7.1 (FetchContent), CTest via `catch_discover_tests`; two executables: `SpatialCoreTests` and the new `SpatialCoreUITests` (links `SpatialCoreUI`, `TEST_PREFIX "ui: "`) |
| **Config file** | `tests/CMakeLists.txt` (hand-maintained source lists) |
| **Quick run command** | `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build --target SpatialCoreTests SpatialCoreUITests -j8 && ./build/tests/SpatialCoreTests "[osc],[trajectory],[route]" && ./build/tests/SpatialCoreUITests "[ui]"` |
| **Full suite command** | Debug: `cmake --build build -j8 && ./build/tests/SpatialCoreTests && ./build/tests/SpatialCoreUITests`. Release: `cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release && cmake --build build-release -j8 && ctest --test-dir build-release/tests --output-on-failure && test "$(ctest --test-dir build-release/tests -N \| grep -c 'ui: ')" -gt 0` |
| **Estimated runtime** | ~25 seconds for tagged runs; ~35 seconds for a clean build of the new target |

The Phase 3 gate form `--target SpatialCoreTests && ctest` is no longer valid: an unbuilt Catch2 target registers a failing `*_NOT_BUILT` test (RESEARCH Pitfall 7). The gate builds every target.

---

## Sampling Rate

- **After every task commit:** Run the quick command, narrowed to the tag of the area touched
- **After every plan wave:** Run the full Debug suite (both executables)
- **Before `/gsd-verify-work`:** Full Debug and full Release (`ctest`) green, with UI tests present in the `ctest -N` listing (D-17)
- **Max feedback latency:** 60 seconds

---

## Per-Task Verification Map

Requirement → test map (task IDs are filled in by the planner; see RESEARCH §Validation Architecture):

| Req / Decision | Behavior | Test Type | Automated Command | File Exists | Status |
|----------------|----------|-----------|-------------------|-------------|--------|
| EXTR-04 / C1 | Real UDP `/adm/obj/N/aed` reaches the Listener | integration | `./build/tests/SpatialCoreTests "[osc][udp]"` | ❌ W0 | ⬜ pending |
| EXTR-04 / C1, D-11, D-18 | `azim elev dist aed xyz` each move the rendered object | engine integration | `./build/tests/SpatialCoreTests "[route][osc]"` | ❌ W0 | ⬜ pending |
| EXTR-04 / D-21 | Wrong-typed, non-finite, out-of-range args are ignored or clamped | unit | `./build/tests/SpatialCoreTests "[osc][edge]"` | ❌ W0 | ⬜ pending |
| EXTR-04 / D-08, D-20 | No-argument queries reach `admPositionQueried`; the reply goes to the configured destination | unit | `./build/tests/SpatialCoreTests "[osc][query],[osc][send][query]"` | ❌ W0 | ⬜ pending |
| EXTR-04 / C2, D-07 | 30 ± 1 msg/s at 120/60/50/30 Hz fake-clock callers, with jitter | unit | `./build/tests/SpatialCoreTests "[osc][send][rate]"` | ❌ W0 | ⬜ pending |
| EXTR-04 / C2, D-09 | Zero messages while still; the first send at (0,0,0) goes out; connect and re-enable send once | unit | `./build/tests/SpatialCoreTests "[osc][send][static],[osc][send][first],[osc][send][connect]"` | ❌ W0 | ⬜ pending |
| EXTR-04 / C3, D-13, D-19 | All 13 shapes animate forward and reverse; 10 identity, Line half-period, Bounce mirror, Random seeded | unit | `./build/tests/SpatialCoreTests "[trajectory][reverse]"` | ❌ W0 | ⬜ pending |
| EXTR-04 / C3, D-11 | A trajectory moves the rendered audio left, then right | engine integration | `./build/tests/SpatialCoreTests "[route][trajectory]"` | ❌ W0 | ⬜ pending |
| EXTR-05 / C4, D-04 | Drag reports az 90.0, d 0.8; elevation preserved; clamp at 1.0 | UI unit | `./build/tests/SpatialCoreUITests "[ui][map][drag]"` | ❌ W0 | ⬜ pending |
| EXTR-05 / C4 | Pixels: distance ring and elevation opacity | UI unit | `./build/tests/SpatialCoreUITests "[ui][map][pixels]"` | ❌ W0 | ⬜ pending |
| EXTR-05 / D-11 | Map drag to the left makes the left channel louder | UI + engine | `./build/tests/SpatialCoreUITests "[ui][route][map]"` | ❌ W0 | ⬜ pending |
| DATA-02 / D-05 | Embedded font bytes equal `fonts/*.ttf` | UI unit | `./build/tests/SpatialCoreUITests "[ui][fonts][bytes]"` | ❌ W0 | ⬜ pending |
| DATA-02 / D-05, D-06, D-22 | Spy default look-and-feel sees 0 font lookups (map and preset overlay) | UI unit | `./build/tests/SpatialCoreUITests "[ui][fonts][spy]"` | ❌ W0 | ⬜ pending |
| D-06 | Render with the SML look-and-feel is byte-identical to the pre-change render | UI unit | `./build/tests/SpatialCoreUITests "[ui][fonts][identical]"` | ❌ W0 | ⬜ pending |
| D-15 | No `-Wunused-parameter` from `ADMOSCReceiver.h` | build | `cmake --build build --target SpatialCore 2>&1 \| grep -c "unused parameter"` returns 0 | n/a | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/UI/UITestMain.cpp`, `tests/UI/UITestSupport.h` and the `SpatialCoreUITests` CMake target (`TEST_PREFIX "ui: "`)
- [ ] `tests/Support/RouteRenderRig.h`: a shared fixture that drives `RenderEngine` with `engineComputesGains` and `engineDerivesDispatch` (D-18)
- [ ] `tests/Engine/ControlRouteTests.cpp`, plus `JUCE_MODAL_LOOPS_PERMITTED=1` on `SpatialCoreTests` for the UDP test
- [ ] Red-first tests: the font spy, sender rate and first send, the reverse sweep
- [ ] Update the documented local gate command (full build + `ctest`, with the UI listing check)

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Demo app before/after-drag screenshots look right | EXTR-05 / D-03 | One human-eye check of the rendered map | Claude runs the demo with `--screenshots .context/sc-shots`, views the PNGs and embeds them in VERIFICATION |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 60s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
