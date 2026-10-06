---
phase: 05
review: 05-REVIEW.md
titles: json
findings:
  - id: WR-01
    severity: warning
    disposition: open
    title: "In-use marks go stale when no block renders, so the documented legacy flow trips the detector (and `jassertfalse` in Debug)"
  - id: WR-02
    severity: warning
    disposition: open
    title: "The documented legacy flow can still load into the renderer the audio thread is about to crossfade from"
  - id: WR-03
    severity: warning
    disposition: open
    title: "Docs claim the scratch guard \"keeps the last good sound\", but the skipped `renderSourceBuffers` call outputs silence"
  - id: WR-04
    severity: warning
    disposition: open
    title: "The `[trajectory][step]` \"pin\" is vacuous and not per shape, contradicting the header"
  - id: WR-05
    severity: warning
    disposition: open
    title: "The multi-instance isolation test can pass vacuously"
  - id: WR-06
    severity: warning
    disposition: open
    title: "Public API changes with no version bump (`TrajectoryEngine` member types, new `RenderEngine` surface)"
  - id: IN-01
    severity: info
    disposition: open
    title: "Stale line-number references in the lock-inventory comment"
  - id: IN-02
    severity: info
    disposition: open
    title: "Test-only seams are public members of production classes"
  - id: IN-03
    severity: info
    disposition: open
    title: "The guard threshold is not \"the prepared size\", so the guard docs are inaccurate"
  - id: IN-04
    severity: info
    disposition: open
    title: "CLAUDE.md and README say the allocation trap is Release-only, but CMake enables it in any macOS non-sanitizer tree"
  - id: IN-05
    severity: info
    disposition: open
    title: "README describes the `[tsan]` subset more narrowly than the tags select"
  - id: IN-06
    severity: info
    disposition: open
    title: "`TrajectoryEngine::reset` stores `active_` before the values, against its own comment"
  - id: IN-07
    severity: info
    disposition: open
    title: "NFC bit-identity test checks a test-local copy of the formula, not the engine's stored coefficients"
open: 13
total: 13
recorded: 2026-10-06T04:00:56.224Z
---

# Phase 05: Code Review Disposition

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

Dispositions: `open` (recorded, not yet triaged), `fixed`, `skipped`, `deferred`.
Set `deferred` by hand and put the reason in the Source cell; both are preserved. A `|` in the reason is kept as prose and escaped on the next run.
Re-running the gate keeps every row it can. A row the current review no longer reports is kept and its Source cell flagged, so a finding does not leave this record silently. ONE exception: when a finding id is REUSED by a different finding, the earlier decision cannot keep a row — the id is taken — and it is dropped. A RECORDED decision (anything but `open`) is named on the console when that happens; a row still at `open` is replaced silently, because `open` records no decision to lose.
