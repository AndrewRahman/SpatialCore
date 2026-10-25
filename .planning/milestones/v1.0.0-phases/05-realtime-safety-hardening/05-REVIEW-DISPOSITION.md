---
phase: 05
review: 05-REVIEW.md
titles: json
findings:
  - id: WR-01
    severity: warning
    disposition: fixed
    title: "`prepare()` settles a pending legacy swap without resetting the crossfade state, so `isPrepareRendererFree()` reports true and the detector is skipped while the next block still fades from the replaced renderer"
  - id: WR-02
    severity: warning
    disposition: fixed
    title: "`isPrepareRendererFree()` reads the two conditions in the wrong order, so it can return true while a crossfade from the replaced renderer has just started"
  - id: WR-03
    severity: warning
    disposition: fixed
    title: "Docs claim the scratch guard \"keeps the last good sound\", but the skipped `renderSourceBuffers` call outputs silence"
  - id: WR-04
    severity: warning
    disposition: fixed
    title: "The `[trajectory][step]` \"pin\" is vacuous and not per shape, contradicting the header"
  - id: WR-05
    severity: warning
    disposition: fixed
    title: "The multi-instance isolation test can pass vacuously"
  - id: WR-06
    severity: warning
    disposition: fixed
    title: "Public API changes with no version bump (`TrajectoryEngine` member types, new `RenderEngine` surface)"
  - id: IN-01
    severity: info
    disposition: fixed
    title: "The `[hrtf-switch][ui-timer]` distinct-profile check fails under machine load"
  - id: IN-02
    severity: info
    disposition: fixed
    title: "Test-only seams are public members of production classes"
  - id: IN-03
    severity: info
    disposition: fixed
    title: "The guard threshold is not \"the prepared size\", so the guard docs are inaccurate"
  - id: IN-04
    severity: info
    disposition: fixed
    title: "CLAUDE.md and README say the allocation trap is Release-only, but CMake enables it in any macOS non-sanitizer tree"
  - id: IN-05
    severity: info
    disposition: fixed
    title: "README describes the `[tsan]` subset more narrowly than the tags select"
  - id: IN-06
    severity: info
    disposition: fixed
    title: "`TrajectoryEngine::reset` stores `active_` before the values, against its own comment"
  - id: IN-07
    severity: info
    disposition: fixed
    title: "NFC bit-identity test checks a test-local copy of the formula, not the engine's stored coefficients"
open: 0
total: 13
recorded: 2026-10-09T01:24:23.813Z
---

# Phase 05: Code Review Disposition

| Finding | Severity | Disposition | Source |
|---------|----------|-------------|--------|
| WR-01 | warning | fixed | 05-REVIEW-FIX.md (not in the current review) |
| WR-02 | warning | fixed | 05-REVIEW-FIX.md (not in the current review) |
| WR-03 | warning | fixed | 05-REVIEW-FIX.iter2.md (not in the current review) |
| WR-04 | warning | fixed | 05-REVIEW-FIX.iter2.md (not in the current review) |
| WR-05 | warning | fixed | 05-REVIEW-FIX.iter2.md (not in the current review) |
| WR-06 | warning | fixed | 05-REVIEW-FIX.iter2.md (not in the current review) |
| IN-01 | info | fixed | 05-REVIEW-FIX.md (not in the current review) |
| IN-02 | info | fixed | 05-REVIEW-FIX.iter2.md (not in the current review) |
| IN-03 | info | fixed | 05-REVIEW-FIX.iter2.md (not in the current review) |
| IN-04 | info | fixed | 05-REVIEW-FIX.iter2.md (not in the current review) |
| IN-05 | info | fixed | 05-REVIEW-FIX.iter2.md (not in the current review) |
| IN-06 | info | fixed | 05-REVIEW-FIX.iter2.md (not in the current review) |
| IN-07 | info | fixed | 05-REVIEW-FIX.iter2.md (not in the current review) |

Dispositions: `open` (recorded, not yet triaged), `fixed`, `skipped`, `deferred`.
Set `deferred` by hand and put the reason in the Source cell; both are preserved. A `|` in the reason is kept as prose and escaped on the next run.
Re-running the gate keeps every row it can. A row the current review no longer reports is kept and its Source cell flagged, so a finding does not leave this record silently. ONE exception: when a finding id is REUSED by a different finding, the earlier decision cannot keep a row — the id is taken — and it is dropped. A RECORDED decision (anything but `open`) is named on the console when that happens; a row still at `open` is replaced silently, because `open` records no decision to lose.
