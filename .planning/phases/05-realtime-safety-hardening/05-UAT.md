---
status: testing
phase: 05-realtime-safety-hardening
source: [05-VERIFICATION.md]
started: 2026-10-06T04:05:22Z
updated: 2026-10-06T04:05:22Z
---

## Current Test

number: 1
name: Mixed trajectory reading is inaudible for the jumping shapes
expected: |
  With a Circle, Figure8, Helix or Spiral trajectory running, a position read that combines two consecutive ticks (the remaining RTSF-02 tear) produces no audible click or jump beyond the shape's own movement.
awaiting: user response

## Tests

### 1. Mixed trajectory reading is inaudible for the jumping shapes
expected: With a Circle, Figure8, Helix or Spiral trajectory running, a mixed-tick position read produces no audible artefact beyond the shape's own movement (05-04 human_judgment item).
result: [pending]

### 2. Several plugin instances in a real DAW
expected: Several OpenSpatialDelay instances with open windows in a real macOS DAW render independently, switch HRTF profiles without affecting each other, and do not crash (in-process proof only so far; OSD#131 CoreGraphics path not claimed).
result: [pending]

## Summary

total: 2
passed: 0
issues: 0
pending: 2
skipped: 0
blocked: 0

## Gaps
