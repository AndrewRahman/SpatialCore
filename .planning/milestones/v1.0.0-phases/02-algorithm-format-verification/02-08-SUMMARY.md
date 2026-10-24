---
phase: 02-algorithm-format-verification
plan: 08
subsystem: spatial-audio-dsp
tags: [vbap, ear, itu-r-bs2127, lower-hemisphere, pair-pan-wedge, gap-closure, catch2]
status: complete
gap_ids: [G-02-2]

requires:
  - phase: 02-algorithm-format-verification
    provides: plan 02-01 lower-hemisphere triplets, [vbap3d-identity] bit-identity oracle, EarReference.h nadir-cap pins
provides:
  - below-horizon band (0 to -30 degrees) on height layouts pans as ITU-R BS.2127 (EAR): the horizon pair pan at the source azimuth, no lean, no side flip
  - appendLowerHemisphereTriplets emits 2n triplets (n nadir caps plus n pair-pan wedges) instead of 5n
  - three-tier triplet selection in computeVBAPGains3D (regular, cap, wedge)
  - 26 ear 2.1.0 band pins on all 8 height layouts, an independent pair-pan property test with a one-enclosing-wedge check
  - band continuity bound tightened from 0.12 to 0.01 per 0.1 degree, plus a band/cap seam scope
affects: [02-09 docs and OSD follow-ups, OpenSpatialDelay below-horizon sound on height layouts, MDAP near the horizon on height layouts]

plan_head_before: 306b0255425ec0d934ecc4589ddd74c02f7f6ed1
plan_head_after: 582fa5ac7e2242cf19e19785361ea9c150af0334

actuals:
  tokens: 10056
  tasks: 2
  commits: 2

tech-stack:
  added: []
  patterns:
    - "pair-pan wedge: a lower-hemisphere VBAPTriplet with nadirVertex >= 0, nadirMask == 0, nadirGain == 0 (no new struct member)"
    - "integer-only tier classifier (selectionTier) drives a three-pass min-sum selection"

key-files:
  created: []
  modified:
    - src/IO/SpeakerLayout.cpp
    - include/SpatialCore/IO/SpeakerLayout.h
    - src/Core/SpatialMath.cpp
    - tests/IO/SpeakerLayoutTests.cpp
    - tests/Core/VBAPTripletSelectionTests.cpp
    - tests/reference/gen_ear_reference.py
    - tests/reference/EarReference.h
    - tests/reference/README.md
    - tests/Algorithms/PanningLawTests.cpp

key-decisions:
  - "Wedge encoded in existing VBAPTriplet fields (zero nadir share), so the public struct, LayoutContext and SpatializationAlgorithm are untouched (DR-3)"
  - "5.1.4's rear gap is not special-cased; it gets the same pair-pan wedge, and its EAR difference is documented and not pinned"
  - "Above-horizon pass-0 loop body left textually unchanged, so [vbap3d-identity] stays bit-identical and issue #22 is untouched"

requirements-completed: [EXTR-01]

coverage:
  - id: D1
    description: "7.1.4 azimuth +/-60 holds 0.7071 / 0.7071 from 0 to -30 degrees through RenderEngine and VBAPAlgorithm (the user's UAT probe)"
    requirement: EXTR-01
    verification:
      - kind: unit
        ref: "tests/Core/VBAPTripletSelectionTests.cpp#EAR band tracer: 7.1.4 at azimuth +/-60 holds 0.7071 / 0.7071 from 0 to -30 degrees"
        status: pass
    human_judgment: false
  - id: D2
    description: "Lower-hemisphere list is n caps plus n pair-pan wedges per height layout, with matching pair sets and no trapezoid triangles"
    requirement: EXTR-01
    verification:
      - kind: unit
        ref: "tests/IO/SpeakerLayoutTests.cpp#SpeakerLayout: lower-hemisphere triplets are flagged, ear-level-only, and appended only to height layouts (D-04)"
        status: pass
    human_judgment: false
  - id: D3
    description: "VBAP band gains match ear 2.1.0 within 1e-5 at 26 pinned directions on all 8 height layouts, regenerate-diff empty"
    requirement: EXTR-01
    verification:
      - kind: unit
        ref: "tests/Core/VBAPTripletSelectionTests.cpp#EAR oracle: VBAP matches PyPI ear 2.1.0 in the below-horizon band on every height layout"
        status: pass
      - kind: other
        ref: "diff <(.context/venv/bin/python tests/reference/gen_ear_reference.py 2>/dev/null) tests/reference/EarReference.h"
        status: pass
    human_judgment: false
  - id: D4
    description: "VBAP band equals an independent double-precision pair pan at every 0.5 degree azimuth and 9 band elevations, with exactly one enclosing wedge and no regular or cap enclosure"
    requirement: EXTR-01
    verification:
      - kind: unit
        ref: "tests/Core/VBAPTripletSelectionTests.cpp#EAR band: below the horizon VBAP keeps the ear-level pair pan of its azimuth"
        status: pass
    human_judgment: false
  - id: D5
    description: "Band and band/cap seam continuity at most 0.01 per 0.1 degree on every height layout"
    requirement: EXTR-01
    verification:
      - kind: unit
        ref: "tests/Algorithms/PanningLawTests.cpp#Panning laws: VBAP lower-hemisphere continuity, scoped (D-04, D-18)"
        status: pass
    human_judgment: false
  - id: D6
    description: "Above-horizon output stays bit-identical to d43cb15 and the public surface (headers) is unchanged apart from comments"
    requirement: EXTR-01
    verification:
      - kind: unit
        ref: "tests/Core/VBAPTripletSelectionTests.cpp#VBAP 3D: above-horizon output is bit-identical to the pre-change function on every height layout"
        status: pass
      - kind: unit
        ref: "[consumer-surface]"
        status: pass
    human_judgment: false
  - id: D7
    description: "How the corrected below-horizon sound feels in a real OpenSpatialDelay session (user chose option B, change the panning)"
    verification: []
    human_judgment: true
    rationale: "Audible character of the band for moving sources is a listening judgment; automated tests prove the numbers equal ear's, not that the user likes the result."

