---
phase: 02
review: 02-REVIEW.md
titles: json
findings:
  - id: WR-01
    severity: warning
    disposition: open
    title: "Integration guide limits the coplanar-tie gain jumps to \"above the horizon\", but the new lower-hemisphere band has the same jumps"
  - id: WR-02
    severity: warning
    disposition: open
    title: "`LayoutState::vbapTriplets` now has mixed semantics, and nothing on the field documents it"
  - id: WR-03
    severity: warning
    disposition: open
    title: "`buildVBAPTripletsForLayout` alone yields incomplete coverage, and the failure mode is a Debug assert on the audio thread"
  - id: WR-04
    severity: warning
    disposition: open
    title: "The \"SpatialCore must not be compiled with fast-math\" invariant is enforced only by comments"
  - id: IN-01
    severity: info
    disposition: open
    title: "The guide's \"return silence\" list leaves out ConstantPower and Ambisonics, which are silent only because of argument order"
  - id: IN-02
    severity: info
    disposition: open
    title: "\"Positive azimuth toward +Y (left)\" uses AmbiX axis names, which contradict SpatialCore's own Cartesian frame"
  - id: IN-03
    severity: info
    disposition: open
    title: "`getDecodeMatrix` fails silently with no way for the caller to detect it"
  - id: IN-04
    severity: info
    disposition: open
    title: "Hold-last-good is keyed by slot and never reset when a slot is reused, which the guide's \"per object\" wording does not reflect"
open: 8
total: 8
recorded: 2026-10-01T07:45:53.017Z
---

# Phase 02: Code Review Disposition

| Finding | Severity | Disposition | Source |
|---------|----------|-------------|--------|
| WR-01 | warning | open | - |
| WR-02 | warning | open | - |
| WR-03 | warning | open | - |
| WR-04 | warning | open | - |
| IN-01 | info | open | - |
| IN-02 | info | open | - |
| IN-03 | info | open | - |
| IN-04 | info | open | - |

Dispositions: `open` (recorded, not yet triaged), `fixed`, `skipped`, `deferred`.
Set `deferred` by hand and put the reason in the Source cell; both are preserved. A `|` in the reason is kept as prose and escaped on the next run.
Re-running the gate keeps every row it can. A row the current review no longer reports is kept and its Source cell flagged, so a finding does not leave this record silently. ONE exception: when a finding id is REUSED by a different finding, the earlier decision cannot keep a row — the id is taken — and it is dropped. A RECORDED decision (anything but `open`) is named on the console when that happens; a row still at `open` is replaced silently, because `open` records no decision to lose.
