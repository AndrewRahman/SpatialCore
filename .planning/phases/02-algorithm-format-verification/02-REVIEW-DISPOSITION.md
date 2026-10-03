---
phase: 02
review: 02-REVIEW.md
titles: json
findings:
  - id: WR-01
    severity: warning
    disposition: open
    title: "NaN decode entries are silently ignored, so the pin (and the new anchor checks) can pass vacuously"
  - id: IN-01
    severity: info
    disposition: open
    title: "The +0.1% epsilon \"smallest change the pin must catch\" is resolved on only 5 of 15 layouts"
  - id: IN-02
    severity: info
    disposition: open
    title: "`doublePrecisionAmbiDecode` drops the library's guards and shares float E with the other two decoders"
  - id: IN-03
    severity: info
    disposition: open
    title: "Comment hygiene"
  - id: WR-02
    severity: warning
    disposition: open
    title: "`LayoutState::vbapTriplets` has three kinds of entry, and the wedge kind is identified by an undocumented implicit sentinel"
  - id: WR-03
    severity: warning
    disposition: open
    title: "A regular-only triplet list still has no coverage below the horizon, and the failure is an assert on the audio thread"
  - id: WR-05
    severity: warning
    disposition: open
    title: "`appendLowerHemisphereTriplets` leaves holes when the ear-level ring has an azimuth gap of 180 degrees or more, contradicting its documented 2n output"
  - id: IN-05
    severity: info
    disposition: open
    title: "The wedge's \"pair pan = EAR QuadRegion\" equivalence assumes 0 degree ear-level speakers, but the code admits up to +/-10 degrees"
  - id: IN-06
    severity: info
    disposition: open
    title: "The test re-implements the production tier classifier, and one comment in PanningLawTests.cpp is badly reflowed"
  - id: WR-04
    severity: warning
    disposition: open
    title: "The \"SpatialCore must not be compiled with fast-math\" invariant is enforced only by comments"
  - id: IN-04
    severity: info
    disposition: open
    title: "Hold-last-good is keyed by slot and never reset when a slot is reused, which the guide's \"per object\" wording does not reflect"
open: 11
total: 11
recorded: 2026-10-03T21:59:28.317Z
---

# Phase 02: Code Review Disposition

| Finding | Severity | Disposition | Source |
|---------|----------|-------------|--------|
| WR-01 | warning | open | - |
| IN-01 | info | open | - |
| IN-02 | info | open | - |
| IN-03 | info | open | - |
| WR-02 | warning | open | - (not in the current review) |
| WR-03 | warning | open | - (not in the current review) |
| WR-05 | warning | open | - (not in the current review) |
| IN-05 | info | open | - (not in the current review) |
| IN-06 | info | open | - (not in the current review) |
| WR-04 | warning | open | - (not in the current review) |
| IN-04 | info | open | - (not in the current review) |

Dispositions: `open` (recorded, not yet triaged), `fixed`, `skipped`, `deferred`.
Set `deferred` by hand and put the reason in the Source cell; both are preserved. A `|` in the reason is kept as prose and escaped on the next run.
Re-running the gate keeps every row it can. A row the current review no longer reports is kept and its Source cell flagged, so a finding does not leave this record silently. ONE exception: when a finding id is REUSED by a different finding, the earlier decision cannot keep a row — the id is taken — and it is dropped. A RECORDED decision (anything but `open`) is named on the console when that happens; a row still at `open` is replaced silently, because `open` records no decision to lose.