duration: 5min
completed: 2026-10-03
---

# Phase 2 Plan 08: Below-horizon band matches EAR Summary

**VBAP's 0 to -30 degree band on every height layout now pans as ITU-R BS.2127: one pair-pan wedge per neighbouring ear-level pair replaces the tied trapezoid triangles, so 7.1.4 at azimuth 60 holds 0.7071 / 0.7071 at every elevation, matching ear 2.1.0 to under 3e-7.**

## Performance

- **Duration:** 5 min (executor wall clock)
- **Started:** 2026-10-03T10:15:46Z
- **Completed:** 2026-10-03T10:21:12Z
- **Tasks:** 2
- **Files modified:** 9

## Accomplishments

- **Root cause fixed at one site.** `appendLowerHemisphereTriplets` no longer emits the four overlapping triangles of each ear-level/-30 degree trapezoid (whose gain sums tie exactly or within 1-2 ULP, so enumeration order picked the lean side). It now emits, per neighbouring ear-level pair, one nadir-cap triangle (unchanged hull code, EAR VirtualNgon) and one pair-pan wedge (two real speakers plus the virtual nadir with zero share). Counts are 2n per layout (10, 10, 14, 14, 14, 18, 18, 16 for 5.1.2, 5.1.4, 7.1.2, 7.1.4, 7.1.6, 9.1.4, 9.1.6, SML13.1), asserted by the `[io][layout][ear]` test (extras == 2n, n caps, n wedges, matching pair sets).
- **Three-tier selection.** `computeVBAPGains3D` runs `pass < 3` with an integer `selectionTier` classifier (regular, cap, wedge). Pass 0's loop body is unchanged, so above the horizon `[vbap3d-identity]` (262,104 assertions) stays bit-identical; no allocation (alloc gate returns 0), `-1e-6f` tolerance kept (D-07).
- **Band proven on all 8 height layouts.** 26 ear 2.1.0 band pins (generator self-checks: QuadRegion, equal to ear's own horizon pan within 1e-12, no gain on elevated speakers, power 1, 5.1.4 within 110 degrees, plus a carve-out check that ear really feeds 5.1.4's height speakers at (180, -10)). The 13 nadir-cap pins are unchanged byte for byte.
- **Independent property test.** `earLevelPairPan` (double precision, source azimuth only) matched on 720 x 9 directions per layout, with exactly one wedge enclosing and no regular or cap triplet enclosing (skipped only within 0.01 degrees of an ear-level speaker azimuth).
- **Continuity.** The band bound drops from 0.12 to 0.01 per 0.1 degree, and a new band/cap seam scope (-35, -40) at 0.01 passes on every rig.

## Measured results

