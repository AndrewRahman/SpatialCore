---
phase: "3"
slug: "binaural-defects-hrtf-packaging"
# status lifecycle: draft (seeded by plan-phase) → validated (set by validate-phase §6)
# audit-milestone §5.5 distinguishes NOT-VALIDATED (draft) from PARTIAL (validated + nyquist_compliant: false) (#2117)
status: draft
nyquist_compliant: false
wave_0_complete: false
created: "2026-10-04"
---

# Phase 3 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.
> Source: `03-RESEARCH.md` § Validation Architecture.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Catch2 v3.7.1 via FetchContent; CTest via `catch_discover_tests` |
| **Config file** | `tests/CMakeLists.txt` (hand-maintained source list) |
| **Quick run command** | `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build --target SpatialCoreTests -j8 && ./build/tests/SpatialCoreTests "[tag]"` |
| **Full suite command** | `./build/tests/SpatialCoreTests` (Debug) and `cmake --build build-release --target SpatialCoreTests -j8 && ctest --test-dir build-release/tests --output-on-failure` (Release) |
| **Estimated runtime** | ~10 s per tag (Debug); `[loudness]` and 5-profile sweeps ≤ ~30 s |

Baseline before this phase: Debug 208/209 (only `HutubsPP2Tests.cpp:47`), Release 209/209.

---

## Sampling Rate

- **After every task commit:** Run the quick command with the tag of the area touched
- **After every plan wave:** Run the full Debug suite
- **Before `/gsd-verify-work`:** Full suite green in Debug **and** Release
- **Max feedback latency:** 30 seconds

---

## Requirement → Test Map

| Req / Criterion | Behavior | Test Type | Automated Command | File |
|---|---|---|---|---|
| BUG-01 / C1 (Simple) | front vs up90 and front vs back ≥ 2.0 dB RMS and ≥ 4.0 dB max; az 0/el 0 and az 90 bit-exact vs today | engine integration | `"[bug01][simple]"` | W0 `tests/Binaural/BinauralCueTests.cpp` |
| BUG-01 / C1 (HRTF) | same metric on all 5 profiles | engine integration | `"[bug01][hrtf]"` | W0 same |
| BUG-01 | cue-weight table, branch DC gain ±0.01 dB, finite over sweep | unit | `"[bug01][weights]"` | W0 same |
| BUG-01 | replace "no elevation/front-back assertion" case | unit | `"[panning-law][directbinaural]"` | `PanningLawTests.cpp` (edit) |
| BUG-02 / C2 | convolver vs double direct conv ≤ 2e-6, block plans {32},{64},{128},{512},{37}, mixed | unit | `"[convolver][blocksize]"` | W0 `tests/Binaural/PartitionedConvolverTests.cpp` |
| BUG-02 / C2 | engine steady-state vs 512 ref ≤ 1e-4 (Simple, KEMAR) | engine integration | `"[bug02][steady]"` | W0 |
| BUG-02 / C2 | moving-source step at 32/64/128 ≤ 1.2× the 512 value | engine integration | `"[bug02][moving]"` | W0 |
| BUG-02 | IR update during warmup/crossfade deferred and applied | unit | `"[convolver][pending-ir]"` | W0 |
| DATA-01 / C3 | embedded bytes == on-disk bytes per profile | unit | `"[hrtf-embed][bytes]"` | W0 `tests/Binaural/HRTFEmbeddedTests.cpp` |
| DATA-01 / C3 | all 5 profiles render with no shared folder | engine integration | `"[hrtf-resolve][embedded]"` | W0 |
| DATA-01 / C3 / D-08 / D-18 | same-name shared file wins | integration | `"[hrtf-resolve][shared]"` | W0 |
| DATA-01 / C3 | missing folder → silent embedded fallback | integration | `"[hrtf-resolve][missing]"` | W0 |
| DATA-01 / D-09 | broken/LFS-stub shared file → embedded + status | integration | `"[hrtf-resolve][broken]"` | W0 |
| DATA-01 / D-06 | unresolvable profile → Failed, audio continues | engine integration | `"[hrtf-switch][failure]"` | W0 |
| DATA-01 / D-11 | file dropped after first load used on next switch | integration | `"[hrtf-resolve][d11]"` | W0 |
| DATA-01 / D-05 | latest request wins; call returns < 50 ms | engine integration | `"[hrtf-switch][latest-wins]"` | W0 |
| DATA-01 | `prepare()` during in-flight load safe | engine integration | `"[hrtf-switch][prepare]"` | W0 |
| C4 | profile swap step ratio ≤ 1.5, no RMS dip < 0.7× at 32/64/128/512 | engine integration | `"[hrtf-switch][click]"` | W0 `tests/Engine/ProfileSwitchTests.cpp` |
| C4 / D-15 | Simple ↔ HRTF, same metric, with opt-in flag | engine integration | `"[hrtf-switch][simple]"` | W0 |
| C4 | concurrent render + switching, finite, no deadlock | engine stress | `"[hrtf-switch][threads]"` | W0 |
| EXTR-02 / C5 | `BinauralRenderer` unit coverage incl. ITD characterisation 44.1/48 kHz (D-16) | unit | `"[renderer]"` | W0 `tests/Binaural/BinauralRendererTests.cpp` |
| EXTR-02 / D-14 | loudness spread printed; finite, < 20 LU | engine integration | `"[loudness]"` | W0 `tests/Binaural/ProfileLoudnessTests.cpp` |
| D-17 | libmysofa v1.3.5: goldens pass (or before/after surfaced for sign-off) | unit | `"[binaural][golden]"` | exists, edit |

Per-task IDs are filled in once PLAN.md files exist.

---

## Wave 0 Requirements

- [ ] `tests/Binaural/BinauralMetrics.h` — shared third-octave / K-weighting / click-ratio helpers
- [ ] `tests/Binaural/PartitionedConvolverTests.cpp`, `BinauralRendererTests.cpp`, `HRTFEmbeddedTests.cpp`, `BinauralCueTests.cpp`, `ProfileLoudnessTests.cpp`, `tests/Engine/ProfileSwitchTests.cpp`
- [ ] `tests/CMakeLists.txt` — single owner plan for source-list edits
- [ ] `SPATIALCORE_EMBED_ALL_HRTF=OFF` configures and links (KEMAR-only)

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Windows `%ProgramData%` shared-folder path | DATA-01 / D-11 | v1 verification is macOS-only | Implemented, unverified; noted in SUMMARY |
| OSD#234 comment text | D-13 | Outward-facing action | User approves exact text before posting |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 30s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
