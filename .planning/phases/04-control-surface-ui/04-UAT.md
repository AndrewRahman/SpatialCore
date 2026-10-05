---
status: complete
phase: 04-control-surface-ui
source: [04-VERIFICATION.md]
started: 2026-10-05T10:19:51Z
updated: 2026-10-05T13:30:26Z
---

## Current Test

none — all tests resolved

## Tests

### 1. D-03 demo map screenshots (gates EXTR-05)
expected: Big red dot moves from the top of the map to the far left between before-drag.png and after-drag.png; the other two dots stay put.
result: pass — approved by the user (drag to d 0.8 is as designed)

### 2. D-22 SAVE PRESET title font (gates DATA-02)
expected: The title in .context/sc-shots/title-after.png (DM Sans Bold) looks acceptable compared with title-before.png. Yes keeps it; no applies the documented revert in 04-08-SUMMARY.md.
result: rejected — DM Sans Bold too small; documented revert applied (52ca7a6), original font kept

## Summary

total: 2
passed: 2
issues: 0
pending: 0
skipped: 0
blocked: 0

## Gaps
