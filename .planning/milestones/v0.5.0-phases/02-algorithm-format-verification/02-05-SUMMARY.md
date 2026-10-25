---
phase: 02-algorithm-format-verification
plan: 05
subsystem: spatial-audio-dsp
tags: [vbip, vbap, dbap, mdap, knn, ambisonics, constant-power, direct-binaural, panning-law, catch2, d-13, d-14]
status: complete

requires:
  - phase: 02-algorithm-format-verification
    provides: "02-01: EAR lower-hemisphere triplets and the engine rig pattern"
  - phase: 02-algorithm-format-verification
    provides: "02-02: tests/reference/PanningReference.h (textbook VBAP/VBIP/DBAP, 7.1.4 3D pins, MDAP port)"
  - phase: 02-algorithm-format-verification
    provides: "02-03: tie-break issue AndrewRahman/SpatialCore#22"
  - phase: 02-algorithm-format-verification
    provides: "02-04: [robust] sweep (its temporary VBIP carve-out is removed here)"
provides:
  - "Textbook VBIP (Pernaux, Boussard & Jot, DAFx-98 sec. 2.2.2): unit power everywhere, energy vector on the source"
  - "VBIP single-band docblock citing AndrewRahman/SpatialCore#20 (D-15); MDAP cited as Pulkki, IEEE WASPAA 1999 (D-17); DBAP docblock describes the code as it is (D-16)"
  - "tests/Algorithms/PanningLawTests.cpp: law table keyed by getName() over AllAlgorithmTypes, on-speaker, power, mirror, scoped continuity, textbook values, height coverage below the horizon, lower-hemisphere continuity, DirectBinaural properties"
affects: [02-07 docs and skill text (README/SKILL.md still say VBIP squares, Pulkki 2000), OpenSpatialDelay VBIP sessions (wider, up to +3 dB between speakers), Phase 3 BUG-01 (DirectBinaural elevation/front-back deliberately unasserted)]

plan_head_before: c15859d48673a5e9b10d0be2e4678cc5a34483aa
plan_head_after: c8cb2f9

actuals:
  tokens: 14275
  tasks: 3
  commits: 4

tech-stack:
  added: []
  patterns:
    - "Law table keyed by getName(): each algorithm declares its on-speaker law, power tolerance, mirror rule and continuity scope; a missing row fails the suite"
    - "Aggregate-then-CHECK: sweeps accumulate failures and the worst point, then assert once per (layout, algorithm) with the worst location in INFO"
    - "Coplanar-tie filter: only rivals of the minimum-sum winner count as ties"

key-files:
  created:
    - tests/Algorithms/PanningLawTests.cpp
  modified:
    - src/Algorithms/VBIPAlgorithm.cpp
    - include/SpatialCore/Algorithms/VBIPAlgorithm.h
    - src/Algorithms/MDAPAlgorithm.cpp
    - include/SpatialCore/Algorithms/MDAPAlgorithm.h
    - include/SpatialCore/Algorithms/DBAPAlgorithm.h
    - tests/Algorithms/SpatializationAlgorithmTests.cpp
    - tests/CMakeLists.txt
    - tests/Core/VBAPTripletSelectionTests.cpp

key-decisions:
  - "VBIP is textbook VBIP: sqrt of the VBAP gains, renormalised to unit power (D-14); single band, dual-band tracked in AndrewRahman/SpatialCore#20 (D-15)"
  - "The coplanar-tie mirror filter counts only rivals of the minimum-sum enclosing triplet; the literal any-two-triplets reading skipped every point on 7.1.6, 9.1.6 and SML13.1 because coplanar triangles always have equal gain sums"
  - "02-04's temporary VBIP exception in the [robust] power check is removed; VBIP is held to power 0 or 1 like every other algorithm"

patterns-established:
  - "Panning-law rows: adding a ninth algorithm to AllAlgorithmTypes fails 'every algorithm in AllAlgorithmTypes has declared laws' until its laws are written"

requirements-completed: [EXTR-01]

