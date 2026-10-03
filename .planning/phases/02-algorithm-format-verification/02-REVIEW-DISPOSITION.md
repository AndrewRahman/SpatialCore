---
phase: 02
review: 02-REVIEW.md
titles: json
findings:
  - id: WR-08
    severity: warning
    disposition: open
    title: "The guide says DBAP \"never goes silent\" and gives equal gains for a non-finite distance, but a non-finite distance with a finite direction gives silence"
  - id: IN-13
    severity: info
    disposition: open
    title: "Three files were converted from CRLF to LF wholesale inside content commits, hiding the real edits from review and blame"
  - id: IN-14
    severity: info
    disposition: open
    title: "`VBAPAlgorithm`, `VBIPAlgorithm` and `MDAPAlgorithm::computeGains` still `jassert` on the audio thread (the fixer's open question)"
  - id: IN-15
    severity: info
    disposition: open
    title: "The WR-05 gap test pins one bridged gap only, so the multi-gap path and the 2(n+2g) formula for g = 2 are unpinned"
  - id: IN-16
    severity: info
    disposition: open
    title: "`FloatSemanticsGuard.h` lists \"ADM-OSC parse guards\" among isfinite tests, but `ADMOSCReceiver.cpp` contains none"
  - id: WR-02
    severity: warning
    disposition: fixed
    title: "`LayoutState::vbapTriplets` has three kinds of entry, and the wedge kind is identified by an undocumented implicit sentinel"
  - id: WR-03
    severity: warning
    disposition: fixed
    title: "A regular-only triplet list still has no coverage below the horizon, and the failure is an assert on the audio thread"
  - id: WR-04
    severity: warning
    disposition: fixed
    title: "The \"SpatialCore must not be compiled with fast-math\" invariant is enforced only by comments"
  - id: WR-05
    severity: warning
    disposition: fixed
    title: "`appendLowerHemisphereTriplets` leaves holes when the ear-level ring has an azimuth gap of 180 degrees or more, contradicting its documented 2n output"
  - id: WR-07
    severity: warning
    disposition: fixed
    title: "The NaN-dropping `std::max` reduction fixed by WR-06 is still present in the Ambisonics codec tests"
  - id: IN-01
    severity: info
    disposition: fixed
    title: "The guide's \"return silence\" list leaves out ConstantPower and Ambisonics, which are silent only because of argument order"
  - id: IN-02
    severity: info
    disposition: fixed
    title: "\"Positive azimuth toward +Y (left)\" uses AmbiX axis names, which contradict SpatialCore's own Cartesian frame"
  - id: IN-03
    severity: info
    disposition: fixed
    title: "`getDecodeMatrix` fails silently with no way for the caller to detect it"
  - id: IN-04
    severity: info
    disposition: fixed
    title: "Hold-last-good is keyed by slot and never reset when a slot is reused, which the guide's \"per object\" wording does not reflect"
  - id: IN-05
    severity: info
    disposition: fixed
    title: "The wedge's \"pair pan = EAR QuadRegion\" equivalence assumes 0 degree ear-level speakers, but the code admits up to +/-10 degrees"
  - id: IN-06
    severity: info
    disposition: fixed
    title: "The test re-implements the production tier classifier, and one comment in PanningLawTests.cpp is badly reflowed"
  - id: IN-07
    severity: info
    disposition: fixed
    title: "The +0.1% epsilon \"smallest change the pin must catch\" is resolved on only 5 of 15 layouts"
  - id: IN-08
    severity: info
    disposition: fixed
    title: "`doublePrecisionAmbiDecode` drops the library's guards and shares float E with the other two decoders"
  - id: IN-09
    severity: info
    disposition: fixed
    title: "Comment hygiene"
  - id: IN-12
    severity: info
    disposition: fixed
    title: "Ambisonics may be silent everywhere on height layouts and still pass every panning-law check"
  - id: IN-10
    severity: info
    disposition: fixed
    title: "The new `REQUIRE`s abort the whole test case on the first non-finite entry and report no location"
  - id: IN-11
    severity: info
    disposition: fixed
    title: "The guard adds about 6.3k assertions and has no negative test proving it fires"
  - id: WR-06
    severity: warning
    disposition: fixed
    title: "NaN decode entries are silently ignored, so the pin (and the new anchor checks) can pass vacuously"
  - id: WR-01
    severity: warning
    disposition: fixed
    title: "Integration guide limits the coplanar-tie gain jumps to \"above the horizon\", but the new lower-hemisphere band has the same jumps"
open: 5
total: 24
recorded: 2026-10-03T23:34:13.705Z
---

# Phase 02: Code Review Disposition

| Finding | Severity | Disposition | Source |
|---------|----------|-------------|--------|
| WR-08 | warning | open | - |
| IN-13 | info | open | - |
| IN-14 | info | open | - |
| IN-15 | info | open | - |
| IN-16 | info | open | - |
| WR-02 | warning | fixed | 02-REVIEW-FIX.md (not in the current review) |
| WR-03 | warning | fixed | 02-REVIEW-FIX.md (not in the current review) |
| WR-04 | warning | fixed | 02-REVIEW-FIX.md (not in the current review) |
| WR-05 | warning | fixed | 02-REVIEW-FIX.md (not in the current review) |
| WR-07 | warning | fixed | 02-REVIEW-FIX.md (not in the current review) |
| IN-01 | info | fixed | 02-REVIEW-FIX.md (not in the current review) |
| IN-02 | info | fixed | 02-REVIEW-FIX.md (not in the current review) |
| IN-03 | info | fixed | 02-REVIEW-FIX.md (not in the current review) |
| IN-04 | info | fixed | 02-REVIEW-FIX.md (not in the current review) |
| IN-05 | info | fixed | 02-REVIEW-FIX.md (not in the current review) |
| IN-06 | info | fixed | 02-REVIEW-FIX.md (not in the current review) |
| IN-07 | info | fixed | 02-REVIEW-FIX.md (not in the current review) |
| IN-08 | info | fixed | 02-REVIEW-FIX.md (not in the current review) |
| IN-09 | info | fixed | 02-REVIEW-FIX.md (not in the current review) |
| IN-12 | info | fixed | 02-REVIEW-FIX.md (not in the current review) |
| IN-10 | info | fixed | 02-REVIEW-FIX.md (not in the current review) |
| IN-11 | info | fixed | 02-REVIEW-FIX.md (not in the current review) |
| WR-06 | warning | fixed | 02-REVIEW-FIX.md (not in the current review) |
| WR-01 | warning | fixed | 02-08 + 02-09: the band no longer ties; guide and skill scope #22 to above the horizon (not in the current review) |

Dispositions: `open` (recorded, not yet triaged), `fixed`, `skipped`, `deferred`.
Set `deferred` by hand and put the reason in the Source cell; both are preserved. A `|` in the reason is kept as prose and escaped on the next run.
Re-running the gate keeps every row it can. A row the current review no longer reports is kept and its Source cell flagged, so a finding does not leave this record silently. ONE exception: when a finding id is REUSED by a different finding, the earlier decision cannot keep a row — the id is taken — and it is dropped. A RECORDED decision (anything but `open`) is named on the console when that happens; a row still at `open` is replaced silently, because `open` records no decision to lose.
