---
status: testing
phase: 04-control-surface-ui
source: [04-VERIFICATION.md]
started: 2026-10-05T10:19:51Z
updated: 2026-10-05T10:19:51Z
---

## Current Test

number: 1
name: D-03 demo map screenshots (gates EXTR-05)
expected: |
  In .context/sc-shots/before-drag.png the big red dot sits at the top of the map.
  In .context/sc-shots/after-drag.png it has moved to the far left. The other two dots do not move.
awaiting: user response

## Tests

### 1. D-03 demo map screenshots (gates EXTR-05)
expected: Big red dot moves from the top of the map to the far left between before-drag.png and after-drag.png; the other two dots stay put.
result: [pending]

### 2. D-22 SAVE PRESET title font (gates DATA-02)
expected: The title in .context/sc-shots/title-after.png (DM Sans Bold) looks acceptable compared with title-before.png. Yes keeps it; no applies the documented revert in 04-08-SUMMARY.md.
result: [pending]

## Summary

total: 2
passed: 0
issues: 0
pending: 2
skipped: 0
blocked: 0

## Gaps
