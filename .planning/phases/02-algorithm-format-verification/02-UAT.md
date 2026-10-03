---
status: diagnosed
phase: 02-algorithm-format-verification
source: [02-VERIFICATION.md]
started: 2026-10-01T07:55:50Z
updated: 2026-10-03T02:25:00Z
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
  root_cause: "appendLowerHemisphereTriplets emits each ear-level/-30deg trapezoid as all 4 overlapping triangles of a planar quad; computeVBAPGains3D's strict min-gain-sum selection then faces an exact (or 1-2 ULP) tie between two mirror-image triangles, so enumeration order and float noise pick the lean side. EAR pans each trapezoid as one QuadRegion, which collapses to the elevation-independent 2D pair pan at the source azimuth. No triangulation (fixed diagonal 0.055, blended 0.015-0.025) reproduces it; a pair-pan region per adjacent ear-level pair is exact (prototype residual <= 4.4e-7, above-horizon bit-identical on 259,920 points)."
  scope_note: "Applies to every lower-ring trapezoid on all 8 height layouts (max deviation today 0.055 / 1.23 dB; also below -30deg where wide-chord facets dip under it). Carve-out: 5.1.4's rear gap (|az| >= 111deg) — EAR there reaches the U+-135 height speakers even at el 0; matching it would break above-horizon bit-identity and reintroduce the RESEARCH F4 0.71 horizon jump. Orchestrator decision (per diagnosis recommendation, consistent with F4 lower-only hull and the 'no sound change above the horizon' constraint): keep the rear gap as is and document it as a deliberate departure. Issue #22 (above-horizon coplanar ties) stays separate and unfixed."
  artifacts:
    - path: "src/IO/SpeakerLayout.cpp"
      issue: "lines 257-339: appendLowerHemisphereTriplets emits 4 overlapping triangles per trapezoid (facet filter :298-311 keeps coplanar quads whole)"
    - path: "src/Core/SpatialMath.cpp"
      issue: "lines 279-316: single min-sum pass over a mixed lower list; :348-382 lower-hemisphere output mapping"
    - path: "include/SpatialCore/IO/SpeakerLayout.h"
      issue: "lines 20-43: VBAPTriplet — add a defaulted field only if the explicit-flag route is chosen (must stay default-constructible)"
    - path: "tests/IO/SpeakerLayoutTests.cpp"
      issue: "lines 360-364 pin nadirMask == earMask and nadirGain == 1/sqrt(earCount) for every lower triplet with nadirVertex >= 0; a zero-share wedge violates it"
    - path: "tests/Algorithms/PanningLawTests.cpp"
      issue: "band continuity bound loosened to 0.12 (3e97ad4); can tighten to the 0.01 cap bound"
    - path: "tests/reference/gen_ear_reference.py"
      issue: "band explicitly 'NOT pinned against ear'; EarReference.h can gain band cases (excluding 5.1.4 rear gap)"
    - path: "docs/integration-guide.md"
      issue: "lines 203-211 describe the band as plain EAR; plus README.md:23, .claude/skills/spatial-audio-dsp/SKILL.md:48 and :287"
  missing:
    - "Replace the 4 overlapping band triangles per adjacent ear-level pair with one pair-pan region (zero-nadir-share wedge triplet or explicit defaulted VBAPTriplet flag) whose gains equal the 2D pair pan at the source azimuth"
    - "Order the lower tiers in computeVBAPGains3D: regular triplets (unchanged, pass 0) -> nadir cap -> pair-pan wedges, with no min-sum tie among pair pans; no alloc/lock on the audio path"
    - "Keep [vbap3d-identity], DR-3 consumer-surface (signature, 4-member LayoutContext, default-constructible VBAPTriplet) and [ear][coverage]/[ear][continuity] green; update SpeakerLayoutTests:360-364 to the new representation"
    - "Pin band cases against ear 2.1.0 in EarReference.h (regenerated by gen_ear_reference.py, byte-identical) and tighten the band continuity bound; exclude and document 5.1.4's rear gap"
    - "Docs: guide/README/skill state the band now matches EAR exactly except 5.1.4's rear gap; widen WR-01 note that #22 remains above the horizon"
    - "Record OSD follow-up updates: follow-up 1 covers 4 OSD files (glossary.md:127-128, SPECIFICATION.md:491, docs/wiki/output-formats.md:88, .claude/skills/spatial-audio-dsp/SKILL.md:554); follow-up 2 gains a fifth audible change (below-horizon band now EAR-exact)"
  debug_session: .planning/debug/below-horizon-band-vs-ear.md
