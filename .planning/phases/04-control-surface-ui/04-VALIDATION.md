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

| Req / Decision | Behavior | Test Type | Automated Command | Task | File Exists | Status |
|----------------|----------|-----------|-------------------|------|-------------|--------|
| EXTR-04 / C1 | Real UDP `/adm/obj/N/aed` from an external sender reaches the Listener and moves rendered sound | integration | `./build/tests/SpatialCoreTests "[osc][udp]"` | 04-01 T1 | ❌ W0 | ⬜ pending |
| EXTR-04 / C1, D-11, D-18 | `azim elev dist aed xyz` each move the rendered object | engine integration | `./build/tests/SpatialCoreTests "[route][osc]"` | 04-01 T1-T2 | ❌ W0 | ⬜ pending |
| EXTR-04 / C2, D-07 | 30 ± 1 msg/s at 120/60/50/30 Hz fake-clock callers, with jitter; no burst after a stall | unit | `./build/tests/SpatialCoreTests "[osc][send][rate]"` | 04-02 T1-T2 | ❌ W0 | ⬜ pending |
| EXTR-04 / C2, D-08b, D-09 | Zero messages while still; first send at (0,0,0); dead-band boundary; connect and re-enable send once | unit | `./build/tests/SpatialCoreTests "[osc][send][static],[osc][send][first],[osc][send][deadband],[osc][send][connect],[osc][send][reenable]"` | 04-02 T2 | ❌ W0 | ⬜ pending |
| EXTR-04 / D-08a, D-20 | No-argument queries reach `admPositionQueried`; the reply mirrors the query, coalesces, and goes to the configured destination | unit + integration | `./build/tests/SpatialCoreTests "[osc][query],[osc][send][query]"` | 04-03 T1-T2 | ❌ W0 | ⬜ pending |
| EXTR-04 / D-21 | Wrong-typed, non-finite, out-of-range args are ignored or clamped; compliant input unchanged | unit | `./build/tests/SpatialCoreTests "[osc][edge]"` | 04-03 T3 | ❌ W0 | ⬜ pending |
| D-15 | No `-Wunused-parameter` from `ADMOSCReceiver.h` (5 at BASE) | build check | `python3 tests/tools/check_unused_params.py` | 04-03 T3 | ❌ W0 | ⬜ pending |
| EXTR-04 / C3, D-11 | A trajectory moves the rendered audio left, then right | engine integration | `./build/tests/SpatialCoreTests "[route][trajectory]"` | 04-04 T1 | ❌ W0 | ⬜ pending |
| EXTR-04 / C3, D-13, D-19 | All 13 shapes animate forward and reverse; 10 identity, Line half-period, Bounce mirror, Random seeded | unit | `./build/tests/SpatialCoreTests "[trajectory][reverse]"` | 04-04 T2 | ❌ W0 | ⬜ pending |
| EXTR-05 / D-11 | Map drag to the left makes the left channel louder | UI + engine | `./build/tests/SpatialCoreUITests "[ui][route][map]"` | 04-05 T1 | ❌ W0 | ⬜ pending |
| EXTR-05 / C4, D-04 | Drag reports az 90.0, d 0.8; elevation preserved; clamp at 1.0 | UI unit | `./build/tests/SpatialCoreUITests "[ui][map][drag]"` | 04-05 T2 | ❌ W0 | ⬜ pending |
| EXTR-05 / C4 | Pixels: dot moves, distance rings, elevation opacity | UI unit | `./build/tests/SpatialCoreUITests "[ui][map][pixels]"` | 04-05 T2 | ❌ W0 | ⬜ pending |
| EXTR-05 / DR-16 | No concrete processor pointer in UI code | grep gate | DR-16 grep in 04-05 T2 acceptance | 04-05 T2 | n/a | ⬜ pending |
| DATA-02 / D-05, D-06, D-22 | Spy default look-and-feel sees 0 font lookups for the map; the overlay keeps its original 1 lookup (D-22 rejected at review, documented exemption) | UI unit | `./build/tests/SpatialCoreUITests "[ui][fonts][spy]"` | 04-06 T1-T2 | ❌ W0 | ⬜ pending |
| D-06 | Map render with the SML look-and-feel is byte-identical to the pre-change render and to the no-look-and-feel render | UI unit + capture | `./build/tests/SpatialCoreUITests "[ui][fonts][identical]"` plus `cmp` of `build/ui-capture-base` vs `build/ui-capture-after` | 04-06 T1 | ❌ W0 | ⬜ pending |
| DATA-02 / D-05 | Embedded font bytes equal `fonts/*.ttf`; no family-name request in UI code | UI unit | `./build/tests/SpatialCoreUITests "[ui][fonts][bytes],[ui][fonts][rule]"` | 04-06 T2 | ❌ W0 | ⬜ pending |
| EXTR-05 / D-02, D-03 | Demo (off by default) writes before/after-drag screenshots; after-drag L/R ≥ 2.0 | app run | `SpatialCoreDemo --screenshots .context/sc-shots` (04-07 T1 verify) | 04-07 T1 | ❌ W0 | ⬜ pending |
| D-12 | Demo self-test: osc, query, trajectory, map through the worked example | app run | `SpatialCoreDemo --selftest` (04-07 T2 verify) | 04-07 T2 | ❌ W0 | ⬜ pending |
| D-17 | Documented gate runs both executables in Debug and Release with `ui: ` tests listed | gate | README gate (04-08 T1 verify) | 04-08 T1 | n/a | ⬜ pending |

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
| Demo app before/after-drag screenshots look right | EXTR-05 / D-03 | One human-eye check of the rendered map | Claude runs the demo with `--screenshots .context/sc-shots`, views the PNGs and embeds them in VERIFICATION (04-07 T1 human-check) |
| SAVE PRESET title in DM Sans Bold under a per-component SML look-and-feel | D-22 | OSD sets SML per component, where this title visibly changes; only the user can accept it | Claude shows `build/ui-capture-base/overlay-sml-component.png` and `build/ui-capture-after/overlay-sml-component.png` side by side (04-06 T2 human-check) |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 60s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