coverage:
  - id: D1
    description: "VBIP computes the textbook law: Quad az 30 gives 0.8880738 / 0.4597008; power within 1e-5 of 1 and energy vector within 1e-3 deg of the source on Quad, 5.0, 7.0, 9.1 and Octaphonic sweeps; wider than VBAP"
    requirement: "EXTR-01"
    verification:
      - kind: unit
        ref: "tests/Algorithms/SpatializationAlgorithmTests.cpp#VBIP: textbook gain vector at az=30 on Quad layout (Pernaux, Boussard & Jot, DAFx-98)"
        status: pass
      - kind: unit
        ref: "tests/Algorithms/SpatializationAlgorithmTests.cpp#VBIP: unit power and energy vector aimed at the source across 2D sweeps (D-14)"
        status: pass
      - kind: unit
        ref: "tests/Algorithms/SpatializationAlgorithmTests.cpp#VBIP: wider than VBAP between two speakers (D-14)"
        status: pass
    human_judgment: false
  - id: D2
    description: "All 8 algorithms checked against declared panning laws through engine-built contexts on all 15 layouts: law table completeness, on-speaker behaviour, unit power, mirror symmetry, scoped continuity, DirectBinaural properties"
    requirement: "EXTR-01"
    verification:
      - kind: unit
        ref: "./build/tests/SpatialCoreTests \"[panning-law]\" (10 test cases, 7175 assertions)"
        status: pass
    human_judgment: false
  - id: D3
    description: "Textbook values pinned from PanningReference.h (VBAP, VBIP, DBAP, 4 x 3D VBAP on 7.1.4) and the MDAP ring port at 5e-4"
    requirement: "EXTR-01"
    verification:
      - kind: unit
        ref: "./build/tests/SpatialCoreTests \"[textbook]\" (2 test cases)"
        status: pass
    human_judgment: false
  - id: D4
    description: "VBAP, VBIP and MDAP finite and unit-power over the whole sphere on all 8 height layouts; no elevated-speaker gain at or below -1 deg (VBAP/VBIP) or -45 deg (MDAP)"
    requirement: "EXTR-01"
    verification:
      - kind: unit
        ref: "tests/Algorithms/PanningLawTests.cpp#Panning laws: VBAP, VBIP and MDAP cover every height layout including below the horizon (D-13, D-04)"
        status: pass
    human_judgment: false
  - id: D5
    description: "VBIP, MDAP and DBAP docblocks/citations describe the code as it is (single band + #20; WASPAA 1999; a = 2, R = 12.04 dB, no blur, no hull projection)"
    requirement: "EXTR-01"
    verification:
      - kind: other
        ref: "grep gates: SpatialCore#20 and '700 Hz' in VBIPAlgorithm.h (1/1); 'Pulkki 2000' 0 and 'WASPAA 1999' 1 in MDAP .cpp/.h and the test; '12.04' and 'blur' in DBAPAlgorithm.h (1/1); git diff d43cb15 -- src/Algorithms/DBAPAlgorithm.cpp empty"
        status: pass
    human_judgment: true
    rationale: "Whether the wording is accurate and honest about VBIP and DBAP is a reading judgment; the greps only prove the required phrases are present."

duration: 10min
completed: 2026-10-01
---

# Phase 2 Plan 05: Textbook VBIP and the panning-law suite Summary

**VBIP is now textbook VBIP (Pernaux, Boussard & Jot, DAFx-98: square root of the VBAP gains, renormalised). Its power is 1 everywhere instead of 0.5-1.0, and its energy vector points at the source. A new `PanningLawTests.cpp` checks all 8 algorithms from `AllAlgorithmTypes` against declared laws on all 15 engine-built layouts. It also pins textbook values and covers the full sphere on every height layout. MDAP's citation (WASPAA 1999) and DBAP's docblock (R = 12.04 dB, no blur, no hull projection) now match the code.**

## Performance

- **Duration:** about 10 min
- **Started:** 2026-10-01T07:01:07Z
- **Completed:** 2026-10-01T07:11:37Z
- **Tasks:** 3 of 3
- **Files modified:** 9 (1 created, 8 modified)

## Accomplishments

