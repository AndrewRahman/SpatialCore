---
phase: "02"
slug: "algorithm-format-verification"
# status lifecycle: draft (seeded by plan-phase) → validated (set by validate-phase §6)
# audit-milestone §5.5 distinguishes NOT-VALIDATED (draft) from PARTIAL (validated + nyquist_compliant: false) (#2117)
status: draft
nyquist_compliant: false
wave_0_complete: false
created: "2026-09-30"
---

# Phase 02 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.
> Source: `02-RESEARCH.md` §Validation Architecture.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Catch2 v3.7.1 (FetchContent), CTest via `catch_discover_tests` |
| **Config file** | `tests/CMakeLists.txt` (explicit file list — new `.cpp` files are added by hand) |
| **Quick run command** | `cmake --build build --target SpatialCoreTests && ./build/tests/SpatialCoreTests "[<tag of the area touched>]"` |
| **Full suite command** | `cmake --build build --target SpatialCoreTests && ./build/tests/SpatialCoreTests` |
| **Estimated runtime** | ~8 seconds (Debug), quick tags < 3 seconds |

**Binary path:** `build/tests/SpatialCoreTests` (not `build/SpatialCoreTests`).
**Known baseline failure:** `HUTUBS PP2 — golden HRIR checksum at az=90deg (own control)` — deferred by Phase 1 (`deferred-items.md`), Phase 3 territory. The full suite is "green" when this is the only failure.

---

## Sampling Rate

- **After every task commit:** Run the quick command with the tag of the area touched
- **After every plan wave:** Run the full suite command
- **Before `/gsd-verify-work`:** Full suite green except the known HUTUBS item, plus the DR-3 OSD build checkpoint (DR-4)
- **Max feedback latency:** 10 seconds
- Property/coverage tests use fixed seeds (`std::mt19937_64 rng (42)`) and ≤ 200k points per layout.

---

## Per-Task Verification Map

Seeded per requirement/decision; the planner assigns task IDs.

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| TBD | TBD | TBD | EXTR-01 (D-01) | — | N/A | grep gate | `test -z "$(grep -rn nearestSpeaker3DFallback src/)"` | ✅ | ⬜ pending |
| TBD | TBD | TBD | EXTR-01 (D-02c) | — | N/A | unit | `./build/tests/SpatialCoreTests "[io][layout]"` | ✅ (`SpeakerLayoutTests.cpp:221`) | ⬜ pending |
| TBD | TBD | TBD | EXTR-01 (D-03) | — | N/A | unit | `./build/tests/SpatialCoreTests "[d03]"` | ❌ W0 | ⬜ pending |
| TBD | TBD | TBD | EXTR-01 (D-04/D-05) | — | N/A | unit | `./build/tests/SpatialCoreTests "[ear]"` | ❌ W0 | ⬜ pending |
| TBD | TBD | TBD | EXTR-01 (D-06a) | — | Non-finite az/el/distance never reach the panner; hold last good | unit | `./build/tests/SpatialCoreTests "[sanitize]"` | ❌ W0 | ⬜ pending |
| TBD | TBD | TBD | EXTR-01 (D-06b) | — | Every algorithm returns finite gains (or zeros) and never hangs | unit | `./build/tests/SpatialCoreTests "[robust]"` | ❌ W0 | ⬜ pending |
| TBD | TBD | TBD | VERIFY-01 (D-08, D-11b/c) | — | N/A | unit | `./build/tests/SpatialCoreTests "[sn3d]"` | ❌ W0 | ⬜ pending |
| TBD | TBD | TBD | VERIFY-01 (D-09) | — | N/A | unit | `./build/tests/SpatialCoreTests "[ambi-pin]"` | ❌ W0 | ⬜ pending |
| TBD | TBD | TBD | VERIFY-01 (D-10) | — | N/A | grep gate | `grep -c "Condon-Shortley"` over the landing sites | ✅ | ⬜ pending |
| TBD | TBD | TBD | EXTR-03 (D-11a) | — | N/A | unit | `./build/tests/SpatialCoreTests "[roundtrip]"` | ❌ W0 | ⬜ pending |
| TBD | TBD | TBD | overview l.27 (D-13, D-14, D-16) | — | N/A | unit | `./build/tests/SpatialCoreTests "[panning-law],[vbip]"` | ❌ W0 / edit | ⬜ pending |
| TBD | TBD | TBD | D-17 | — | N/A | grep gate | `test -z "$(grep -rn 'Pulkki 2000' --exclude-dir=.planning .)"` | ✅ | ⬜ pending |
| TBD | TBD | TBD | Criterion 3 | — | N/A | unit + static_assert | `./build/tests/SpatialCoreTests "[layout]"` | partial | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/reference/` generators (`gen_sh_reference.py`, `gen_ear_reference.py`, `gen_panning_reference.py`, `layouts_from_cpp.py`) + generated headers (`ShReference.h`, `EarReference.h`, `PanningReference.h`) + README with the install recipe (checked in, not run in CI)
- [ ] Pre-change characterisation snapshot: ~50 above-horizon VBAP vectors on 7.1.4 / 9.1.6 / 5.1.2, and the test-local `referenceAmbiDecode` (verbatim copy of `RenderEngine.cpp:653-734` from `bf10fac`) — must be captured before D-04 / D-09 land
- [ ] New Catch2 tags by use: `[ear]`, `[sn3d]`, `[roundtrip]`, `[sanitize]`, `[robust]`, `[panning-law]`, `[ambi-pin]`, `[d03]`
- [ ] Any new test `.cpp` added to `tests/CMakeLists.txt`
- [ ] Tie-break GitHub issue filed (RESEARCH F5); number referenced in scoped-test comments

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Release abort when a height layout yields no regular triplets | EXTR-01 (D-02a) | An abort cannot be caught by Catch2 | Code review of `RenderEngine::activateLayout`; the D-03 test and `[io][layout]` are the automated primary catch |
| Debug-only `jassertfalse` on the best-triplet path | EXTR-01 (D-06c) | Debug assert, not a behavior | Code review |
| OpenSpatialDelay still builds against the changed SpatialCore | DR-3 | Cross-repo build | Build OSD at its migration branch with the SpatialCore submodule pointed at this branch |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 10s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
