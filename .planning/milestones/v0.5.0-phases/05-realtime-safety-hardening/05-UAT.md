---
status: complete
phase: 05-realtime-safety-hardening
source: [05-VERIFICATION.md]
started: 2026-10-06T04:05:22Z
updated: 2026-10-17T00:17:21Z
---

## Current Test

[testing complete]

## Tests

### 1. Mixed trajectory reading is inaudible for the jumping shapes
expected: With a Circle, Figure8, Helix or Spiral trajectory running, a mixed-tick position read produces no audible artefact beyond the shape's own movement (05-04 human_judgment item).
result: pass
evidence: |
  Run by Claude (user asked Claude to run every test itself; no human listening pass).
  Throwaway harness (not committed) rendered each shape at speed 4, 60 Hz tick, 512-sample blocks,
  48 kHz, through RenderEngine (Simple and KEMAR profile 5): A = clean per-block reads, B = every
  tick-crossing block (479 of 750) reads the worst N/N+1 axis mix. Worst mixed-point error equals
  the shape's own per-tick step (Circle 0.208, Figure8 0.211 vs 0.207, Helix 1.000, Spiral 0.506).
  Click detector (max 5 ms second-difference RMS) is identical A vs B for all 8 renders; >2 kHz
  energy share unchanged or lower in B. Audible effect is a one-tick (1/60 s) position offset, no
  click. WAVs: .context/uat05-trajectory/ (git-ignored).

### 2. Several plugin instances in a real DAW
expected: Several OpenSpatialDelay instances with open windows in a real macOS DAW render independently, switch HRTF profiles without affecting each other, and do not crash (in-process proof only so far; OSD#131 CoreGraphics path not claimed).
result: pass
evidence: |
  Run by Claude in REAPER 7.81 (macOS, Apple M4 Pro) via ReaScript: 6 instances of OpenSpatialDelay
  @30391cd built Release against this SpatialCore (symlinked), VST3. Profiles switched by rewriting
  each instance's state (configHrtfProfile) through vst_chunk, i.e. the plugin's real
  setStateInformation path.
  - Live stress, audio playing (playstate 1, transport advancing at 85/85 checks): 90 s, 379 editor
    open/close actions incl. 7 bursts opening all 6 editors at once, 59 profile switches, 0 failures.
    A first 90 s pass (426 editor actions, 60 switches) ran with the audio device still closed.
  - No crash: REAPER exited 0 at the end, no REAPER crash report.
  - Isolation: 6 instances on the same profile render bit-identical stems (A). Track 4 (profile 3)
    renders bit-identical with the other five running profiles 0,1,2,4,5 vs bypassed (B vs C).
    Each profile renders a different output (B), so the switches really applied.
  - No buzz: no non-finite samples, no clipping (peak 0.68); click metric of every stem stays under
    the source file's own transient ratio.
  - After the live stress, 5 of 6 stems are bit-identical to the pre-stress render; track 1 differs
    only in its last 0.17 s by a smooth crossfade (no click), most likely OSD's own timer finishing
    an HRTF load during that render (OSD at this pin still uses the legacy load route).
  Caveats: macOS/REAPER only (Windows/Linux are Phase 6, D-13); OSD pin 30391cd predates the
  getBlockLayout() migration.

## Summary

total: 2
passed: 2
issues: 0
pending: 0
skipped: 0
blocked: 0

## Gaps