- **Textbook VBIP (D-14).** `VBIPAlgorithm::computeGains` takes `std::sqrt (std::max (0.0f, g))` of the VBAP gains, then scales them to unit power. Quad az 30 now gives 0.8880738 / 0.4597008. The pre-fix golden `0.9330127 / 0.0669873` is gone.
- **VBIP power before/after (F1):** before, 0.50-1.00 on 2D layouts (RESEARCH F1, measured on the pre-fix code). After, 1.0 everywhere: worst |power - 1| is 4.8e-7 over the `[power]` sweeps on all 15 layouts and 3.6e-7 to 4.8e-7 over the `[height]` sphere grid. The energy vector is within 1e-3 degrees of the source on Quad, 5.0, 7.0, 9.1 and Octaphonic.
- **Docs (D-15, D-16, D-17).**
  - The VBIP docblock states the law, the energy-vector aim and the width change. It also says the paper's VBAP < 700 Hz / VBIP > 700 Hz pairing is reduced here to single-band VBIP, tracked in #20, and that below the horizon VBIP acts on the EAR-downmixed vector.
  - MDAP is cited as Pulkki, "Uniform spreading of amplitude panned virtual sources", IEEE WASPAA 1999.
  - The DBAP docblock describes a = 2, R = 12.04 dB, the d^2 clamp at 0.001, no blur, no user rolloff and no hull projection, with on-speaker unity only to about 1e-3.
- **Panning-law suite (D-13).** 10 new test cases, about 6 s in Debug:
  - The law table covers all 8 algorithms, including a DirectBinaural row marked binaural-only.
  - On-speaker behaviour, unit power and mirror symmetry are checked through a mirror map built from each layout.
  - Continuity is asserted only where D-18 allows it.
  - Textbook values and the MDAP ring cross-check are pinned.
  - Height coverage includes below the horizon.
  - Lower-hemisphere continuity is scoped.
  - DirectBinaural gets property checks only.

### Measured maximum step per continuity scope (Debug, Apple clang arm64)

Each value is the largest per-speaker |delta g| between consecutive azimuths over a full 360-degree sweep, including the step that closes the circle.

| Scope | Algorithm | Step | Measured max (layout, az) | Bound |
|-------|-----------|------|---------------------------|-------|
| Flat, el 0 / 20 / 40 | ConstantPower | 0.1 deg | 0.00392 (5.1, 173.6) | 0.02 |
| Flat, el 0 / 20 / 40 | DBAP (dist 0.5) | 0.1 deg | 0.00169 (5.1, -66.3) | 0.02 |
| Flat, el 0 / 20 / 40 | Ambisonics | 0.1 deg | 0.00384 (5.1, -20.7) | 0.02 |
| Height, el 0 / 20 / 40 | ConstantPower | 0.1 deg | 0.00403 (5.1.2, -173.9) | 0.02 |
| Height, el 0 / 20 / 40 | DBAP | 0.1 deg | 0.00141 (7.1.2, -109.3) | 0.02 |
| Height, el 0 / 20 / 40 | Ambisonics | 0.1 deg | 0.00425 (5.1.4, -22.8) | 0.02 |
| Flat horizon | VBAP | 0.1 deg | 0.00530 (9.1, 70.6) | 0.01 |
| Height horizon | VBAP | 0.1 deg | 0.00530 (9.1.6, 79.5) | 0.01 |
| Flat horizon | VBIP | 0.1 deg | 0.05907 (9.1, -59.9) | 0.08 |
| Height horizon | VBIP | 0.1 deg | 0.05907 (9.1.6, -59.9) | 0.08 |
| Flat horizon | MDAP | 0.1 deg | 0.00317 (9.1, 77.0) | 0.01 |
| Height horizon | MDAP | 0.5 deg | 0.08849 (5.1.2, 118.5) | 0.12 |
| Nadir cap (-50/-60/-75; 5.1.x -65/-75) | VBAP | 0.1 deg | 0.00354 (9.1.x, el -50) | 0.01 |
| -30..0 band (-5/-15/-25) | VBAP | 0.1 deg | 0.10374 (all but SML, el -15, az 16.7) | 0.12 |

VBIP's 0.0591 matches the derivation in the test comment, sqrt(sin 0.1 deg / sin 30 deg) = 0.059. That is the square-root shape of the textbook law next to a speaker, not a defect. MDAP on height layouts at the horizon is 0.088, down from RESEARCH's pre-D-04 0.16-0.19.