- **Red `[g02-2]` before the change** (pre-change code, 7.1.4, azimuth 60, elevation -5): speaker 0 = 0.721033931, speaker 3 = 0.692899704 (expected 0.70710678 each); azimuth -60 mirrored on speakers 1 and 4 (0.7210 / 0.6929). 24 of 203 assertions failed. This reproduces the UAT numbers exactly.
- **Largest band deviation from `earLevelPairPan`** (720 x 9 directions per layout): 1.79e-7 on 5.1.2, 5.1.4, 7.1.2, 7.1.4, 7.1.6; 2.98e-7 on 9.1.4, 9.1.6; 2.47e-7 on SML13.1 (limit 1e-5).
- **Largest deviation from the ear pins** (26 cases, engine and direct paths): 1.79e-7 (limit 1e-5).
- **MDAP horizon maximum step on height layouts after the fix** (`[panning-law][continuity] -s`, the test sweeps height layouts at a 0.5 degree step, not 0.1): 5.1.2 0.0885, 5.1.4 0.0813, 7.1.2 0.0739, 7.1.4 0.0747, 7.1.6 0.0651, 9.1.4 0.0651, 9.1.6 0.0725, SML13.1 0.0170. The worst speakers are all elevated ones (indices 5, 7, 8, 10, 12, 14), so what remains comes from MDAP's ring reaching above-horizon coplanar ties (issue #22, deliberately untouched). The diagnosis figure of 0.076-0.077 per 0.1 degree is on a different step, so the two are not directly comparable; the row bounds in `kLaws` were not changed.
- **`[vbap3d-identity]` mismatches investigated:** none.

## Task Commits

1. **Task 1: tracer, end-to-end pair-pan band on 7.1.4 azimuth 60** - `a0595dc` (fix)
2. **Task 2: ear band pins, pair-pan property test, tightened continuity** - `582fa5a` (test)

**Plan metadata:** committed separately (docs: complete plan).

## Files Created/Modified

- `src/IO/SpeakerLayout.cpp` - cap triangles plus pair-pan wedges; shared `setInverse` helper (cap arithmetic character-for-character the same); banner rewritten
- `include/SpatialCore/IO/SpeakerLayout.h` - comment-only: wedge encoding and new `appendLowerHemisphereTriplets` docblock (comments-only gate passes)
- `src/Core/SpatialMath.cpp` - `selectionTier` and three-pass selection
- `tests/IO/SpeakerLayoutTests.cpp` - exact 2n structure test
- `tests/Core/VBAPTripletSelectionTests.cpp` - `[ear][band][g02-2]` tracer, `[ear][golden][band]` oracle, `[ear][band]` property test
- `tests/reference/gen_ear_reference.py`, `EarReference.h`, `README.md` - band pins, self-checks, regenerated header, rewritten caveat
- `tests/Algorithms/PanningLawTests.cpp` - band bound 0.01, new band/cap seam scope

## Decisions Made

- Wedge uses existing fields (`nadirVertex = 2`, `nadirMask = 0`, `nadirGain = 0`), so no public member is added and the integer discriminator avoids float equality.
- 5.1.4's rear gap gets the standard wedge; its EAR difference (ear feeds U+135/U-135 there even at and above the horizon) is documented in the README and generator, not pinned and not special-cased.

## Deviations from Plan

None - plan executed exactly as written. Two cosmetic adjustments: the header-comment gate rejected continuation lines of a `/** */` block, so those were reflowed with leading `*`; and one explanatory comment in `PanningLawTests.cpp` was reworded so the acceptance greps (`0.12` count 0, `band/cap seam` count 1) hold.

**Total deviations:** 0 auto-fixed.

## Issues Encountered

- The full-suite run prints a JUCE leaked-object notice for one `FFT` instance after "All tests passed" (process exit still 0). It comes from the process-global `SharedFFTCache` singleton at teardown, which this plan does not touch; not investigated further.
- The HEAD-safety agent-branch allow-list is for parallel worktree executors; this run is sequential on `gsd-remap` per the orchestrator, and the protected-branch check passed (not default).

## Known Stubs

None.

## Threat Flags

None. No new network, auth or file-access surface; the wedges are built on the layout-build thread and `computeVBAPGains3D` gained no allocation, lock or log.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Ready for 02-09: docs/README/skill wording (band matches EAR except 5.1.4's rear gap, #22 scoped to above the horizon) and OSD follow-up notes. OSD's DR-3 consumer build is re-run there.
- OSD sessions will hear a changed below-horizon sound on height layouts (the band stops leaning), and MDAP changes slightly near the horizon there.

## Self-Check: PASSED

- All modified files exist and both task commits (`a0595dc`, `582fa5a`) are in `git log`.
- Gates re-run: `[g02-2]`, `[band]`, `[ear]`, `[vbap3d-identity]`, `[io][layout]`, `[consumer-surface]`, `[panning-law][continuity]` exit 0; full suite (HUTUBS control excluded) exit 0, 192 cases passed; regenerate-diff of `EarReference.h` empty; 13 cap rows unchanged; interface-file diff, alloc, tolerance, `pass < 3`, builder/table hunk gates all pass.

---
*Phase: 02-algorithm-format-verification*
*Completed: 2026-10-03*
