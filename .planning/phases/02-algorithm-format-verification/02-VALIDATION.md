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

Task IDs assigned by plan-phase (2026-09-30). "❌ W0" means the test is created inside the named
task (there is no separate Wave 0 plan); the task writes the test before or alongside the code it
checks. Full-suite command used by every plan:
`./build/tests/SpatialCoreTests "~HUTUBS PP2 — golden HRIR checksum at az=90deg (own control)"` (exit 0).

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 02-01-T1 | 02-01 | 1 | EXTR-01 (D-04, D-05) — tracer | T-02-03 | Below-horizon sources never reach elevated speakers | unit (e2e via RenderEngine) | `./build/tests/SpatialCoreTests "[tracer]"` | ❌ W0 (created in 02-01-T1) | ⬜ pending |
| 02-01-T1 | 02-01 | 1 | EXTR-01 (D-04/D-06b above-horizon bit-identity vs d43cb15) | — | N/A | unit | `./build/tests/SpatialCoreTests "[vbap3d-identity]"` | ❌ W0 (02-01-T1) | ⬜ pending |
| 02-01-T2 | 02-01 | 1 | EXTR-01 (D-03) | T-02-01 | N/A | unit | `./build/tests/SpatialCoreTests "[d03]"` | ❌ W0 (02-01-T2) | ⬜ pending |
| 02-01-T2 | 02-01 | 1 | EXTR-01 (D-02c, D-04 builder), Criterion 3 (`kLayoutExpectations` static_assert) | T-02-01, T-02-02 | N/A | unit + static_assert | `./build/tests/SpatialCoreTests "[io][layout]"` | ✅ (`SpeakerLayoutTests.cpp:221`, extended) | ⬜ pending |
| 02-01-T3 | 02-01 | 1 | EXTR-01 (D-01) | — | N/A | grep gate | `test -z "$(grep -rn nearestSpeaker3DFallback src/)"` | ✅ | ⬜ pending |
| 02-01-T3 | 02-01 | 1 | DR-3 (D-02b consumer surface) | T-02-03 | N/A | compile-only unit | `./build/tests/SpatialCoreTests "[consumer-surface]"` | ❌ W0 (02-01-T3) | ⬜ pending |
| 02-02-T1 | 02-02 | 1 | D-11c oracles (all three reqs) | T-02-SC | Packages verified by a human before install/use | checkpoint (blocking-human) | — (manual) | n/a | ⬜ pending |
| 02-02-T3 | 02-02 | 1 | D-11c oracles | T-02-04, T-02-05 | Reference literals are generator output only | compile + regenerate-diff | `c++ -std=c++17 -fsyntax-only -x c++ tests/reference/ShReference.h` and `diff <(.context/venv/bin/python tests/reference/gen_sh_reference.py) tests/reference/ShReference.h` (same for Ear/Panning) | ❌ W0 (02-02-T2/T3) | ⬜ pending |
| 02-03-T2 | 02-03 | 1 | EXTR-01 (D-18 issue filed) | T-02-06 | Outward action only after human approval | gh query | `gh issue list --repo AndrewRahman/SpatialCore --state open --search 'coplanar-quad triangulation in:title' --json number --jq 'length'` (>= 1) | n/a | ⬜ pending |
| 02-04-T1 | 02-04 | 2 | EXTR-01 (D-06b, D-06c, D-19i) | T-02-07, T-02-09, T-02-10 | Every algorithm returns finite gains (or zeros) and never hangs | unit (watchdog) | `./build/tests/SpatialCoreTests "[robust]"` | ❌ W0 (02-04-T1) | ⬜ pending |
| 02-04-T2 | 02-04 | 2 | EXTR-01 (D-06a, D-19ii) | T-02-08 | Non-finite az/el/distance never reach the panner; hold last good | unit | `./build/tests/SpatialCoreTests "[sanitize]"` | ❌ W0 (02-04-T2) | ⬜ pending |
| 02-04-T3 | 02-04 | 2 | EXTR-01 (D-04 ear oracle, meridian, horizon, coverage) | — | N/A | unit | `./build/tests/SpatialCoreTests "[ear]"` | ❌ W0 (02-04-T3; data from 02-02) | ⬜ pending |
| 02-05-T1 | 02-05 | 2 | overview l.27 (D-14, D-15, D-17) | T-02-13 | N/A | unit | `./build/tests/SpatialCoreTests "[vbip]"` | edit (`SpatializationAlgorithmTests.cpp:203`) | ⬜ pending |
| 02-05-T2 | 02-05 | 2 | overview l.27 (D-13, D-18 scoped continuity) | T-02-12 | N/A | unit | `./build/tests/SpatialCoreTests "[panning-law]"` | ❌ W0 (02-05-T2) | ⬜ pending |
| 02-05-T3 | 02-05 | 2 | overview l.27 (D-13 textbook, D-16), EXTR-01 (D-04 height coverage) | T-02-12 | N/A | unit | `./build/tests/SpatialCoreTests "[textbook]"` and `"[height]"` | ❌ W0 (02-05-T3) | ⬜ pending |
| 02-06-T1 | 02-06 | 3 | VERIFY-01 (D-08, D-11b/c, D-05 parity) | T-02-15 | N/A | unit | `./build/tests/SpatialCoreTests "[sn3d]"` | ❌ W0 (02-06-T1) | ⬜ pending |
| 02-06-T2 | 02-06 | 3 | EXTR-03 (D-11a, D-12, D-20), VERIFY-01 (D-10 in code) | T-02-14 | Decode cannot write out of bounds | unit + grep gate | `./build/tests/SpatialCoreTests "[decode-guard]"`, `"[roundtrip]"`, `grep -q 'Condon-Shortley'` on the three code sites | ❌ W0 (02-06-T2) | ⬜ pending |
| 02-06-T3 | 02-06 | 3 | VERIFY-01 (D-09), Criterion 3 (23 formats via engine) | T-02-16 | N/A | unit | `./build/tests/SpatialCoreTests "[ambi-pin]"` and `"[format-resolve]"` | ❌ W0 (02-06-T3) | ⬜ pending |
| 02-07-T1 | 02-07 | 4 | VERIFY-01 (D-10 docs), D-14/D-15/D-16/D-17 docs, nearest-speaker helper comment-only deprecation (Discretion; moved from 02-01-T3) | T-02-19 | N/A | grep gate | Task 1 `<verify>` in `02-07-PLAN.md` | ✅ | ⬜ pending |
| 02-07-T2 | 02-07 | 4 | D-18, F12 skill drift | T-02-19 | N/A | grep gate | Task 2 `<verify>` in `02-07-PLAN.md` | ✅ | ⬜ pending |
| 02-07-T3 | 02-07 | 4 | DR-4 phase gate, D-17 repo-wide, DR-3 OSD build | T-02-17, T-02-18 | OSD repo and plugin folders untouched | full suite + tag sweep + cross-repo build | full-suite command above, per-tag loop, `test -z "$(git grep -n 'Pulkki 2000' -- ':!.planning')"`, `cmake --build /tmp/osd-dr3-check/build --target OpenSpatialDelay OpenSpatialDelayTests` | n/a | ⬜ pending |
| 02-08-T1 | 02-08 | 1 | EXTR-01, D-04 band tracer (G-02-2) | T-02-20, T-02-21 | N/A | unit e2e via RenderEngine | `./build/tests/SpatialCoreTests "[g02-2]"` | ✅ | ⬜ pending |
| 02-08-T2 | 02-08 | 1 | EXTR-01, D-04 band oracle and continuity | T-02-22 | N/A | unit + regenerate-diff | `./build/tests/SpatialCoreTests "[band]"` and the `EarReference.h` regenerate-diff | ✅ | ⬜ pending |
| 02-09-T1 | 02-09 | 2 | EXTR-01 docs (guide, README, skill) | T-02-24 | N/A | grep gate | Task 1 `<verify>` in `02-09-PLAN.md` | ✅ | ⬜ pending |
| 02-09-T2 | 02-09 | 2 | DR-4 phase gate, DR-3 OSD build | T-02-25, T-02-26 | OSD repo and plugin folders untouched | full suite + tag sweep + cross-repo build | full-suite command above, per-tag loop (incl. `[band]`, `[g02-2]`), `cmake --build /tmp/osd-dr3-check/build --target OpenSpatialDelay OpenSpatialDelayTests` | n/a | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/reference/` generators (`gen_sh_reference.py`, `gen_ear_reference.py`, `gen_panning_reference.py`, `layouts_from_cpp.py`) + generated headers (`ShReference.h`, `EarReference.h`, `PanningReference.h`) + README with the install recipe (checked in, not run in CI) — **02-02-T2/T3**
- [ ] Pre-change characterisation: instead of a ~50-vector literal snapshot, a test-local verbatim copy of the pre-change `computeVBAPGains3D` (`git show d43cb15:src/Core/SpatialMath.cpp` lines 223-291) compared with `==` over every above-horizon direction on all 8 height layouts — **02-01-T1** `[vbap3d-identity]`; and the test-local `referenceAmbiDecode` (verbatim `git show d43cb15:src/Engine/RenderEngine.cpp` lines 650-734) — **02-06-T3** `[ambi-pin]`. Both come from git blobs, so they are pre-change by construction regardless of when they are written.
- [ ] New Catch2 tags by use: `[tracer]`, `[vbap3d-identity]`, `[d03]`, `[consumer-surface]`, `[ear]`, `[robust]`, `[sanitize]`, `[vbip]`, `[panning-law]`, `[textbook]`, `[height]`, `[sn3d]`, `[roundtrip]`, `[decode-guard]`, `[ambi-pin]`, `[format-resolve]`
- [ ] New test `.cpp` files added to `tests/CMakeLists.txt`: `Core/VBAPTripletSelectionTests.cpp` (**02-01-T1**), `Algorithms/PanningLawTests.cpp` (**02-05-T2**)
- [ ] Tie-break GitHub issue filed (RESEARCH F5) — **02-03**; number referenced in scoped-test comments — **02-05-T2/T3** and the skill — **02-07-T2**

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Release abort when a height layout yields no regular triplets | EXTR-01 (D-02a) | An abort cannot be caught by Catch2 | **02-01-T2**: region-scoped grep proves the abort is in `activateLayout`, absent from `renderBlock`, and precedes the lower-hemisphere append; the D-03 test and `[io][layout]` are the automated primary catch |
| Debug-only `jassertfalse` on the best-triplet path | EXTR-01 (D-06c) | Debug assert, not a behavior | **02-04-T1**: region-scoped grep counts exactly one in `computeVBAPGains3D`; `[robust]` exercises the path (the assert line prints in Debug) |
| OpenSpatialDelay still builds against the changed SpatialCore | DR-3 | Cross-repo build | **02-07-T3**: the executor builds OSD 30391cd (read-only `git archive` into `/tmp/osd-dr3-check`, SpatialCore symlinked to this worktree, targets `OpenSpatialDelay` and `OpenSpatialDelayTests` only); the human reviews the result and the cross-repo follow-ups via the task's `<human-check>` (end-of-phase UAT) |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 10s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