**Mirror symmetry measured:** the worst |a - b| at checked points is 1.5e-5 (Ambisonics, 5.1.2 el 20). Every other algorithm is at or below 9e-7. On height layouts the coplanar-tie filter skipped 90-180 of 360 VBAP/VBIP points per layout (0 on SML13.1, 106 on 5.1.4, 180 on 7.1.6 and 9.1.6). The KNN tie filter skipped 1-4 points per height layout and none on flat layouts.

## Task Commits

1. **Task 1: Textbook VBIP, single-band docblock, MDAP citation, DBAP golden comment** - `bafee74` (fix). TDD: the red run had 3 of 3 `[vbip]` cases failing (7203 failed assertions) before the code change.
2. **Task 2: Panning-law property suite** - `669780d` (test)
3. **Task 3: Textbook values, height coverage, DBAP docblock** - `3e97ad4` (test)
4. **Deviation fix: restore CRLF in tests/CMakeLists.txt** - `c8cb2f9` (fix)

**Plan metadata:** the docs(02-05) commit that adds this SUMMARY.

## Files Created/Modified

- `src/Algorithms/VBIPAlgorithm.cpp` - textbook transform and header comment
- `include/SpatialCore/Algorithms/VBIPAlgorithm.h` - single-band docblock, #20
- `src/Algorithms/MDAPAlgorithm.cpp`, `include/SpatialCore/Algorithms/MDAPAlgorithm.h` - WASPAA 1999 citation (comment only)
- `include/SpatialCore/Algorithms/DBAPAlgorithm.h` - as-implemented docblock (D-16); `src/Algorithms/DBAPAlgorithm.cpp` untouched
- `tests/Algorithms/SpatializationAlgorithmTests.cpp` - VBIP golden retitled and re-pinned to the reference header, 2 new `[vbip]` cases, DBAP Lossius comment plus 1e-5 reference checks, MDAP title
- `tests/Algorithms/PanningLawTests.cpp` - new, 10 test cases
- `tests/CMakeLists.txt` - registers `Algorithms/PanningLawTests.cpp` (net +1 line)
- `tests/Core/VBAPTripletSelectionTests.cpp` - 02-04's VBIP carve-out removed (see Deviation 1)

## Decisions Made

See `key-decisions`. The tie-filter scope is spelled out under Deviation 2.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking, cross-plan] Removed 02-04's temporary VBIP exception from the `[robust]` power check**
- **Found during:** Task 1
- **Issue:** Plan 02-04 let VBIP pass the robustness power check with |sum g - 1| <= 1e-4 (the pre-D-14 law). That file is outside this plan's `files_modified`, but the exception is dead once VBIP is unit-power, and leaving it would let a regression back to the old law pass silently.
- **Fix:** Deleted the `vbipPreD14` branch. VBIP is now held to `p == 0 || |p - 1| <= 1e-4` like every other algorithm.
- **Files modified:** `tests/Core/VBAPTripletSelectionTests.cpp`
- **Verification:** `[robust]` passes, 4 cases, 5276 assertions.
- **Committed in:** `bafee74`

