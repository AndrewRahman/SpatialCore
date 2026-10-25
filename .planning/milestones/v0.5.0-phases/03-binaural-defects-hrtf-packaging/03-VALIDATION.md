---
phase: "3"
slug: "binaural-defects-hrtf-packaging"
# status lifecycle: draft (seeded by plan-phase) → validated (set by validate-phase §6)
# audit-milestone §5.5 distinguishes NOT-VALIDATED (draft) from PARTIAL (validated + nyquist_compliant: false) (#2117)
status: validated
nyquist_compliant: true
wave_0_complete: true
created: "2026-10-04"
validated: "2026-10-05"
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

| Req / Criterion | Behavior | Test Type | Automated Command | File | Plan | Status |
|---|---|---|---|---|---|---|
| BUG-01 / C1 (Simple) | front vs up90 and front vs back ≥ 2.0 dB RMS and ≥ 4.0 dB max; az 0/el 0 and az 90 bit-exact vs today | engine integration | `"[bug01][simple]"` | W0 `tests/Binaural/BinauralCueTests.cpp` | 03-04 | ✅ COVERED (12/1) |
| BUG-01 / C1 (HRTF) | same metric on all 5 profiles | engine integration | `"[bug01][hrtf]"` | W0 same | 03-04 | ✅ COVERED (40/1) |
| BUG-01 | cue-weight table, branch DC gain ±0.01 dB, finite over sweep | unit | `"[bug01][weights]"` | W0 same | 03-04 | ✅ COVERED (30/1) |
| BUG-01 | replace "no elevation/front-back assertion" case | unit | `"[panning-law][directbinaural]"` | `PanningLawTests.cpp` (edit) | 03-04 | ✅ COVERED (406/1) |
| BUG-02 / C2 | convolver vs double direct conv ≤ 2e-6, block plans {32},{64},{128},{512},{37}, mixed | unit | `"[convolver][blocksize]"` | W0 `tests/Binaural/PartitionedConvolverTests.cpp` | 03-03 | ✅ COVERED (14/1) |
| BUG-02 / C2 | engine steady-state vs 512 ref ≤ 1e-4 (Simple, KEMAR) | engine integration | `"[bug02][steady]"` | W0 | 03-03 | ✅ COVERED (33/1) |
| BUG-02 / C2 | moving-source step at 32/64/128 ≤ 1.2× the 512 value | engine integration | `"[bug02][moving]"` | W0 | 03-03 | ✅ COVERED (12/1) |
| BUG-02 | IR update during warmup/crossfade deferred and applied | unit | `"[convolver][pending-ir]"` | W0 | 03-03 | ✅ COVERED (3/1) |
| DATA-01 / C3 | embedded bytes == on-disk bytes per profile | unit | `"[hrtf-embed][bytes]"` | W0 `tests/Binaural/HRTFEmbeddedTests.cpp` | 03-05 | ✅ COVERED (30/1) |
| DATA-01 / C3 | all 5 profiles render with no shared folder | engine integration | `"[hrtf-resolve][embedded]"` | W0 | 03-05, 03-06 | ✅ COVERED (72/2) |
| DATA-01 / C3 / D-08 / D-18 | same-name shared file wins | integration | `"[hrtf-resolve][shared]"` | W0 | 03-05 | ✅ COVERED (24/1) |
| DATA-01 / C3 | missing folder → silent embedded fallback | integration | `"[hrtf-resolve][missing]"` | W0 | 03-05 | ✅ COVERED (20/1) |
| DATA-01 / D-09 | broken/LFS-stub shared file → embedded + status | integration | `"[hrtf-resolve][broken]"` | W0 | 03-05 | ✅ COVERED (40/1) |
| DATA-01 / D-06 | unresolvable profile → Failed, audio continues | engine integration | `"[hrtf-switch][failure]"` | W0 | 03-06, 03-10 | ✅ COVERED (30/1) |
| DATA-01 / D-11 | file dropped after first load used on next switch | integration | `"[hrtf-resolve][d11]"` | W0 | 03-05 | ✅ COVERED (8/1) |
| DATA-01 / D-05 | latest request wins; call returns < 50 ms | engine integration | `"[hrtf-switch][latest-wins]"` | W0 | 03-06 | ✅ COVERED (30/1) |
| DATA-01 | `prepare()` during in-flight load safe | engine integration | `"[hrtf-switch][prepare]"` | W0 | 03-06 | ✅ COVERED (22/1) |
| C4 | profile swap step ratio ≤ 1.5, no RMS dip < 0.7× at 32/64/128/512 | engine integration | `"[hrtf-switch][click]"` | W0 `tests/Engine/ProfileSwitchTests.cpp` | 03-06, 03-09 | ✅ COVERED (86/3) |
| C4 / D-15 | Simple ↔ HRTF, same metric, with opt-in flag | engine integration | `"[hrtf-switch][simple]"` | W0 | 03-09 | ✅ COVERED (89/3) |
| C4 | concurrent render + switching, finite, no deadlock | engine stress | `"[hrtf-switch][threads]"` | W0 | 03-06, 03-07 | ✅ COVERED (7/1) |
| EXTR-02 / C5 | `BinauralRenderer` unit coverage incl. ITD characterisation 44.1/48 kHz (D-16) | unit | `"[renderer]"` | W0 `tests/Binaural/BinauralRendererTests.cpp` | 03-03, 03-08 | ✅ COVERED (280/6) |
| EXTR-02 / D-14 | loudness spread printed; finite, < 20 LU | engine integration | `"[loudness]"` | W0 `tests/Binaural/ProfileLoudnessTests.cpp` | 03-08 | ✅ COVERED (26/2) |
| D-17 | libmysofa v1.3.5: goldens pass (or before/after surfaced for sign-off) | unit | `"[binaural][golden]"` | exists, edit | 03-02, 03-08 | ✅ COVERED (51/8) |

