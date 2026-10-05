---
phase: 04
review: 04-REVIEW.md
titles: json
findings:
  - id: WR-01
    severity: warning
    disposition: open
    title: "A single non-finite position permanently silences an object in `ADMOSCSender::tick`"
  - id: WR-02
    severity: warning
    disposition: open
    title: "`ADMOSCSender.cpp` uses `std::isfinite` without the fast-math guard"
  - id: WR-03
    severity: warning
    disposition: open
    title: "The NaN \"axis not sent\" contract is cited as documented on `Listener::admPositionReceived`, but the header does not document it"
  - id: WR-04
    severity: warning
    disposition: open
    title: "`/xyz` overflows float for large components and returns a wrong elevation; it is also the only position form with no input range limit"
  - id: WR-05
    severity: warning
    disposition: open
    title: "Range hardening is partial: `/osd/global/*`, `/doppler`, `/pitch`, `/speed` are forwarded unbounded, and `wrapAzimuth` still loops without bound"
  - id: WR-06
    severity: warning
    disposition: open
    title: "New network tests bind fixed UDP ports and judge counts by \"quiet period\" heuristics"
  - id: IN-01
    severity: info
    disposition: open
    title: "`ADMOSCSender.h` includes the whole receiver header only for one enum"
  - id: IN-02
    severity: info
    disposition: open
    title: "`kNumQueryKinds` is not tied to the enum"
  - id: IN-03
    severity: info
    disposition: open
    title: "Object-number parsing accepts trailing garbage"
  - id: IN-04
    severity: info
    disposition: open
    title: "Duplicated test helpers and unchecked argument getters"
  - id: IN-05
    severity: info
    disposition: open
    title: "Font loading is per instance and a null typeface falls back silently"
  - id: IN-06
    severity: info
    disposition: open
    title: "Demo shares audio state between message and audio paths without a guard"
  - id: IN-07
    severity: info
    disposition: open
    title: "Font provenance rule scans comments and hard-codes a resource count"
  - id: IN-08
    severity: info
    disposition: open
    title: "Magic numbers in the dead-band"
open: 14
total: 14
recorded: 2026-10-05T10:07:48.276Z
---

# Phase 04: Code Review Disposition

| Finding | Severity | Disposition | Source |
|---------|----------|-------------|--------|
| WR-01 | warning | open | - |
| WR-02 | warning | open | - |
| WR-03 | warning | open | - |
| WR-04 | warning | open | - |
| WR-05 | warning | open | - |
| WR-06 | warning | open | - |
| IN-01 | info | open | - |
| IN-02 | info | open | - |
| IN-03 | info | open | - |
| IN-04 | info | open | - |
| IN-05 | info | open | - |
| IN-06 | info | open | - |
| IN-07 | info | open | - |
| IN-08 | info | open | - |

Dispositions: `open` (recorded, not yet triaged), `fixed`, `skipped`, `deferred`.
Set `deferred` by hand and put the reason in the Source cell; both are preserved. A `|` in the reason is kept as prose and escaped on the next run.
Re-running the gate keeps every row it can. A row the current review no longer reports is kept and its Source cell flagged, so a finding does not leave this record silently. ONE exception: when a finding id is REUSED by a different finding, the earlier decision cannot keep a row — the id is taken — and it is dropped. A RECORDED decision (anything but `open`) is named on the console when that happens; a row still at `open` is replaced silently, because `open` records no decision to lose.
