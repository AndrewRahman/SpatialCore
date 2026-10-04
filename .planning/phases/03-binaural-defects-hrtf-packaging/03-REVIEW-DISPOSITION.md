---
phase: 03
review: 03-REVIEW.md
titles: json
findings:
  - id: WR-01
    severity: warning
    disposition: fixed
    title: "The decoded-size bound runs after libmysofa has already resampled, so a file's declared sample rate still drives an unbounded allocation"
  - id: WR-02
    severity: warning
    disposition: fixed
    title: "The loader thread has no failure path, so one throwing load leaves the engine \"Loading\" forever"
  - id: WR-03
    severity: warning
    disposition: fixed
    title: "The size cap is a stat-then-read check, so special files and symlinks bypass it"
  - id: WR-04
    severity: warning
    disposition: fixed
    title: "`prepare()` changed contract (stops and restarts a thread, can block 15 s) but is undocumented and unsynchronised with `setHRTFProfile`"
  - id: WR-05
    severity: warning
    disposition: fixed
    title: "The 64-sample ITD line wraps real SOFA delays, and the characterisation tests pin the defect"
  - id: WR-06
    severity: warning
    disposition: fixed
    title: "`engineSelectsHRTF` without `engineComputesGains` renders silence on the Simple path, with no guard"
  - id: WR-07
    severity: warning
    disposition: fixed
    title: "Legacy (flag-off) consumers see output changes, which sits uneasily with the \"ALWAYS maintain backward compatibility\" rule"
  - id: IN-01
    severity: info
    disposition: fixed
    title: "Custom-file limits documentation misstates the shipped IR length and omits the combined float budget"
  - id: IN-02
    severity: info
    disposition: fixed
    title: "POSIX open lacks O_NOCTTY"
  - id: IN-03
    severity: info
    disposition: fixed
    title: "The throw hook sits in the consumer-facing header, and two tests have no watchdog"
  - id: IN-04
    severity: info
    disposition: fixed
    title: "`loadFromFile` read failure leaves a stale handle behind `loaded = false`"
  - id: IN-05
    severity: info
    disposition: fixed
    title: "Test provenance comment names libmysofa v1.3.2; the tree pins v1.3.5"
  - id: IN-06
    severity: info
    disposition: fixed
    title: "`SKILL.md` and `SimpleBinauralCues.h` disagree on the measured overhead figure"
  - id: IN-07
    severity: info
    disposition: fixed
    title: "Test reliability, wall-clock assertions and CMake edge case"
open: 0
total: 14
recorded: 2026-10-04T21:27:14.687Z
---

# Phase 03: Code Review Disposition

| Finding | Severity | Disposition | Source |
|---------|----------|-------------|--------|
| WR-01 | warning | fixed | 03-REVIEW-FIX.iter4.md (not in the current review) |
| WR-02 | warning | fixed | 03-REVIEW-FIX.iter2.md (not in the current review) |
| WR-03 | warning | fixed | 03-REVIEW-FIX.iter2.md (not in the current review) |
| WR-04 | warning | fixed | 03-REVIEW-FIX.iter2.md (not in the current review) |
| WR-05 | warning | fixed | 03-REVIEW-FIX.iter2.md (not in the current review) |
| WR-06 | warning | fixed | 03-REVIEW-FIX.iter2.md (not in the current review) |
| WR-07 | warning | fixed | 03-REVIEW-FIX.iter2.md (not in the current review) |
| IN-01 | info | fixed | 03-REVIEW-FIX.md (not in the current review) |
| IN-02 | info | fixed | 03-REVIEW-FIX.md (not in the current review) |
| IN-03 | info | fixed | 03-REVIEW-FIX.iter4.md (not in the current review) |
| IN-04 | info | fixed | 03-REVIEW-FIX.iter2.md (not in the current review) |
| IN-05 | info | fixed | 03-REVIEW-FIX.iter2.md (not in the current review) |
| IN-06 | info | fixed | 03-REVIEW-FIX.iter2.md (not in the current review) |
| IN-07 | info | fixed | 03-REVIEW-FIX.iter2.md (not in the current review) |

Dispositions: `open` (recorded, not yet triaged), `fixed`, `skipped`, `deferred`.
Set `deferred` by hand and put the reason in the Source cell; both are preserved. A `|` in the reason is kept as prose and escaped on the next run.
Re-running the gate keeps every row it can. A row the current review no longer reports is kept and its Source cell flagged, so a finding does not leave this record silently. ONE exception: when a finding id is REUSED by a different finding, the earlier decision cannot keep a row — the id is taken — and it is dropped. A RECORDED decision (anything but `open`) is named on the console when that happens; a row still at `open` is replaced silently, because `open` records no decision to lose.
