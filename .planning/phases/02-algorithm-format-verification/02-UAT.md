---
status: complete
phase: 02-algorithm-format-verification
source: [02-VERIFICATION.md]
started: 2026-10-01T07:55:50Z
updated: 2026-10-03T01:58:53Z
---

## Current Test

[testing complete]

## Tests

### 1. DR-3 cross-repo build and OSD follow-ups
expected: OSD 30391cd build accepted; ADMOSCReceiver.h warnings deferred; four follow-ups confirmed.
result: pass
source: claude-verified
evidence: |
  Re-ran 2026-10-01 against HEAD f6aac62: `cmake --build /tmp/osd-dr3-check/build --target OpenSpatialDelay OpenSpatialDelayTests`
  -> OSD BUILD EXIT 0; `git -C ~/conductor/repos/openspatialdelay-v1 status --porcelain` empty.
  ADMOSCReceiver.h untouched in Phase 2 (`git log d43cb15..HEAD` on it is empty) -> warnings pre-existing, deferred.
  Follow-ups 2-4 line refs confirmed at OSD 30391cd (PluginProcessor.cpp :2115 / :2147 / :2262 / :2327 / :2431).
amendment: |
  Follow-up 1 is incomplete. Besides docs/wiki/glossary.md:127-128, three more live OSD surfaces describe VBIP
  as squared gains: SPECIFICATION.md:491, docs/wiki/output-formats.md:88, .claude/skills/spatial-audio-dsp/SKILL.md:554
  (Archive/ and docs/VERSION_HISTORY.md are historical, leave as-is). Follow-up 1 covers all four.
  Follow-up 2 also gains a fifth audible change if the test 2 gap closes by changing the panning (user chose B).

### 2. Below-horizon 0 to -30 degree band versus EAR
expected: Decide (a) accept as the #22 triangulation defect and correct the docs (guide, README, skill; widen WR-01 tie scope), or (b) treat as a gap for a gap-closure plan. Probe: 7.1.4 az 60 — SpatialCore 0.7210/0.6929 (-5), 0.7472/0.6646 (-15), 0.6768/0.7361 (-25) vs EAR 0.7071/0.7071; max deviation 0.057 (~0.5 dB).
result: issue
reported: "b"
severity: major
evidence: |
  Re-probed 2026-10-01 (temporary Catch2 case, removed after): 7.1.4, az 60, VBAPAlgorithm via RenderEngine context.
  el 0: M+030 0.7071 / M+090 0.7071; -5: 0.7210/0.6929; -10: 0.7343/0.6788; -15: 0.7472/0.6646;
  -20: 0.7513/0.6599; -25: 0.6768/0.7361; -30: 0.6941/0.7199.
  PyPI ear 2.1.0 (.context/venv, bs2051 4+7+0 without LFE, point_source): 0.7071/0.7071 at every elevation 0 to -30.

### 3. Residual silent degradation on hand-built LayoutContexts (WR-03)
expected: Confirm D-01/D-02 ("harden upstream") covers (1) empty triplets on a height layout falling to 2D pairwise pan in Release, and (2) builder-only contexts sending below-horizon sources to the largest-min-gain fallback; builder docblock fix is a follow-up, not a blocker.
result: pass
source: claude-recommended
reason: Only reachable by bypassing RenderEngine (outside SC-13); the engine path is covered. User accepted the recommendation with the test 2 answer; WR-03 docblock/entry-point fix stays a follow-up.

### 4. Process gates recorded only in SUMMARY narrative
expected: Confirm the delegated 'approved-recreate' (02-02) discharges the package gate, and that #22's text was approved before filing (02-03).
result: pass
source: claude-verified
evidence: |
  02-02: user's own delegation quoted in 02-02-SUMMARY ("You decide about 1 and 2 ... Pick your recommendations.").
  02-03: `gh issue view 22 -R AndrewRahman/SpatialCore` body is identical (after strip) to 02-03-ISSUE-BODY.md, the approved draft.

### 5. Judgment-tier prohibitions (17)
expected: Accept the verifier's non-binding judgments in 02-VERIFICATION.md "Prohibitions" (16 hold; 02-07 docs-match-code partly fails via test 2).
result: pass
source: claude-verified
evidence: Full suite re-run 2026-10-01 — 190 cases, 189 pass; only the pre-existing HUTUBS PP2 checksum (Phase 3) fails. The one partly-failing judgment is tracked as the test 2 gap.

## Summary

total: 5
passed: 4
issues: 1
pending: 0
skipped: 0
blocked: 0

## Gaps

- gap_id: G-02-2
  truth: "On height layouts, a source between 0 and -30 degrees elevation pans as ITU-R BS.2127 (EAR) does: the horizon pan holds (7.1.4 az 60 stays 0.7071/0.7071 on M+030/M+090), with no lean and no side flip — as the integration guide, README, skill and CONTEXT D-04 already claim"
  status: failed
  reason: "User reported: b (treat as a gap and change the panning to match EAR rather than correct the docs)"
  severity: major
  test: 2
  artifacts: []  # Filled by diagnosis
  missing: []    # Filled by diagnosis