**2. [Rule 1 - Bug in test logic] The coplanar-tie mirror filter counts only rivals of the minimum-sum triplet**
- **Found during:** Task 2
- **Issue:** Read literally ("no two enclosing regular triplets have gain sums within 1e-4 and gain vectors differing by more than 1e-3"), the filter skipped all 360 points on 7.1.6, 9.1.6 and SML13.1, so the mirror check was vacuous there. The cause is that the regular list holds every valid triple. Any two triangles drawn from one coplanar speaker set have identical gain sums (sum g = p.n / l.n), so non-minimal ties are everywhere. Those ties never reach the output, because only the minimum-sum winner is used.
- **Fix:** A point is excluded only when the minimum-sum enclosing triplet, the one `computeVBAPGains3D` selects, has a rival within 1e-4 whose gains differ by more than 1e-3. That is exactly the case where float rounding picks the output (F5, #22). A `CHECK (checked > 0)` keeps the check from going vacuous again.
- **Files modified:** `tests/Algorithms/PanningLawTests.cpp`
- **Verification:** mirror checks pass with 180-360 checked points per height layout.
- **Committed in:** `669780d`

**3. [Rule 1 - Bug, self-introduced] CRLF line endings restored in `tests/CMakeLists.txt`**
- **Found during:** pre-SUMMARY diff review
- **Issue:** The Python one-liner that inserted the source-list entry read the CRLF file in text mode and wrote it back with LF, churning all 52 lines.
- **Fix:** Restored CRLF byte-for-byte. `git diff --stat c15859d HEAD -- tests/CMakeLists.txt` now shows a 1-line insertion.
- **Committed in:** `c8cb2f9`

**Criterion-wording note (no behaviour change):** `grep -c 'reference/PanningReference.h' tests/Algorithms/PanningLawTests.cpp` must return exactly 1. Two of my comments also quoted the path, so I reworded them to "the generated PanningReference.h header". The `#include` is now the only match.

**Total deviations:** 3 auto-fixed (1 blocking cross-plan, 2 bugs).
**Impact:** None on shipped behaviour beyond the approved VBIP change. No threshold was widened, and every bound in the plan held on the first run.

## Issues Encountered

- **Runtime.** `[panning-law]` takes about 6 s in Debug. The height-coverage case alone takes 4.0 s (8 layouts x 3 algorithms x 92 elevations x 180 azimuths, on the plan's grid) and the power sweep 1.0 s. The full suite grows from about 8 s to about 14 s.
- **Full suite:** 181 test cases, 180 pass, and 1 fails: the pre-existing `HUTUBS PP2 — golden HRIR checksum at az=90deg (own control)`, owned by Phase 3. Excluding it by name, the suite exits 0 with 180 cases. The baseline was 169; this plan adds 2 `[vbip]` cases and 10 `[panning-law]` cases. The JUCE "Leaked objects: FFT" print at exit is the known false positive in `deferred-items.md`.
- **Out of scope, owned by Plan 02-07:** `README.md` and `.claude/skills/spatial-audio-dsp/SKILL.md` still say VBIP "squares" the gains, and "Pulkki 2000" survives in `README.md:27` and `SKILL.md:108` (RESEARCH F12). The OSD glossary lines 127-128 and the release note on VBIP width and level are also 02-07's.

## Known Stubs

None.

## Threat Flags

None. T-02-12 is mitigated: every pinned value comes from `PanningReference.h`, the MDAP port is labelled a cross-check, and no threshold was widened. T-02-13 is mitigated: the VBIP docblock states the law, the single-band limit and the width change.

## User Setup Required

None.

## Next Phase Readiness

- Plan 02-07 can document textbook VBIP (wider, up to +3 dB between speakers for OSD VBIP sessions) and sweep the README and skill drift listed above.
- The scoped continuity and mirror tests cite #22. If #22 is fixed later (a deterministic tie-break), the height-layout continuity exclusions and the tie filter can be tightened.

## Self-Check: PASSED

- FOUND: `tests/Algorithms/PanningLawTests.cpp`, and all 8 modified files are present.
- FOUND commits: `bafee74`, `669780d`, `3e97ad4`, `c8cb2f9`. `git rev-list --count c15859d..HEAD` = 4.
- Task 1 criteria re-run: `[vbip]` 3 cases pass; `std::sqrt (std::max (0.0f` 1; 'squared gains|tighter focus' 0/0; `SpatialCore#20` 1; `700 Hz` 1; `Pulkki 2000` 0/0/0; `WASPAA 1999` 1/1/1; `0.9330127` 0.
- Task 2 criteria: `[panning-law]` exits 0 with at least 6 cases (10); CMakeLists entry 1; `AllAlgorithmTypes` 9 with no hand-written list of the 8; `AndrewRahman/SpatialCore#22` matches the number in 02-03-SUMMARY; `[directbinaural]` exits 0.
- Task 3 criteria: `[textbook]` 2 cases exit 0; `[height]` exits 0; `reference/PanningReference.h` 1; `kVbap3D_S7_1_4` 1; `kMdapPort_` 3; `12.04` 1; `blur` 1; `git diff d43cb15 -- src/Algorithms/DBAPAlgorithm.cpp` empty.
- Plan verification: build exit 0; `[vbip]`, `[panning-law]`, `[algorithms]` (20 cases) and `[robust]` each exit 0; the suite minus HUTUBS exits 0.

---
*Phase: 02-algorithm-format-verification*
*Completed: 2026-10-01*
