---
phase: 03
review: 03-REVIEW.md
titles: json
findings:
  - id: WR-01
    severity: warning
    disposition: open
    title: "Decoded IR size is not bounded, so a file under the 256 MB cap can force a multi-GB allocation"
  - id: WR-02
    severity: warning
    disposition: open
    title: "The loader thread has no failure path, so one throwing load leaves the engine \"Loading\" forever"
  - id: WR-03
    severity: warning
    disposition: open
    title: "The size cap is a stat-then-read check, so special files and symlinks bypass it"
  - id: WR-04
    severity: warning
    disposition: open
    title: "`prepare()` changed contract (stops and restarts a thread, can block 15 s) but is undocumented and unsynchronised with `setHRTFProfile`"
  - id: WR-05
    severity: warning
    disposition: open
    title: "The 64-sample ITD line wraps real SOFA delays, and the characterisation tests pin the defect"
  - id: WR-06
    severity: warning
    disposition: open
    title: "`engineSelectsHRTF` without `engineComputesGains` renders silence on the Simple path, with no guard"
  - id: WR-07
    severity: warning
    disposition: open
    title: "Legacy (flag-off) consumers see output changes, which sits uneasily with the \"ALWAYS maintain backward compatibility\" rule"
  - id: IN-01
    severity: info
    disposition: open
    title: "Docs state \"SOFA load up to 36 MB\" but the code accepts shared files up to 256 MB"
  - id: IN-02
    severity: info
    disposition: open
    title: "\"No lock, allocation or file access is ever on the audio side\" is stronger than the code"
  - id: IN-03
    severity: info
    disposition: open
    title: "Worker polls every 2 ms indefinitely when the audio thread is not rendering"
  - id: IN-04
    severity: info
    disposition: open
    title: "`loadFromFile` read failure leaves a stale handle behind `loaded = false`"
  - id: IN-05
    severity: info
    disposition: open
    title: "Test provenance comment names libmysofa v1.3.2; the tree pins v1.3.5"
  - id: IN-06
    severity: info
    disposition: open
    title: "`SKILL.md` and `SimpleBinauralCues.h` disagree on the measured overhead figure"
  - id: IN-07
    severity: info
    disposition: open
    title: "Test reliability, wall-clock assertions and CMake edge case"
open: 14
total: 14
recorded: 2026-10-04T17:44:48.495Z
---

# Phase 03: Code Review Disposition

| Finding | Severity | Disposition | Source |
|---------|----------|-------------|--------|
| WR-01 | warning | open | - |
| WR-02 | warning | open | - |
| WR-03 | warning | open | - |
| WR-04 | warning | open | - |
| WR-05 | warning | open | - |
| WR-06 | warning | open | - |
| WR-07 | warning | open | - |
| IN-01 | info | open | - |
| IN-02 | info | open | - |
| IN-03 | info | open | - |
| IN-04 | info | open | - |
| IN-05 | info | open | - |
| IN-06 | info | open | - |
| IN-07 | info | open | - |

Dispositions: `open` (recorded, not yet triaged), `fixed`, `skipped`, `deferred`.
Set `deferred` by hand and put the reason in the Source cell; both are preserved. A `|` in the reason is kept as prose and escaped on the next run.
Re-running the gate keeps every row it can. A row the current review no longer reports is kept and its Source cell flagged, so a finding does not leave this record silently. ONE exception: when a finding id is REUSED by a different finding, the earlier decision cannot keep a row — the id is taken — and it is dropped. A RECORDED decision (anything but `open`) is named on the console when that happens; a row still at `open` is replaced silently, because `open` records no decision to lose.