Status column: assertions / test cases, from the 2026-10-05 audit run (Debug, HEAD `58f8abb`). `W0` in the File column marks a file created in Wave 0; all exist.

---

## Wave 0 Requirements

- [x] `tests/Binaural/BinauralMetrics.h` — shared third-octave / K-weighting / click-ratio helpers
- [x] `tests/Binaural/PartitionedConvolverTests.cpp`, `BinauralRendererTests.cpp`, `HRTFEmbeddedTests.cpp`, `BinauralCueTests.cpp`, `ProfileLoudnessTests.cpp`, `tests/Engine/ProfileSwitchTests.cpp`
- [x] `tests/CMakeLists.txt` — single owner plan for source-list edits
- [x] `SPATIALCORE_EMBED_ALL_HRTF=OFF` configures and links (KEMAR-only). Re-run 2026-10-05 on HEAD `58f8abb` (after the review fixes): `libSpatialCoreHRTFData.a` 1,168,912 bytes; `"[hrtf-embed],[hrtf-resolve],[hrtf-switch][failure]"` 1101 assertions in 21 test cases pass

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Windows `%ProgramData%` shared-folder path | DATA-01 / D-11 | v1 verification is macOS-only | Implemented, unverified; noted in SUMMARY |
| OSD#234 comment text | D-13 | Outward-facing action | User approves exact text before posting |

---

## Validation Sign-Off

- [x] All tasks have `<automated>` verify or Wave 0 dependencies
- [x] Sampling continuity: no 3 consecutive tasks without automated verify
- [x] Wave 0 covers all MISSING references
- [x] No watch-mode flags
- [x] Feedback latency < 30s
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** approved 2026-10-05 (validate-phase audit, no gaps)

---

## Validation Audit 2026-10-05

| Metric | Count |
|--------|-------|
| Gaps found | 0 |
| Resolved | 0 |
| Escalated | 0 |

- 23 of 23 requirement rows COVERED; every tag matched at least one test case and passed.
- Full suite: Debug 266/266 and Release 266/266 (309,483 assertions). The exit-time `Leaked objects detected ... class FFT` line in Debug is the known SharedFFTCache false positive tracked in STATE.md, not a failure.
- The KEMAR-only (`SPATIALCORE_EMBED_ALL_HRTF=OFF`) build, previously evidenced only by a deleted `/tmp` tree (03-VERIFICATION finding), was rebuilt and re-run on the post-review code; it passes.
- Manual-only rows unchanged: Windows `%ProgramData%` path (no Windows machine), OSD#234 comment text (posted after user approval in 03-11).
